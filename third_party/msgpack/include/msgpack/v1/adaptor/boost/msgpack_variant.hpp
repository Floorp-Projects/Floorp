//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2015-2016 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_TYPE_BOOST_MSGPACK_VARIANT_HPP
#define MSGPACK_V1_TYPE_BOOST_MSGPACK_VARIANT_HPP

#if defined(MSGPACK_USE_BOOST)

#include "msgpack/v1/adaptor/boost/msgpack_variant_decl.hpp"

#include "msgpack/adaptor/check_container_size.hpp"
#include "msgpack/adaptor/boost/string_ref.hpp"

#include "msgpack/adaptor/nil.hpp"
#include "msgpack/adaptor/bool.hpp"
#include "msgpack/adaptor/int.hpp"
#include "msgpack/adaptor/float.hpp"
#include "msgpack/adaptor/string.hpp"
#include "msgpack/adaptor/vector_char.hpp"
#include "msgpack/adaptor/raw.hpp"
#include "msgpack/adaptor/ext.hpp"
#include "msgpack/adaptor/vector.hpp"
#include "msgpack/adaptor/map.hpp"

#include <boost/variant.hpp>
#include <boost/operators.hpp>

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

namespace type {


template <typename STR, typename BIN, typename EXT>
struct basic_variant :
    boost::variant<
        nil_t,             // NIL
        bool,              // BOOL
        int64_t,           // NEGATIVE_INTEGER
        uint64_t,          // POSITIVE_INTEGER
        double,            // FLOAT32, FLOAT64
        std::string,       // STR
#if (BOOST_VERSION / 100000) >= 1 && ((BOOST_VERSION / 100) % 1000) >= 53
        boost::string_ref, // STR
#endif // (BOOST_VERSION / 100000) >= 1 && ((BOOST_VERSION / 100) % 1000) >= 53
        std::vector<char>, // BIN
        msgpack::type::raw_ref, // BIN
        msgpack::type::ext,               // EXT
        msgpack::type::ext_ref,           // EXT
        boost::recursive_wrapper<std::vector<basic_variant<STR, BIN, EXT> > >, // ARRAY
        boost::recursive_wrapper<std::map<basic_variant<STR, BIN, EXT>, basic_variant<STR, BIN, EXT> > >, // MAP
        boost::recursive_wrapper<std::multimap<basic_variant<STR, BIN, EXT>, basic_variant<STR, BIN, EXT> > >// MAP
    >,
    private boost::totally_ordered<basic_variant<STR, BIN, EXT> > {
    typedef boost::variant<
        nil_t,             // NIL
        bool,              // BOOL
        int64_t,           // NEGATIVE_INTEGER
        uint64_t,          // POSITIVE_INTEGER
        double,            // FLOAT32, FLOAT64
        std::string,       // STR
#if (BOOST_VERSION / 100000) >= 1 && ((BOOST_VERSION / 100) % 1000) >= 53
        boost::string_ref, // STR
#endif // (BOOST_VERSION / 100000) >= 1 && ((BOOST_VERSION / 100) % 1000) >= 53
        std::vector<char>, // BIN
        msgpack::type::raw_ref, // BIN
        msgpack::type::ext,               // EXT
        msgpack::type::ext_ref,           // EXT
        boost::recursive_wrapper<std::vector<basic_variant<STR, BIN, EXT> > >, // ARRAY
        boost::recursive_wrapper<std::map<basic_variant<STR, BIN, EXT>, basic_variant<STR, BIN, EXT> > >, // MAP
        boost::recursive_wrapper<std::multimap<basic_variant<STR, BIN, EXT>, basic_variant<STR, BIN, EXT> > >// MAP
    > base;
    basic_variant() {}
    template <typename T>
    basic_variant(T const& t):base(t) {}

#if defined(_MSC_VER) && _MSC_VER < 1700
    // The following redundant functions are required to avoid MSVC
    // See https://svn.boost.org/trac/boost/ticket/592
    basic_variant(basic_variant const& other):base(static_cast<base const&>(other)) {}
    basic_variant& operator=(basic_variant const& other) {
        *static_cast<base*>(this) = static_cast<base const&>(other);
        return *this;
    }
#endif // defined(_MSC_VER) && _MSC_VER < 1700

    basic_variant(char const* p):base(std::string(p)) {}
    basic_variant(char v) {
        int_init(v);
    }
    basic_variant(signed char v) {
        int_init(v);
    }
    basic_variant(unsigned char v):base(uint64_t(v)) {}
    basic_variant(signed int v) {
        int_init(v);
    }
    basic_variant(unsigned int v):base(uint64_t(v)) {}
    basic_variant(signed long v) {
        int_init(v);
    }
    basic_variant(unsigned long v):base(uint64_t(v)) {}
    basic_variant(signed long long v) {
        int_init(v);
    }
    basic_variant(unsigned long long v):base(uint64_t(v)) {}

    bool is_nil() const {
        return boost::get<msgpack::type::nil_t>(this) != MSGPACK_NULLPTR;
    }
    bool is_bool() const {
        return boost::get<bool>(this) != MSGPACK_NULLPTR;
    }
    bool is_int64_t() const {
        return boost::get<int64_t>(this) != MSGPACK_NULLPTR;
    }
    bool is_uint64_t() const {
        return boost::get<uint64_t>(this) != MSGPACK_NULLPTR;
    }
    bool is_double() const {
        return boost::get<double>(this) != MSGPACK_NULLPTR;
    }
    bool is_string() const {
        return boost::get<std::string>(this) != MSGPACK_NULLPTR;
    }
#if (BOOST_VERSION / 100000) >= 1 && ((BOOST_VERSION / 100) % 1000) >= 53
    bool is_boost_string_ref() const {
        return boost::get<boost::string_ref>(this) != MSGPACK_NULLPTR;
    }
#endif // (BOOST_VERSION / 100000) >= 1 && ((BOOST_VERSION / 100) % 1000) >= 53
    bool is_vector_char() const {
        return boost::get<std::vector<char> >(this) != MSGPACK_NULLPTR;
    }
    bool is_vector_char() {
        return boost::get<std::vector<char> >(this) != MSGPACK_NULLPTR;
    }
    bool is_raw_ref() const {
        return boost::get<raw_ref>(this) != MSGPACK_NULLPTR;
    }
    bool is_ext() const {
        return boost::get<ext>(this) != MSGPACK_NULLPTR;
    }
    bool is_ext_ref() const {
        return boost::get<ext_ref>(this) != MSGPACK_NULLPTR;
    }
    bool is_vector() const {
        return boost::get<std::vector<basic_variant<STR, BIN, EXT> > >(this) != MSGPACK_NULLPTR;
    }
    bool is_map() const {
        return boost::get<std::map<basic_variant<STR, BIN, EXT>, basic_variant<STR, BIN, EXT> > >(this) != MSGPACK_NULLPTR;
    }
    bool is_multimap() const {
        return boost::get<std::multimap<basic_variant<STR, BIN, EXT>, basic_variant<STR, BIN, EXT> > >(this) != MSGPACK_NULLPTR;
    }

    bool as_bool() const {
        return boost::get<bool>(*this);
    }
    int64_t as_int64_t() const {
        return boost::get<int64_t>(*this);
    }
    int64_t& as_int64_t() {
        return boost::get<int64_t>(*this);
    }
    uint64_t as_uint64_t() const {
        return boost::get<uint64_t>(*this);
    }
    uint64_t& as_uint64_t() {
        return boost::get<uint64_t>(*this);
    }
    double as_double() const {
        return boost::get<double>(*this);
    }
    double& as_double() {
        return boost::get<double>(*this);
    }
    std::string const& as_string() const {
        return boost::get<std::string>(*this);
    }
    std::string& as_string() {
        return boost::get<std::string>(*this);
    }
#if (BOOST_VERSION / 100000) >= 1 && ((BOOST_VERSION / 100) % 1000) >= 53
    boost::string_ref const& as_boost_string_ref() const {
        return boost::get<boost::string_ref>(*this);
    }
    boost::string_ref& as_boost_string_ref() {
        return boost::get<boost::string_ref>(*this);
    }
#endif // (BOOST_VERSION / 100000) >= 1 && ((BOOST_VERSION / 100) % 1000) >= 53
    std::vector<char> const& as_vector_char() const {
        return boost::get<std::vector<char> >(*this);
    }
    std::vector<char>& as_vector_char() {
        return boost::get<std::vector<char> >(*this);
    }
    raw_ref const& as_raw_ref() const {
        return boost::get<raw_ref>(*this);
    }
    ext const& as_ext() const {
        return boost::get<ext>(*this);
    }
    ext& as_ext() {
        return boost::get<ext>(*this);
    }
    ext_ref const& as_ext_ref() const {
        return boost::get<ext_ref>(*this);
    }
    std::vector<basic_variant<STR, BIN, EXT> > const& as_vector() const {
        return boost::get<std::vector<basic_variant<STR, BIN, EXT> > >(*this);
    }
    std::vector<basic_variant<STR, BIN, EXT> >& as_vector() {
        return boost::get<std::vector<basic_variant<STR, BIN, EXT> > >(*this);
    }
    std::map<basic_variant<STR, BIN, EXT>, basic_variant<STR, BIN, EXT> > const& as_map() const {
        return boost::get<std::map<basic_variant<STR, BIN, EXT>, basic_variant<STR, BIN, EXT> > >(*this);
    }
    std::map<basic_variant<STR, BIN, EXT>, basic_variant<STR, BIN, EXT> >& as_map() {
        return boost::get<std::map<basic_variant<STR, BIN, EXT>, basic_variant<STR, BIN, EXT> > >(*this);
    }
    std::multimap<basic_variant<STR, BIN, EXT>, basic_variant<STR, BIN, EXT> > const& as_multimap() const {
        return boost::get<std::multimap<basic_variant<STR, BIN, EXT>, basic_variant<STR, BIN, EXT> > >(*this);
    }
    std::multimap<basic_variant<STR, BIN, EXT>, basic_variant<STR, BIN, EXT> >& as_multimap() {
        return boost::get<std::multimap<basic_variant<STR, BIN, EXT>, basic_variant<STR, BIN, EXT> > >(*this);
    }
private:
    template <typename T>
    void int_init(T v) {
        if (v < 0) {
            static_cast<base&>(*this) = int64_t(v);
        }
        else {
            static_cast<base&>(*this) = uint64_t(v);
        }
    }
};

template <typename STR, typename BIN, typename EXT>
inline bool operator<(basic_variant<STR, BIN, EXT> const& lhs, basic_variant<STR, BIN, EXT> const& rhs) {
    return
        static_cast<typename basic_variant<STR, BIN, EXT>::base const&>(lhs) <
        static_cast<typename basic_variant<STR, BIN, EXT>::base const&>(rhs);
}

template <typename STR, typename BIN, typename EXT>
inline bool operator==(basic_variant<STR, BIN, EXT> const& lhs, basic_variant<STR, BIN, EXT> const& rhs) {
    return
        static_cast<typename basic_variant<STR, BIN, EXT>::base const&>(lhs) ==
        static_cast<typename basic_variant<STR, BIN, EXT>::base const&>(rhs);
}

typedef basic_variant<std::string, std::vector<char>, ext> variant;
typedef basic_variant<
#if (BOOST_VERSION / 100000) >= 1 && ((BOOST_VERSION / 100) % 1000) >= 53
    boost::string_ref,
#else  // (BOOST_VERSION / 100000) >= 1 && ((BOOST_VERSION / 100) % 1000) >= 53
    std::string,
#endif // (BOOST_VERSION / 100000) >= 1 && ((BOOST_VERSION / 100) % 1000) >= 53
    raw_ref, ext_ref> variant_ref;

} // namespace type

