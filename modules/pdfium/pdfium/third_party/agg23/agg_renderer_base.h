
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
// class renderer_base
//
//----------------------------------------------------------------------------
#ifndef AGG_RENDERER_BASE_INCLUDED
#define AGG_RENDERER_BASE_INCLUDED
#include "agg_basics.h"
#include "agg_rendering_buffer.h"
namespace agg
{
template<class PixelFormat> class renderer_base 
{
public:
    typedef PixelFormat pixfmt_type;
    typedef typename pixfmt_type::color_type color_type;
    typedef typename pixfmt_type::row_data row_data;
    typedef typename pixfmt_type::span_data span_data;
    renderer_base() : m_ren(0), m_clip_box(1, 1, 0, 0) {}
    renderer_base(pixfmt_type& ren) :
        m_ren(&ren),
        m_clip_box(0, 0, ren.width() - 1, ren.height() - 1)
    {}
    void attach(pixfmt_type& ren)
    {
        m_ren = &ren;
        m_clip_box = rect(0, 0, ren.width() - 1, ren.height() - 1);
    }
    const pixfmt_type& ren() const
    {
        return *m_ren;
    }
    pixfmt_type& ren()
    {
        return *m_ren;
    }
    unsigned width()  const
    {
        return m_ren->width();
    }
    unsigned height() const
    {
        return m_ren->height();
    }
    void first_clip_box() {}
    bool next_clip_box()
    {
        return false;
    }
    const rect& clip_box() const
    {
        return m_clip_box;
    }
    int         xmin()     const
    {
        return m_clip_box.x1;
    }
    int         ymin()     const
    {
        return m_clip_box.y1;
    }
    int         xmax()     const
    {
        return m_clip_box.x2;
    }
    int         ymax()     const
    {
        return m_clip_box.y2;
    }
    const rect& bounding_clip_box() const
    {
        return m_clip_box;
    }
    int         bounding_xmin()     const
    {
        return m_clip_box.x1;
    }
    int         bounding_ymin()     const
    {
        return m_clip_box.y1;
    }
    int         bounding_xmax()     const
    {
        return m_clip_box.x2;
    }
    int         bounding_ymax()     const
    {
        return m_clip_box.y2;
    }
    void blend_hline(int x1, int y, int x2,
                     const color_type& c, cover_type cover)
    {
        if(x1 > x2) {
            int t = x2;
            x2 = x1;
            x1 = t;
        }
        if(y  > ymax()) {
            return;
        }
        if(y  < ymin()) {
            return;
        }
        if(x1 > xmax()) {
            return;
        }
        if(x2 < xmin()) {
            return;
        }
        if(x1 < xmin()) {
            x1 = xmin();
        }
        if(x2 > xmax()) {
            x2 = xmax();
        }
        m_ren->blend_hline(x1, y, x2 - x1 + 1, c, cover);
    }
    void blend_solid_hspan(int x, int y, int len,
                           const color_type& c,
                           const cover_type* covers)
    {
        if(y > ymax()) {
            return;
        }
        if(y < ymin()) {
            return;
        }
        if(x < xmin()) {
            len -= xmin() - x;
            if(len <= 0) {
                return;
            }
            covers += xmin() - x;
            x = xmin();
        }
        if(x + len > xmax()) {
            len = xmax() - x + 1;
            if(len <= 0) {
                return;
            }
        }
        m_ren->blend_solid_hspan(x, y, len, c, covers);
    }
private:
    pixfmt_type* m_ren;
    rect         m_clip_box;
};
}
#endif
