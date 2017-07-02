
//----------------------------------------------------------------------------
// Anti-Grain Geometry - Version 2.3
// Copyright (C) 2002-2005 Maxim Shemanarev (http://www.antigrain.com)
// Copyright (C) 2005 Tony Juricic (tonygeek@yahoo.com)
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
#ifndef AGG_CURVES_INCLUDED
#define AGG_CURVES_INCLUDED
#include "agg_array.h"
namespace agg
{
struct curve4_points  {
    FX_FLOAT cp[8];
    curve4_points() {}
    curve4_points(FX_FLOAT x1, FX_FLOAT y1,
                  FX_FLOAT x2, FX_FLOAT y2,
                  FX_FLOAT x3, FX_FLOAT y3,
                  FX_FLOAT x4, FX_FLOAT y4)
    {
        cp[0] = x1;
        cp[1] = y1;
        cp[2] = x2;
        cp[3] = y2;
        cp[4] = x3;
        cp[5] = y3;
        cp[6] = x4;
        cp[7] = y4;
    }
    void init(FX_FLOAT x1, FX_FLOAT y1,
              FX_FLOAT x2, FX_FLOAT y2,
              FX_FLOAT x3, FX_FLOAT y3,
              FX_FLOAT x4, FX_FLOAT y4)
    {
        cp[0] = x1;
        cp[1] = y1;
        cp[2] = x2;
        cp[3] = y2;
        cp[4] = x3;
        cp[5] = y3;
        cp[6] = x4;
        cp[7] = y4;
    }
    FX_FLOAT  operator [] (unsigned i) const
    {
        return cp[i];
    }
    FX_FLOAT& operator [] (unsigned i)
    {
        return cp[i];
    }
};
class curve4_div 
{
public:
    curve4_div() :
        m_count(0)
    {}
    curve4_div(FX_FLOAT x1, FX_FLOAT y1,
               FX_FLOAT x2, FX_FLOAT y2,
               FX_FLOAT x3, FX_FLOAT y3,
               FX_FLOAT x4, FX_FLOAT y4) :
        m_count(0)
    {
        init(x1, y1, x2, y2, x3, y3, x4, y4);
    }
    curve4_div(const curve4_points& cp) :
        m_count(0)
    {
        init(cp[0], cp[1], cp[2], cp[3], cp[4], cp[5], cp[6], cp[7]);
    }
    void reset()
    {
        m_points.remove_all();
        m_count = 0;
    }
    void init(FX_FLOAT x1, FX_FLOAT y1,
              FX_FLOAT x2, FX_FLOAT y2,
              FX_FLOAT x3, FX_FLOAT y3,
              FX_FLOAT x4, FX_FLOAT y4);
    void init(const curve4_points& cp)
    {
        init(cp[0], cp[1], cp[2], cp[3], cp[4], cp[5], cp[6], cp[7]);
    }
    void rewind(unsigned)
    {
        m_count = 0;
    }
    unsigned vertex(FX_FLOAT* x, FX_FLOAT* y)
    {
        if(m_count >= m_points.size()) {
            return path_cmd_stop;
        }
        const point_type& p = m_points[m_count++];
        *x = p.x;
        *y = p.y;
        return (m_count == 1) ? path_cmd_move_to : path_cmd_line_to;
    }
    unsigned vertex_flag(FX_FLOAT* x, FX_FLOAT* y, int& flag)
    {
        if(m_count >= m_points.size()) {
            return path_cmd_stop;
        }
        const point_type& p = m_points[m_count++];
        *x = p.x;
        *y = p.y;
        flag = p.flag;
        return (m_count == 1) ? path_cmd_move_to : path_cmd_line_to;
    }
    int count()
    {
        return m_points.size();
    }
private:
    void bezier(FX_FLOAT x1, FX_FLOAT y1,
                FX_FLOAT x2, FX_FLOAT y2,
                FX_FLOAT x3, FX_FLOAT y3,
                FX_FLOAT x4, FX_FLOAT y4);
    void recursive_bezier(FX_FLOAT x1, FX_FLOAT y1,
                          FX_FLOAT x2, FX_FLOAT y2,
                          FX_FLOAT x3, FX_FLOAT y3,
                          FX_FLOAT x4, FX_FLOAT y4,
                          unsigned level);
    FX_FLOAT    m_distance_tolerance_square;
    FX_FLOAT    m_distance_tolerance_manhattan;
    unsigned              m_count;
    pod_deque<point_type> m_points;
};
class curve4 
{
public:
    curve4() {}
    curve4(FX_FLOAT x1, FX_FLOAT y1,
           FX_FLOAT x2, FX_FLOAT y2,
           FX_FLOAT x3, FX_FLOAT y3,
           FX_FLOAT x4, FX_FLOAT y4)
    {
        init(x1, y1, x2, y2, x3, y3, x4, y4);
    }
    curve4(const curve4_points& cp)
    {
        init(cp[0], cp[1], cp[2], cp[3], cp[4], cp[5], cp[6], cp[7]);
    }
    void reset()
    {
        m_curve_div.reset();
    }
    void init(FX_FLOAT x1, FX_FLOAT y1,
              FX_FLOAT x2, FX_FLOAT y2,
              FX_FLOAT x3, FX_FLOAT y3,
              FX_FLOAT x4, FX_FLOAT y4)
    {
        m_curve_div.init(x1, y1, x2, y2, x3, y3, x4, y4);
    }
    void init(const curve4_points& cp)
    {
        init(cp[0], cp[1], cp[2], cp[3], cp[4], cp[5], cp[6], cp[7]);
    }
    void rewind(unsigned path_id)
    {
        m_curve_div.rewind(path_id);
    }
    unsigned vertex(FX_FLOAT* x, FX_FLOAT* y)
    {
        return m_curve_div.vertex(x, y);
    }
    unsigned vertex_curve_flag(FX_FLOAT* x, FX_FLOAT* y, int& flag)
    {
        return m_curve_div.vertex_flag(x, y, flag);
    }
    int count()
    {
        return m_curve_div.count();
    }
private:
    curve4_div m_curve_div;
};
}
#endif