namespace adaptor {

#if !defined (MSGPACK_USE_CPP03)

template <typename STR, typename BIN, typename EXT>
struct as<type::basic_variant<STR, BIN, EXT> > {
    type::basic_variant<STR, BIN, EXT> operator()(msgpack::object const& o) const {
        switch(o.type) {
        case type::NIL:
            return o.as<msgpack::type::nil_t>();
        case type::BOOLEAN:
            return o.as<bool>();
        case type::POSITIVE_INTEGER:
            return o.as<uint64_t>();
        case type::NEGATIVE_INTEGER:
            return o.as<int64_t>();
        case type::FLOAT32:
        case type::FLOAT64:
            return o.as<double>();
        case type::STR:
            return o.as<STR>();
        case type::BIN:
            return o.as<BIN>();
        case type::EXT:
            return o.as<EXT>();
        case type::ARRAY:
            return o.as<std::vector<type::basic_variant<STR, BIN, EXT> > >();
        case type::MAP:
            return o.as<std::multimap<type::basic_variant<STR, BIN, EXT>, type::basic_variant<STR, BIN, EXT> > >();
        default:
            break;
        }
        return type::basic_variant<STR, BIN, EXT>();
    }
};

#endif // !defined (MSGPACK_USE_CPP03)


template <typename STR, typename BIN, typename EXT>
struct convert<type::basic_variant<STR, BIN, EXT> > {
    msgpack::object const& operator()(
        msgpack::object const& o,
        type::basic_variant<STR, BIN, EXT>& v) const {
        switch(o.type) {
        case type::NIL:
            v = o.as<msgpack::type::nil_t>();
            break;
        case type::BOOLEAN:
            v = o.as<bool>();
            break;
        case type::POSITIVE_INTEGER:
            v = o.as<uint64_t>();
            break;
        case type::NEGATIVE_INTEGER:
            v = o.as<int64_t>();
            break;
        case type::FLOAT32:
        case type::FLOAT64:
            v = o.as<double>();
            break;
        case type::STR:
            v = o.as<STR>();
            break;
        case type::BIN:
            v = o.as<BIN>();
            break;
        case type::EXT:
            v = o.as<EXT>();
            break;
        case type::ARRAY:
            v = o.as<std::vector<type::basic_variant<STR, BIN, EXT> > >();
            break;
        case type::MAP:
            v = o.as<std::multimap<type::basic_variant<STR, BIN, EXT>, type::basic_variant<STR, BIN, EXT> > >();
            break;
        default:
            break;
        }
        return o;
    }
};

namespace detail {

template <typename Stream>
struct pack_imp : boost::static_visitor<void> {
    template <typename T>
    void operator()(T const& value) const {
        pack<T>()(o_, value);
    }
    pack_imp(packer<Stream>& o):o_(o) {}
    packer<Stream>& o_;
};

} // namespace detail

template <typename STR, typename BIN, typename EXT>
struct pack<type::basic_variant<STR, BIN, EXT> > {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const type::basic_variant<STR, BIN, EXT>& v) const {
        boost::apply_visitor(detail::pack_imp<Stream>(o), v);
        return o;
    }
};

