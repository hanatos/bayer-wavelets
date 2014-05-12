#pragma once
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#define CLAMP(A, L, H) ((A) > (L) ? ((A) < (H) ? (A) : (H)) : (L))
#define MAX(A, B) (((A) > (B)) ? (A) : (B))
#define MIN(A, B) (((A) < (B)) ? (A) : (B))

typedef enum buffer_type_t
{
  s_buf_none,                // error type
  s_buf_raw,                 // 1x uint16_t per pixel, normalised to max = 0xffff
  s_buf_float,               // 3x float per pixel in [0,1]
  s_buf_raw_stabilise,       // raw, but evaluation should take variance stabilisation into account
  s_buf_float_backtransform, // float, but evaluation should undo the variance transform.
}
buffer_type_t;

typedef struct buffer_t
{
  buffer_type_t type;        // type, see above
  void *data;                // allocated data, depends on type
  int width, height;         // dimensions of the buffer
  float noise_a, noise_b;    // noise variance model parameters
}
buffer_t;

static inline int buffer_get_channel(
    const int x,
    const int y)
{
  // equivalent to dcraw's FC()
  // hardcoded rggb for now. should work on x-trans style sensors, too.
  const int ch[4] = {0, 1, 1, 2};
  return ch[(x&1)+2*(y&1)];
}

static inline float buffer_get(
    const buffer_t *b,
    int x,
    int y,
    const int channel)
{
  // handle buffer boundaries (sample and hold)
  if(x < 0) x = 0;
  if(y < 0) y = 0;
  if(x >= b->width) x = b->width-1;
  if(y >= b->height) y = b->height-1;

  switch(b->type)
  {
    case s_buf_raw:
      if(channel != buffer_get_channel(x, y)) return -1.0f; // mark as not set
      return ((uint16_t *)b->data)[x + b->width*y]/(float)0xffff;
    case s_buf_float:
      return ((float *)b->data)[3*(x + b->width*y) + channel];
    case s_buf_raw_stabilise:
      { // apply variance stabilising transform (should be 1.0 after this)
      if(channel != buffer_get_channel(x, y)) return -1.0f; // mark as not set
      const float sigma2 = (b->noise_b/b->noise_a)*(b->noise_b/b->noise_a);
      const float v = ((uint16_t *)b->data)[x + b->width*y]/(float)0xffff;
      return 2.0f*sqrtf(fmaxf(0.0f, v/b->noise_a + 3./8. + sigma2));
      }
    case s_buf_float_backtransform:
      { // backtransform to normal domain
      const float sigma2 = (b->noise_b/b->noise_a)*(b->noise_b/b->noise_a);
      float v = ((float *)b->data)[3*(x + b->width*y) + channel];
      if(v < .5f) return 0.0f;
      v = 1./4.*v*v + 1./4.*sqrtf(3./2.)/v - 11./8./(v*v) + 5./8.*sqrtf(3./2.)/(v*v*v) - 1./8. - sigma2;
      return v * b->noise_a;
      }
    default:
      return 0.0f;
  }
}

static inline void buffer_set(
    buffer_t *b,
    const int x,
    const int y,
    const int channel,
    const float value)
{
  switch(b->type)
  {
    case s_buf_raw:
    case s_buf_raw_stabilise:
      if(channel != buffer_get_channel(x, y)) return; // wrong color channel
      ((uint16_t *)b->data)[x + b->width*y] = CLAMP(value * 0xffff, 0, 0xffff);
      return;
    case s_buf_float:
    case s_buf_float_backtransform:
      ((float *)b->data)[3*(x + b->width*y) + channel] = value;
      return;
    default:
      return;
  }
}

static inline void buffer_destroy(
    buffer_t *b)
{
  free(b->data);
  free(b);
}

