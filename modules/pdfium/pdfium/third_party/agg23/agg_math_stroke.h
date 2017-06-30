
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
// Stroke math
//
//----------------------------------------------------------------------------
#ifndef AGG_STROKE_MATH_INCLUDED
#define AGG_STROKE_MATH_INCLUDED
#include "agg_math.h"
#include "agg_vertex_sequence.h"
namespace agg
{
enum line_cap_e {
    butt_cap,
    square_cap,
    round_cap
};
enum line_join_e {
    miter_join         = 0,
    miter_join_revert  = 1,
    miter_join_round   = 4,
    round_join         = 2,
    bevel_join         = 3
};
enum inner_join_e {
    inner_bevel,
    inner_miter,
    inner_jag,
    inner_round
};
const FX_FLOAT stroke_theta = 1.0f / 1000.0f;
template<class VertexConsumer>
void stroke_calc_arc(VertexConsumer& out_vertices,
                     FX_FLOAT x,   FX_FLOAT y,
                     FX_FLOAT dx1, FX_FLOAT dy1,
                     FX_FLOAT dx2, FX_FLOAT dy2,
                     FX_FLOAT width,
                     FX_FLOAT approximation_scale)
{
    typedef typename VertexConsumer::value_type coord_type;
    FX_FLOAT a1 = FXSYS_atan2(dy1, dx1);
    FX_FLOAT a2 = FXSYS_atan2(dy2, dx2);
    FX_FLOAT da = a1 - a2;
    bool ccw = da > 0 && da < FX_PI;
    if(width < 0) {
        width = -width;
    }
    da = FXSYS_acos(width / (width + ((1.0f / 8) / approximation_scale))) * 2;
    out_vertices.add(coord_type(x + dx1, y + dy1));
    if (da > 0) {
      if (!ccw) {
        if (a1 > a2) {
          a2 += 2 * FX_PI;
        }
        a2 -= da / 4;
        a1 += da;
        while (a1 < a2) {
          out_vertices.add(coord_type(x + (width * FXSYS_cos(a1)),
                                      y + (width * FXSYS_sin(a1))));
          a1 += da;
        }
      } else {
        if (a1 < a2) {
          a2 -= 2 * FX_PI;
        }
        a2 += da / 4;
        a1 -= da;
        while (a1 > a2) {
          out_vertices.add(coord_type(x + (width * FXSYS_cos(a1)),
                                      y + (width * FXSYS_sin(a1))));
          a1 -= da;
        }
      }
    }
    out_vertices.add(coord_type(x + dx2, y + dy2));
}
template<class VertexConsumer>
void stroke_calc_miter(VertexConsumer& out_vertices,
                       const vertex_dist& v0,
                       const vertex_dist& v1,
                       const vertex_dist& v2,
                       FX_FLOAT dx1, FX_FLOAT dy1,
                       FX_FLOAT dx2, FX_FLOAT dy2,
                       FX_FLOAT width,
                       line_join_e line_join,
                       FX_FLOAT miter_limit,
                       FX_FLOAT approximation_scale)
{
    typedef typename VertexConsumer::value_type coord_type;
    FX_FLOAT xi = v1.x;
    FX_FLOAT yi = v1.y;
    bool miter_limit_exceeded = true;
    if(calc_intersection(v0.x + dx1, v0.y - dy1,
                         v1.x + dx1, v1.y - dy1,
                         v1.x + dx2, v1.y - dy2,
                         v2.x + dx2, v2.y - dy2,
                         &xi, &yi)) {
        FX_FLOAT d1 = calc_distance(v1.x, v1.y, xi, yi);
        FX_FLOAT lim = width * miter_limit;
        if(d1 <= lim) {
            out_vertices.add(coord_type(xi, yi));
            miter_limit_exceeded = false;
        }
    } else {
        FX_FLOAT x2 = v1.x + dx1;
        FX_FLOAT y2 = v1.y - dy1;
        if ((((x2 - v0.x) * dy1) - ((v0.y - y2) * dx1) < 0) !=
            (((x2 - v2.x) * dy1) - ((v2.y - y2) * dx1) < 0)) {
            out_vertices.add(coord_type(v1.x + dx1, v1.y - dy1));
            miter_limit_exceeded = false;
        }
    }
    if(miter_limit_exceeded) {
        switch(line_join) {
            case miter_join_revert:
                out_vertices.add(coord_type(v1.x + dx1, v1.y - dy1));
                out_vertices.add(coord_type(v1.x + dx2, v1.y - dy2));
                break;
            case miter_join_round:
                stroke_calc_arc(out_vertices,
                                v1.x, v1.y, dx1, -dy1, dx2, -dy2,
                                width, approximation_scale);
                break;
            default:
              out_vertices.add(coord_type(v1.x + dx1 + (dy1 * miter_limit),
                                          v1.y - dy1 + (dx1 * miter_limit)));
              out_vertices.add(coord_type(v1.x + dx2 - (dy2 * miter_limit),
                                          v1.y - dy2 - (dx2 * miter_limit)));
                break;
        }
    }
}
template<class VertexConsumer>
void stroke_calc_cap(VertexConsumer& out_vertices,
                     const vertex_dist& v0,
                     const vertex_dist& v1,
                     FX_FLOAT len,
                     line_cap_e line_cap,
                     FX_FLOAT width,
                     FX_FLOAT approximation_scale)
{
    typedef typename VertexConsumer::value_type coord_type;
    out_vertices.remove_all();
    FX_FLOAT dx1 = (v1.y - v0.y) / len;
    FX_FLOAT dy1 = (v1.x - v0.x) / len;
    FX_FLOAT dx2 = 0;
    FX_FLOAT dy2 = 0;
    dx1 = dx1 * width;
    dy1 = dy1 * width;
    if(line_cap != round_cap) {
        if(line_cap == square_cap) {
            dx2 = dy1;
            dy2 = dx1;
        }
        out_vertices.add(coord_type(v0.x - dx1 - dx2, v0.y + dy1 - dy2));
        out_vertices.add(coord_type(v0.x + dx1 - dx2, v0.y - dy1 - dy2));
    } else {
        FX_FLOAT a1 = FXSYS_atan2(dy1, -dx1);
        FX_FLOAT a2 = a1 + FX_PI;
        FX_FLOAT da =
            FXSYS_acos(width / (width + ((1.0f / 8) / approximation_scale))) *
            2;
        out_vertices.add(coord_type(v0.x - dx1, v0.y + dy1));
        a1 += da;
        a2 -= da / 4;
        while(a1 < a2) {
          out_vertices.add(coord_type(v0.x + (width * FXSYS_cos(a1)),
                                      v0.y + (width * FXSYS_sin(a1))));
            a1 += da;
        }
        out_vertices.add(coord_type(v0.x + dx1, v0.y - dy1));
    }
}
template<class VertexConsumer>
void stroke_calc_join(VertexConsumer& out_vertices,
                      const vertex_dist& v0,
                      const vertex_dist& v1,
                      const vertex_dist& v2,
                      FX_FLOAT len1,
                      FX_FLOAT len2,
                      FX_FLOAT width,
                      line_join_e line_join,
                      inner_join_e inner_join,
                      FX_FLOAT miter_limit,
                      FX_FLOAT inner_miter_limit,
                      FX_FLOAT approximation_scale)
{
    typedef typename VertexConsumer::value_type coord_type;
    FX_FLOAT dx1, dy1, dx2, dy2;
    dx1 = width * (v1.y - v0.y) / len1;
    dy1 = width * (v1.x - v0.x) / len1;
    dx2 = width * (v2.y - v1.y) / len2;
    dy2 = width * (v2.x - v1.x) / len2;
    out_vertices.remove_all();
    if(calc_point_location(v0.x, v0.y, v1.x, v1.y, v2.x, v2.y) > 0) {
        switch(inner_join) {
            default:
                out_vertices.add(coord_type(v1.x + dx1, v1.y - dy1));
                out_vertices.add(coord_type(v1.x + dx2, v1.y - dy2));
                break;
            case inner_miter:
                stroke_calc_miter(out_vertices,
                                  v0, v1, v2, dx1, dy1, dx2, dy2,
                                  width,
                                  miter_join_revert,
                                  inner_miter_limit,
                                  1.0f);
                break;
            case inner_jag:
            case inner_round: {
                    FX_FLOAT d = (dx1 - dx2) * (dx1 - dx2) + (dy1 - dy2) * (dy1 - dy2);
                    if(d < len1 * len1 && d < len2 * len2) {
                        stroke_calc_miter(out_vertices,
                                          v0, v1, v2, dx1, dy1, dx2, dy2,
                                          width,
                                          miter_join_revert,
                                          inner_miter_limit,
                                          1.0f);
                    } else {
                        if(inner_join == inner_jag) {
                            out_vertices.add(coord_type(v1.x + dx1, v1.y - dy1));
                            out_vertices.add(coord_type(v1.x,       v1.y      ));
                            out_vertices.add(coord_type(v1.x + dx2, v1.y - dy2));
                        } else {
                            out_vertices.add(coord_type(v1.x + dx1, v1.y - dy1));
                            out_vertices.add(coord_type(v1.x,       v1.y      ));
                            stroke_calc_arc(out_vertices,
                                            v1.x, v1.y, dx2, -dy2, dx1, -dy1,
                                            width, approximation_scale);
                            out_vertices.add(coord_type(v1.x,       v1.y      ));
                            out_vertices.add(coord_type(v1.x + dx2, v1.y - dy2));
                        }
                    }
                }
                break;
        }
    } else {
        switch(line_join) {
            case miter_join:
            case miter_join_revert:
            case miter_join_round:
                stroke_calc_miter(out_vertices,
                                  v0, v1, v2, dx1, dy1, dx2, dy2,
                                  width,
                                  line_join,
                                  miter_limit,
                                  approximation_scale);
                break;
            case round_join:
                stroke_calc_arc(out_vertices,
                                v1.x, v1.y, dx1, -dy1, dx2, -dy2,
                                width, approximation_scale);
                break;
            default:
                out_vertices.add(coord_type(v1.x + dx1, v1.y - dy1));
                out_vertices.add(coord_type(v1.x + dx2, v1.y - dy2));
                break;
        }
    }
}
}
#endif
