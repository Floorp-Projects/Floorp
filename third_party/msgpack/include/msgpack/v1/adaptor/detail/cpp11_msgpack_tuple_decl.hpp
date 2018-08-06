//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2008-2015 FURUHASHI Sadayuki and KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_CPP11_MSGPACK_TUPLE_DECL_HPP
#define MSGPACK_V1_CPP11_MSGPACK_TUPLE_DECL_HPP

#include "msgpack/versioning.hpp"
#include "msgpack/object_fwd.hpp"
#include "msgpack/meta.hpp"

#include <tuple>

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

namespace type {
    // tuple
    using std::get;
    using std::tuple_size;
    using std::tuple_element;
    using std::uses_allocator;
    using std::ignore;
    using std::swap;

    template< class... Types >
    class tuple : public std::tuple<Types...> {
    public:
        using base = std::tuple<Types...>;

        tuple(tuple const&) = default;
        tuple(tuple&&) = default;

        template<typename... OtherTypes>
        tuple(OtherTypes&&... other):base(std::forward<OtherTypes>(other)...) {}

        template<typename... OtherTypes>
        tuple(tuple<OtherTypes...> const& other):base(static_cast<std::tuple<OtherTypes...> const&>(other)) {}
        template<typename... OtherTypes>
        tuple(tuple<OtherTypes...> && other):base(static_cast<std::tuple<OtherTypes...> &&>(other)) {}

        tuple& operator=(tuple const&) = default;
        tuple& operator=(tuple&&) = default;

        template<typename... OtherTypes>
        tuple& operator=(tuple<OtherTypes...> const& other) {
            *static_cast<base*>(this) = static_cast<std::tuple<OtherTypes...> const&>(other);
            return *this;
        }
        template<typename... OtherTypes>
        tuple& operator=(tuple<OtherTypes...> && other) {
            *static_cast<base*>(this) = static_cast<std::tuple<OtherTypes...> &&>(other);
            return *this;
        }

        template< std::size_t I>
        typename tuple_element<I, base >::type&
        get() & { return std::get<I>(*this); }

        template< std::size_t I>
        typename tuple_element<I, base >::type const&
        get() const& { return std::get<I>(*this); }

        template< std::size_t I>
        typename tuple_element<I, base >::type&&
        get() && { return std::get<I>(*this); }

        std::size_t size() const { return sizeof...(Types); }
    };

    template <class... Args>
    tuple<Args...> make_tuple(Args&&... args);

    template<class... Args>
    tuple<Args&&...> forward_as_tuple (Args&&... args) noexcept;

    template <class... Tuples>
    auto tuple_cat(Tuples&&... args) ->
        decltype(
            std::tuple_cat(std::forward<typename std::remove_reference<Tuples>::type::base>(args)...)
        );

    template <class... Args>
    tuple<Args&...> tie(Args&... args);

} // namespace type

// --- Pack from tuple to packer stream ---
template <typename Stream, typename Tuple, std::size_t N>
struct MsgpackTuplePacker;

// --- Convert from tuple to object ---
template <typename... Args>
struct MsgpackTupleAs;

template <typename T, typename... Args>
struct MsgpackTupleAsImpl;

template <typename Tuple, std::size_t N>
struct MsgpackTupleConverter;

// --- Convert from tuple to object with zone ---
template <typename Tuple, std::size_t N>
struct MsgpackTupleToObjectWithZone;

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v1)
///@endcond

}  // namespace msgpack

#endif // MSGPACK_V1_CPP11_MSGPACK_TUPLE_DECL_HPP
