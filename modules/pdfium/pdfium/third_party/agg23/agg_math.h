
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
// Bessel function (besj) was adapted for use in AGG library by Andy Wilk
// Contact: castor.vulgaris@gmail.com
//----------------------------------------------------------------------------
#ifndef AGG_MATH_INCLUDED
#define AGG_MATH_INCLUDED
#include "agg_basics.h"
namespace agg
{
const FX_FLOAT intersection_epsilon = 1.0e-30f;
AGG_INLINE FX_FLOAT calc_point_location(FX_FLOAT x1, FX_FLOAT y1,
                                        FX_FLOAT x2, FX_FLOAT y2,
                                        FX_FLOAT x,  FX_FLOAT y)
{
  return ((x - x2) * (y2 - y1)) - ((y - y2) * (x2 - x1));
}
AGG_INLINE FX_FLOAT calc_distance(FX_FLOAT x1, FX_FLOAT y1, FX_FLOAT x2, FX_FLOAT y2)
{
    FX_FLOAT dx = x2 - x1;
    FX_FLOAT dy = y2 - y1;
    return FXSYS_sqrt2(dx, dy);
}
AGG_INLINE FX_FLOAT calc_line_point_distance(FX_FLOAT x1, FX_FLOAT y1,
        FX_FLOAT x2, FX_FLOAT y2,
        FX_FLOAT x,  FX_FLOAT y)
{
    FX_FLOAT dx = x2 - x1;
    FX_FLOAT dy = y2 - y1;
    FX_FLOAT d = FXSYS_sqrt2(dx, dy);
    if(d < intersection_epsilon) {
        return calc_distance(x1, y1, x, y);
    }
    return ((x - x2) * dy / d) - ((y - y2) * dx / d);
}
AGG_INLINE bool calc_intersection(FX_FLOAT ax, FX_FLOAT ay, FX_FLOAT bx, FX_FLOAT by,
                                  FX_FLOAT cx, FX_FLOAT cy, FX_FLOAT dx, FX_FLOAT dy,
                                  FX_FLOAT* x, FX_FLOAT* y)
{
  FX_FLOAT num = ((ay - cy) * (dx - cx)) - ((ax - cx) * (dy - cy));
  FX_FLOAT den = ((bx - ax) * (dy - cy)) - ((by - ay) * (dx - cx));
    if (FXSYS_fabs(den) < intersection_epsilon) {
        return false;
    }
    *x = ax + ((bx - ax) * num / den);
    *y = ay + ((by - ay) * num / den);
    return true;
}
}
#endif
