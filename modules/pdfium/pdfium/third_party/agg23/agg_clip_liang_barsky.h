
//----------------------------------------------------------------------------
// Anti-Grain Geometry - Version 2.3
// Copyright (C) 2002-2005 Maxim Shemanarev (http://www.antigrain.com)
//
// Permission to copy, use, modify, sell and distribute this software
// is granted provided this copyright notice appears in all copies.
// This software is provided "as is" without express or implied
// warranty, and with no claim as to its suitability for any purpose.
//
//----------------------------------------------------------------------------
// Contact: mcseem@antigrain.com
//          mcseemagg@yahoo.com
//          http://www.antigrain.com
//----------------------------------------------------------------------------
//
// Liang-Barsky clipping
//
//----------------------------------------------------------------------------
#ifndef AGG_CLIP_LIANG_BARSKY_INCLUDED
#define AGG_CLIP_LIANG_BARSKY_INCLUDED
#include "agg_basics.h"
#include "third_party/base/numerics/safe_math.h"
namespace agg
{
template<class T>
inline unsigned clipping_flags(T x, T y, const rect_base<T>& clip_box)
{
    return  (x > clip_box.x2) |
            ((y > clip_box.y2) << 1) |
            ((x < clip_box.x1) << 2) |
            ((y < clip_box.y1) << 3);
}
template<class T>
inline unsigned clip_liang_barsky(T x1, T y1, T x2, T y2,
                                  const rect_base<T>& clip_box,
                                  T* x, T* y)
{
    const FX_FLOAT nearzero = 1e-30f;

    pdfium::base::CheckedNumeric<FX_FLOAT> width = x2;
    width -= x1;
    if (!width.IsValid())
        return 0;
    pdfium::base::CheckedNumeric<FX_FLOAT> height = y2;
    height -= y1;
    if (!height.IsValid())
        return 0;

    FX_FLOAT deltax = width.ValueOrDefault(0);
    FX_FLOAT deltay = height.ValueOrDefault(0);
    unsigned np = 0;
    if(deltax == 0) {
        deltax = (x1 > clip_box.x1) ? -nearzero : nearzero;
    }
    FX_FLOAT xin, xout;
    if(deltax > 0) {
        xin  = (FX_FLOAT)clip_box.x1;
        xout = (FX_FLOAT)clip_box.x2;
    } else {
        xin  = (FX_FLOAT)clip_box.x2;
        xout = (FX_FLOAT)clip_box.x1;
    }
    FX_FLOAT tinx = (xin - x1) / deltax;
    if(deltay == 0) {
        deltay = (y1 > clip_box.y1) ? -nearzero : nearzero;
    }
    FX_FLOAT yin, yout;
    if(deltay > 0) {
        yin  = (FX_FLOAT)clip_box.y1;
        yout = (FX_FLOAT)clip_box.y2;
    } else {
        yin  = (FX_FLOAT)clip_box.y2;
        yout = (FX_FLOAT)clip_box.y1;
    }
    FX_FLOAT tiny = (yin - y1) / deltay;
    FX_FLOAT tin1, tin2;
    if (tinx < tiny) {
        tin1 = tinx;
        tin2 = tiny;
    } else {
        tin1 = tiny;
        tin2 = tinx;
    }
    if(tin1 <= 1.0f) {
        if(0 < tin1) {
            *x++ = (T)xin;
            *y++ = (T)yin;
            ++np;
        }
        if(tin2 <= 1.0f) {
          FX_FLOAT toutx = (xout - x1) / deltax;
          FX_FLOAT touty = (yout - y1) / deltay;
          FX_FLOAT tout1 = (toutx < touty) ? toutx : touty;
          if (tin2 > 0 || tout1 > 0) {
                if(tin2 <= tout1) {
                    if(tin2 > 0) {
                        if(tinx > tiny) {
                          *x++ = (T)xin;
                          *y++ = (T)(y1 + (deltay * tinx));
                        } else {
                          *x++ = (T)(x1 + (deltax * tiny));
                          *y++ = (T)yin;
                        }
                        ++np;
                    }
                    if(tout1 < 1.0f) {
                        if(toutx < touty) {
                          *x++ = (T)xout;
                          *y++ = (T)(y1 + (deltay * toutx));
                        } else {
                          *x++ = (T)(x1 + (deltax * touty));
                          *y++ = (T)yout;
                        }
                    } else {
                        *x++ = x2;
                        *y++ = y2;
                    }
                    ++np;
                } else {
                    if(tinx > tiny) {
                        *x++ = (T)xin;
                        *y++ = (T)yout;
                    } else {
                        *x++ = (T)xout;
                        *y++ = (T)yin;
                    }
                    ++np;
                }
          }
        }
    }
    return np;
}
}
#endif
