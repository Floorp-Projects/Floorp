//
// MessagePack for C++ simple buffer implementation
//
// Copyright (C) 2008-2016 FURUHASHI Sadayuki and KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_SBUFFER_HPP
#define MSGPACK_V1_SBUFFER_HPP

#include "msgpack/v1/sbuffer_decl.hpp"

#include <stdexcept>
#include <cstring>

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

class sbuffer {
public:
    sbuffer(size_t initsz = MSGPACK_SBUFFER_INIT_SIZE):m_size(0), m_alloc(initsz)
    {
        if(initsz == 0) {
            m_data = MSGPACK_NULLPTR;
        } else {
            m_data = (char*)::malloc(initsz);
            if(!m_data) {
                throw std::bad_alloc();
            }
        }
    }

    ~sbuffer()
    {
        ::free(m_data);
    }

#if !defined(MSGPACK_USE_CPP03)
    sbuffer(const sbuffer&) = delete;
    sbuffer& operator=(const sbuffer&) = delete;

    sbuffer(sbuffer&& other) :
        m_size(other.m_size), m_data(other.m_data), m_alloc(other.m_alloc)
    {
        other.m_size = other.m_alloc = 0;
        other.m_data = MSGPACK_NULLPTR;
    }

    sbuffer& operator=(sbuffer&& other)
    {
        ::free(m_data);

        m_size = other.m_size;
        m_alloc = other.m_alloc;
        m_data = other.m_data;

        other.m_size = other.m_alloc = 0;
        other.m_data = MSGPACK_NULLPTR;

        return *this;
    }
#endif // !defined(MSGPACK_USE_CPP03)

    void write(const char* buf, size_t len)
    {
        if(m_alloc - m_size < len) {
            expand_buffer(len);
        }
        std::memcpy(m_data + m_size, buf, len);
        m_size += len;
    }

    char* data()
    {
        return m_data;
    }

    const char* data() const
    {
        return m_data;
    }

    size_t size() const
    {
        return m_size;
    }

    char* release()
    {
        char* tmp = m_data;
        m_size = 0;
        m_data = MSGPACK_NULLPTR;
        m_alloc = 0;
        return tmp;
    }

    void clear()
    {
        m_size = 0;
    }

private:
    void expand_buffer(size_t len)
    {
        size_t nsize = (m_alloc > 0) ?
                m_alloc * 2 : MSGPACK_SBUFFER_INIT_SIZE;

        while(nsize < m_size + len) {
            size_t tmp_nsize = nsize * 2;
            if (tmp_nsize <= nsize) {
                nsize = m_size + len;
                break;
            }
            nsize = tmp_nsize;
        }

        void* tmp = ::realloc(m_data, nsize);
        if(!tmp) {
            throw std::bad_alloc();
        }

        m_data = static_cast<char*>(tmp);
        m_alloc = nsize;
    }

#if defined(MSGPACK_USE_CPP03)
private:
    sbuffer(const sbuffer&);
    sbuffer& operator=(const sbuffer&);
#endif  // defined(MSGPACK_USE_CPP03)

private:
    size_t m_size;
    char* m_data;
    size_t m_alloc;
};

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond

}  // namespace msgpack

#endif // MSGPACK_V1_SBUFFER_HPP
