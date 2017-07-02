
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
// conv_stroke
//
//----------------------------------------------------------------------------
#ifndef AGG_CONV_STROKE_INCLUDED
#define AGG_CONV_STROKE_INCLUDED
#include "agg_basics.h"
#include "agg_vcgen_stroke.h"
#include "agg_conv_adaptor_vcgen.h"
namespace agg
{
template<class VertexSource, class Markers = null_markers>
struct conv_stroke :
        public conv_adaptor_vcgen<VertexSource, vcgen_stroke, Markers> {
    typedef Markers marker_type;
    typedef conv_adaptor_vcgen<VertexSource, vcgen_stroke, Markers> base_type;
    conv_stroke(VertexSource& vs) :
        conv_adaptor_vcgen<VertexSource, vcgen_stroke, Markers>(vs)
    {
    }
    void line_cap(line_cap_e lc)
    {
        base_type::generator().line_cap(lc);
    }
    void line_join(line_join_e lj)
    {
        base_type::generator().line_join(lj);
    }
    void inner_join(inner_join_e ij)
    {
        base_type::generator().inner_join(ij);
    }
    line_cap_e   line_cap()   const
    {
        return base_type::generator().line_cap();
    }
    line_join_e  line_join()  const
    {
        return base_type::generator().line_join();
    }
    inner_join_e inner_join() const
    {
        return base_type::generator().inner_join();
    }
    void width(FX_FLOAT w)
    {
        base_type::generator().width(w);
    }
    void miter_limit(FX_FLOAT ml)
    {
        base_type::generator().miter_limit(ml);
    }
    void miter_limit_theta(FX_FLOAT t)
    {
        base_type::generator().miter_limit_theta(t);
    }
    void inner_miter_limit(FX_FLOAT ml)
    {
        base_type::generator().inner_miter_limit(ml);
    }
    void approximation_scale(FX_FLOAT as)
    {
        base_type::generator().approximation_scale(as);
    }
    FX_FLOAT width() const
    {
        return base_type::generator().width();
    }
    FX_FLOAT miter_limit() const
    {
        return base_type::generator().miter_limit();
    }
    FX_FLOAT inner_miter_limit() const
    {
        return base_type::generator().inner_miter_limit();
    }
    FX_FLOAT approximation_scale() const
    {
        return base_type::generator().approximation_scale();
    }
    void shorten(FX_FLOAT s)
    {
        base_type::generator().shorten(s);
    }
    FX_FLOAT shorten() const
    {
        return base_type::generator().shorten();
    }
private:
    conv_stroke(const conv_stroke<VertexSource, Markers>&);
    const conv_stroke<VertexSource, Markers>&
    operator = (const conv_stroke<VertexSource, Markers>&);
};
}
#endif
