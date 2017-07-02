
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
// class rendering_buffer
//
//----------------------------------------------------------------------------
#ifndef AGG_RENDERING_BUFFER_INCLUDED
#define AGG_RENDERING_BUFFER_INCLUDED
#include "agg_basics.h"
namespace agg
{
class rendering_buffer 
{
public:
    struct row_data  {
        int x1, x2;
        const int8u* ptr;
        row_data() {}
        row_data(int x1_, int x2_, const int8u* ptr_) :
            x1(x1_), x2(x2_), ptr(ptr_) {}
    };
    struct span_data  {
        int x;
        unsigned len;
        int8u* ptr;
        span_data() {}
        span_data(int) : x(0), len(0), ptr(0) {}
        span_data(int x_, unsigned len_, int8u* ptr_) :
            x(x_), len(len_), ptr(ptr_) {}
    };
    ~rendering_buffer()
    {
        FX_Free(m_rows);
    }
    rendering_buffer() :
        m_buf(0),
        m_rows(0),
        m_width(0),
        m_height(0),
        m_stride(0),
        m_max_height(0)
    {
    }
    rendering_buffer(int8u* buf, unsigned width, unsigned height, int stride) :
        m_buf(0),
        m_rows(0),
        m_width(0),
        m_height(0),
        m_stride(0),
        m_max_height(0)
    {
        attach(buf, width, height, stride);
    }
    void attach(int8u* buf, unsigned width, unsigned height, int stride)
    {
        m_buf = buf;
        m_width = width;
        m_height = height;
        m_stride = stride;
        if(height > m_max_height) {
            FX_Free(m_rows);
            m_rows = FX_Alloc(int8u*, m_max_height = height);
        }
        int8u* row_ptr = m_buf;
        if(stride < 0) {
            row_ptr = m_buf - int(height - 1) * stride;
        }
        int8u** rows = m_rows;
        while(height--) {
            *rows++ = row_ptr;
            row_ptr += stride;
        }
    }
    int8u* buf()
    {
        return m_buf;
    }
    const int8u* buf()    const
    {
        return m_buf;
    }
    unsigned width()  const
    {
        return m_width;
    }
    unsigned height() const
    {
        return m_height;
    }
    int      stride() const
    {
        return m_stride;
    }
    unsigned stride_abs() const
    {
        return (m_stride < 0) ?
               unsigned(-m_stride) :
               unsigned(m_stride);
    }
    int8u* row(unsigned y)
    {
        return m_rows[y];
    }
    const int8u* row(unsigned y) const
    {
        return m_rows[y];
    }
    int8u* next_row(void* p)
    {
        return (int8u*)p + m_stride;
    }
    const int8u* next_row(const void* p) const
    {
        return (int8u*)p + m_stride;
    }
    int8u const* const* rows() const
    {
        return m_rows;
    }
private:
    rendering_buffer(const rendering_buffer&);
    const rendering_buffer& operator = (const rendering_buffer&);
private:
    int8u*       m_buf;
    int8u**      m_rows;
    unsigned m_width;
    unsigned m_height;
    int      m_stride;
    unsigned m_max_height;
};
}
#endif
