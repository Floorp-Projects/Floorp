//
// MessagePack for C++ deserializing routine
//
// Copyright (C) 2008-2016 FURUHASHI Sadayuki and KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_UNPACK_DECL_HPP
#define MSGPACK_V1_UNPACK_DECL_HPP

#include "msgpack/versioning.hpp"
#include "msgpack/unpack_define.h"
#include "msgpack/object.hpp"
#include "msgpack/zone.hpp"
#include "msgpack/cpp_config.hpp"
#include "msgpack/sysdep.h"
#include "msgpack/parse_return.hpp"

#include <memory>
#include <stdexcept>

#if !defined(MSGPACK_USE_CPP03)
#include <atomic>
#endif


#if defined(_MSC_VER)
// avoiding confliction std::max, std::min, and macro in windows.h
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif // defined(_MSC_VER)

#ifdef _msgpack_atomic_counter_header
#include _msgpack_atomic_counter_header
#endif

const size_t COUNTER_SIZE = sizeof(_msgpack_atomic_counter_t);

#ifndef MSGPACK_UNPACKER_INIT_BUFFER_SIZE
#define MSGPACK_UNPACKER_INIT_BUFFER_SIZE (64*1024)
#endif

#ifndef MSGPACK_UNPACKER_RESERVE_SIZE
#define MSGPACK_UNPACKER_RESERVE_SIZE (32*1024)
#endif


// backward compatibility
#ifndef MSGPACK_UNPACKER_DEFAULT_INITIAL_BUFFER_SIZE
#define MSGPACK_UNPACKER_DEFAULT_INITIAL_BUFFER_SIZE MSGPACK_UNPACKER_INIT_BUFFER_SIZE
#endif


namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

/// The type of reference or copy judging function.
/**
 * @param type msgpack data type.
 * @param size msgpack data size.
 * @param user_data The user_data that is set by msgpack::unpack functions.
 *
 * @return If the data should be referenced, then return true, otherwise (should be copied) false.
 *
 * This function is called when unpacking STR, BIN, or EXT.
 *
 */
typedef bool (*unpack_reference_func)(msgpack::type::object_type type, std::size_t size, void* user_data);

struct unpack_error;
struct parse_error;
struct insufficient_bytes;
struct size_overflow;
struct array_size_overflow;
struct map_size_overflow;
struct str_size_overflow;
struct bin_size_overflow;
struct ext_size_overflow;
struct depth_size_overflow;

class unpack_limit {
public:
    unpack_limit(
        std::size_t array = 0xffffffff,
        std::size_t map = 0xffffffff,
        std::size_t str = 0xffffffff,
        std::size_t bin = 0xffffffff,
        std::size_t ext = 0xffffffff,
        std::size_t depth = 0xffffffff)
        :array_(array),
         map_(map),
         str_(str),
         bin_(bin),
         ext_(ext),
         depth_(depth) {}
    std::size_t array() const { return array_; }
    std::size_t map() const { return map_; }
    std::size_t str() const { return str_; }
    std::size_t bin() const { return bin_; }
    std::size_t ext() const { return ext_; }
    std::size_t depth() const { return depth_; }

private:
    std::size_t array_;
    std::size_t map_;
    std::size_t str_;
    std::size_t bin_;
    std::size_t ext_;
    std::size_t depth_;
};

