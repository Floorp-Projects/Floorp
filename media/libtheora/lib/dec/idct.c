/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2007                *
 * by the Xiph.Org Foundation and contributors http://www.xiph.org/ *
 *                                                                  *
 ********************************************************************

  function:
    last mod: $Id: idct.c 15400 2008-10-15 12:10:58Z tterribe $

 ********************************************************************/

#include <string.h>
#include <ogg/ogg.h>
#include "dct.h"
#include "idct.h"

/*Performs an inverse 8 point Type-II DCT transform.
  The output is scaled by a factor of 2 relative to the orthonormal version of
   the transform.
  _y: The buffer to store the result in.
      Data will be placed in every 8th entry (e.g., in a column of an 8x8
       block).
  _x: The input coefficients.
      The first 8 entries are used (e.g., from a row of an 8x8 block).*/
static void idct8(ogg_int16_t *_y,const ogg_int16_t _x[8]){
  ogg_int32_t t[8];
  ogg_int32_t r;
  /*Stage 1:*/
  /*0-1 butterfly.*/
  t[0]=OC_C4S4*(ogg_int16_t)(_x[0]+_x[4])>>16;
  t[1]=OC_C4S4*(ogg_int16_t)(_x[0]-_x[4])>>16;
  /*2-3 rotation by 6pi/16.*/
  t[2]=(OC_C6S2*_x[2]>>16)-(OC_C2S6*_x[6]>>16);
  t[3]=(OC_C2S6*_x[2]>>16)+(OC_C6S2*_x[6]>>16);
  /*4-7 rotation by 7pi/16.*/
  t[4]=(OC_C7S1*_x[1]>>16)-(OC_C1S7*_x[7]>>16);
  /*5-6 rotation by 3pi/16.*/
  t[5]=(OC_C3S5*_x[5]>>16)-(OC_C5S3*_x[3]>>16);
  t[6]=(OC_C5S3*_x[5]>>16)+(OC_C3S5*_x[3]>>16);
  t[7]=(OC_C1S7*_x[1]>>16)+(OC_C7S1*_x[7]>>16);
  /*Stage 2:*/
  /*4-5 butterfly.*/
  r=t[4]+t[5];
  t[5]=OC_C4S4*(ogg_int16_t)(t[4]-t[5])>>16;
  t[4]=r;
  /*7-6 butterfly.*/
  r=t[7]+t[6];
  t[6]=OC_C4S4*(ogg_int16_t)(t[7]-t[6])>>16;
  t[7]=r;
  /*Stage 3:*/
  /*0-3 butterfly.*/
  r=t[0]+t[3];
  t[3]=t[0]-t[3];
  t[0]=r;
  /*1-2 butterfly.*/
  r=t[1]+t[2];
  t[2]=t[1]-t[2];
  t[1]=r;
  /*6-5 butterfly.*/
  r=t[6]+t[5];
  t[5]=t[6]-t[5];
  t[6]=r;
  /*Stage 4:*/
  /*0-7 butterfly.*/
  _y[0<<3]=(ogg_int16_t)(t[0]+t[7]);
  /*1-6 butterfly.*/
  _y[1<<3]=(ogg_int16_t)(t[1]+t[6]);
  /*2-5 butterfly.*/
  _y[2<<3]=(ogg_int16_t)(t[2]+t[5]);
  /*3-4 butterfly.*/
  _y[3<<3]=(ogg_int16_t)(t[3]+t[4]);
  _y[4<<3]=(ogg_int16_t)(t[3]-t[4]);
  _y[5<<3]=(ogg_int16_t)(t[2]-t[5]);
  _y[6<<3]=(ogg_int16_t)(t[1]-t[6]);
  _y[7<<3]=(ogg_int16_t)(t[0]-t[7]);
}

/*Performs an inverse 8 point Type-II DCT transform.
  The output is scaled by a factor of 2 relative to the orthonormal version of
   the transform.
  _y: The buffer to store the result in.
      Data will be placed in every 8th entry (e.g., in a column of an 8x8
       block).
  _x: The input coefficients.
      Only the first 4 entries are used.
      The other 4 are assumed to be 0.*/
