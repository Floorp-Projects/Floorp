
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
#ifndef AGG_VCGEN_STROKE_INCLUDED
#define AGG_VCGEN_STROKE_INCLUDED
#include "agg_math_stroke.h"
namespace agg
{
class vcgen_stroke 
{
    enum status_e {
        initial,
        ready,
        cap1,
        cap2,
        outline1,
        close_first,
        outline2,
        out_vertices,
        end_poly1,
        end_poly2,
        stop
    };
public:
    typedef vertex_sequence<vertex_dist_cmd, 6> vertex_storage;
    typedef pod_deque<point_type, 6>        coord_storage;
    vcgen_stroke();
    void line_cap(line_cap_e lc)
    {
        m_line_cap = lc;
    }
    void line_join(line_join_e lj)
    {
        m_line_join = lj;
    }
    void inner_join(inner_join_e ij)
    {
        m_inner_join = ij;
    }
    line_cap_e   line_cap()   const
    {
        return m_line_cap;
    }
    line_join_e  line_join()  const
    {
        return m_line_join;
    }
    inner_join_e inner_join() const
    {
        return m_inner_join;
    }
    void width(FX_FLOAT w)
    {
        m_width = w / 2;
    }
    void miter_limit(FX_FLOAT ml)
    {
        m_miter_limit = ml;
    }
    void miter_limit_theta(FX_FLOAT t);
    void inner_miter_limit(FX_FLOAT ml)
    {
        m_inner_miter_limit = ml;
    }
    void approximation_scale(FX_FLOAT as)
    {
        m_approx_scale = as;
    }
    FX_FLOAT width() const
    {
        return m_width * 2;
    }
    FX_FLOAT miter_limit() const
    {
        return m_miter_limit;
    }
    FX_FLOAT inner_miter_limit() const
    {
        return m_inner_miter_limit;
    }
    FX_FLOAT approximation_scale() const
    {
        return m_approx_scale;
    }
    void remove_all();
    void add_vertex(FX_FLOAT x, FX_FLOAT y, unsigned cmd);
    void     rewind(unsigned path_id);
    unsigned vertex(FX_FLOAT* x, FX_FLOAT* y);
private:
    vcgen_stroke(const vcgen_stroke&);
    const vcgen_stroke& operator = (const vcgen_stroke&);
    vertex_storage m_src_vertices;
    coord_storage  m_out_vertices;
    FX_FLOAT         m_width;
    FX_FLOAT         m_miter_limit;
    FX_FLOAT         m_inner_miter_limit;
    FX_FLOAT         m_approx_scale;
    line_cap_e     m_line_cap;
    line_join_e    m_line_join;
    inner_join_e   m_inner_join;
    unsigned       m_closed;
    status_e       m_status;
    status_e       m_prev_status;
    unsigned       m_src_vertex;
    unsigned       m_out_vertex;
};
}
#endif
