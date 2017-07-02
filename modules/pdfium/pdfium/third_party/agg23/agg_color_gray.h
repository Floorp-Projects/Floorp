
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
//
// color types gray8, gray16
//
//----------------------------------------------------------------------------
#ifndef AGG_COLOR_GRAY_INCLUDED
#define AGG_COLOR_GRAY_INCLUDED
#include "agg_basics.h"
namespace agg
{
struct gray8 {
    typedef int8u  value_type;
    typedef int32u calc_type;
    typedef int32  long_type;
    enum base_scale_e {
        base_shift = 8,
        base_size  = 1 << base_shift,
        base_mask  = base_size - 1
    };
    typedef gray8 self_type;
    value_type v;
    value_type a;
    gray8() {}
    gray8(unsigned v_, unsigned a_ = base_mask) :
        v(int8u(v_)), a(int8u(a_)) {}
};
}
#endif
