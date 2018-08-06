//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2008-2018 FURUHASHI Sadayuki and KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef MSGPACK_V3_OBJECT_FWD_HPP
#define MSGPACK_V3_OBJECT_FWD_HPP

#include "msgpack/v3/object_fwd_decl.hpp"
#include "msgpack/object_fwd.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v3) {
/// @endcond

#if !defined(MSGPACK_USE_CPP03)

namespace adaptor {

// If v2 has as specialization for T, then dispatch v2::adaptor::as<T>.
// So I call v2::has_as<T> meta function intentionally.
template <typename T>
struct as<T, typename std::enable_if<v2::has_as<T>::value>::type> : v2::adaptor::as<T> {
};

} // namespace adaptor

template <typename T>
struct has_as {
private:
    template <typename U>
    static auto check(U*) ->
        typename std::enable_if<
            // check v3 specialization
            std::is_same<
                decltype(adaptor::as<U>()(std::declval<msgpack::object>())),
                U
            >::value
            ||
            // check v2 specialization
            v2::has_as<U>::value
            ||
            // check v1 specialization
            v1::has_as<U>::value,
            std::true_type
        >::type;
    template <typename...>
    static std::false_type check(...);
public:
    using type = decltype(check<T>(MSGPACK_NULLPTR));
    static constexpr bool value = type::value;
};

#endif // !defined(MSGPACK_USE_CPP03)


/// @cond
} // MSGPACK_API_VERSION_NAMESPACE(v3)
/// @endcond

} // namespace msgpack

#endif // MSGPACK_V3_OBJECT_FWD_HPP
