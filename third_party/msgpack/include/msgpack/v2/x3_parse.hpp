//
// MessagePack for C++ deserializing routine
//
// Copyright (C) 2017 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V2_X3_PARSE_HPP
#define MSGPACK_V2_X3_PARSE_HPP

#if defined(MSGPACK_USE_X3_PARSE)

#include <boost/version.hpp>

#if BOOST_VERSION >= 106100

#include "msgpack/versioning.hpp"
#include "msgpack/x3_parse_decl.hpp"

#if __GNUC__ >= 4
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif // __GNUC__ >= 4

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/binary.hpp>

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v2) {
/// @endcond

namespace detail {

namespace x3 = boost::spirit::x3;

using x3::byte_;

// byte range utility
const auto byte_range = [](const std::uint8_t from, const std::uint8_t to) {
    const auto check = [from, to](auto& ctx)
    {
        const std::uint8_t value = x3::_attr(ctx);
        x3::_val(ctx) = value;
        x3::_pass(ctx) = from <= value && value <= to;
    };
    return x3::byte_ [check];
};

// MessagePack rule
const auto mp_positive_fixint = byte_range(0x00, 0x7f);
const auto mp_fixmap          = byte_range(0x80, 0x8f);
const auto mp_fixarray        = byte_range(0x90, 0x9f);
const auto mp_fixstr          = byte_range(0xa0, 0xbf);
const auto mp_nil             = x3::byte_(0xc0);
const auto mp_false           = x3::byte_(0xc2);
const auto mp_true            = x3::byte_(0xc3);
const auto mp_bin8            = x3::byte_(0xc4);
const auto mp_bin16           = x3::byte_(0xc5);
const auto mp_bin32           = x3::byte_(0xc6);
const auto mp_ext8            = x3::byte_(0xc7);
const auto mp_ext16           = x3::byte_(0xc8);
const auto mp_ext32           = x3::byte_(0xc9);
const auto mp_float32         = x3::byte_(0xca);
const auto mp_float64         = x3::byte_(0xcb);
const auto mp_uint8           = x3::byte_(0xcc);
const auto mp_uint16          = x3::byte_(0xcd);
const auto mp_uint32          = x3::byte_(0xce);
const auto mp_uint64          = x3::byte_(0xcf);
const auto mp_int8            = x3::byte_(0xd0);
const auto mp_int16           = x3::byte_(0xd1);
const auto mp_int32           = x3::byte_(0xd2);
const auto mp_int64           = x3::byte_(0xd3);
const auto mp_fixext1         = x3::byte_(0xd4);
const auto mp_fixext2         = x3::byte_(0xd5);
const auto mp_fixext4         = x3::byte_(0xd6);
const auto mp_fixext8         = x3::byte_(0xd7);
const auto mp_fixext16        = x3::byte_(0xd8);
const auto mp_str8            = x3::byte_(0xd9);
const auto mp_str16           = x3::byte_(0xda);
const auto mp_str32           = x3::byte_(0xdb);
const auto mp_array16         = x3::byte_(0xdc);
const auto mp_array32         = x3::byte_(0xdd);
const auto mp_map16           = x3::byte_(0xde);
const auto mp_map32           = x3::byte_(0xdf);
const auto mp_negative_fixint = byte_range(0xe0, 0xff);

const auto mp_d_uint8 = x3::byte_;
const auto mp_d_uint16 = x3::big_word;
const auto mp_d_uint32 = x3::big_dword;
const auto mp_d_uint64 = x3::big_qword;

const auto mp_d_int8 = x3::byte_;
const auto mp_d_int16 = x3::big_word;
const auto mp_d_int32 = x3::big_dword;
const auto mp_d_int64 = x3::big_qword;

x3::rule<class mp_object> const mp_object("mp_object");
x3::rule<class array_items> const array_item("array_item");
x3::rule<class map_items> const map_item("map_item");
x3::rule<class kv> const kv("kv");

struct tag_app_specific {};
struct index_size {
    enum struct type_t {
        array,
        map,
        other
    };
    index_size(std::size_t size, type_t type = type_t::other):size(size), type(type) {}
    std::size_t index = 0;
    std::size_t size;
    type_t type;
};

template <typename Visitor>
struct app_specific {
    template <typename Vis>
    app_specific(Vis&& vis):vis(vis) {}
    std::vector<index_size> index_sizes;
    Visitor vis;
};

template <typename Visitor>
app_specific<Visitor> make_app_specific(Visitor&& vis) {
    return app_specific<Visitor>(std::forward<Visitor>(vis));
}

const auto more   = [](auto &ctx) {
    auto& app_specific = x3::get<tag_app_specific>(ctx).get();
    _pass(ctx) = app_specific.index_sizes.back().index++ < app_specific.index_sizes.back().size;
};

const auto done   = [](auto &ctx) {
    auto& app_specific = x3::get<tag_app_specific>(ctx).get();
    if (app_specific.index_sizes.back().index == app_specific.index_sizes.back().size + 1) {
        _pass(ctx) = true;
        switch (app_specific.index_sizes.back().type) {
        case index_size::type_t::array:
            app_specific.vis.end_array();
            break;
        case index_size::type_t::map:
            app_specific.vis.end_map();
            break;
        case index_size::type_t::other:
            break;
        }
        app_specific.index_sizes.pop_back();
    }
    else {
        _pass(ctx) = false;
    }
};

const auto mp_object_def =
    // -----------------------------------------------
    mp_nil [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.vis.visit_nil();
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_true [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.vis.visit_boolean(true);
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_false [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.vis.visit_boolean(false);
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_positive_fixint [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.vis.visit_positive_integer(_attr(ctx));
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_negative_fixint [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                std::int8_t val = _attr(ctx);
                app_specific.vis.visit_negative_integer(val);
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_uint8 >> mp_d_uint8 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.vis.visit_negative_integer(_attr(ctx));
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_uint16 >> mp_d_uint16 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.vis.visit_positive_integer(_attr(ctx));
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_uint32 >> mp_d_uint32 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.vis.visit_positive_integer(_attr(ctx));
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_uint64 >> mp_d_uint64 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.vis.visit_positive_integer(_attr(ctx));
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_int8 >> mp_d_int8 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                std::int8_t val = _attr(ctx);
                app_specific.vis.visit_negative_integer(val);
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_int16 >> mp_d_int16 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                std::int16_t val = _attr(ctx);
                app_specific.vis.visit_negative_integer(val);
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_int32 >> mp_d_int32 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                std::int32_t val = _attr(ctx);
                app_specific.vis.visit_negative_integer(val);
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_int64 >> mp_d_int64 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                std::int64_t val = _attr(ctx);
                app_specific.vis.visit_negative_integer(val);
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_float32 >> mp_d_uint32 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                union { uint32_t i; float f; } mem = { _attr(ctx) };
                app_specific.vis.visit_float32(mem.f);
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_float64 >> mp_d_uint64 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                union { uint64_t i; double f; } mem = { _attr(ctx) };
#if defined(TARGET_OS_IPHONE)
                // ok
#elif defined(__arm__) && !(__ARM_EABI__) // arm-oabi
                // https://github.com/msgpack/msgpack-perl/pull/1
                mem.i = (mem.i & 0xFFFFFFFFUL) << 32UL | (mem.i >> 32UL);
#endif
                app_specific.vis.visit_float64(mem.f);
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_fixstr [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                std::size_t size = _attr(ctx) & 0b00011111;
                app_specific.index_sizes.emplace_back(size);
            }
        )
    ]
    >>
    x3::raw [
        *(x3::eps [more] >> x3::char_)
        >> x3::eps [done]
    ][
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                auto const& str = _attr(ctx);
                std::size_t size = std::distance(str.begin(), str.end());
                app_specific.vis.visit_str(size ? &str.front() : nullptr, size);
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_str8 >> mp_d_uint8 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.index_sizes.emplace_back(_attr(ctx));
            }
        )
    ]
    >>
    x3::raw [
        *(x3::eps [more] >> x3::char_)
        >> x3::eps [done]
    ][
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                auto const& str = _attr(ctx);
                std::size_t size = std::distance(str.begin(), str.end());
                app_specific.vis.visit_str(size ? &str.front() : nullptr, size);
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_str16 >> mp_d_uint16 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.index_sizes.emplace_back(_attr(ctx));
            }
        )
    ]
    >>
    x3::raw [
        *(x3::eps [more] >> x3::char_)
        >> x3::eps [done]
    ][
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                auto const& str = _attr(ctx);
                std::size_t size = std::distance(str.begin(), str.end());
                app_specific.vis.visit_str(size ? &str.front() : nullptr, size);
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_str32 >> mp_d_uint32 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.index_sizes.emplace_back(_attr(ctx));
            }
        )
    ]
    >>
    x3::raw [
        *(x3::eps [more] >> x3::char_)
        >> x3::eps [done]
    ][
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                auto const& str = _attr(ctx);
                std::size_t size = std::distance(str.begin(), str.end());
                app_specific.vis.visit_str(size ? &str.front() : nullptr, size);
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_bin8 >> mp_d_uint8 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.index_sizes.emplace_back(_attr(ctx));
            }
        )
    ]
    >>
    x3::raw [
        *(x3::eps [more] >> x3::char_)
        >> x3::eps [done]
    ][
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                auto const& bin = _attr(ctx);
                std::size_t size = std::distance(bin.begin(), bin.end());
                app_specific.vis.visit_bin(size ? &bin.front() : nullptr, size);
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_bin16 >> mp_d_uint16 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.index_sizes.emplace_back(_attr(ctx));
            }
        )
    ]
    >>
    x3::raw [
        *(x3::eps [more] >> x3::char_)
        >> x3::eps [done]
    ][
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                auto const& bin = _attr(ctx);
                std::size_t size = std::distance(bin.begin(), bin.end());
                app_specific.vis.visit_bin(size ? &bin.front() : nullptr, size);
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_bin32 >> mp_d_uint32 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.index_sizes.emplace_back(_attr(ctx));
            }
        )
    ]
    >>
    x3::raw [
        *(x3::eps [more] >> x3::char_)
        >> x3::eps [done]
    ][
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                auto const& bin = _attr(ctx);
                std::size_t size = std::distance(bin.begin(), bin.end());
                app_specific.vis.visit_bin(size ? &bin.front() : nullptr, size);
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_fixarray [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                std::size_t size = _attr(ctx) & 0b00001111;
                app_specific.index_sizes.emplace_back(size, index_size::type_t::array);
                app_specific.vis.start_array(size);
            }
        )
    ]
    >> *(x3::eps [more] >> array_item)
    >> x3::eps [done]
    |
    // -----------------------------------------------
    mp_array16 >> mp_d_uint16 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                std::size_t size = _attr(ctx);
                app_specific.index_sizes.emplace_back(size, index_size::type_t::array);
                app_specific.vis.start_array(size);
            }
        )
    ]
    >> *(x3::eps [more] >> array_item)
    >> x3::eps [done]
    |
    // -----------------------------------------------
    mp_array32 >> mp_d_uint32 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                std::size_t size = _attr(ctx);
                app_specific.index_sizes.emplace_back(size, index_size::type_t::array);
                app_specific.vis.start_array(size);
            }
        )
    ]
    >> *(x3::eps [more] >> array_item)
    >> x3::eps [done]
    |
    // -----------------------------------------------
    mp_fixmap [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                std::size_t size = _attr(ctx) & 0b00001111;
                app_specific.index_sizes.emplace_back(size, index_size::type_t::map);
                app_specific.vis.start_map(size);
            }
        )
    ]
    >> *(x3::eps [more] >> map_item)
    >> x3::eps [done]
    |
    // -----------------------------------------------
    mp_map16 >> mp_d_uint16 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                std::size_t size = _attr(ctx);
                app_specific.index_sizes.emplace_back(size, index_size::type_t::map);
                app_specific.vis.start_map(size);
            }
        )
    ]
    >> *(x3::eps [more] >> map_item)
    >> x3::eps [done]
    |
    // -----------------------------------------------
    mp_map32 >> mp_d_uint32 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                std::size_t size = _attr(ctx);
                app_specific.index_sizes.emplace_back(size, index_size::type_t::map);
                app_specific.vis.start_map(size);
            }
        )
    ]
    >> *(x3::eps [more] >> map_item)
    >> x3::eps [done]
    |
    // -----------------------------------------------
    mp_fixext1 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.index_sizes.emplace_back(1+1);
            }
        )
    ]
    >>
    x3::raw [
        *(x3::eps [more] >> x3::char_)
        >> x3::eps [done]
    ][
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                auto const& ext = _attr(ctx);
                std::size_t size = std::distance(ext.begin(), ext.end());
                app_specific.vis.visit_ext(size ? &ext.front() : nullptr, size);
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_fixext2 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.index_sizes.emplace_back(2+1);
            }
        )
    ]
    >>
    x3::raw [
        *(x3::eps [more] >> x3::char_)
        >> x3::eps [done]
    ][
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                auto const& ext = _attr(ctx);
                std::size_t size = std::distance(ext.begin(), ext.end());
                app_specific.vis.visit_ext(size ? &ext.front() : nullptr, size);
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_fixext4 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.index_sizes.emplace_back(4+1);
            }
        )
    ]
    >>
    x3::raw [
        *(x3::eps [more] >> x3::char_)
        >> x3::eps [done]
    ][
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                auto const& ext = _attr(ctx);
                std::size_t size = std::distance(ext.begin(), ext.end());
                app_specific.vis.visit_ext(size ? &ext.front() : nullptr, size);
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_fixext8 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.index_sizes.emplace_back(8+1);
            }
        )
    ]
    >>
    x3::raw [
        *(x3::eps [more] >> x3::char_)
        >> x3::eps [done]
    ][
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                auto const& ext = _attr(ctx);
                std::size_t size = std::distance(ext.begin(), ext.end());
                app_specific.vis.visit_ext(size ? &ext.front() : nullptr, size);
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_fixext16 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.index_sizes.emplace_back(16+1);
            }
        )
    ]
    >>
    x3::raw [
        *(x3::eps [more] >> x3::char_)
        >> x3::eps [done]
    ][
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                auto const& ext = _attr(ctx);
                std::size_t size = std::distance(ext.begin(), ext.end());
                app_specific.vis.visit_ext(size ? &ext.front() : nullptr, size);
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_ext8 >> mp_d_uint8 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.index_sizes.emplace_back(_attr(ctx)+1);
            }
        )
    ]
    >>
    x3::raw [
        *(x3::eps [more] >> x3::char_)
        >> x3::eps [done]
    ][
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                auto const& ext = _attr(ctx);
                std::size_t size = std::distance(ext.begin(), ext.end());
                app_specific.vis.visit_ext(size ? &ext.front() : nullptr, size);
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_ext16 >> mp_d_uint16 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.index_sizes.emplace_back(_attr(ctx)+1);
            }
        )
    ]
    >>
    x3::raw [
        *(x3::eps [more] >> x3::char_)
        >> x3::eps [done]
    ][
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                auto const& ext = _attr(ctx);
                std::size_t size = std::distance(ext.begin(), ext.end());
                app_specific.vis.visit_ext(size ? &ext.front() : nullptr, size);
            }
        )
    ]
    |
    // -----------------------------------------------
    mp_ext32 >> mp_d_uint32 [
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.index_sizes.emplace_back(_attr(ctx)+1);
            }
        )
    ]
    >>
    x3::raw [
        *(x3::eps [more] >> x3::char_)
        >> x3::eps [done]
    ][
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                auto const& ext = _attr(ctx);
                std::size_t size = std::distance(ext.begin(), ext.end());
                app_specific.vis.visit_ext(size ? &ext.front() : nullptr, size);
            }
        )
    ];