static inline void buffer_write_pfm(
    const buffer_t *b,
    const char *filename)
{
  FILE *f = fopen(filename, "wb");
  if(f)
  {
    // write sse aligned pfm:
    char header[1024];
    snprintf(header, 1024, "PF\n%d %d\n-1.0", b->width, b->height);
    size_t len = strlen(header);
    fprintf(f, "PF\n%d %d\n-1.0", b->width, b->height);
    ssize_t off = 0;
    while((len + 1 + off) & 0xf) off++;
    while(off-- > 0) fprintf(f, "0");
    fprintf(f, "\n");
    if(b->type == s_buf_float)
      fwrite(b->data, b->width*b->height, 3*sizeof(float), f);
    else if(b->type == s_buf_float_backtransform)
    { // write one-by-one and backtransform
      for(int j=0;j<b->height;j++) for(int i=0;i<b->width;i++) for(int k=0;k<3;k++)
      {
        const float v = buffer_get(b, i, j, k);
        fwrite(&v, sizeof(float), 1, f);
      }
    }
    fclose(f);
  }
}

static inline buffer_t *buffer_read_pgm16(
    const char *filename)
{
  FILE *f = fopen(filename, "rb");
  if(f)
  {
    int max = 0, wd = 0, ht = 0;
    int res = fscanf(f, "P5\n%d %d\n%d", &wd, &ht, &max);
    if(res != 3) goto error;
    fgetc(f); // newline
    if(max != 65535) goto error;
    buffer_t *b = (buffer_t *)malloc(sizeof(buffer_t));
    memset(b, 0, sizeof(buffer_t));
    b->type = s_buf_raw;
    b->width = wd;
    b->height = ht;
    b->data = malloc(sizeof(uint16_t)*wd*ht);
    res = fread(b->data, sizeof(uint16_t), wd*ht, f);
    if(res != wd*ht) goto error;
    // swap byte order :(
    uint16_t maxval = 0;
    for(int k=0;k<wd*ht;k++)
    {
      ((uint16_t*)b->data)[k] = (((uint16_t*)b->data)[k]<<8) | (((uint16_t*)b->data)[k]>>8);
      maxval = MAX(maxval, ((uint16_t*)b->data)[k]);
    }
    fprintf(stderr, "[read_ppm16] scaling by %u\n", maxval);
    // rescale to full range:
    for(int k=0;k<wd*ht;k++)
      ((uint16_t*)b->data)[k] = (uint16_t)CLAMP(((uint16_t*)b->data)[k]/(float)maxval*0xffff, 0, 0xffff);
    fclose(f);
    return b;
error:
    fprintf(stderr, "[read_ppm16] not a 16-bit pgm file: `%s'\n", filename);
    fclose(f);
    return 0;
  }
  return 0;
}

static inline buffer_t *buffer_create_float(
    const int wd,
    const int ht)
{
  buffer_t *b = (buffer_t *)malloc(sizeof(buffer_t));
  memset(b, 0, sizeof(buffer_t));
  b->type = s_buf_float;
  b->width = wd;
  b->height = ht;
  b->data = malloc(sizeof(float)*3*wd*ht);
  memset(b->data, 0, wd*ht*3*sizeof(float));
  return b;
}

static inline float weight(
    const buffer_t *a,
    const int ax,
    const int ay,
    const int ac,
    const buffer_t *b,
    const int bx,
    const int by)
{
  const float pbc = buffer_get(b, bx, by, ac); // source buffer is unknown, never use it
  if(pbc < 0.0) return 0.0;
  // destination buffer is unknown, weight all others with 1
  const float pac = buffer_get(a, ax, ay, ac);
  if(pac < 0.0) return 1.0f;

  float d = 0.0f;
  const float cw[3] = {1.0, 2.0, 1.0};
  int dims = 0;
  for(int k=0;k<3;k++)
  {
    const float pa = buffer_get(a, ax, ay, k);
    const float pb = buffer_get(b, bx, by, k);
    if(pa < 0.0 || pb < 0.0) continue;

    // this threshold subtraction considers part of the signal as noise and subtracts that.
    // noise sigma is normalised to 1.0, so 3sigma^2 = 9 is our noise floor (this is a
    // two-noisy-estimator distance, so it's actually 1.5sigma for either side).
    const float dd = fmaxf(0.0f, (pa - pb)*(pa - pb) - 16.0f);
    d += cw[k]*dd;
    dims++;
  }
  // XXX use superfast patented approximation from darktable code:
  const float weight = expf(-d/dims*1e6f);
   // fprintf(stderr, "weight %g of d=%g %g\n", weight, d, -d/dims*1e10f);
  return weight;
}

