
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
#ifndef AGG_BASICS_INCLUDED
#define AGG_BASICS_INCLUDED
#ifndef AGG_INT8
#define AGG_INT8 signed char
#endif
#ifndef AGG_INT8U
#define AGG_INT8U unsigned char
#endif
#ifndef AGG_INT16
#define AGG_INT16 short
#endif
#ifndef AGG_INT16U
#define AGG_INT16U unsigned short
#endif
#ifndef AGG_INT32
#define AGG_INT32 int
#endif
#ifndef AGG_INT32U
#define AGG_INT32U unsigned
#endif
#ifndef AGG_INT64
#define AGG_INT64 signed long long
#endif
#ifndef AGG_INT64U
#define AGG_INT64U unsigned long long
#endif
#define AGG_INLINE inline

#include "core/fxcrt/fx_system.h"  // For FX_FLOAT

namespace agg
{
typedef AGG_INT8   int8;
typedef AGG_INT8U  int8u;
typedef AGG_INT16  int16;
typedef AGG_INT16U int16u;
typedef AGG_INT32  int32;
typedef AGG_INT32U int32u;
typedef AGG_INT64  int64;
typedef AGG_INT64U int64u;
typedef unsigned char cover_type;
enum cover_scale_e {
    cover_shift = 8,
    cover_size  = 1 << cover_shift,
    cover_mask  = cover_size - 1,
    cover_none  = 0,
    cover_full  = cover_mask
};
template<class T> struct rect_base  {
    typedef rect_base<T> self_type;
    T x1;
    T y1;
    T x2;
    T y2;
    rect_base() {}
    rect_base(T x1_, T y1_, T x2_, T y2_) :
        x1(x1_), y1(y1_), x2(x2_), y2(y2_) {}
    const self_type& normalize()
    {
        T t;
        if(x1 > x2) {
            t = x1;
            x1 = x2;
            x2 = t;
        }
        if(y1 > y2) {
            t = y1;
            y1 = y2;
            y2 = t;
        }
        return *this;
    }
    bool clip(const self_type& r)
    {
        if(x2 > r.x2) {
            x2 = r.x2;
        }
        if(y2 > r.y2) {
            y2 = r.y2;
        }
        if(x1 < r.x1) {
            x1 = r.x1;
        }
        if(y1 < r.y1) {
            y1 = r.y1;
        }
        return x1 <= x2 && y1 <= y2;
    }
    bool is_valid() const
    {
        return x1 <= x2 && y1 <= y2;
    }
};
template<class Rect>
inline Rect intersect_rectangles(const Rect& r1, const Rect& r2)
{
    Rect r = r1;
    if(r.x2 > r2.x2) {
        r.x2 = r2.x2;
    }
    if(r.y2 > r2.y2) {
        r.y2 = r2.y2;
    }
    if(r.x1 < r2.x1) {
        r.x1 = r2.x1;
    }
    if(r.y1 < r2.y1) {
        r.y1 = r2.y1;
    }
    return r;
}
template<class Rect>
inline Rect unite_rectangles(const Rect& r1, const Rect& r2)
{
    Rect r = r1;
    if(r.x2 < r2.x2) {
        r.x2 = r2.x2;
    }
    if(r.y2 < r2.y2) {
        r.y2 = r2.y2;
    }
    if(r.x1 > r2.x1) {
        r.x1 = r2.x1;
    }
    if(r.y1 > r2.y1) {
        r.y1 = r2.y1;
    }
    return r;
}
typedef rect_base<int>    rect;
typedef rect_base<FX_FLOAT> rect_d;
enum path_commands_e {
    path_cmd_stop     = 0,
    path_cmd_move_to  = 1,
    path_cmd_line_to  = 2,
    path_cmd_curve3   = 3,
    path_cmd_curve4   = 4,
    path_cmd_curveN   = 5,
    path_cmd_catrom   = 6,
    path_cmd_ubspline = 7,
    path_cmd_end_poly = 0x0F,
    path_cmd_mask     = 0x0F
};
enum path_flags_e {
    path_flags_none  = 0,
    path_flags_ccw   = 0x10,
    path_flags_cw    = 0x20,
    path_flags_close = 0x40,
    path_flags_jr	 = 0x80,
    path_flags_mask  = 0xF0
};
inline bool is_vertex(unsigned c)
{
    c &= ~path_flags_jr;
    return c >= path_cmd_move_to && c < path_cmd_end_poly;
}
inline bool is_drawing(unsigned c)
{
    c &= ~path_flags_jr;
    return c >= path_cmd_line_to && c < path_cmd_end_poly;
}
inline bool is_stop(unsigned c)
{
    c &= ~path_flags_jr;
    return c == path_cmd_stop;
}
inline bool is_move_to(unsigned c)
{
    c &= ~path_flags_jr;
    return c == path_cmd_move_to;
}
inline bool is_line_to(unsigned c)
{
    c &= ~path_flags_jr;
    return c == path_cmd_line_to;
}
inline bool is_curve(unsigned c)
{
    c &= ~path_flags_jr;
    return c == path_cmd_curve3 || c == path_cmd_curve4;
}
inline bool is_curve3(unsigned c)
{
    c &= ~path_flags_jr;
    return c == path_cmd_curve3;
}
inline bool is_curve4(unsigned c)
{
    c &= ~path_flags_jr;
    return c == path_cmd_curve4;
}
inline bool is_end_poly(unsigned c)
{
    c &= ~path_flags_jr;
    return (c & path_cmd_mask) == path_cmd_end_poly;
}
inline bool is_close(unsigned c)
{
    c &= ~path_flags_jr;
    return (c & ~(path_flags_cw | path_flags_ccw)) ==
           (path_cmd_end_poly | path_flags_close);
}
inline bool is_next_poly(unsigned c)
{
    c &= ~path_flags_jr;
    return is_stop(c) || is_move_to(c) || is_end_poly(c);
}
inline bool is_cw(unsigned c)
{
    c &= ~path_flags_jr;
    return (c & path_flags_cw) != 0;
}
inline bool is_ccw(unsigned c)
{
    c &= ~path_flags_jr;
    return (c & path_flags_ccw) != 0;
}
inline bool is_oriented(unsigned c)
{
    c &= ~path_flags_jr;
    return (c & (path_flags_cw | path_flags_ccw)) != 0;
}
inline bool is_closed(unsigned c)
{
    c &= ~path_flags_jr;
    return (c & path_flags_close) != 0;
}
inline unsigned get_close_flag(unsigned c)
{
    c &= ~path_flags_jr;
    return c & path_flags_close;
}
inline unsigned clear_orientation(unsigned c)
{
    c &= ~path_flags_jr;
    return c & ~(path_flags_cw | path_flags_ccw);
}
inline unsigned get_orientation(unsigned c)
{
    c &= ~path_flags_jr;
    return c & (path_flags_cw | path_flags_ccw);
}
inline unsigned set_orientation(unsigned c, unsigned o)
{
    c &= ~path_flags_jr;
    return clear_orientation(c) | o;
}
struct point_type  {
    FX_FLOAT x, y;
    unsigned flag;
    point_type() {}
    point_type(FX_FLOAT x_, FX_FLOAT y_, unsigned flag_ = 0) : x(x_), y(y_), flag(flag_) {}
};
struct point_type_flag : public point_type {
    unsigned flag;
    point_type_flag()
    {
        flag = 0;
    }
    point_type_flag(FX_FLOAT x_, FX_FLOAT y_, unsigned flag_ = 0) : point_type(x_, y_), flag(flag_) {}
};
struct vertex_type  {
    FX_FLOAT   x, y;
    unsigned cmd;
    vertex_type() {}
    vertex_type(FX_FLOAT x_, FX_FLOAT y_, unsigned cmd_) :
        x(x_), y(y_), cmd(cmd_) {}
};
}
#endif
