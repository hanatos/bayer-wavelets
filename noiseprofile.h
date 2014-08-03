#include "wtf.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

typedef float elem_type;
#define ELEM_SWAP(a,b) { elem_type t=(a);(a)=(b);(b)=t; }

/*---------------------------------------------------------------------------
Function :   kth_smallest()
In       :   array of elements, # of elements in the array, rank k
Out      :   one element
Job      :   find the kth smallest element in the array
Notice   :   use the median() macro defined below to get the median. 

Reference:

Author: Wirth, Niklaus 
Title: Algorithms + data structures = programs 
Publisher: Englewood Cliffs: Prentice-Hall, 1976 
Physical description: 366 p. 
Series: Prentice-Hall Series in Automatic Computation 

---------------------------------------------------------------------------*/

static elem_type
kth_smallest(elem_type a[], int n, int k)
{
  int i,j,l,m ;
  elem_type x ;

  l=0 ; m=n-1 ;
  while (l<m) {
    x=a[2*k+1] ;
    i=l ;
    j=m ;
    do {
      while (a[2*i+1]<x) i++ ;
      while (x<a[2*j+1]) j-- ;
      if (i<=j) {
        ELEM_SWAP(a[2*i+1],a[2*j+1]) ;
        i++ ; j-- ;
      }
    } while (i<=j) ;
    if (j<k) l=i ;
    if (k<i) m=j ;
  }
  return a[2*k+1] ;
}

#define median(a,n) kth_smallest(a,n,(((n)&1)?((n)/2):(((n)/2)-1)))

#define N 200

static inline float
clamp(float f, float m, float M)
{
  return MAX(MIN(f, M), m);
}

// compare helper for coarse/detail sort
int compare_llhh(const void *a, const void *b)
{
  return (int)clamp(((float *)a)[0]*N, 0, N-1) - (int)clamp(((float *)b)[0]*N, 0, N-1);
}

void noiseprofile(buffer_t *raw)
{
  raw->type = s_buf_raw; // read plain raw data
  buffer_t *coarse0 = buffer_create_float(raw->width, raw->height);
  buffer_t *coarse1 = buffer_create_float(raw->width, raw->height);
  buffer_t *coarse2 = buffer_create_float(raw->width, raw->height);
  buffer_t *detail0 = buffer_create_float(raw->width, raw->height);

  for(int channel=0;channel<3;channel++)
  {
    int incomplete = decompose_raw(raw, coarse0, detail0, channel, 0);
    if(incomplete) fprintf(stderr, "[noiseprofile] arrgh, this filter pattern is not filled after first iteration (channel %d)!\n", channel);
  }
  for(int channel=0;channel<3;channel++)
  {
    int incomplete = decompose_raw(coarse0, coarse1, detail0, channel, 1);
    if(incomplete) fprintf(stderr, "[noiseprofile] arrgh, this filter pattern is not filled after second iteration (channel %d)!\n", channel);
  }
  for(int channel=0;channel<3;channel++)
  {
    int incomplete = decompose_raw(coarse1, coarse2, detail0, channel, 2);
    if(incomplete) fprintf(stderr, "[noiseprofile] arrgh, this filter pattern is not filled after third iteration (channel %d)!\n", channel);
  }
  for(int j=0;j<raw->height;j++)
  {
    for(int i=0;i<raw->width;i++)
    {
      for(int c=0;c<3;c++)
      {
        float val = buffer_get(raw, i, j, c);
        if(val == -1.0f)
          buffer_set(detail0, i, j, c, 0.0f);
        else
          buffer_set(detail0, i, j, c, buffer_get(raw, i, j, c) - buffer_get(coarse2, i, j, c));
      }
    }
  }



  const int wd = raw->width, ht = raw->height;
  float std[N][3] = {{0.0f}};
  float cnt[N][3] = {{0.0f}};

  // sort pairs (LL,HH) for each color channel:
  float *llhh = (float *)malloc(sizeof(float)*wd*ht*2);
  for(int c=0;c<3;c++)
  {
    int k = 0;
    for(int j=0;j<ht;j++)
    {
      for(int i=0;i<wd;i++)
      {
        if(buffer_get(raw, i, j, c) != -1.0f)
        { // only if there is this color channel in the input:
          llhh[2*k]   = buffer_get(coarse0, i, j, c);
          assert(llhh[2*k] != -1.0f); // or else complained above.
          llhh[2*k+1] = fabsf(buffer_get(detail0, i, j, c));
          k++;
        }
      }
    }
    qsort(llhh, k, 2*sizeof(float), compare_llhh);
    // estimate std deviation for every bin we've got:
    for(int begin=0;begin<k;)
    {
      // coarse is used to estimate brightness:
      const int bin = (int)clamp(llhh[2*begin]*N, 0, N-1);
      int end = begin+1;
      while((end < k) && ((int)clamp(llhh[2*end]*N, 0, N-1) == bin))
        end++;
      assert(end >= k || bin <= (int)clamp(llhh[2*end]*N, 0, N-1));
      // fprintf(stderr, "from %d (%d) -- %d (%d)\n", begin, bin, end, (int)clamp(llhh[2*end]*N, 0, N-1));

      // estimate noise by robust statistic (assumes zero mean of HH band):
      // MAD: median(|Y - med(Y)|) = 0.6745 sigma
      // if(end - begin > 10)
        // fprintf(stdout, "%d %f %d\n", bin, median(llhh+2*begin, end-begin)/0.6745, end - begin);
      std[bin][c] += median(llhh+2*begin, end-begin)/0.6745;
      cnt[bin][c] = end - begin;

      begin = end;
    }
  }
  free(llhh);

  // correction factor accounting for relative frequency of color channels
  // in mosaic pattern. this is for a std bayer pattern, i.e. there are
  // twice as many green pixels as red and blue.
  // float corr[3] = {1.0, 1.0/sqrtf(2.0), 1.0};
  // when using input - coarse1, this results about in even noise levels:
  float corr[3] = {1.0, 1.0, 1.0};

  float sum[3] = {0.0f};
  for(int i=0;i<N;i++)
    for(int k=0;k<3;k++) sum[k] += std[i][k];
  float cdf[3] = {0.0f};
  for(int i=0;i<N;i++)
  {
    fprintf(stdout, "%f %f %f %f %f %f %f %f %f %f\n", i/(float)N, std[i][0]*corr[0], std[i][1]*corr[1], std[i][2]*corr[2],
        cnt[i][0], cnt[i][1], cnt[i][2],
        cdf[0]/sum[0], cdf[1]/sum[1], cdf[2]/sum[2]);
        // cdf[0], cdf[1], cdf[2]);
    for(int k=0;k<3;k++) cdf[k] += std[i][k]*corr[k];
  }

  // buffer_write_pfm(detail0, "detail.pfm");
  // buffer_write_pfm(coarse0, "coarse.pfm");

  buffer_destroy(coarse0);
  buffer_destroy(coarse1);
  buffer_destroy(coarse2);
  buffer_destroy(detail0);
}

#undef N
