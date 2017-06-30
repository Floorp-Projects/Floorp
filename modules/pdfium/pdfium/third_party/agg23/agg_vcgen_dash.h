
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
// Line dash generator
//
//----------------------------------------------------------------------------
#ifndef AGG_VCGEN_DASH_INCLUDED
#define AGG_VCGEN_DASH_INCLUDED
#include "agg_basics.h"
#include "agg_vertex_sequence.h"
namespace agg
{
class vcgen_dash 
{
    enum max_dashes_e {
        max_dashes = 32
    };
    enum status_e {
        initial,
        ready,
        polyline,
        stop
    };
public:
    typedef vertex_sequence<vertex_dist, 6> vertex_storage;
    vcgen_dash();
    void remove_all_dashes();
    void add_dash(FX_FLOAT dash_len, FX_FLOAT gap_len);
    void dash_start(FX_FLOAT ds);
    void shorten(FX_FLOAT s)
    {
        m_shorten = s;
    }
    double shorten() const
    {
        return m_shorten;
    }
    void remove_all();
    void add_vertex(FX_FLOAT x, FX_FLOAT y, unsigned cmd);
    void     rewind(unsigned path_id);
    unsigned vertex(FX_FLOAT* x, FX_FLOAT* y);
private:
    vcgen_dash(const vcgen_dash&);
    const vcgen_dash& operator = (const vcgen_dash&);
    void calc_dash_start(FX_FLOAT ds);
    FX_FLOAT     m_dashes[max_dashes];
    FX_FLOAT		m_total_dash_len;
    unsigned        m_num_dashes;
    FX_FLOAT     m_dash_start;
    FX_FLOAT     m_shorten;
    FX_FLOAT     m_curr_dash_start;
    unsigned        m_curr_dash;
    FX_FLOAT     m_curr_rest;
    const vertex_dist* m_v1;
    const vertex_dist* m_v2;
    vertex_storage m_src_vertices;
    unsigned       m_closed;
    status_e       m_status;
    unsigned       m_src_vertex;
};
}
#endif
