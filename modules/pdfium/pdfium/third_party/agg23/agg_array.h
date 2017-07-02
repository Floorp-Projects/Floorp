
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
#ifndef AGG_ARRAY_INCLUDED
#define AGG_ARRAY_INCLUDED

#include "agg_basics.h"
#include "core/fxcrt/fx_memory.h"  // For FXSYS_* macros.

namespace agg
{
template <class T>
class pod_array {
public:
    typedef T value_type;
    ~pod_array()
    {
        FX_Free(m_array);
    }
    pod_array() : m_size(0), m_capacity(0), m_array(0) {}
    pod_array(unsigned cap, unsigned extra_tail = 0);
    pod_array(const pod_array<T>&);
    const pod_array<T>& operator = (const pod_array<T>&);
    void capacity(unsigned cap, unsigned extra_tail = 0);
    unsigned capacity() const
    {
        return m_capacity;
    }
    void allocate(unsigned size, unsigned extra_tail = 0);
    void resize(unsigned new_size);
    void zero()
    {
        FXSYS_memset(m_array, 0, sizeof(T) * m_size);
    }
    void add(const T& v)
    {
        m_array[m_size++] = v;
    }
    void inc_size(unsigned size)
    {
        m_size += size;
    }
    unsigned size()      const
    {
        return m_size;
    }
    unsigned byte_size() const
    {
        return m_size * sizeof(T);
    }
    const T& operator [] (unsigned i) const
    {
        return m_array[i];
    }
    T& operator [] (unsigned i)
    {
        return m_array[i];
    }
    const T& at(unsigned i) const
    {
        return m_array[i];
    }
    T& at(unsigned i)
    {
        return m_array[i];
    }
    T  value_at(unsigned i) const
    {
        return m_array[i];
    }
    const T* data() const
    {
        return m_array;
    }
    T* data()
    {
        return m_array;
    }
    void remove_all()
    {
        m_size = 0;
    }
    void cut_at(unsigned num)
    {
        if(num < m_size) {
            m_size = num;
        }
    }
private:
    unsigned m_size;
    unsigned m_capacity;
    T*       m_array;
};
template<class T>
void pod_array<T>::capacity(unsigned cap, unsigned extra_tail)
{
    m_size = 0;
    unsigned full_cap = cap + extra_tail;
    if(full_cap < cap) {
        FX_Free(m_array);
        m_array = 0;
        m_capacity = 0;
    } else if(full_cap > m_capacity) {
        FX_Free(m_array);
        m_array = FX_Alloc(T, full_cap);
        m_capacity = full_cap;
    }
}
template<class T>
void pod_array<T>::allocate(unsigned size, unsigned extra_tail)
{
    capacity(size, extra_tail);
    m_size = size;
}
template<class T>
void pod_array<T>::resize(unsigned new_size)
{
    if(new_size > m_size) {
        if(new_size > m_capacity) {
            T* data = FX_Alloc(T, new_size);
            FXSYS_memcpy(data, m_array, m_size * sizeof(T));
            FX_Free(m_array);
            m_array = data;
        }
    } else {
        m_size = new_size;
    }
}
template<class T> pod_array<T>::pod_array(unsigned cap, unsigned extra_tail) :
    m_size(0), m_capacity(cap + extra_tail), m_array(FX_Alloc(T, m_capacity)) {}
template<class T> pod_array<T>::pod_array(const pod_array<T>& v) :
    m_size(v.m_size),
    m_capacity(v.m_capacity),
    m_array(v.m_capacity ? FX_Alloc(T, v.m_capacity) : 0)
{
    FXSYS_memcpy(m_array, v.m_array, sizeof(T) * v.m_size);
}
template<class T> const pod_array<T>&
pod_array<T>::operator = (const pod_array<T>&v)
{
    allocate(v.m_size);
    if(v.m_size) {
        FXSYS_memcpy(m_array, v.m_array, sizeof(T) * v.m_size);
    }
    return *this;
}
template<class T, unsigned S = 6> class pod_deque 
{
public:
    enum block_scale_e {
        block_shift = S,
        block_size  = 1 << block_shift,
        block_mask  = block_size - 1
    };
    typedef T value_type;
    ~pod_deque();
    pod_deque();
    pod_deque(unsigned block_ptr_inc);
    pod_deque(const pod_deque<T, S>& v);
    const pod_deque<T, S>& operator = (const pod_deque<T, S>& v);
    void remove_all()
    {
        m_size = 0;
    }
    void free_all()
    {
        free_tail(0);
    }
    void free_tail(unsigned size);
    void add(const T& val);
    void modify_last(const T& val);
    void remove_last();
    int allocate_continuous_block(unsigned num_elements);
    void add_array(const T* ptr, unsigned num_elem)
    {
        while(num_elem--) {
            add(*ptr++);
        }
    }
    template<class DataAccessor> void add_data(DataAccessor& data)
    {
        while(data.size()) {
            add(*data);
            ++data;
        }
    }
    void cut_at(unsigned size)
    {
        if(size < m_size) {
            m_size = size;
        }
    }
    unsigned size() const
    {
        return m_size;
    }
    const T& operator [] (unsigned i) const
    {
        return m_blocks[i >> block_shift][i & block_mask];
    }
    T& operator [] (unsigned i)
    {
        return m_blocks[i >> block_shift][i & block_mask];
    }
    const T& at(unsigned i) const
    {
        return m_blocks[i >> block_shift][i & block_mask];
    }
    T& at(unsigned i)
    {
        return m_blocks[i >> block_shift][i & block_mask];
    }
    T value_at(unsigned i) const
    {
        return m_blocks[i >> block_shift][i & block_mask];
    }
    const T& curr(unsigned idx) const
    {
        return (*this)[idx];
    }
    T& curr(unsigned idx)
    {
        return (*this)[idx];
    }
    const T& prev(unsigned idx) const
    {
        return (*this)[(idx + m_size - 1) % m_size];
    }
    T& prev(unsigned idx)
    {
        return (*this)[(idx + m_size - 1) % m_size];
    }
    const T& next(unsigned idx) const
    {
        return (*this)[(idx + 1) % m_size];
    }
    T& next(unsigned idx)
    {
        return (*this)[(idx + 1) % m_size];
    }
    const T& last() const
    {
        return (*this)[m_size - 1];
    }
    T& last()
    {
        return (*this)[m_size - 1];
    }
    unsigned byte_size() const;
    const T* block(unsigned nb) const
    {
        return m_blocks[nb];
    }
public:
    void allocate_block(unsigned nb);
    T*   data_ptr();
    unsigned        m_size;
    unsigned        m_num_blocks;
    unsigned        m_max_blocks;
    T**             m_blocks;
    unsigned        m_block_ptr_inc;
};
template<class T, unsigned S> pod_deque<T, S>::~pod_deque()
{
    if(m_num_blocks) {
        T** blk = m_blocks + m_num_blocks - 1;
        while(m_num_blocks--) {
            FX_Free(*blk);
            --blk;
        }
        FX_Free(m_blocks);
    }
}
template<class T, unsigned S>
void pod_deque<T, S>::free_tail(unsigned size)
{
    if(size < m_size) {
        unsigned nb = (size + block_mask) >> block_shift;
        while(m_num_blocks > nb) {
            FX_Free(m_blocks[--m_num_blocks]);
        }
        m_size = size;
    }
}
template<class T, unsigned S> pod_deque<T, S>::pod_deque() :
    m_size(0),
    m_num_blocks(0),
    m_max_blocks(0),
    m_blocks(0),
    m_block_ptr_inc(block_size)
{
}
template<class T, unsigned S>
pod_deque<T, S>::pod_deque(unsigned block_ptr_inc) :
    m_size(0),
    m_num_blocks(0),
    m_max_blocks(0),
    m_blocks(0),
    m_block_ptr_inc(block_ptr_inc)
{
}
template<class T, unsigned S>
pod_deque<T, S>::pod_deque(const pod_deque<T, S>& v) :
    m_size(v.m_size),
    m_num_blocks(v.m_num_blocks),
    m_max_blocks(v.m_max_blocks),
    m_blocks(v.m_max_blocks ? FX_Alloc(T*, v.m_max_blocks) : 0),
    m_block_ptr_inc(v.m_block_ptr_inc)
{
    unsigned i;
    for(i = 0; i < v.m_num_blocks; ++i) {
        m_blocks[i] = FX_Alloc(T, block_size);
        FXSYS_memcpy(m_blocks[i], v.m_blocks[i], block_size * sizeof(T));
    }
}
template<class T, unsigned S>
const pod_deque<T, S>& pod_deque<T, S>::operator = (const pod_deque<T, S>& v)
{
    unsigned i;
    for(i = m_num_blocks; i < v.m_num_blocks; ++i) {
        allocate_block(i);
    }
    for(i = 0; i < v.m_num_blocks; ++i) {
        FXSYS_memcpy(m_blocks[i], v.m_blocks[i], block_size * sizeof(T));
    }
    m_size = v.m_size;
    return *this;
}
template<class T, unsigned S>
void pod_deque<T, S>::allocate_block(unsigned nb)
{
    if(nb >= m_max_blocks) {
        T** new_blocks = FX_Alloc(T*, m_max_blocks + m_block_ptr_inc);
        if(m_blocks) {
            FXSYS_memcpy(new_blocks,
                         m_blocks,
                         m_num_blocks * sizeof(T*));
            FX_Free(m_blocks);
        }
        m_blocks = new_blocks;
        m_max_blocks += m_block_ptr_inc;
    }
    m_blocks[nb] = FX_Alloc(T, block_size);
    m_num_blocks++;
}
template<class T, unsigned S>
inline T* pod_deque<T, S>::data_ptr()
{
    unsigned nb = m_size >> block_shift;
    if(nb >= m_num_blocks) {
        allocate_block(nb);
    }
    return m_blocks[nb] + (m_size & block_mask);
}
template<class T, unsigned S>
inline void pod_deque<T, S>::add(const T& val)
{
    *data_ptr() = val;
    ++m_size;
}
template<class T, unsigned S>
inline void pod_deque<T, S>::remove_last()
{
    if(m_size) {
        --m_size;
    }
}
template<class T, unsigned S>
void pod_deque<T, S>::modify_last(const T& val)
{
    remove_last();
    add(val);
}
template<class T, unsigned S>
int pod_deque<T, S>::allocate_continuous_block(unsigned num_elements)
{
    if(num_elements < block_size) {
        data_ptr();
        unsigned rest = block_size - (m_size & block_mask);
        unsigned index;
        if(num_elements <= rest) {
            index = m_size;
            m_size += num_elements;
            return index;
        }
        m_size += rest;
        data_ptr();
        index = m_size;
        m_size += num_elements;
        return index;
    }
    return -1;
}
template<class T, unsigned S>
unsigned pod_deque<T, S>::byte_size() const
{
    return m_size * sizeof(T);
}
class pod_allocator 
{
public:
    void remove_all()
    {
        if(m_num_blocks) {
            int8u** blk = m_blocks + m_num_blocks - 1;
            while(m_num_blocks--) {
                FX_Free(*blk);
                --blk;
            }
            FX_Free(m_blocks);
        }
        m_num_blocks = 0;
        m_max_blocks = 0;
        m_blocks = 0;
        m_buf_ptr = 0;
        m_rest = 0;
    }
    ~pod_allocator()
    {
        remove_all();
    }
    pod_allocator(unsigned block_size, unsigned block_ptr_inc = 256 - 8) :
        m_block_size(block_size),
        m_block_ptr_inc(block_ptr_inc),
        m_num_blocks(0),
        m_max_blocks(0),
        m_blocks(0),
        m_buf_ptr(0),
        m_rest(0)
    {
    }
    int8u* allocate(unsigned size, unsigned alignment = 1)
    {
        if(size == 0) {
            return 0;
        }
        if(size <= m_rest) {
            int8u* ptr = m_buf_ptr;
            if(alignment > 1) {
                unsigned align = (alignment - unsigned((size_t)ptr) % alignment) % alignment;
                size += align;
                ptr += align;
                if(size <= m_rest) {
                    m_rest -= size;
                    m_buf_ptr += size;
                    return ptr;
                }
                allocate_block(size);
                return allocate(size - align, alignment);
            }
            m_rest -= size;
            m_buf_ptr += size;
            return ptr;
        }
        allocate_block(size + alignment - 1);
        return allocate(size, alignment);
    }
private:
    void allocate_block(unsigned size)
    {
        if(size < m_block_size) {
            size = m_block_size;
        }
        if(m_num_blocks >= m_max_blocks) {
            int8u** new_blocks = FX_Alloc(int8u*, m_max_blocks + m_block_ptr_inc);
            if(m_blocks) {
                FXSYS_memcpy(new_blocks,
                             m_blocks,
                             m_num_blocks * sizeof(int8u*));
                FX_Free(m_blocks);
            }
            m_blocks = new_blocks;
            m_max_blocks += m_block_ptr_inc;
        }
        m_blocks[m_num_blocks] = m_buf_ptr = FX_Alloc(int8u, size);
        m_num_blocks++;
        m_rest = size;
    }
    unsigned m_block_size;
    unsigned m_block_ptr_inc;
    unsigned m_num_blocks;
    unsigned m_max_blocks;
    int8u**  m_blocks;
    int8u*   m_buf_ptr;
    unsigned m_rest;
};
enum quick_sort_threshold_e {
    quick_sort_threshold = 9
};
template<class T> inline void swap_elements(T& a, T& b)
{
    T temp = a;
    a = b;
    b = temp;
}
}
#endif
