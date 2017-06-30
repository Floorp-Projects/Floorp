
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
// vertex_sequence container and vertex_dist struct
//
//----------------------------------------------------------------------------
#ifndef AGG_VERTEX_SEQUENCE_INCLUDED
#define AGG_VERTEX_SEQUENCE_INCLUDED
#include "agg_basics.h"
#include "agg_array.h"
#include "agg_math.h"
namespace agg
{
template<class T, unsigned S = 6>
class vertex_sequence : public pod_deque<T, S>
{
public:
    typedef pod_deque<T, S> base_type;
    void add(const T& val);
    void modify_last(const T& val);
    void close(bool remove_flag);
};
template<class T, unsigned S>
void vertex_sequence<T, S>::add(const T& val)
{
    if(base_type::size() > 1) {
        if(!(*this)[base_type::size() - 2]((*this)[base_type::size() - 1])) {
            base_type::remove_last();
        }
    }
    base_type::add(val);
}
template<class T, unsigned S>
void vertex_sequence<T, S>::modify_last(const T& val)
{
    base_type::remove_last();
    add(val);
}
template<class T, unsigned S>
void vertex_sequence<T, S>::close(bool closed)
{
    while(base_type::size() > 1) {
        if((*this)[base_type::size() - 2]((*this)[base_type::size() - 1])) {
            break;
        }
        T t = (*this)[base_type::size() - 1];
        base_type::remove_last();
        modify_last(t);
    }
    if(closed) {
        while(base_type::size() > 1) {
            if((*this)[base_type::size() - 1]((*this)[0])) {
                break;
            }
            base_type::remove_last();
        }
    }
}
const FX_FLOAT vertex_dist_epsilon = 1e-14f;
struct vertex_dist  {
    FX_FLOAT   x;
    FX_FLOAT   y;
    FX_FLOAT   dist;
    vertex_dist() {}
    vertex_dist(FX_FLOAT x_, FX_FLOAT y_) :
        x(x_),
        y(y_),
        dist(0)
    {
    }
    bool operator () (const vertex_dist& val)
    {
        bool ret = (dist = calc_distance(x, y, val.x, val.y)) > vertex_dist_epsilon;
        return ret;
    }
};
struct vertex_dist_cmd : public vertex_dist {
    unsigned cmd;
    vertex_dist_cmd() {}
    vertex_dist_cmd(FX_FLOAT x_, FX_FLOAT y_, unsigned cmd_) :
        vertex_dist(x_, y_),
        cmd(cmd_)
    {
    }
};
}
#endif