namespace detail {

class unpack_user;

void unpack_uint8(uint8_t d, msgpack::object& o);

void unpack_uint16(uint16_t d, msgpack::object& o);

void unpack_uint32(uint32_t d, msgpack::object& o);

void unpack_uint64(uint64_t d, msgpack::object& o);

void unpack_int8(int8_t d, msgpack::object& o);

void unpack_int16(int16_t d, msgpack::object& o);

void unpack_int32(int32_t d, msgpack::object& o);

void unpack_int64(int64_t d, msgpack::object& o);

void unpack_float(float d, msgpack::object& o);

void unpack_double(double d, msgpack::object& o);

void unpack_nil(msgpack::object& o);

void unpack_true(msgpack::object& o);

void unpack_false(msgpack::object& o);

struct unpack_array;

void unpack_array_item(msgpack::object& c, msgpack::object const& o);

struct unpack_map;

void unpack_map_item(msgpack::object& c, msgpack::object const& k, msgpack::object const& v);

void unpack_str(unpack_user& u, const char* p, uint32_t l, msgpack::object& o);

void unpack_bin(unpack_user& u, const char* p, uint32_t l, msgpack::object& o);

void unpack_ext(unpack_user& u, const char* p, std::size_t l, msgpack::object& o);

class unpack_stack;

void init_count(void* buffer);

void decr_count(void* buffer);

void incr_count(void* buffer);

#if defined(MSGPACK_USE_CPP03)

_msgpack_atomic_counter_t get_count(void* buffer);

#else  // defined(MSGPACK_USE_CPP03)

std::atomic<unsigned int> const& get_count(void* buffer);

#endif // defined(MSGPACK_USE_CPP03)

struct fix_tag {
    char f1[65]; // FIXME unique size is required. or use is_same meta function.
};

template <typename T>
struct value;

template <typename T>
typename msgpack::enable_if<sizeof(T) == sizeof(fix_tag)>::type load(uint32_t& dst, const char* n);

template <typename T>
typename msgpack::enable_if<sizeof(T) == 1>::type load(T& dst, const char* n);

template <typename T>
typename msgpack::enable_if<sizeof(T) == 2>::type load(T& dst, const char* n);

template <typename T>
typename msgpack::enable_if<sizeof(T) == 4>::type load(T& dst, const char* n);

template <typename T>
typename msgpack::enable_if<sizeof(T) == 8>::type load(T& dst, const char* n);

class context;

} // detail


typedef object_handle unpacked;

/// Unpacking class for a stream deserialization.
class unpacker;

/// Unpack msgpack::object from a buffer.
/**
 * @param data The pointer to the buffer.
 * @param len The length of the buffer.
 * @param off The offset position of the buffer. It is read and overwritten.
 * @param referenced If the unpacked object contains reference of the buffer, then set as true, otherwise false.
 * @param f A judging function that msgpack::object refer to the buffer.
 * @param user_data This parameter is passed to f.
 * @param limit The size limit information of msgpack::object.
 *
 * @return object_handle that contains unpacked data.
 *
 */
object_handle unpack(
    const char* data, std::size_t len, std::size_t& off, bool& referenced,
    unpack_reference_func f = MSGPACK_NULLPTR, void* user_data = MSGPACK_NULLPTR, unpack_limit const& limit = unpack_limit());

/// Unpack msgpack::object from a buffer.
/**
 * @param data The pointer to the buffer.
 * @param len The length of the buffer.
 * @param off The offset position of the buffer. It is read and overwritten.
 * @param f A judging function that msgpack::object refer to the buffer.
 * @param user_data This parameter is passed to f.
 * @param limit The size limit information of msgpack::object.
 *
 * @return object_handle that contains unpacked data.
 *
 */
object_handle unpack(
    const char* data, std::size_t len, std::size_t& off,
    unpack_reference_func f = MSGPACK_NULLPTR, void* user_data = MSGPACK_NULLPTR, unpack_limit const& limit = unpack_limit());

/// Unpack msgpack::object from a buffer.
/**
 * @param data The pointer to the buffer.
 * @param len The length of the buffer.
 * @param referenced If the unpacked object contains reference of the buffer, then set as true, otherwise false.
 * @param f A judging function that msgpack::object refer to the buffer.
 * @param user_data This parameter is passed to f.
 * @param limit The size limit information of msgpack::object.
 *
 * @return object_handle that contains unpacked data.
 *
 */
object_handle unpack(
    const char* data, std::size_t len, bool& referenced,
    unpack_reference_func f = MSGPACK_NULLPTR, void* user_data = MSGPACK_NULLPTR, unpack_limit const& limit = unpack_limit());

