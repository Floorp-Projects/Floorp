//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2015-2016 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_TYPE_BOOST_MSGPACK_VARIANT_DECL_HPP
#define MSGPACK_V1_TYPE_BOOST_MSGPACK_VARIANT_DECL_HPP

#if defined(MSGPACK_USE_BOOST)

#include "msgpack/versioning.hpp"
#include "msgpack/adaptor/adaptor_base.hpp"
#include "msgpack/adaptor/check_container_size.hpp"
#include "msgpack/adaptor/boost/string_ref.hpp"
#include "msgpack/adaptor/ext.hpp"
#include "msgpack/adaptor/raw.hpp"

#include <string>
#include <vector>
namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

namespace type {

template <typename STR, typename BIN, typename EXT>
struct basic_variant;

template <typename STR, typename BIN, typename EXT>
bool operator<(basic_variant<STR, BIN, EXT> const& lhs, basic_variant<STR, BIN, EXT> const& rhs);

template <typename STR, typename BIN, typename EXT>
bool operator==(basic_variant<STR, BIN, EXT> const& lhs, basic_variant<STR, BIN, EXT> const& rhs);

template <typename STR, typename BIN, typename EXT>
bool operator!=(basic_variant<STR, BIN, EXT> const& lhs, basic_variant<STR, BIN, EXT> const& rhs);

typedef basic_variant<std::string, std::vector<char>, msgpack::type::ext> variant;
typedef basic_variant<
#if (BOOST_VERSION / 100000) >= 1 && ((BOOST_VERSION / 100) % 1000) >= 53
    boost::string_ref,
#else  // (BOOST_VERSION / 100000) >= 1 && ((BOOST_VERSION / 100) % 1000) >= 53
    std::string,
#endif // (BOOST_VERSION / 100000) >= 1 && ((BOOST_VERSION / 100) % 1000) >= 53
    msgpack::type::raw_ref, msgpack::type::ext_ref> variant_ref;

} // namespace type

/// @cond
} // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond

} // namespace msgpack

#endif // MSGPACK_USE_BOOST
#endif // MSGPACK_V1_TYPE_BOOST_MSGPACK_VARIANT_DECL_HPP