namespace detail {

struct object_imp : boost::static_visitor<void> {
    void operator()(msgpack::type::nil_t const& v) const {
        object<msgpack::type::nil_t>()(o_, v);
    }
    void operator()(bool const& v) const {
        object<bool>()(o_, v);
    }
    void operator()(uint64_t const& v) const {
        object<uint64_t>()(o_, v);
    }
    void operator()(int64_t const& v) const {
        object<int64_t>()(o_, v);
    }
    void operator()(double const& v) const {
        object<double>()(o_, v);
    }
    template <typename T>
    void operator()(T const&) const {
        throw msgpack::type_error();
    }
    object_imp(msgpack::object& o):o_(o) {}
    msgpack::object& o_;
};

} // namespace detail

template <typename STR, typename BIN, typename EXT>
struct object<type::basic_variant<STR, BIN, EXT> > {
    void operator()(msgpack::object& o, const type::basic_variant<STR, BIN, EXT>& v) const {
        boost::apply_visitor(detail::object_imp(o), v);
    }
};

namespace detail {

struct object_with_zone_imp : boost::static_visitor<void> {
    template <typename T>
    void operator()(T const& v) const {
        object_with_zone<T>()(o_, v);
    }
    object_with_zone_imp(msgpack::object::with_zone& o):o_(o) {}
    msgpack::object::with_zone& o_;
};

} // namespace detail

template <typename STR, typename BIN, typename EXT>
struct object_with_zone<type::basic_variant<STR, BIN, EXT> > {
    void operator()(msgpack::object::with_zone& o, const type::basic_variant<STR, BIN, EXT>& v) const {
        boost::apply_visitor(detail::object_with_zone_imp(o), v);
    }
};

} // namespace adaptor

/// @cond
} // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond

} // namespace msgpack

#endif // MSGPACK_USE_BOOST
#endif // MSGPACK_V1_TYPE_BOOST_MSGPACK_VARIANT_HPP