static void idct8_4(ogg_int16_t *_y,const ogg_int16_t _x[8]){
  ogg_int32_t t[8];
  ogg_int32_t r;
  /*Stage 1:*/
  t[0]=OC_C4S4*_x[0]>>16;
  t[2]=OC_C6S2*_x[2]>>16;
  t[3]=OC_C2S6*_x[2]>>16;
  t[4]=OC_C7S1*_x[1]>>16;
  t[5]=-(OC_C5S3*_x[3]>>16);
  t[6]=OC_C3S5*_x[3]>>16;
  t[7]=OC_C1S7*_x[1]>>16;
  /*Stage 2:*/
  r=t[4]+t[5];
  t[5]=OC_C4S4*(ogg_int16_t)(t[4]-t[5])>>16;
  t[4]=r;
  r=t[7]+t[6];
  t[6]=OC_C4S4*(ogg_int16_t)(t[7]-t[6])>>16;
  t[7]=r;
  /*Stage 3:*/
  t[1]=t[0]+t[2];
  t[2]=t[0]-t[2];
  r=t[0]+t[3];
  t[3]=t[0]-t[3];
  t[0]=r;
  r=t[6]+t[5];
  t[5]=t[6]-t[5];
  t[6]=r;
  /*Stage 4:*/
  _y[0<<3]=(ogg_int16_t)(t[0]+t[7]);
  _y[1<<3]=(ogg_int16_t)(t[1]+t[6]);
  _y[2<<3]=(ogg_int16_t)(t[2]+t[5]);
  _y[3<<3]=(ogg_int16_t)(t[3]+t[4]);
  _y[4<<3]=(ogg_int16_t)(t[3]-t[4]);
  _y[5<<3]=(ogg_int16_t)(t[2]-t[5]);
  _y[6<<3]=(ogg_int16_t)(t[1]-t[6]);
  _y[7<<3]=(ogg_int16_t)(t[0]-t[7]);
}

/*Performs an inverse 8 point Type-II DCT transform.
  The output is scaled by a factor of 2 relative to the orthonormal version of
   the transform.
  _y: The buffer to store the result in.
      Data will be placed in every 8th entry (e.g., in a column of an 8x8
       block).
  _x: The input coefficients.
      Only the first 3 entries are used.
      The other 5 are assumed to be 0.*/
static void idct8_3(ogg_int16_t *_y,const ogg_int16_t _x[8]){
  ogg_int32_t t[8];
  ogg_int32_t r;
  /*Stage 1:*/
  t[0]=OC_C4S4*_x[0]>>16;
  t[2]=OC_C6S2*_x[2]>>16;
  t[3]=OC_C2S6*_x[2]>>16;
  t[4]=OC_C7S1*_x[1]>>16;
  t[7]=OC_C1S7*_x[1]>>16;
  /*Stage 2:*/
  t[5]=OC_C4S4*t[4]>>16;
  t[6]=OC_C4S4*t[7]>>16;
  /*Stage 3:*/
  t[1]=t[0]+t[2];
  t[2]=t[0]-t[2];
  r=t[0]+t[3];
  t[3]=t[0]-t[3];
  t[0]=r;
  r=t[6]+t[5];
  t[5]=t[6]-t[5];
  t[6]=r;
  /*Stage 4:*/
  _y[0<<3]=(ogg_int16_t)(t[0]+t[7]);
  _y[1<<3]=(ogg_int16_t)(t[1]+t[6]);
  _y[2<<3]=(ogg_int16_t)(t[2]+t[5]);
  _y[3<<3]=(ogg_int16_t)(t[3]+t[4]);
  _y[4<<3]=(ogg_int16_t)(t[3]-t[4]);
  _y[5<<3]=(ogg_int16_t)(t[2]-t[5]);
  _y[6<<3]=(ogg_int16_t)(t[1]-t[6]);
  _y[7<<3]=(ogg_int16_t)(t[0]-t[7]);
}

/*Performs an inverse 8 point Type-II DCT transform.
  The output is scaled by a factor of 2 relative to the orthonormal version of
   the transform.
  _y: The buffer to store the result in.
      Data will be placed in every 8th entry (e.g., in a column of an 8x8
       block).
  _x: The input coefficients.
      Only the first 2 entries are used.
      The other 6 are assumed to be 0.*/
