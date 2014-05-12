#include "wtf.h"

int main(int argc, char *argv[])
{
  if(argc < 2)
  {
    fprintf(stderr, "usage: %s input.pgm\n", argv[0]);
    fprintf(stderr, "create pgm with dcraw -D -W -6 input.cr2\n");
    exit(1);
  }

  buffer_t *raw = buffer_read_pgm16(argv[1]);
  // green values from 5dm2 measured after debayer+matrix, so more or less meaningless:
  raw->noise_a = 7.34335232069023e-05;
  raw->noise_b = 3.47619065786586e-07;
  raw->type = s_buf_raw_stabilise; // instruct that this should be read out transformed
  buffer_t *coarse0 = buffer_create_float(raw->width, raw->height);
  buffer_t *coarse1 = buffer_create_float(raw->width, raw->height);
  buffer_t *coarse2 = buffer_create_float(raw->width, raw->height);
  buffer_t *detail0 = buffer_create_float(raw->width, raw->height);
  buffer_t *detail1 = buffer_create_float(raw->width, raw->height);
  buffer_t *detail2 = buffer_create_float(raw->width, raw->height);
  coarse0->noise_a = raw->noise_a;
  coarse0->noise_b = raw->noise_b;
  coarse1->noise_a = raw->noise_a;
  coarse1->noise_b = raw->noise_b;
  coarse2->noise_a = raw->noise_a;
  coarse2->noise_b = raw->noise_b;

  for(int channel=0;channel<3;channel++)
    decompose(raw, coarse0, detail0, channel, 0);

  for(int channel=0;channel<3;channel++)
    decompose(coarse0, coarse1, detail1, channel, 1);

  for(int channel=0;channel<3;channel++)
    decompose(coarse1, coarse2, detail2, channel, 2);

  // now switch type before writing out:
  coarse0->type = s_buf_float_backtransform;
  coarse1->type = s_buf_float_backtransform;
  coarse2->type = s_buf_float_backtransform;
  buffer_write_pfm(coarse0, "coarse0.pfm");
  buffer_write_pfm(detail0, "detail0.pfm");
  buffer_write_pfm(coarse1, "coarse1.pfm");
  buffer_write_pfm(detail1, "detail1.pfm");
  buffer_write_pfm(coarse2, "coarse2.pfm");
  buffer_write_pfm(detail2, "detail2.pfm");

  exit(0);
}

