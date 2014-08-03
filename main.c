#include "wtf.h"
#include "noiseprofile.h"

int main(int argc, char *argv[])
{
  if(argc < 2)
  {
    fprintf(stderr, "usage: %s input.pgm\n", argv[0]);
    fprintf(stderr, "input should be non-demosaiced raw raw data (no wb, no black/white scaling, etc)\n");
    fprintf(stderr, "create pgm with dcraw -D -W -6 input.cr2\n");
    fprintf(stderr, "create pgm with dcraw -4 -E -c -t 0 -o 0 -M -r 1 1 1 1 input.cr2 > input.pgm\n");
    exit(1);
  }

  buffer_t *raw = buffer_read_pgm16(argv[1]);
  noiseprofile(raw);
  return 0;
  // green values from 5dm2 measured after debayer+matrix, so more or less meaningless:
  // raw->noise_a = 7.34335232069023e-05;
  // raw->noise_b = 3.47619065786586e-07;

  // hand dialed for iso6400:
  // raw->noise_a = 14.7e-5;
  // raw->noise_b = 7.0e-7;

  // iso 25600
  // raw->noise_a = 120.e-5;
  // raw->noise_b = 60.e-7;
  raw->noise_a = 240.e-5;
  raw->noise_b = 120.e-7;

  // from noiseprofiles.h for iso 1600
  // raw->noise_a = 2.4e-5;
  // raw->noise_b = -6e-7;

  // noiseprofiles.h for iso 100
  // raw->noise_a = 2.5e-6;
  // raw->noise_b = -1.3e-7;

  // measured for this particular image:
  // 8.39358241472871e-05 3.39274120804604e-05 8.0308395485184e-05 -1.16520884450047e-06 -4.33714555176887e-07 -1.09341959573161e-06
  // sigma(scale) = sigma * 2^{-scale}, and sigma = sqrt(a + b*x)
  // compensate for half-size image:
  // const float maxval = 16383.0f/0xffff;
  // raw->noise_a = 8.39358241472871e-05 * 4.0f / (maxval*maxval);
  // raw->noise_b = -1.16520884450047e-06 * 4.0f / (maxval*maxval);
  raw->type = s_buf_raw_stabilise; // instruct that this should be read out transformed
  buffer_t *coarse0 = buffer_create_float(raw->width, raw->height);
  buffer_t *coarse1 = buffer_create_float(raw->width, raw->height);
  buffer_t *coarse2 = buffer_create_float(raw->width, raw->height);
  buffer_t *detail0 = buffer_create_float(raw->width, raw->height);
  buffer_t *detail1 = buffer_create_float(raw->width, raw->height);
  buffer_t *detail2 = buffer_create_float(raw->width, raw->height);
  buffer_t *output0 = buffer_create_float(raw->width, raw->height);
  buffer_t *output1 = buffer_create_float(raw->width, raw->height);
  coarse0->noise_a = raw->noise_a;
  coarse0->noise_b = raw->noise_b;
  coarse1->noise_a = raw->noise_a;
  coarse1->noise_b = raw->noise_b;
  coarse2->noise_a = raw->noise_a;
  coarse2->noise_b = raw->noise_b;
  output0->noise_a = raw->noise_a;
  output0->noise_b = raw->noise_b;
  output1->noise_a = raw->noise_a;
  output1->noise_b = raw->noise_b;

  for(int channel=0;channel<3;channel++)
    decompose(raw, coarse0, detail0, channel, 0);

  for(int channel=0;channel<3;channel++)
    decompose(coarse0, coarse1, detail1, channel, 1);

  for(int channel=0;channel<3;channel++)
    decompose(coarse1, coarse2, detail2, channel, 2);

  for(int channel=0;channel<3;channel++)
    synthesize(output0, coarse2, detail2, channel, 2);

  for(int channel=0;channel<3;channel++)
    synthesize(output1, output0, detail1, channel, 1);

  for(int channel=0;channel<3;channel++)
    synthesize(output0, output1, detail0, channel, 0);

  // now switch type before writing out:
  coarse0->type = s_buf_float_backtransform;
  coarse1->type = s_buf_float_backtransform;
  coarse2->type = s_buf_float_backtransform;
  output0->type = s_buf_float_backtransform;
  output1->type = s_buf_float_backtransform;
  buffer_write_pfm(coarse0, "coarse0.pfm");
  buffer_write_pfm(coarse1, "coarse1.pfm");
  buffer_write_pfm(coarse2, "coarse2.pfm");
  // buffer_write_pfm(detail0, "detail0.pfm");
  // buffer_write_pfm(detail1, "detail1.pfm");
  // buffer_write_pfm(detail2, "detail2.pfm");
  // buffer_write_pfm(output0, "output0.pfm");
  // buffer_write_pfm(output1, "output1.pfm");

  exit(0);
}

