//
// MessagePack for C++ serializing routine
//
// Copyright (C) 2008-2016 FURUHASHI Sadayuki and KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_PACK_HPP
#define MSGPACK_V1_PACK_HPP

#include "msgpack/v1/pack_decl.hpp"

#include <stdexcept>
#include <limits>
#include <cstring>
#include <climits>

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

/// The class template that supports continuous packing.
/**
 * @tparam Stream  Any type that have a member function `Stream write(const char*, size_t s)`
 *
 */
template <typename Stream>
class packer {
public:
    /// Constructor
    /**
     * This constructor is left for compatibility.
     * Use `packer(Stream& s)` instead of the constructor.
     *
     * @param s A pointer to packing destination stream object.
     */
    packer(Stream* s);
    /// Constructor
    /**
     * @param s Packing destination stream object.
     */
    packer(Stream& s);

public:
    /// Packing function template
    /**
     * @tparam T The type of packing object.
     *
     * @param v a packing object.
     *
     * @return The reference of `*this`.
     */
    template <typename T>
    packer<Stream>& pack(const T& v);

    /// Packing uint8
    /**
     * The byte size of the packed data depends on `d`.
     * The packed type is positive fixnum or uint8.
     * The minimum byte size expression is used.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_uint8(uint8_t d);

    /// Packing uint16
    /**
     * The byte size of the packed data depends on `d`.
     * The packed type is positive fixnum, uint8 or uint16.
     * The minimum byte size expression is used.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_uint16(uint16_t d);

    /// Packing uint32
    /**
     * The byte size of the packed data depends on `d`.
     * The packed type is positive fixnum, uint8, uint16 or uint32.
     * The minimum byte size expression is used.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_uint32(uint32_t d);

    /// Packing uint16
    /**
     * The byte size of the packed data depends on `d`.
     * The packed type is positive fixnum, uint8, uint16, uint32 or uint64.
     * The minimum byte size expression is used.
     * positive fixnum, uint8, uint16, or uint32 is used.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_uint64(uint64_t d);

    /// Packing int8
    /**
     * The byte size of the packed data depends on `d`.
     * If `d` is zero or positive, the packed type is positive fixnum, or uint8,
     * else the packed type is negative fixnum, or int8
     * The minimum byte size expression is used.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_int8(int8_t d);

    /// Packing int16
    /**
     * The byte size of the packed data depends on `d`.
     * If `d` is zero or positive, the packed type is positive fixnum, uint8, or uint16,
     * else the packed type is negative fixnum, int8, or int16.
     * The minimum byte size expression is used.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_int16(int16_t d);

    /// Packing int32
    /**
     * The byte size of the packed data depends on `d`.
     * If `d` is zero or positive, the packed type is positive fixnum, uint8, uint16, or uint32,
     * else the packed type is negative fixnum, int8, int16, or int32.
     * The minimum byte size expression is used.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_int32(int32_t d);

    /// Packing int32
    /**
     * The byte size of the packed data depends on `d`.
     * If `d` is zero or positive, the packed type is positive fixnum, uint8, uint16, uint32, or uint64,
     * else the packed type is negative fixnum, int8, int16, int32, or int64.
     * The minimum byte size expression is used.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_int64(int64_t d);



    /// Packing uint8 (fixed packed type).
    /**
     * The packed type is always uint8.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_fix_uint8(uint8_t d);

    /// Packing uint8 (fixed packed type).
    /**
     * The packed type is always uint16.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_fix_uint16(uint16_t d);

    /// Packing uint8 (fixed packed type).
    /**
     * The packed type is always uint32.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_fix_uint32(uint32_t d);

    /// Packing uint8 (fixed packed type).
    /**
     * The packed type is always uint64.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_fix_uint64(uint64_t d);

    /// Packing uint8 (fixed packed type).
    /**
     * The packed type is always int8.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_fix_int8(int8_t d);

    /// Packing uint8 (fixed packed type).
    /**
     * The packed type is always int16.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_fix_int16(int16_t d);

    /// Packing uint8 (fixed packed type).
    /**
     * The packed type is always int32.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_fix_int32(int32_t d);

    /// Packing uint8 (fixed packed type).
    /**
     * The packed type is always int64.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_fix_int64(int64_t d);


    /// Packing char
    /**
     * The byte size of the packed data depends on `d`.
     * If `d` is zero or positive, the packed type is positive fixnum, or uint*,
     * else the packed type is negative fixnum, or int*
     * The minimum byte size expression is used.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_char(char d);

    /// Packing signed char
    /**
     * The byte size of the packed data depends on `d`.
     * If `d` is zero or positive, the packed type is positive fixnum, or uint*,
     * else the packed type is negative fixnum, or int*
     * The minimum byte size expression is used.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_signed_char(signed char d);

    /// Packing short
    /**
     * The byte size of the packed data depends on `d`.
     * If `d` is zero or positive, the packed type is positive fixnum, or uint*,
     * else the packed type is negative fixnum, or int*
     * The minimum byte size expression is used.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_short(short d);

    /// Packing int
    /**
     * The byte size of the packed data depends on `d`.
     * If `d` is zero or positive, the packed type is positive fixnum, or uint*,
     * else the packed type is negative fixnum, or int*
     * The minimum byte size expression is used.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_int(int d);

    /// Packing long
    /**
     * The byte size of the packed data depends on `d`.
     * If `d` is zero or positive, the packed type is positive fixnum, or uint*,
     * else the packed type is negative fixnum, or int*
     * The minimum byte size expression is used.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_long(long d);

    /// Packing long long
    /**
     * The byte size of the packed data depends on `d`.
     * If `d` is zero or positive, the packed type is positive fixnum, or uint*,
     * else the packed type is negative fixnum, or int*
     * The minimum byte size expression is used.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_long_long(long long d);


    /// Packing unsigned char
    /**
     * The byte size of the packed data depends on `d`.
     * The packed type is positive fixnum, or uint*.
     * The minimum byte size expression is used.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_unsigned_char(unsigned char d);

    /// Packing unsigned short
    /**
     * The byte size of the packed data depends on `d`.
     * The packed type is positive fixnum, or uint*.
     * The minimum byte size expression is used.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_unsigned_short(unsigned short d);

    /// Packing unsigned int
    /**
     * The byte size of the packed data depends on `d`.
     * The packed type is positive fixnum, or uint*.
     * The minimum byte size expression is used.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_unsigned_int(unsigned int d);

    /// Packing unsigned long
    /**
     * The byte size of the packed data depends on `d`.
     * The packed type is positive fixnum, or uint*.
     * The minimum byte size expression is used.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_unsigned_long(unsigned long d);

    /// Packing unsigned long long
    /**
     * The byte size of the packed data depends on `d`.
     * The packed type is positive fixnum, or uint*.
     * The minimum byte size expression is used.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-int
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_unsigned_long_long(unsigned long long d);

    /// Packing float
    /**
     * The packed type is float32.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-float
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_float(float d);

    /// Packing double
    /**
     * The packed type is float64.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-float
     *
     * @param d a packing object.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_double(double d);


    /// Packing nil
    /**
     * The packed type is nil.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-nil
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_nil();

    /// Packing true
    /**
     * The packed type is bool, value is true.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#bool-format-family
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_true();

    /// Packing false
    /**
     * The packed type is bool, value is false.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#bool-format-family
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_false();

    /// Packing array header and size
    /**
     * The packed type is array header and array size.
     * You need to pack `n` msgpack objects following this header and size.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#array-format-family
     *
     * @param n The number of array elements (array size).
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_array(uint32_t n);

    /// Packing map header and size
    /**
     * The packed type is map header and map size.
     * You need to pack `n` pairs of msgpack objects following this header and size.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#map-format-family
     *
     * @param n The number of array elements (array size).
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_map(uint32_t n);


    /// Packing str header and length
    /**
     * The packed type is str header and length.
     * The minimum byte size length expression is used.
     * You need to call `pack_str_body(const char* b, uint32_t l)` after this function calling with the same `l` value.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-str
     *
     * @param l The length of string.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_str(uint32_t l);

    /// Packing str body
    /**
     * You need to call this function just after `pack_str(uint32_t l)` calling.
     * The value `l` should be the same as `pack_str(uint32_t l)` argument `l`.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-str
     *
     * @param l The length of string.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_str_body(const char* b, uint32_t l);

    /// Packing raw (v4) header and length
    /**
     * The packed type is raw header and length.
     * The minimum byte size length expression is used.
     * The format raw (v4) is old MessagePack version4 format.
     * You need to call `pack_v4raw_body(const char* b, uint32_t l)` after this function calling with the same `l` value.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-str
     *
     * @param l The length of string.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_v4raw(uint32_t l);

    /// Packing raw (v4) body
    /**
     * The format raw (v4) is old MessagePack version4 format.
     * You need to call this function just after `pack_v4raw(uint32_t l)` calling.
     * The value `l` should be the same as `pack_v4raw(uint32_t l)` argument `l`.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-str
     *
     * @param l The length of string.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_v4raw_body(const char* b, uint32_t l);

    /// Packing bin header and length
    /**
     * The packed type is bin header and length.
     * The minimum byte size length expression is used.
     * You need to call `pack_bin_body(const char* b, uint32_t l)` after this function calling with the same `l` value.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#bin-format-family
     *
     * @param l The length of string.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_bin(uint32_t l);

    /// Packing bin body
    /**
     * You need to call this function just after `pack_bin(uint32_t l)` calling.
     * The value `l` should be the same as `pack_bin(uint32_t l)` argument `l`.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#bin-format-family
     *
     * @param l The length of string.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_bin_body(const char* b, uint32_t l);

    /// Packing ext header, type, and length
    /**
     * The packed type is ext.
     * The minimum byte size length expression is used.
     * The length 1, 2, 4, 8, and 16 can be encoded in the header.
     * You need to call `pack_ext_body(const char* b, uint32_t l)` after this function calling with the same `l` value.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#formats-ext
     *
     * @param l The length of string.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_ext(size_t l, int8_t type);

    /// Packing ext body
    /**
     * You need to call this function just after `pack_ext(size_t l, int8_t type)` calling.
     * The value `l` should be the same as `pack_ext(size_t l, int8_t type)` argument `l`.
     * See https://github.com/msgpack/msgpack/blob/master/spec.md#bin-format-family
     *
     * @param l The length of string.
     *
     * @return The reference of `*this`.
     */
    packer<Stream>& pack_ext_body(const char* b, uint32_t l);

private:
    template <typename T>
    void pack_imp_uint8(T d);
    template <typename T>
    void pack_imp_uint16(T d);
    template <typename T>
    void pack_imp_uint32(T d);
    template <typename T>
    void pack_imp_uint64(T d);
    template <typename T>
    void pack_imp_int8(T d);
    template <typename T>
    void pack_imp_int16(T d);
    template <typename T>
    void pack_imp_int32(T d);
    template <typename T>
    void pack_imp_int64(T d);

