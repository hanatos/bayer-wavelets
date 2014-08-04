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

  // from dcraw -v:
  // const int black = 1023; // used in fit.gp
  const int white = 15600;
  buffer_t *raw = buffer_read_pgm16(argv[1], white);

  // noiseprofile(raw);
  // return 0;

  // noiseprofiled with the above procedure:
  // 5dm2 iso1600, wavelet scale2:
  // raw->noise_a = 0.000234565466234752;
  // raw->noise_b = -1.41864661910691e-05;

  // 5dm2 iso1600, wavelet scale0:
  raw->noise_a = 7.44e-05;
  raw->noise_b = -4.82e-06;

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
  buffer_write_pfm(output0, "output0.pfm");
  // buffer_write_pfm(output1, "output1.pfm");

  exit(0);
}

