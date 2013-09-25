/* Copyright (c) 2011 Xiph.Org Foundation, Skype Limited
   Written by Jean-Marc Valin and Koen Vos */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "opus.h"
#include "opus_private.h"

#ifndef DISABLE_FLOAT_API
OPUS_EXPORT void opus_pcm_soft_clip(float *_x, int N, int C, float *declip_mem)
{
   int c;
   int i;
   float *x;

   /* First thing: saturate everything to +/- 2 which is the highest level our
      non-linearity can handle. At the point where the signal reaches +/-2,
      the derivative will be zero anyway, so this doesn't introduce any
      discontinuity in the derivative. */
   for (i=0;i<N*C;i++)
      _x[i] = MAX16(-2.f, MIN16(2.f, _x[i]));
   for (c=0;c<C;c++)
   {
      float a;
      float x0;
      int curr;

      x = _x+c;
      a = declip_mem[c];
      /* Continue applying the non-linearity from the previous frame to avoid
         any discontinuity. */
      for (i=0;i<N;i++)
      {
         if (x[i*C]*a>=0)
            break;
         x[i*C] = x[i*C]+a*x[i*C]*x[i*C];
      }

      curr=0;
      x0 = x[0];
      while(1)
      {
         int start, end;
         float maxval;
         int special=0;
         int peak_pos;
         for (i=curr;i<N;i++)
         {
            if (x[i*C]>1 || x[i*C]<-1)
               break;
         }
         if (i==N)
         {
            a=0;
            break;
         }
         peak_pos = i;
         start=end=i;
         maxval=ABS16(x[i*C]);
         /* Look for first zero crossing before clipping */
         while (start>0 && x[i*C]*x[(start-1)*C]>=0)
            start--;
         /* Look for first zero crossing after clipping */
         while (end<N && x[i*C]*x[end*C]>=0)
         {
            /* Look for other peaks until the next zero-crossing. */
            if (ABS16(x[end*C])>maxval)
            {
               maxval = ABS16(x[end*C]);
               peak_pos = end;
            }
            end++;
         }
         /* Detect the special case where we clip before the first zero crossing */
         special = (start==0 && x[i*C]*x[0]>=0);

         /* Compute a such that maxval + a*maxval^2 = 1 */
         a=(maxval-1)/(maxval*maxval);
         if (x[i*C]>0)
            a = -a;
         /* Apply soft clipping */
         for (i=start;i<end;i++)
            x[i*C] = x[i*C]+a*x[i*C]*x[i*C];

         if (special && peak_pos>=2)
         {
            /* Add a linear ramp from the first sample to the signal peak.
               This avoids a discontinuity at the beginning of the frame. */
            float delta;
            float offset = x0-x[0];
            delta = offset / peak_pos;
            for (i=curr;i<peak_pos;i++)
            {
               offset -= delta;
               x[i*C] += offset;
               x[i*C] = MAX16(-1.f, MIN16(1.f, x[i*C]));
            }
         }
         curr = end;
         if (curr==N)
            break;
      }
      declip_mem[c] = a;
   }
}
#endif

int encode_size(int size, unsigned char *data)
{
   if (size < 252)
   {
      data[0] = size;
      return 1;
   } else {
      data[0] = 252+(size&0x3);
      data[1] = (size-(int)data[0])>>2;
      return 2;
   }
}