    void append_buffer(const char* buf, size_t len)
        { m_stream.write(buf, len); }

private:
    Stream& m_stream;

#if defined(MSGPACK_USE_CPP03)
private:
    packer(const packer&);
    packer& operator=(const packer&);
    packer();
#else  // defined(MSGPACK_USE_CPP03)
public:
    packer(const packer&) = delete;
    packer& operator=(const packer&) = delete;
    packer() = delete;
#endif // defined(MSGPACK_USE_CPP03)
};


/// Pack the value as MessagePack format into the stream
/**
 * This function template is left for compatibility.
 * Use `void pack(Stream& s, const T& v)` instead of the function template.
 *
 * @tparam Stream Any type that have a member function `Stream write(const char*, size_t s)`
 * @tparam T Any type that is adapted to MessagePack
 * @param s The pointer to packing destination stream
 * @param v Packing value
 */
template <typename Stream, typename T>
inline void pack(Stream* s, const T& v)
{
    packer<Stream>(*s).pack(v);
}

/// Pack the value as MessagePack format into the stream
/**
 * @tparam Stream Any type that have a member function `Stream write(const char*, size_t s)`
 * @tparam T Any type that is adapted to MessagePack
 * @param s Packing destination stream
 * @param v Packing value
 */
template <typename Stream, typename T>
inline void pack(Stream& s, const T& v)
{
    packer<Stream>(s).pack(v);
}


