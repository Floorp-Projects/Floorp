
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
// Adaptation for 32-bit screen coordinates (scanline32_u) has been sponsored by
// Liberty Technology Systems, Inc., visit http://lib-sys.com
//
// Liberty Technology Systems, Inc. is the provider of
// PostScript and PDF technology for software developers.
//
//----------------------------------------------------------------------------
#ifndef AGG_SCANLINE_U_INCLUDED
#define AGG_SCANLINE_U_INCLUDED
#include "agg_array.h"
namespace agg
{
template<class CoverT> class scanline_u 
{
public:
    typedef scanline_u<CoverT> self_type;
    typedef CoverT cover_type;
    typedef int16  coord_type;
    struct span  {
        coord_type  x;
        coord_type  len;
        cover_type* covers;
    };
    typedef span* iterator;
    typedef const span* const_iterator;
    ~scanline_u()
    {
        FX_Free(m_spans);
        FX_Free(m_covers);
    }
    scanline_u() :
        m_min_x(0),
        m_max_len(0),
        m_last_x(0x7FFFFFF0),
        m_covers(0),
        m_spans(0),
        m_cur_span(0)
    {}
    void reset(int min_x, int max_x)
    {
        unsigned max_len = max_x - min_x + 2;
        if(max_len > m_max_len) {
            FX_Free(m_spans);
            FX_Free(m_covers);
            m_covers  = FX_Alloc( cover_type , max_len);
            m_spans   = FX_Alloc( span       , max_len);
            m_max_len = max_len;
        }
        m_last_x        = 0x7FFFFFF0;
        m_min_x         = min_x;
        m_cur_span      = m_spans;
    }
    void add_cell(int x, unsigned cover)
    {
        x -= m_min_x;
        m_covers[x] = (cover_type)cover;
        if(x == m_last_x + 1) {
            m_cur_span->len++;
        } else {
            m_cur_span++;
            m_cur_span->x      = (coord_type)(x + m_min_x);
            m_cur_span->len    = 1;
            m_cur_span->covers = m_covers + x;
        }
        m_last_x = x;
    }
    void add_cells(int x, unsigned len, const CoverT* covers)
    {
        x -= m_min_x;
        FXSYS_memcpy(m_covers + x, covers, len * sizeof(CoverT));
        if(x == m_last_x + 1) {
            m_cur_span->len += (coord_type)len;
        } else {
            m_cur_span++;
            m_cur_span->x      = (coord_type)(x + m_min_x);
            m_cur_span->len    = (coord_type)len;
            m_cur_span->covers = m_covers + x;
        }
        m_last_x = x + len - 1;
    }
    void add_span(int x, unsigned len, unsigned cover)
    {
        x -= m_min_x;
        FXSYS_memset(m_covers + x, cover, len);
        if(x == m_last_x + 1) {
            m_cur_span->len += (coord_type)len;
        } else {
            m_cur_span++;
            m_cur_span->x      = (coord_type)(x + m_min_x);
            m_cur_span->len    = (coord_type)len;
            m_cur_span->covers = m_covers + x;
        }
        m_last_x = x + len - 1;
    }
    void finalize(int y)
    {
        m_y = y;
    }
    void reset_spans()
    {
        m_last_x    = 0x7FFFFFF0;
        m_cur_span  = m_spans;
    }
    int      y()           const
    {
        return m_y;
    }
    unsigned num_spans()   const
    {
        return unsigned(m_cur_span - m_spans);
    }
    const_iterator begin() const
    {
        return m_spans + 1;
    }
    iterator       begin()
    {
        return m_spans + 1;
    }
private:
    scanline_u(const self_type&);
    const self_type& operator = (const self_type&);
private:
    int           m_min_x;
    unsigned      m_max_len;
    int           m_last_x;
    int           m_y;
    cover_type*   m_covers;
    span*         m_spans;
    span*         m_cur_span;
};
typedef scanline_u<int8u> scanline_u8;
}
#endif
