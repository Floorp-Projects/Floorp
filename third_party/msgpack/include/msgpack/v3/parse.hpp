//
// MessagePack for C++ deserializing routine
//
// Copyright (C) 2018 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V3_PARSE_HPP
#define MSGPACK_V3_PARSE_HPP

#if MSGPACK_DEFAULT_API_VERSION >= 2

#include <cstddef>

#include "msgpack/parse_return.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v3) {
/// @endcond

namespace detail {

template <typename VisitorHolder>
class context {
public:
    context()
        :m_trail(0), m_cs(MSGPACK_CS_HEADER)
    {
    }

    void init()
    {
        m_cs = MSGPACK_CS_HEADER;
        m_trail = 0;
        m_stack.clear();
        holder().visitor().init();
    }

    parse_return execute(const char* data, std::size_t len, std::size_t& off);

private:
    template <typename T>
    static uint32_t next_cs(T p)
    {
        return static_cast<uint32_t>(*p) & 0x1f;
    }

    VisitorHolder& holder() {
        return static_cast<VisitorHolder&>(*this);
    }

    template <typename T, typename StartVisitor, typename EndVisitor>
    parse_return start_aggregate(
        StartVisitor const& sv,
        EndVisitor const& ev,
        const char* load_pos,
        std::size_t& off) {
        typename value<T>::type size;
        load<T>(size, load_pos);
        if (size == 0) {
            if (!sv(size)) {
                off = m_current - m_start;
                return PARSE_STOP_VISITOR;
            }
            if (!ev()) {
                off = m_current - m_start;
                return PARSE_STOP_VISITOR;
            }
            parse_return ret = m_stack.consume(holder(), m_current);
            ++m_current;
            if (ret != PARSE_CONTINUE) {
                off = m_current - m_start;
                return ret;
            }
        }
        else {
            if (!sv(size)) {
                off = m_current - m_start;
                return PARSE_STOP_VISITOR;
            }
            parse_return ret = m_stack.push(holder(), sv.type(), static_cast<uint32_t>(size));
            ++m_current;
            if (ret != PARSE_CONTINUE) {
                off = m_current - m_start;
                return ret;
            }
        }
        m_cs = MSGPACK_CS_HEADER;
        return PARSE_CONTINUE;
    }

    parse_return after_visit_proc(bool visit_result, std::size_t& off) {
        if (!visit_result) {
            off = m_current - m_start;
            return PARSE_STOP_VISITOR;
        }
        parse_return ret = m_stack.consume(holder(), m_current);
        ++m_current;
        if (ret != PARSE_CONTINUE) {
            off = m_current - m_start;
        }
        m_cs = MSGPACK_CS_HEADER;
        return ret;
    }

    struct array_sv {
        array_sv(VisitorHolder& visitor_holder):m_visitor_holder(visitor_holder) {}
        bool operator()(uint32_t size) const {
            return m_visitor_holder.visitor().start_array(size);
        }
        msgpack_container_type type() const { return MSGPACK_CT_ARRAY_ITEM; }
    private:
        VisitorHolder& m_visitor_holder;
    };
    struct array_ev {
        array_ev(VisitorHolder& visitor_holder):m_visitor_holder(visitor_holder) {}
        bool operator()() const {
            return m_visitor_holder.visitor().end_array();
        }
    private:
        VisitorHolder& m_visitor_holder;
    };
    struct map_sv {
        map_sv(VisitorHolder& visitor_holder):m_visitor_holder(visitor_holder) {}
        bool operator()(uint32_t size) const {
            return m_visitor_holder.visitor().start_map(size);
        }
        msgpack_container_type type() const { return MSGPACK_CT_MAP_KEY; }
    private:
        VisitorHolder& m_visitor_holder;
    };
    struct map_ev {
        map_ev(VisitorHolder& visitor_holder):m_visitor_holder(visitor_holder) {}
        bool operator()() const {
            return m_visitor_holder.visitor().end_map();
        }
    private:
        VisitorHolder& m_visitor_holder;
    };

