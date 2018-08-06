//
// MessagePack for C++ deserializing routine
//
// Copyright (C) 2018 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V2_PARSE_DECL_HPP
#define MSGPACK_V2_PARSE_DECL_HPP

#include "msgpack/parse_return.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v2) {
/// @endcond

namespace detail {

template <typename VisitorHolder>
class context;

} // detail


/// Parsing class for a stream deserialization.

template <typename VisitorHolder, typename ReferencedBufferHook>
class parser;


/// Unpack msgpack formatted data via a visitor
/**
 * @param data The pointer to the buffer.
 * @param len The length of the buffer.
 * @param off The offset position of the buffer. It is read and overwritten.
 * @param v The visitor that satisfies visitor concept. https://github.com/msgpack/msgpack-c/wiki/v2_0_cpp_visitor#visitor-concept
 *
 * @return if unpacking process finishes without error then return true, otherwise return false.
 *
 */
template <typename Visitor>
bool parse(const char* data, size_t len, size_t& off, Visitor& v);


/// Unpack msgpack formatted data via a visitor
/**
 * @param data The pointer to the buffer.
 * @param len The length of the buffer.
 * @param v The visitor that satisfies visitor concept. https://github.com/msgpack/msgpack-c/wiki/v2_0_cpp_visitor#visitor-concept
 *
 * @return if unpacking process finishes without error then return true, otherwise return false.
 *
 */
template <typename Visitor>
bool parse(const char* data, size_t len, Visitor& v);

namespace detail {

template <typename Visitor>
struct parse_helper;

template <typename Visitor>
inline parse_return
parse_imp(const char* data, size_t len, size_t& off, Visitor& v);

} // detail

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v2)
/// @endcond

}  // namespace msgpack


#endif // MSGPACK_V2_PARSE_DECL_HPP
