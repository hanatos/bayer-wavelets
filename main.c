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
  buffer_t *coarse = buffer_create_float(raw->width, raw->height);
  buffer_t *temp   = buffer_create_float(raw->width, raw->height);
  buffer_t *detail = buffer_create_float(raw->width, raw->height);

  const float sigma_noise = 0.0f;
  for(int channel=0;channel<3;channel++)
    decompose(raw, temp, detail, channel, 0, sigma_noise);

  for(int channel=0;channel<3;channel++)
    decompose(temp, coarse, detail, channel, 1, sigma_noise);

  buffer_write_pfm(temp, "coarse0.pfm");
  buffer_write_pfm(coarse, "coarse1.pfm");
  buffer_write_pfm(detail, "detail1.pfm");

  exit(0);
}