static inline void decompose(
    const buffer_t *input,
    buffer_t *coarse,
    buffer_t *detail,
    int channel,
    int scale)
{
  const int mult = 1<<scale;
  const float filter[5] = {1.0f/16.0f, 4.0f/16.0f, 6.0f/16.0f, 4.0f/16.0f, 1.0f/16.0f};
  int cnt = 0;
#pragma omp parallel for default(shared)
  for(int y=0;y<coarse->height;y++)
  {
    const int progress = __sync_fetch_and_add(&cnt, 1);
    if((progress & 0xff) == 0xff || progress == coarse->height-1)
      fprintf(stderr, "decompose scale %d channel %d %d/%d\r", scale, channel, progress, coarse->height);
    for(int x=0;x<coarse->width;x++)
    {
      float wgt = 0.0f, sum = 0.0f;
      for(int j=0;j<5;j++) for(int i=0;i<5;i++)
      {
        const int xx = x+mult*(i-2), yy = y+mult*(j-2);
        const float px = buffer_get(input, xx, yy, channel);
        const float w = filter[i]*filter[j]*weight(input, x, y, channel, input, xx, yy);
        sum += w*px;
        wgt += w;
      }
      if(wgt <= 0.0)
      { // no neighbours with this color found. probably x-trans :(
        buffer_set(detail, x, y, channel, 0.0);
        buffer_set(coarse, x, y, channel, -1.0);
      }
      else
      { // have some estimated coarse value, yay
        sum /= wgt;
        const float pixel = buffer_get(input, x, y, channel);
        if(pixel >= 0.0) // do we also have a previous value? if yes, encode difference:
          buffer_set(detail, x, y, channel, buffer_get(input, x, y, channel) - sum);
        else // or else make it smooth
          buffer_set(detail, x, y, channel, 0.0);
        buffer_set(coarse, x, y, channel, sum);
      }
    }
  }
}

static inline void synthesize(
    buffer_t *output,
    const buffer_t *coarse,
    const buffer_t *detail,
    int channel,
    int scale)
{
  fprintf(stderr, "synthesizing scale %d channel %d                     \r", scale, channel);
  // noise variance level 0: 1.0
  const float sigma_n = powf(2.0f, -scale);
  // bayes shrink: T = sigma_n^2 / sqrtf(sigma_d^2 - sigma_n^2)
  // sigma_d^2 = 1/N sum detail(i)^2
  float sigma_d2 = 0.0f;
  int k = 0;
  for(int y=0;y<detail->height;y++) for(int x=0;x<detail->width;x++)
  {
    const float d = buffer_get(detail, x, y, channel);
    sigma_d2 = sigma_d2 * k/(k+1.0) + d*d * 1.0/(k+1.0);
    k++;
  }

  // wavelet shrinkage threshold.
  const float thrs = sigma_n*sigma_n / sqrtf(fmaxf(1e-20f, sigma_d2 - sigma_n*sigma_n));
  const float boost = 1.0f;
  fprintf(stderr, "\nscale %d sigma noise %g signal %g => thrs %g boost %g\n", scale, sigma_n, sqrtf(sigma_d2), thrs, boost);
#pragma omp parallel for default(shared)
  for(int y=0;y<coarse->height;y++)
  {
    for(int x=0;x<coarse->width;x++)
    {
      // coarse should not have any unset pixels any more at this point.
      const float px = buffer_get(detail, x, y, channel);
      const float d = fmaxf(0.0f, fabsf(px) - thrs)*boost;
      if(px > 0.0f) buffer_set(output, x, y, channel, buffer_get(coarse, x, y, channel) + d);
      else          buffer_set(output, x, y, channel, buffer_get(coarse, x, y, channel) - d);
    }
  }
}