    struct unpack_stack {
        struct stack_elem {
            stack_elem(msgpack_container_type type, uint32_t rest):m_type(type), m_rest(rest) {}
            msgpack_container_type m_type;
            uint32_t m_rest;
        };
        unpack_stack() {
            m_stack.reserve(MSGPACK_EMBED_STACK_SIZE);
        }
        parse_return push(VisitorHolder& visitor_holder, msgpack_container_type type, uint32_t rest) {
            m_stack.push_back(stack_elem(type, rest));
            switch (type) {
            case MSGPACK_CT_ARRAY_ITEM:
                return visitor_holder.visitor().start_array_item() ? PARSE_CONTINUE : PARSE_STOP_VISITOR;
            case MSGPACK_CT_MAP_KEY:
                return visitor_holder.visitor().start_map_key() ? PARSE_CONTINUE : PARSE_STOP_VISITOR;
            case MSGPACK_CT_MAP_VALUE:
                assert(0);
                return PARSE_STOP_VISITOR;
            }
            assert(0);
            return PARSE_STOP_VISITOR;
        }
        parse_return consume(VisitorHolder& visitor_holder, char const*& current) {
            while (!m_stack.empty()) {
                stack_elem& e = m_stack.back();
                switch (e.m_type) {
                case MSGPACK_CT_ARRAY_ITEM:
                    if (!visitor_holder.visitor().end_array_item()) {
                        --current;
                        return PARSE_STOP_VISITOR;
                    }
                    if (--e.m_rest == 0)  {
                        m_stack.pop_back();
                        if (!visitor_holder.visitor().end_array()) {
                            --current;
                            return PARSE_STOP_VISITOR;
                        }
                    }
                    else {
                        if (!visitor_holder.visitor().start_array_item()) return PARSE_STOP_VISITOR;
                        return PARSE_CONTINUE;
                    }
                    break;
                case MSGPACK_CT_MAP_KEY:
                    if (!visitor_holder.visitor().end_map_key()) {
                        --current;
                        return PARSE_STOP_VISITOR;
                    }
                    if (!visitor_holder.visitor().start_map_value()) return PARSE_STOP_VISITOR;
                    e.m_type = MSGPACK_CT_MAP_VALUE;
                    return PARSE_CONTINUE;
                case MSGPACK_CT_MAP_VALUE:
                    if (!visitor_holder.visitor().end_map_value()) {
                        --current;
                        return PARSE_STOP_VISITOR;
                    }
                    if (--e.m_rest == 0) {
                        m_stack.pop_back();
                        if (!visitor_holder.visitor().end_map()) {
                            --current;
                            return PARSE_STOP_VISITOR;
                        }
                    }
                    else {
                        e.m_type = MSGPACK_CT_MAP_KEY;
                        if (!visitor_holder.visitor().start_map_key()) return PARSE_STOP_VISITOR;
                        return PARSE_CONTINUE;
                    }
                    break;
                }
            }
            return PARSE_SUCCESS;
        }
        bool empty() const { return m_stack.empty(); }
        void clear() { m_stack.clear(); }
    private:
        std::vector<stack_elem> m_stack;
    };

    char const* m_start;
    char const* m_current;

