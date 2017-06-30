
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
// Adaptation for high precision colors has been sponsored by
// Liberty Technology Systems, Inc., visit http://lib-sys.com
//
// Liberty Technology Systems, Inc. is the provider of
// PostScript and PDF technology for software developers.
//
//----------------------------------------------------------------------------
#ifndef AGG_PIXFMT_GRAY_INCLUDED
#define AGG_PIXFMT_GRAY_INCLUDED
#include "agg_basics.h"
#include "agg_color_gray.h"
#include "agg_rendering_buffer.h"
namespace agg
{
template<class ColorT> struct blender_gray  {
    typedef ColorT color_type;
    typedef typename color_type::value_type value_type;
    typedef typename color_type::calc_type calc_type;
    enum base_scale_e { base_shift = color_type::base_shift };
    static AGG_INLINE void blend_pix(value_type* p, unsigned cv,
                                     unsigned alpha, unsigned cover = 0)
    {
        *p = (value_type)((((cv - calc_type(*p)) * alpha) + (calc_type(*p) << base_shift)) >> base_shift);
    }
};
template<class Blender, unsigned Step = 1, unsigned Offset = 0>
class pixel_formats_gray 
{
public:
    typedef rendering_buffer::row_data row_data;
    typedef rendering_buffer::span_data span_data;
    typedef typename Blender::color_type color_type;
    typedef typename color_type::value_type value_type;
    typedef typename color_type::calc_type calc_type;
    enum base_scale_e {
        base_shift = color_type::base_shift,
        base_size  = color_type::base_size,
        base_mask  = color_type::base_mask
    };
private:
    static AGG_INLINE void copy_or_blend_pix(value_type* p,
            const color_type& c,
            unsigned cover)
    {
        if (c.a) {
            calc_type alpha = (calc_type(c.a) * (cover + 1)) >> 8;
            if(alpha == base_mask) {
                *p = c.v;
            } else {
                Blender::blend_pix(p, c.v, alpha, cover);
            }
        }
    }
    static AGG_INLINE void copy_or_blend_pix(value_type* p,
            const color_type& c)
    {
        if (c.a) {
            if(c.a == base_mask) {
                *p = c.v;
            } else {
                Blender::blend_pix(p, c.v, c.a);
            }
        }
    }
public:
    pixel_formats_gray(rendering_buffer& rb) :
        m_rbuf(&rb)
    {}
    AGG_INLINE unsigned width()  const
    {
        return m_rbuf->width();
    }
    AGG_INLINE unsigned height() const
    {
        return m_rbuf->height();
    }
    AGG_INLINE color_type pixel(int x, int y) const
    {
        value_type* p = (value_type*)m_rbuf->row(y) + x * Step + Offset;
        return color_type(*p);
    }
    row_data row(int x, int y) const
    {
        return row_data(x,
                        width() - 1,
                        m_rbuf->row(y) +
                        x * Step * sizeof(value_type) +
                        Offset * sizeof(value_type));
    }
    span_data span(int x, int y, unsigned len)
    {
        return span_data(x, len,
                         m_rbuf->row(y) +
                         x * Step * sizeof(value_type) +
                         Offset * sizeof(value_type));
    }
    AGG_INLINE void copy_pixel(int x, int y, const color_type& c)
    {
        *((value_type*)m_rbuf->row(y) + x * Step + Offset) = c.v;
    }
    AGG_INLINE void blend_pixel(int x, int y, const color_type& c, int8u cover)
    {
        copy_or_blend_pix((value_type*)m_rbuf->row(y) + x * Step + Offset, c, cover);
    }
    AGG_INLINE void copy_hline(int x, int y,
                               unsigned len,
                               const color_type& c)
    {
        value_type* p = (value_type*)m_rbuf->row(y) + x * Step + Offset;
        do {
            *p = c.v;
            p += Step;
        } while(--len);
    }
    void blend_hline(int x, int y,
                     unsigned len,
                     const color_type& c,
                     int8u cover)
    {
        if (c.a) {
            value_type* p = (value_type*)m_rbuf->row(y) + x * Step + Offset;
            calc_type alpha = (calc_type(c.a) * (cover + 1)) >> 8;
            if(alpha == base_mask) {
                do {
                    *p = c.v;
                    p += Step;
                } while(--len);
            } else {
                do {
                    Blender::blend_pix(p, c.v, alpha, cover);
                    p += Step;
                } while(--len);
            }
        }
    }
    void blend_solid_hspan(int x, int y,
                           unsigned len,
                           const color_type& c,
                           const int8u* covers)
    {
        if (c.a) {
            value_type* p = (value_type*)m_rbuf->row(y) + x * Step + Offset;
            do {
                calc_type alpha = (calc_type(c.a) * (calc_type(*covers) + 1)) >> 8;
                if(alpha == base_mask) {
                    *p = c.v;
                } else {
                    Blender::blend_pix(p, c.v, alpha, *covers);
                }
                p += Step;
                ++covers;
            } while(--len);
        }
    }
private:
    rendering_buffer* m_rbuf;
};
typedef blender_gray<gray8>      blender_gray8;
typedef pixel_formats_gray<blender_gray8, 1, 0> pixfmt_gray8;
}
#endif