#if MSGPACK_ENDIAN_LITTLE_BYTE
template <typename T>
inline char take8_8(T d) {
    return static_cast<char>(reinterpret_cast<uint8_t*>(&d)[0]);
}
template <typename T>
inline char take8_16(T d) {
    return static_cast<char>(reinterpret_cast<uint8_t*>(&d)[0]);
}
template <typename T>
inline char take8_32(T d) {
    return static_cast<char>(reinterpret_cast<uint8_t*>(&d)[0]);
}
template <typename T>
inline char take8_64(T d) {
    return static_cast<char>(reinterpret_cast<uint8_t*>(&d)[0]);
}

#elif MSGPACK_ENDIAN_BIG_BYTE

template <typename T>
inline char take8_8(T d) {
    return static_cast<char>(reinterpret_cast<uint8_t*>(&d)[0]);
}
template <typename T>
inline char take8_16(T d) {
    return static_cast<char>(reinterpret_cast<uint8_t*>(&d)[1]);
}
template <typename T>
inline char take8_32(T d) {
    return static_cast<char>(reinterpret_cast<uint8_t*>(&d)[3]);
}
template <typename T>
inline char take8_64(T d) {
    return static_cast<char>(reinterpret_cast<uint8_t*>(&d)[7]);
}

#else
#error msgpack-c supports only big endian and little endian
#endif

template <typename Stream>
inline packer<Stream>::packer(Stream* s) : m_stream(*s) { }

template <typename Stream>
inline packer<Stream>::packer(Stream& s) : m_stream(s) { }