/// Unpack msgpack::object from a buffer.
/**
 * @param data The pointer to the buffer.
 * @param len The length of the buffer.
 * @param f A judging function that msgpack::object refer to the buffer.
 * @param user_data This parameter is passed to f.
 * @param limit The size limit information of msgpack::object.
 *
 * @return object_handle that contains unpacked data.
 *
 */
object_handle unpack(
    const char* data, std::size_t len,
    unpack_reference_func f = MSGPACK_NULLPTR, void* user_data = MSGPACK_NULLPTR, unpack_limit const& limit = unpack_limit());


/// Unpack msgpack::object from a buffer.
/**
 * @param result The object_handle that contains unpacked data.
 * @param data The pointer to the buffer.
 * @param len The length of the buffer.
 * @param off The offset position of the buffer. It is read and overwritten.
 * @param referenced If the unpacked object contains reference of the buffer, then set as true, otherwise false.
 * @param f A judging function that msgpack::object refer to the buffer.
 * @param user_data This parameter is passed to f.
 * @param limit The size limit information of msgpack::object.
 *
 *
 */
void unpack(
    object_handle& result,
    const char* data, std::size_t len, std::size_t& off, bool& referenced,
    unpack_reference_func f = MSGPACK_NULLPTR, void* user_data = MSGPACK_NULLPTR, unpack_limit const& limit = unpack_limit());

/// Unpack msgpack::object from a buffer.
/**
 * @param result The object_handle that contains unpacked data.
 * @param data The pointer to the buffer.
 * @param len The length of the buffer.
 * @param off The offset position of the buffer. It is read and overwritten.
 * @param f A judging function that msgpack::object refer to the buffer.
 * @param user_data This parameter is passed to f.
 * @param limit The size limit information of msgpack::object.
 *
 *
 */
void unpack(
    object_handle& result,
    const char* data, std::size_t len, std::size_t& off,
    unpack_reference_func f = MSGPACK_NULLPTR, void* user_data = MSGPACK_NULLPTR, unpack_limit const& limit = unpack_limit());

/// Unpack msgpack::object from a buffer.
/**
 * @param result The object_handle that contains unpacked data.
 * @param data The pointer to the buffer.
 * @param len The length of the buffer.
 * @param referenced If the unpacked object contains reference of the buffer, then set as true, otherwise false.
 * @param f A judging function that msgpack::object refer to the buffer.
 * @param user_data This parameter is passed to f.
 * @param limit The size limit information of msgpack::object.
 *
 *
 */
void unpack(
    object_handle& result,
    const char* data, std::size_t len, bool& referenced,
    unpack_reference_func f = MSGPACK_NULLPTR, void* user_data = MSGPACK_NULLPTR, unpack_limit const& limit = unpack_limit());

/// Unpack msgpack::object from a buffer.
/**
 * @param result The object_handle that contains unpacked data.
 * @param data The pointer to the buffer.
 * @param len The length of the buffer.
 * @param f A judging function that msgpack::object refer to the buffer.
 * @param user_data This parameter is passed to f.
 * @param limit The size limit information of msgpack::object.
 *
 *
 */
void unpack(
    object_handle& result,
    const char* data, std::size_t len,
    unpack_reference_func f = MSGPACK_NULLPTR, void* user_data = MSGPACK_NULLPTR, unpack_limit const& limit = unpack_limit());

/// Unpack msgpack::object from a buffer.
/**
 * @param z The msgpack::zone that is used as a memory of unpacked msgpack objects.
 * @param data The pointer to the buffer.
 * @param len The length of the buffer.
 * @param off The offset position of the buffer. It is read and overwritten.
 * @param referenced If the unpacked object contains reference of the buffer, then set as true, otherwise false.
 * @param f A judging function that msgpack::object refer to the buffer.
 * @param user_data This parameter is passed to f.
 * @param limit The size limit information of msgpack::object.
 *
 * @return msgpack::object that contains unpacked data.
 *
 */