const auto array_item_def =
    x3::eps[
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.vis.start_array_item();
                _pass(ctx) = true;
            }
        )
    ]
    >>
    mp_object
    >>
    x3::eps[
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.vis.end_array_item();
                _pass(ctx) = true;
            }
        )
    ];

const auto map_item_def = kv;
const auto kv_def =
    x3::eps[
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.vis.start_map_key();
                _pass(ctx) = true;
            }
        )
    ]
    >>
    mp_object
    >>
    x3::eps[
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.vis.end_map_key();
                _pass(ctx) = true;
            }
        )
    ]
    >>
    x3::eps[
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.vis.start_map_value();
                _pass(ctx) = true;
            }
        )
    ]
    >>
    mp_object
    >>
    x3::eps[
        (
            [](auto& ctx){
                auto& app_specific = x3::get<tag_app_specific>(ctx).get();
                app_specific.vis.end_map_value();
                _pass(ctx) = true;
            }
        )
    ];

BOOST_SPIRIT_DEFINE(
    mp_object, array_item, map_item, kv
);

const auto rule = mp_object;

} // namespace detail

template <typename Iterator, typename Visitor>
inline bool parse(Iterator&& begin, Iterator&& end, Visitor&& vis) {
    auto data = detail::make_app_specific(std::forward<Visitor>(vis));
    return detail::x3::parse(
        std::forward<Iterator>(begin),
        std::forward<Iterator>(end),
        detail::x3::with<detail::tag_app_specific>(std::ref(data))[detail::rule]
    );
}

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v2)
/// @endcond

}  // namespace msgpack

#if __GNUC__ >= 4
#pragma GCC diagnostic pop
#endif // __GNUC__ >= 4

#else  // BOOST_VERSION >= 106100

#error Boost 1.61.0 or later is required to use x3 parse

#endif // BOOST_VERSION >= 106100

#endif // defined(MSGPACK_USE_X3_PARSE)

#endif // MSGPACK_V2_X3_PARSE_HPP