    std::size_t m_trail;
    uint32_t m_cs;
    uint32_t m_num_elements;
    unpack_stack m_stack;
};

template <std::size_t N>
inline void check_ext_size(std::size_t /*size*/) {
}

template <>
inline void check_ext_size<4>(std::size_t size) {
    if (size == 0xffffffff) throw msgpack::ext_size_overflow("ext size overflow");
}

template <typename VisitorHolder>
inline parse_return context<VisitorHolder>::execute(const char* data, std::size_t len, std::size_t& off)
{
    assert(len >= off);

    m_start = data;
    m_current = data + off;
    const char* const pe = data + len;
    const char* n = MSGPACK_NULLPTR;

    msgpack::object obj;

    if(m_current == pe) {
        off = m_current - m_start;
        return PARSE_CONTINUE;
    }
    bool fixed_trail_again = false;
    do {
        if (m_cs == MSGPACK_CS_HEADER) {
            fixed_trail_again = false;
            int selector = *reinterpret_cast<const unsigned char*>(m_current);
            if (0x00 <= selector && selector <= 0x7f) { // Positive Fixnum
                uint8_t tmp = *reinterpret_cast<const uint8_t*>(m_current);
                bool visret = holder().visitor().visit_positive_integer(tmp);
                parse_return upr = after_visit_proc(visret, off);
                if (upr != PARSE_CONTINUE) return upr;
            } else if(0xe0 <= selector && selector <= 0xff) { // Negative Fixnum
                int8_t tmp = *reinterpret_cast<const int8_t*>(m_current);
                bool visret = holder().visitor().visit_negative_integer(tmp);
                parse_return upr = after_visit_proc(visret, off);
                if (upr != PARSE_CONTINUE) return upr;
            } else if (0xc4 <= selector && selector <= 0xdf) {
                const uint32_t trail[] = {
                    1, // bin     8  0xc4
                    2, // bin    16  0xc5
                    4, // bin    32  0xc6
                    1, // ext     8  0xc7
                    2, // ext    16  0xc8
                    4, // ext    32  0xc9
                    4, // float  32  0xca
                    8, // float  64  0xcb
                    1, // uint    8  0xcc
                    2, // uint   16  0xcd
                    4, // uint   32  0xce
                    8, // uint   64  0xcf
                    1, // int     8  0xd0
                    2, // int    16  0xd1
                    4, // int    32  0xd2
                    8, // int    64  0xd3
                    2, // fixext  1  0xd4
                    3, // fixext  2  0xd5
                    5, // fixext  4  0xd6
                    9, // fixext  8  0xd7
                    17,// fixext 16  0xd8
                    1, // str     8  0xd9
                    2, // str    16  0xda
                    4, // str    32  0xdb
                    2, // array  16  0xdc
                    4, // array  32  0xdd
                    2, // map    16  0xde
                    4, // map    32  0xdf
                };
                m_trail = trail[selector - 0xc4];
                m_cs = next_cs(m_current);
                fixed_trail_again = true;
            } else if(0xa0 <= selector && selector <= 0xbf) { // FixStr
                m_trail = static_cast<uint32_t>(*m_current) & 0x1f;
                if(m_trail == 0) {
                    bool visret = holder().visitor().visit_str(n, static_cast<uint32_t>(m_trail));
                    parse_return upr = after_visit_proc(visret, off);
                    if (upr != PARSE_CONTINUE) return upr;
                }
                else {
                    m_cs = MSGPACK_ACS_STR_VALUE;
                    fixed_trail_again = true;
                }
            } else if(0x90 <= selector && selector <= 0x9f) { // FixArray
                parse_return ret = start_aggregate<fix_tag>(array_sv(holder()), array_ev(holder()), m_current, off);
                if (ret != PARSE_CONTINUE) return ret;
            } else if(0x80 <= selector && selector <= 0x8f) { // FixMap
                parse_return ret = start_aggregate<fix_tag>(map_sv(holder()), map_ev(holder()), m_current, off);
                if (ret != PARSE_CONTINUE) return ret;
            } else if(selector == 0xc2) { // false
                bool visret = holder().visitor().visit_boolean(false);
                parse_return upr = after_visit_proc(visret, off);
                if (upr != PARSE_CONTINUE) return upr;
            } else if(selector == 0xc3) { // true
                bool visret = holder().visitor().visit_boolean(true);
                parse_return upr = after_visit_proc(visret, off);
                if (upr != PARSE_CONTINUE) return upr;
            } else if(selector == 0xc0) { // nil
                bool visret = holder().visitor().visit_nil();
                parse_return upr = after_visit_proc(visret, off);
                if (upr != PARSE_CONTINUE) return upr;
            } else {
                off = m_current - m_start;
                holder().visitor().parse_error(off - 1, off);
                return PARSE_PARSE_ERROR;
            }
            // end MSGPACK_CS_HEADER
        }
        if (m_cs != MSGPACK_CS_HEADER || fixed_trail_again) {
            if (fixed_trail_again) {
                ++m_current;
                fixed_trail_again = false;
            }
            if(static_cast<std::size_t>(pe - m_current) < m_trail) {
                off = m_current - m_start;
                return PARSE_CONTINUE;
            }
            n = m_current;
            m_current += m_trail - 1;
            switch(m_cs) {
                //case MSGPACK_CS_
                //case MSGPACK_CS_
            case MSGPACK_CS_FLOAT: {
                union { uint32_t i; float f; } mem;
                load<uint32_t>(mem.i, n);
                bool visret = holder().visitor().visit_float32(mem.f);
                parse_return upr = after_visit_proc(visret, off);
                if (upr != PARSE_CONTINUE) return upr;
            } break;
            case MSGPACK_CS_DOUBLE: {
                union { uint64_t i; double f; } mem;
                load<uint64_t>(mem.i, n);
#if defined(TARGET_OS_IPHONE)
                // ok
#elif defined(__arm__) && !(__ARM_EABI__) // arm-oabi
                // https://github.com/msgpack/msgpack-perl/pull/1
                mem.i = (mem.i & 0xFFFFFFFFUL) << 32UL | (mem.i >> 32UL);
#endif
                bool visret = holder().visitor().visit_float64(mem.f);
                parse_return upr = after_visit_proc(visret, off);
                if (upr != PARSE_CONTINUE) return upr;
            } break;
            case MSGPACK_CS_UINT_8: {
                uint8_t tmp;
                load<uint8_t>(tmp, n);
                bool visret = holder().visitor().visit_positive_integer(tmp);
                parse_return upr = after_visit_proc(visret, off);
                if (upr != PARSE_CONTINUE) return upr;
            } break;
            case MSGPACK_CS_UINT_16: {
                uint16_t tmp;
                load<uint16_t>(tmp, n);
                bool visret = holder().visitor().visit_positive_integer(tmp);
                parse_return upr = after_visit_proc(visret, off);
                if (upr != PARSE_CONTINUE) return upr;
            } break;
            case MSGPACK_CS_UINT_32: {
                uint32_t tmp;
                load<uint32_t>(tmp, n);
                bool visret = holder().visitor().visit_positive_integer(tmp);
                parse_return upr = after_visit_proc(visret, off);
                if (upr != PARSE_CONTINUE) return upr;
            } break;
            case MSGPACK_CS_UINT_64: {
                uint64_t tmp;
                load<uint64_t>(tmp, n);
                bool visret = holder().visitor().visit_positive_integer(tmp);
                parse_return upr = after_visit_proc(visret, off);
                if (upr != PARSE_CONTINUE) return upr;
            } break;
            case MSGPACK_CS_INT_8: {
                int8_t tmp;
                load<int8_t>(tmp, n);
                bool visret = holder().visitor().visit_negative_integer(tmp);
                parse_return upr = after_visit_proc(visret, off);
                if (upr != PARSE_CONTINUE) return upr;
            } break;
            case MSGPACK_CS_INT_16: {
                int16_t tmp;
                load<int16_t>(tmp, n);
                bool visret = holder().visitor().visit_negative_integer(tmp);
                parse_return upr = after_visit_proc(visret, off);
                if (upr != PARSE_CONTINUE) return upr;
            } break;
            case MSGPACK_CS_INT_32: {
                int32_t tmp;
                load<int32_t>(tmp, n);
                bool visret = holder().visitor().visit_negative_integer(tmp);
                parse_return upr = after_visit_proc(visret, off);
                if (upr != PARSE_CONTINUE) return upr;
            } break;
            case MSGPACK_CS_INT_64: {
                int64_t tmp;
                load<int64_t>(tmp, n);
                bool visret = holder().visitor().visit_negative_integer(tmp);
                parse_return upr = after_visit_proc(visret, off);
                if (upr != PARSE_CONTINUE) return upr;
            } break;
            case MSGPACK_CS_FIXEXT_1: {
                bool visret = holder().visitor().visit_ext(n, 1+1);
                parse_return upr = after_visit_proc(visret, off);
                if (upr != PARSE_CONTINUE) return upr;
            } break;
            case MSGPACK_CS_FIXEXT_2: {
                bool visret = holder().visitor().visit_ext(n, 2+1);
                parse_return upr = after_visit_proc(visret, off);
                if (upr != PARSE_CONTINUE) return upr;
            } break;
            case MSGPACK_CS_FIXEXT_4: {
                bool visret = holder().visitor().visit_ext(n, 4+1);
                parse_return upr = after_visit_proc(visret, off);
                if (upr != PARSE_CONTINUE) return upr;
            } break;
            case MSGPACK_CS_FIXEXT_8: {
                bool visret = holder().visitor().visit_ext(n, 8+1);
                parse_return upr = after_visit_proc(visret, off);
                if (upr != PARSE_CONTINUE) return upr;
            } break;
            case MSGPACK_CS_FIXEXT_16: {
                bool visret = holder().visitor().visit_ext(n, 16+1);
                parse_return upr = after_visit_proc(visret, off);
                if (upr != PARSE_CONTINUE) return upr;
            } break;
            case MSGPACK_CS_STR_8: {
                uint8_t tmp;
                load<uint8_t>(tmp, n);
                m_trail = tmp;
                if(m_trail == 0) {
                    bool visret = holder().visitor().visit_str(n, static_cast<uint32_t>(m_trail));
                    parse_return upr = after_visit_proc(visret, off);
                    if (upr != PARSE_CONTINUE) return upr;
                }
                else {
                    m_cs = MSGPACK_ACS_STR_VALUE;
                    fixed_trail_again = true;
                }
            } break;
            case MSGPACK_CS_BIN_8: {
                uint8_t tmp;
                load<uint8_t>(tmp, n);
                m_trail = tmp;
                if(m_trail == 0) {
                    bool visret = holder().visitor().visit_bin(n, static_cast<uint32_t>(m_trail));
                    parse_return upr = after_visit_proc(visret, off);
                    if (upr != PARSE_CONTINUE) return upr;
                }
                else {
                    m_cs = MSGPACK_ACS_BIN_VALUE;
                    fixed_trail_again = true;
                }
            } break;
            case MSGPACK_CS_EXT_8: {
                uint8_t tmp;
                load<uint8_t>(tmp, n);
                m_trail = tmp + 1;
                if(m_trail == 0) {
                    bool visret = holder().visitor().visit_ext(n, static_cast<uint32_t>(m_trail));
                    parse_return upr = after_visit_proc(visret, off);
                    if (upr != PARSE_CONTINUE) return upr;
                }
                else {
                    m_cs = MSGPACK_ACS_EXT_VALUE;
                    fixed_trail_again = true;
                }
            } break;
            case MSGPACK_CS_STR_16: {
                uint16_t tmp;
                load<uint16_t>(tmp, n);
                m_trail = tmp;
                if(m_trail == 0) {
                    bool visret = holder().visitor().visit_str(n, static_cast<uint32_t>(m_trail));
                    parse_return upr = after_visit_proc(visret, off);
                    if (upr != PARSE_CONTINUE) return upr;
                }
                else {
                    m_cs = MSGPACK_ACS_STR_VALUE;
                    fixed_trail_again = true;
                }
            } break;
            case MSGPACK_CS_BIN_16: {
                uint16_t tmp;
                load<uint16_t>(tmp, n);
                m_trail = tmp;
                if(m_trail == 0) {
                    bool visret = holder().visitor().visit_bin(n, static_cast<uint32_t>(m_trail));
                    parse_return upr = after_visit_proc(visret, off);
                    if (upr != PARSE_CONTINUE) return upr;
                }
                else {
                    m_cs = MSGPACK_ACS_BIN_VALUE;
                    fixed_trail_again = true;
                }
            } break;
            case MSGPACK_CS_EXT_16: {
                uint16_t tmp;
                load<uint16_t>(tmp, n);
                m_trail = tmp + 1;
                if(m_trail == 0) {
                    bool visret = holder().visitor().visit_ext(n, static_cast<uint32_t>(m_trail));
                    parse_return upr = after_visit_proc(visret, off);
                    if (upr != PARSE_CONTINUE) return upr;
                }
                else {
                    m_cs = MSGPACK_ACS_EXT_VALUE;
                    fixed_trail_again = true;
                }
            } break;
            case MSGPACK_CS_STR_32: {
                uint32_t tmp;
                load<uint32_t>(tmp, n);
                m_trail = tmp;
                if(m_trail == 0) {
                    bool visret = holder().visitor().visit_str(n, static_cast<uint32_t>(m_trail));
                    parse_return upr = after_visit_proc(visret, off);
                    if (upr != PARSE_CONTINUE) return upr;
                }
                else {
                    m_cs = MSGPACK_ACS_STR_VALUE;
                    fixed_trail_again = true;
                }
            } break;
            case MSGPACK_CS_BIN_32: {
                uint32_t tmp;
                load<uint32_t>(tmp, n);
                m_trail = tmp;
                if(m_trail == 0) {
                    bool visret = holder().visitor().visit_bin(n, static_cast<uint32_t>(m_trail));
                    parse_return upr = after_visit_proc(visret, off);
                    if (upr != PARSE_CONTINUE) return upr;
                }
                else {
                    m_cs = MSGPACK_ACS_BIN_VALUE;
                    fixed_trail_again = true;
                }
            } break;
            case MSGPACK_CS_EXT_32: {
                uint32_t tmp;
                load<uint32_t>(tmp, n);
                check_ext_size<sizeof(std::size_t)>(tmp);
                m_trail = tmp;
                ++m_trail;
                if(m_trail == 0) {
                    bool visret = holder().visitor().visit_ext(n, static_cast<uint32_t>(m_trail));
                    parse_return upr = after_visit_proc(visret, off);
                    if (upr != PARSE_CONTINUE) return upr;
                }
                else {
                    m_cs = MSGPACK_ACS_EXT_VALUE;
                    fixed_trail_again = true;
                }
            } break;
            case MSGPACK_ACS_STR_VALUE: {
                bool visret = holder().visitor().visit_str(n, static_cast<uint32_t>(m_trail));
                parse_return upr = after_visit_proc(visret, off);
                if (upr != PARSE_CONTINUE) return upr;
            } break;
            case MSGPACK_ACS_BIN_VALUE: {
                bool visret = holder().visitor().visit_bin(n, static_cast<uint32_t>(m_trail));
                parse_return upr = after_visit_proc(visret, off);
                if (upr != PARSE_CONTINUE) return upr;
            } break;
            case MSGPACK_ACS_EXT_VALUE: {
                bool visret = holder().visitor().visit_ext(n, static_cast<uint32_t>(m_trail));
                parse_return upr = after_visit_proc(visret, off);
                if (upr != PARSE_CONTINUE) return upr;
            } break;
            case MSGPACK_CS_ARRAY_16: {
                parse_return ret = start_aggregate<uint16_t>(array_sv(holder()), array_ev(holder()), n, off);
                if (ret != PARSE_CONTINUE) return ret;

            } break;
            case MSGPACK_CS_ARRAY_32: {
                parse_return ret = start_aggregate<uint32_t>(array_sv(holder()), array_ev(holder()), n, off);
                if (ret != PARSE_CONTINUE) return ret;
            } break;
            case MSGPACK_CS_MAP_16: {
                parse_return ret = start_aggregate<uint16_t>(map_sv(holder()), map_ev(holder()), n, off);
                if (ret != PARSE_CONTINUE) return ret;
            } break;
            case MSGPACK_CS_MAP_32: {
                parse_return ret = start_aggregate<uint32_t>(map_sv(holder()), map_ev(holder()), n, off);
                if (ret != PARSE_CONTINUE) return ret;
            } break;
            default:
                off = m_current - m_start;
                holder().visitor().parse_error(n - m_start - 1, n - m_start);
                return PARSE_PARSE_ERROR;
            }
        }
    } while(m_current != pe);

    off = m_current - m_start;
    return PARSE_CONTINUE;
}

template <typename Visitor>
struct parse_helper : detail::context<parse_helper<Visitor> > {
    parse_helper(Visitor& v):m_visitor(v) {}
    parse_return execute(const char* data, std::size_t len, std::size_t& off) {
        return detail::context<parse_helper<Visitor> >::execute(data, len, off);
    }
    Visitor& visitor() const { return m_visitor; }
    Visitor& m_visitor;
};

template <typename Visitor>
inline parse_return
parse_imp(const char* data, size_t len, size_t& off, Visitor& v) {
    std::size_t noff = off;
    if(len <= noff) {
        // FIXME
        v.insufficient_bytes(noff, noff);
        return PARSE_CONTINUE;
    }
    detail::parse_helper<Visitor> h(v);
    parse_return ret = h.execute(data, len, noff);
    off = noff;
    switch (ret) {
    case PARSE_CONTINUE:
        v.insufficient_bytes(noff - 1, noff);
        return ret;
    case PARSE_SUCCESS:
        if(noff < len) {
            return PARSE_EXTRA_BYTES;
        }
        return ret;
    default:
        return ret;
    }
}

} // detail

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v3)
/// @endcond

}  // namespace msgpack

#endif // MSGPACK_DEFAULT_API_VERSION >= 2

#endif // MSGPACK_V3_PARSE_HPP