static void idct8_2(ogg_int16_t *_y,const ogg_int16_t _x[8]){
  ogg_int32_t t[8];
  ogg_int32_t r;
  /*Stage 1:*/
  t[0]=OC_C4S4*_x[0]>>16;
  t[4]=OC_C7S1*_x[1]>>16;
  t[7]=OC_C1S7*_x[1]>>16;
  /*Stage 2:*/
  t[5]=OC_C4S4*t[4]>>16;
  t[6]=OC_C4S4*t[7]>>16;
  /*Stage 3:*/
  r=t[6]+t[5];
  t[5]=t[6]-t[5];
  t[6]=r;
  /*Stage 4:*/
  _y[0<<3]=(ogg_int16_t)(t[0]+t[7]);
  _y[1<<3]=(ogg_int16_t)(t[0]+t[6]);
  _y[2<<3]=(ogg_int16_t)(t[0]+t[5]);
  _y[3<<3]=(ogg_int16_t)(t[0]+t[4]);
  _y[4<<3]=(ogg_int16_t)(t[0]-t[4]);
  _y[5<<3]=(ogg_int16_t)(t[0]-t[5]);
  _y[6<<3]=(ogg_int16_t)(t[0]-t[6]);
  _y[7<<3]=(ogg_int16_t)(t[0]-t[7]);
}

/*Performs an inverse 8 point Type-II DCT transform.
  The output is scaled by a factor of 2 relative to the orthonormal version of
   the transform.
  _y: The buffer to store the result in.
      Data will be placed in every 8th entry (e.g., in a column of an 8x8
       block).
  _x: The input coefficients.
      Only the first entry is used.
      The other 7 are assumed to be 0.*/
static void idct8_1(ogg_int16_t *_y,const ogg_int16_t _x[1]){
  _y[0<<3]=_y[1<<3]=_y[2<<3]=_y[3<<3]=
   _y[4<<3]=_y[5<<3]=_y[6<<3]=_y[7<<3]=(ogg_int16_t)(OC_C4S4*_x[0]>>16);
}

/*Performs an inverse 8x8 Type-II DCT transform.
  The input is assumed to be scaled by a factor of 4 relative to orthonormal
   version of the transform.
  _y: The buffer to store the result in.
      This may be the same as _x.
  _x: The input coefficients. */
void oc_idct8x8_c(ogg_int16_t _y[64],const ogg_int16_t _x[64]){
  const ogg_int16_t *in;
  ogg_int16_t       *end;
  ogg_int16_t       *out;
  ogg_int16_t        w[64];
  /*Transform rows of x into columns of w.*/
  for(in=_x,out=w,end=out+8;out<end;in+=8,out++)idct8(out,in);
  /*Transform rows of w into columns of y.*/
  for(in=w,out=_y,end=out+8;out<end;in+=8,out++)idct8(out,in);
  /*Adjust for scale factor.*/
  for(out=_y,end=out+64;out<end;out++)*out=(ogg_int16_t)(*out+8>>4);
}

/*Performs an inverse 8x8 Type-II DCT transform.
  The input is assumed to be scaled by a factor of 4 relative to orthonormal
   version of the transform.
  All coefficients but the first 10 in zig-zag scan order are assumed to be 0:
   x  x  x  x  0  0  0  0
   x  x  x  0  0  0  0  0
   x  x  0  0  0  0  0  0
   x  0  0  0  0  0  0  0
   0  0  0  0  0  0  0  0
   0  0  0  0  0  0  0  0
   0  0  0  0  0  0  0  0
   0  0  0  0  0  0  0  0
  _y: The buffer to store the result in.
      This may be the same as _x.
  _x: The input coefficients. */
void oc_idct8x8_10_c(ogg_int16_t _y[64],const ogg_int16_t _x[64]){
  const ogg_int16_t *in;
  ogg_int16_t       *end;
  ogg_int16_t       *out;
  ogg_int16_t        w[64];
  /*Transform rows of x into columns of w.*/
  idct8_4(w,_x);
  idct8_3(w+1,_x+8);
  idct8_2(w+2,_x+16);
  idct8_1(w+3,_x+24);
  /*Transform rows of w into columns of y.*/
  for(in=w,out=_y,end=out+8;out<end;in+=8,out++)idct8_4(out,in);
  /*Adjust for scale factor.*/
  for(out=_y,end=out+64;out<end;out++)*out=(ogg_int16_t)(*out+8>>4);
}