template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_uint8(uint8_t d)
{ pack_imp_uint8(d); return *this; }

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_uint16(uint16_t d)
{ pack_imp_uint16(d); return *this; }

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_uint32(uint32_t d)
{ pack_imp_uint32(d); return *this; }

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_uint64(uint64_t d)
{ pack_imp_uint64(d); return *this; }

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_int8(int8_t d)
{ pack_imp_int8(d); return *this; }

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_int16(int16_t d)
{ pack_imp_int16(d); return *this; }

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_int32(int32_t d)
{ pack_imp_int32(d); return *this; }

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_int64(int64_t d)
{ pack_imp_int64(d); return *this;}


template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_fix_uint8(uint8_t d)
{
    char buf[2] = {static_cast<char>(0xccu), take8_8(d)};
    append_buffer(buf, 2);
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_fix_uint16(uint16_t d)
{
    char buf[3];
    buf[0] = static_cast<char>(0xcdu); _msgpack_store16(&buf[1], d);
    append_buffer(buf, 3);
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_fix_uint32(uint32_t d)
{
    char buf[5];
    buf[0] = static_cast<char>(0xceu); _msgpack_store32(&buf[1], d);
    append_buffer(buf, 5);
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_fix_uint64(uint64_t d)
{
    char buf[9];
    buf[0] = static_cast<char>(0xcfu); _msgpack_store64(&buf[1], d);
    append_buffer(buf, 9);
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_fix_int8(int8_t d)
{
    char buf[2] = {static_cast<char>(0xd0u), take8_8(d)};
    append_buffer(buf, 2);
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_fix_int16(int16_t d)
{
    char buf[3];
    buf[0] = static_cast<char>(0xd1u); _msgpack_store16(&buf[1], d);
    append_buffer(buf, 3);
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_fix_int32(int32_t d)
{
    char buf[5];
    buf[0] = static_cast<char>(0xd2u); _msgpack_store32(&buf[1], d);
    append_buffer(buf, 5);
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_fix_int64(int64_t d)
{
    char buf[9];
    buf[0] = static_cast<char>(0xd3u); _msgpack_store64(&buf[1], d);
    append_buffer(buf, 9);
    return *this;
}


template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_char(char d)
{
#if defined(CHAR_MIN)
#if CHAR_MIN < 0
    pack_imp_int8(d);
#else
    pack_imp_uint8(d);
#endif
#else
#error CHAR_MIN is not defined
#endif
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_signed_char(signed char d)
{
    pack_imp_int8(d);
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_short(short d)
{
#if defined(SIZEOF_SHORT)
#if SIZEOF_SHORT == 2
    pack_imp_int16(d);
#elif SIZEOF_SHORT == 4
    pack_imp_int32(d);
#else
    pack_imp_int64(d);
#endif

#elif defined(SHRT_MAX)
#if SHRT_MAX == 0x7fff
    pack_imp_int16(d);
#elif SHRT_MAX == 0x7fffffff
    pack_imp_int32(d);
#else
    pack_imp_int64(d);
#endif

#else
    if(sizeof(short) == 2) {
        pack_imp_int16(d);
    } else if(sizeof(short) == 4) {
        pack_imp_int32(d);
    } else {
        pack_imp_int64(d);
    }
#endif
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_int(int d)
{
#if defined(SIZEOF_INT)
#if SIZEOF_INT == 2
    pack_imp_int16(d);
#elif SIZEOF_INT == 4
    pack_imp_int32(d);
#else
    pack_imp_int64(d);
#endif

#elif defined(INT_MAX)
#if INT_MAX == 0x7fff
    pack_imp_int16(d);
#elif INT_MAX == 0x7fffffff
    pack_imp_int32(d);
#else
    pack_imp_int64(d);
#endif

#else
    if(sizeof(int) == 2) {
        pack_imp_int16(d);
    } else if(sizeof(int) == 4) {
        pack_imp_int32(d);
    } else {
        pack_imp_int64(d);
    }
#endif
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_long(long d)
{
#if defined(SIZEOF_LONG)
#if SIZEOF_LONG == 2
    pack_imp_int16(d);
#elif SIZEOF_LONG == 4
    pack_imp_int32(d);
#else
    pack_imp_int64(d);
#endif

#elif defined(LONG_MAX)
#if LONG_MAX == 0x7fffL
    pack_imp_int16(d);
#elif LONG_MAX == 0x7fffffffL
    pack_imp_int32(d);
#else
    pack_imp_int64(d);
#endif

#else
    if(sizeof(long) == 2) {
        pack_imp_int16(d);
    } else if(sizeof(long) == 4) {
        pack_imp_int32(d);
    } else {
        pack_imp_int64(d);
    }
#endif
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_long_long(long long d)
{
#if defined(SIZEOF_LONG_LONG)
#if SIZEOF_LONG_LONG == 2
    pack_imp_int16(d);
#elif SIZEOF_LONG_LONG == 4
    pack_imp_int32(d);
#else
    pack_imp_int64(d);
#endif

#elif defined(LLONG_MAX)
#if LLONG_MAX == 0x7fffL
    pack_imp_int16(d);
#elif LLONG_MAX == 0x7fffffffL
    pack_imp_int32(d);
#else
    pack_imp_int64(d);
#endif

#else
    if(sizeof(long long) == 2) {
        pack_imp_int16(d);
    } else if(sizeof(long long) == 4) {
        pack_imp_int32(d);
    } else {
        pack_imp_int64(d);
    }
#endif
    return *this;
}


template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_unsigned_char(unsigned char d)
{
    pack_imp_uint8(d);
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_unsigned_short(unsigned short d)
{
#if defined(SIZEOF_SHORT)
#if SIZEOF_SHORT == 2
    pack_imp_uint16(d);
#elif SIZEOF_SHORT == 4
    pack_imp_uint32(d);
#else
    pack_imp_uint64(d);
#endif

#elif defined(USHRT_MAX)
#if USHRT_MAX == 0xffffU
    pack_imp_uint16(d);
#elif USHRT_MAX == 0xffffffffU
    pack_imp_uint32(d);
#else
    pack_imp_uint64(d);
#endif

#else
    if(sizeof(unsigned short) == 2) {
        pack_imp_uint16(d);
    } else if(sizeof(unsigned short) == 4) {
        pack_imp_uint32(d);
    } else {
        pack_imp_uint64(d);
    }
#endif
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_unsigned_int(unsigned int d)
{
#if defined(SIZEOF_INT)
#if SIZEOF_INT == 2
    pack_imp_uint16(d);
#elif SIZEOF_INT == 4
    pack_imp_uint32(d);
#else
    pack_imp_uint64(d);
#endif

#elif defined(UINT_MAX)
#if UINT_MAX == 0xffffU
    pack_imp_uint16(d);
#elif UINT_MAX == 0xffffffffU
    pack_imp_uint32(d);
#else
    pack_imp_uint64(d);
#endif

#else
    if(sizeof(unsigned int) == 2) {
        pack_imp_uint16(d);
    } else if(sizeof(unsigned int) == 4) {
        pack_imp_uint32(d);
    } else {
        pack_imp_uint64(d);
    }
#endif
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_unsigned_long(unsigned long d)
{
#if defined(SIZEOF_LONG)
#if SIZEOF_LONG == 2
    pack_imp_uint16(d);
#elif SIZEOF_LONG == 4
    pack_imp_uint32(d);
#else
    pack_imp_uint64(d);
#endif

#elif defined(ULONG_MAX)
#if ULONG_MAX == 0xffffUL
    pack_imp_uint16(d);
#elif ULONG_MAX == 0xffffffffUL
    pack_imp_uint32(d);
#else
    pack_imp_uint64(d);
#endif

#else
    if(sizeof(unsigned long) == 2) {
        pack_imp_uint16(d);
    } else if(sizeof(unsigned long) == 4) {
        pack_imp_uint32(d);
    } else {
        pack_imp_uint64(d);
    }
#endif
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_unsigned_long_long(unsigned long long d)
{
#if defined(SIZEOF_LONG_LONG)
#if SIZEOF_LONG_LONG == 2
    pack_imp_uint16(d);
#elif SIZEOF_LONG_LONG == 4
    pack_imp_uint32(d);
#else
    pack_imp_uint64(d);
#endif

#elif defined(ULLONG_MAX)
#if ULLONG_MAX == 0xffffUL
    pack_imp_uint16(d);
#elif ULLONG_MAX == 0xffffffffUL
    pack_imp_uint32(d);
#else
    pack_imp_uint64(d);
#endif

#else
    if(sizeof(unsigned long long) == 2) {
        pack_imp_uint16(d);
    } else if(sizeof(unsigned long long) == 4) {
        pack_imp_uint32(d);
    } else {
        pack_imp_uint64(d);
    }
#endif
    return *this;
}


template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_float(float d)
{
    union { float f; uint32_t i; } mem;
    mem.f = d;
    char buf[5];
    buf[0] = static_cast<char>(0xcau); _msgpack_store32(&buf[1], mem.i);
    append_buffer(buf, 5);
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_double(double d)
{
    union { double f; uint64_t i; } mem;
    mem.f = d;
    char buf[9];
    buf[0] = static_cast<char>(0xcbu);

#if defined(TARGET_OS_IPHONE)
    // ok
#elif defined(__arm__) && !(__ARM_EABI__) // arm-oabi
    // https://github.com/msgpack/msgpack-perl/pull/1
    mem.i = (mem.i & 0xFFFFFFFFUL) << 32UL | (mem.i >> 32UL);
#endif
    _msgpack_store64(&buf[1], mem.i);
    append_buffer(buf, 9);
    return *this;
}


template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_nil()
{
    const char d = static_cast<char>(0xc0u);
    append_buffer(&d, 1);
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_true()
{
    const char d = static_cast<char>(0xc3u);
    append_buffer(&d, 1);
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_false()
{
    const char d = static_cast<char>(0xc2u);
    append_buffer(&d, 1);
    return *this;
}


template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_array(uint32_t n)
{
    if(n < 16) {
        char d = static_cast<char>(0x90u | n);
        append_buffer(&d, 1);
    } else if(n < 65536) {
        char buf[3];
        buf[0] = static_cast<char>(0xdcu); _msgpack_store16(&buf[1], static_cast<uint16_t>(n));
        append_buffer(buf, 3);
    } else {
        char buf[5];
        buf[0] = static_cast<char>(0xddu); _msgpack_store32(&buf[1], static_cast<uint32_t>(n));
        append_buffer(buf, 5);
    }
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_map(uint32_t n)
{
    if(n < 16) {
        unsigned char d = static_cast<unsigned char>(0x80u | n);
        char buf = take8_8(d);
        append_buffer(&buf, 1);
    } else if(n < 65536) {
        char buf[3];
        buf[0] = static_cast<char>(0xdeu); _msgpack_store16(&buf[1], static_cast<uint16_t>(n));
        append_buffer(buf, 3);
    } else {
        char buf[5];
        buf[0] = static_cast<char>(0xdfu); _msgpack_store32(&buf[1], static_cast<uint32_t>(n));
        append_buffer(buf, 5);
    }
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_str(uint32_t l)
{
    if(l < 32) {
        unsigned char d = static_cast<uint8_t>(0xa0u | l);
        char buf = take8_8(d);
        append_buffer(&buf, 1);
    } else if(l < 256) {
        char buf[2];
        buf[0] = static_cast<char>(0xd9u); buf[1] = static_cast<uint8_t>(l);
        append_buffer(buf, 2);
    } else if(l < 65536) {
        char buf[3];
        buf[0] = static_cast<char>(0xdau); _msgpack_store16(&buf[1], static_cast<uint16_t>(l));
        append_buffer(buf, 3);
    } else {
        char buf[5];
        buf[0] = static_cast<char>(0xdbu); _msgpack_store32(&buf[1], static_cast<uint32_t>(l));
        append_buffer(buf, 5);
    }
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_str_body(const char* b, uint32_t l)
{
    append_buffer(b, l);
    return *this;
}

// Raw (V4)

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_v4raw(uint32_t l)
{
    if(l < 32) {
        unsigned char d = static_cast<uint8_t>(0xa0u | l);
        char buf = take8_8(d);
        append_buffer(&buf, 1);
    } else if(l < 65536) {
        char buf[3];
        buf[0] = static_cast<char>(0xdau); _msgpack_store16(&buf[1], static_cast<uint16_t>(l));
        append_buffer(buf, 3);
    } else {
        char buf[5];
        buf[0] = static_cast<char>(0xdbu); _msgpack_store32(&buf[1], static_cast<uint32_t>(l));
        append_buffer(buf, 5);
    }
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_v4raw_body(const char* b, uint32_t l)
{
    append_buffer(b, l);
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_bin(uint32_t l)
{
    if(l < 256) {
        char buf[2];
        buf[0] = static_cast<char>(0xc4u); buf[1] = static_cast<uint8_t>(l);
        append_buffer(buf, 2);
    } else if(l < 65536) {
        char buf[3];
        buf[0] = static_cast<char>(0xc5u); _msgpack_store16(&buf[1], static_cast<uint16_t>(l));
        append_buffer(buf, 3);
    } else {
        char buf[5];
        buf[0] = static_cast<char>(0xc6u); _msgpack_store32(&buf[1], static_cast<uint32_t>(l));
        append_buffer(buf, 5);
    }
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_bin_body(const char* b, uint32_t l)
{
    append_buffer(b, l);
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_ext(size_t l, int8_t type)
{
    switch(l) {
    case 1: {
        char buf[2];
        buf[0] = static_cast<char>(0xd4u);
        buf[1] = static_cast<char>(type);
        append_buffer(buf, 2);
    } break;
    case 2: {
        char buf[2];
        buf[0] = static_cast<char>(0xd5u);
        buf[1] = static_cast<char>(type);
        append_buffer(buf, 2);
    } break;
    case 4: {
        char buf[2];
        buf[0] = static_cast<char>(0xd6u);
        buf[1] = static_cast<char>(type);
        append_buffer(buf, 2);
    } break;
    case 8: {
        char buf[2];
        buf[0] = static_cast<char>(0xd7u);
        buf[1] = static_cast<char>(type);
        append_buffer(buf, 2);
    } break;
    case 16: {
        char buf[2];
        buf[0] = static_cast<char>(0xd8u);
        buf[1] = static_cast<char>(type);
        append_buffer(buf, 2);
    } break;
    default:
        if(l < 256) {
            char buf[3];
            buf[0] = static_cast<char>(0xc7u);
            buf[1] = static_cast<char>(l);
            buf[2] = static_cast<char>(type);
            append_buffer(buf, 3);
        } else if(l < 65536) {
            char buf[4];
            buf[0] = static_cast<char>(0xc8u);
            _msgpack_store16(&buf[1], static_cast<uint16_t>(l));
            buf[3] = static_cast<char>(type);
            append_buffer(buf, 4);
        } else {
            char buf[6];
            buf[0] = static_cast<char>(0xc9u);
            _msgpack_store32(&buf[1], static_cast<uint32_t>(l));
            buf[5] = static_cast<char>(type);
            append_buffer(buf, 6);
        }
        break;
    }
    return *this;
}

template <typename Stream>
inline packer<Stream>& packer<Stream>::pack_ext_body(const char* b, uint32_t l)
{
    append_buffer(b, l);
    return *this;
}

template <typename Stream>
template <typename T>
inline void packer<Stream>::pack_imp_uint8(T d)
{
    if(d < (1<<7)) {
        /* fixnum */
        char buf = take8_8(d);
        append_buffer(&buf, 1);
    } else {
        /* unsigned 8 */
        char buf[2] = {static_cast<char>(0xccu), take8_8(d)};
        append_buffer(buf, 2);
    }
}

template <typename Stream>
template <typename T>
inline void packer<Stream>::pack_imp_uint16(T d)
{
    if(d < (1<<7)) {
        /* fixnum */
        char buf = take8_16(d);
        append_buffer(&buf, 1);
    } else if(d < (1<<8)) {
        /* unsigned 8 */
        char buf[2] = {static_cast<char>(0xccu), take8_16(d)};
        append_buffer(buf, 2);
    } else {
        /* unsigned 16 */
        char buf[3];
        buf[0] = static_cast<char>(0xcdu); _msgpack_store16(&buf[1], static_cast<uint16_t>(d));
        append_buffer(buf, 3);
    }
}

template <typename Stream>
template <typename T>
inline void packer<Stream>::pack_imp_uint32(T d)
{
    if(d < (1<<8)) {
        if(d < (1<<7)) {
            /* fixnum */
            char buf = take8_32(d);
            append_buffer(&buf, 1);
        } else {
            /* unsigned 8 */
            char buf[2] = {static_cast<char>(0xccu), take8_32(d)};
            append_buffer(buf, 2);
        }
    } else {
        if(d < (1<<16)) {
            /* unsigned 16 */
            char buf[3];
            buf[0] = static_cast<char>(0xcdu); _msgpack_store16(&buf[1], static_cast<uint16_t>(d));
            append_buffer(buf, 3);
        } else {
            /* unsigned 32 */
            char buf[5];
            buf[0] = static_cast<char>(0xceu); _msgpack_store32(&buf[1], static_cast<uint32_t>(d));
            append_buffer(buf, 5);
        }
    }
}

template <typename Stream>
template <typename T>
inline void packer<Stream>::pack_imp_uint64(T d)
{
    if(d < (1ULL<<8)) {
        if(d < (1ULL<<7)) {
            /* fixnum */
            char buf = take8_64(d);
            append_buffer(&buf, 1);
        } else {
            /* unsigned 8 */
            char buf[2] = {static_cast<char>(0xccu), take8_64(d)};
            append_buffer(buf, 2);
        }
    } else {
        if(d < (1ULL<<16)) {
            /* unsigned 16 */
            char buf[3];
            buf[0] = static_cast<char>(0xcdu); _msgpack_store16(&buf[1], static_cast<uint16_t>(d));
            append_buffer(buf, 3);
        } else if(d < (1ULL<<32)) {
            /* unsigned 32 */
            char buf[5];
            buf[0] = static_cast<char>(0xceu); _msgpack_store32(&buf[1], static_cast<uint32_t>(d));
            append_buffer(buf, 5);
        } else {
            /* unsigned 64 */
            char buf[9];
            buf[0] = static_cast<char>(0xcfu); _msgpack_store64(&buf[1], d);
            append_buffer(buf, 9);
        }
    }
}

template <typename Stream>
template <typename T>
inline void packer<Stream>::pack_imp_int8(T d)
{
    if(d < -(1<<5)) {
        /* signed 8 */
        char buf[2] = {static_cast<char>(0xd0u), take8_8(d)};
        append_buffer(buf, 2);
    } else {
        /* fixnum */
        char buf = take8_8(d);
        append_buffer(&buf, 1);
    }
}

template <typename Stream>
template <typename T>
inline void packer<Stream>::pack_imp_int16(T d)
{
    if(d < -(1<<5)) {
        if(d < -(1<<7)) {
            /* signed 16 */
            char buf[3];
            buf[0] = static_cast<char>(0xd1u); _msgpack_store16(&buf[1], static_cast<int16_t>(d));
            append_buffer(buf, 3);
        } else {
            /* signed 8 */
            char buf[2] = {static_cast<char>(0xd0u), take8_16(d)};
            append_buffer(buf, 2);
        }
    } else if(d < (1<<7)) {
        /* fixnum */
        char buf = take8_16(d);
        append_buffer(&buf, 1);
    } else {
        if(d < (1<<8)) {
            /* unsigned 8 */
            char buf[2] = {static_cast<char>(0xccu), take8_16(d)};
            append_buffer(buf, 2);
        } else {
            /* unsigned 16 */
            char buf[3];
            buf[0] = static_cast<char>(0xcdu); _msgpack_store16(&buf[1], static_cast<uint16_t>(d));
            append_buffer(buf, 3);
        }
    }
}

template <typename Stream>
template <typename T>
inline void packer<Stream>::pack_imp_int32(T d)
{
    if(d < -(1<<5)) {
        if(d < -(1<<15)) {
            /* signed 32 */
            char buf[5];
            buf[0] = static_cast<char>(0xd2u); _msgpack_store32(&buf[1], static_cast<int32_t>(d));
            append_buffer(buf, 5);
        } else if(d < -(1<<7)) {
            /* signed 16 */
            char buf[3];
            buf[0] = static_cast<char>(0xd1u); _msgpack_store16(&buf[1], static_cast<int16_t>(d));
            append_buffer(buf, 3);
        } else {
            /* signed 8 */
            char buf[2] = { static_cast<char>(0xd0u), take8_32(d)};
            append_buffer(buf, 2);
        }
    } else if(d < (1<<7)) {
        /* fixnum */
        char buf = take8_32(d);
        append_buffer(&buf, 1);
    } else {
        if(d < (1<<8)) {
            /* unsigned 8 */
            char buf[2] = { static_cast<char>(0xccu), take8_32(d)};
            append_buffer(buf, 2);
        } else if(d < (1<<16)) {
            /* unsigned 16 */
            char buf[3];
            buf[0] = static_cast<char>(0xcdu); _msgpack_store16(&buf[1], static_cast<uint16_t>(d));
            append_buffer(buf, 3);
        } else {
            /* unsigned 32 */
            char buf[5];
            buf[0] = static_cast<char>(0xceu); _msgpack_store32(&buf[1], static_cast<uint32_t>(d));
            append_buffer(buf, 5);
        }
    }
}

template <typename Stream>
template <typename T>
inline void packer<Stream>::pack_imp_int64(T d)
{
    if(d < -(1LL<<5)) {
        if(d < -(1LL<<15)) {
            if(d < -(1LL<<31)) {
                /* signed 64 */
                char buf[9];
                buf[0] = static_cast<char>(0xd3u); _msgpack_store64(&buf[1], d);
                append_buffer(buf, 9);
            } else {
                /* signed 32 */
                char buf[5];
                buf[0] = static_cast<char>(0xd2u); _msgpack_store32(&buf[1], static_cast<int32_t>(d));
                append_buffer(buf, 5);
            }
        } else {
            if(d < -(1<<7)) {
                /* signed 16 */
                char buf[3];
                buf[0] = static_cast<char>(0xd1u); _msgpack_store16(&buf[1], static_cast<int16_t>(d));
                append_buffer(buf, 3);
            } else {
                /* signed 8 */
                char buf[2] = {static_cast<char>(0xd0u), take8_64(d)};
                append_buffer(buf, 2);
            }
        }
    } else if(d < (1<<7)) {
        /* fixnum */
        char buf = take8_64(d);
        append_buffer(&buf, 1);
    } else {
        if(d < (1LL<<16)) {
            if(d < (1<<8)) {
                /* unsigned 8 */
                char buf[2] = {static_cast<char>(0xccu), take8_64(d)};
                append_buffer(buf, 2);
            } else {
                /* unsigned 16 */
                char buf[3];
                buf[0] = static_cast<char>(0xcdu); _msgpack_store16(&buf[1], static_cast<uint16_t>(d));
                append_buffer(buf, 3);
            }
        } else {
            if(d < (1LL<<32)) {
                /* unsigned 32 */
                char buf[5];
                buf[0] = static_cast<char>(0xceu); _msgpack_store32(&buf[1], static_cast<uint32_t>(d));
                append_buffer(buf, 5);
            } else {
                /* unsigned 64 */
                char buf[9];
                buf[0] = static_cast<char>(0xcfu); _msgpack_store64(&buf[1], d);
                append_buffer(buf, 9);
            }
        }
    }
}

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond

}  // namespace msgpack

#endif // MSGPACK_V1_PACK_HPP