msgpack::object unpack(
    msgpack::zone& z,
    const char* data, std::size_t len, std::size_t& off, bool& referenced,
    unpack_reference_func f = MSGPACK_NULLPTR, void* user_data = MSGPACK_NULLPTR, unpack_limit const& limit = unpack_limit());

/// Unpack msgpack::object from a buffer.
/**
 * @param z The msgpack::zone that is used as a memory of unpacked msgpack objects.
 * @param data The pointer to the buffer.
 * @param len The length of the buffer.
 * @param off The offset position of the buffer. It is read and overwritten.
 * @param f A judging function that msgpack::object refer to the buffer.
 * @param user_data This parameter is passed to f.
 * @param limit The size limit information of msgpack::object.
 *
 * @return msgpack::object that contains unpacked data.
 *
 */
msgpack::object unpack(
    msgpack::zone& z,
    const char* data, std::size_t len, std::size_t& off,
    unpack_reference_func f = MSGPACK_NULLPTR, void* user_data = MSGPACK_NULLPTR, unpack_limit const& limit = unpack_limit());

/// Unpack msgpack::object from a buffer.
/**
 * @param z The msgpack::zone that is used as a memory of unpacked msgpack objects.
 * @param data The pointer to the buffer.
 * @param len The length of the buffer.
 * @param referenced If the unpacked object contains reference of the buffer, then set as true, otherwise false.
 * @param f A judging function that msgpack::object refer to the buffer.
 * @param user_data This parameter is passed to f.
 * @param limit The size limit information of msgpack::object.
 *
 * @return msgpack::object that contains unpacked data.
 *
 */
msgpack::object unpack(
    msgpack::zone& z,
    const char* data, std::size_t len, bool& referenced,
    unpack_reference_func f = MSGPACK_NULLPTR, void* user_data = MSGPACK_NULLPTR, unpack_limit const& limit = unpack_limit());

/// Unpack msgpack::object from a buffer.
/**
 * @param z The msgpack::zone that is used as a memory of unpacked msgpack objects.
 * @param data The pointer to the buffer.
 * @param len The length of the buffer.
 * @param f A judging function that msgpack::object refer to the buffer.
 * @param user_data This parameter is passed to f.
 * @param limit The size limit information of msgpack::object.
 *
 * @return msgpack::object that contains unpacked data.
 *
 */
msgpack::object unpack(
    msgpack::zone& z,
    const char* data, std::size_t len,
    unpack_reference_func f = MSGPACK_NULLPTR, void* user_data = MSGPACK_NULLPTR, unpack_limit const& limit = unpack_limit());


/// Unpack msgpack::object from a buffer. [obsolete]
/**
 * @param result The object_handle that contains unpacked data.
 * @param data The pointer to the buffer.
 * @param len The length of the buffer.
 * @param off The offset position of the buffer. It is read and overwritten.
 * @param referenced If the unpacked object contains reference of the buffer, then set as true, otherwise false.
 * @param f A judging function that msgpack::object refer to the buffer.
 * @param user_data This parameter is passed to f.
 * @param limit The size limit information of msgpack::object.
 *
 * This function is obsolete. Use the reference inteface version of unpack functions instead of the pointer interface version.
 */
void unpack(
    object_handle* result,
    const char* data, std::size_t len, std::size_t* off = MSGPACK_NULLPTR, bool* referenced = MSGPACK_NULLPTR,
    unpack_reference_func f = MSGPACK_NULLPTR, void* user_data = MSGPACK_NULLPTR, unpack_limit const& limit = unpack_limit());


namespace detail {

parse_return
unpack_imp(const char* data, std::size_t len, std::size_t& off,
           msgpack::zone& result_zone, msgpack::object& result, bool& referenced,
           unpack_reference_func f, void* user_data,
           unpack_limit const& limit);

} // detail


/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond

}  // namespace msgpack

#endif // MSGPACK_V1_UNPACK_DECL_HPP
