
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
#ifndef AGG_RENDERER_SCANLINE_INCLUDED
#define AGG_RENDERER_SCANLINE_INCLUDED
#include "agg_basics.h"
#include "agg_renderer_base.h"
#include "agg_render_scanlines.h"
namespace agg
{
template<class BaseRenderer, class SpanGenerator> class renderer_scanline_aa 
{
public:
    typedef BaseRenderer  base_ren_type;
    typedef SpanGenerator span_gen_type;
    renderer_scanline_aa() : m_ren(0), m_span_gen(0) {}
    renderer_scanline_aa(base_ren_type& ren, span_gen_type& span_gen) :
        m_ren(&ren),
        m_span_gen(&span_gen)
    {}
    void attach(base_ren_type& ren, span_gen_type& span_gen)
    {
        m_ren = &ren;
        m_span_gen = &span_gen;
    }
    void prepare(unsigned max_span_len)
    {
        m_span_gen->prepare(max_span_len);
    }
    template<class Scanline> void render(const Scanline& sl)
    {
        int y = sl.y();
        m_ren->first_clip_box();
        do {
            int xmin = m_ren->xmin();
            int xmax = m_ren->xmax();
            if(y >= m_ren->ymin() && y <= m_ren->ymax()) {
                unsigned num_spans = sl.num_spans();
                typename Scanline::const_iterator span = sl.begin();
                for(;;) {
                    int x = span->x;
                    int len = span->len;
                    bool solid = false;
                    const typename Scanline::cover_type* covers = span->covers;
                    if(len < 0) {
                        solid = true;
                        len = -len;
                    }
                    if(x < xmin) {
                        len -= xmin - x;
                        if(!solid) {
                            covers += xmin - x;
                        }
                        x = xmin;
                    }
                    if(len > 0) {
                        if(x + len > xmax) {
                            len = xmax - x + 1;
                        }
                        if(len > 0) {
                            m_ren->blend_color_hspan_no_clip(
                                x, y, len,
                                m_span_gen->generate(x, y, len),
                                solid ? 0 : covers,
                                *covers);
                        }
                    }
                    if(--num_spans == 0) {
                        break;
                    }
                    ++span;
                }
            }
        } while(m_ren->next_clip_box());
    }
private:
    base_ren_type* m_ren;
    SpanGenerator* m_span_gen;
};
}
#endif
