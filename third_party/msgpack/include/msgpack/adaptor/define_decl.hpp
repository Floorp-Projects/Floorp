//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2016 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_DEFINE_DECL_HPP
#define MSGPACK_DEFINE_DECL_HPP

// BOOST_PP_VARIADICS is defined in boost/preprocessor/config/config.hpp
// http://www.boost.org/libs/preprocessor/doc/ref/variadics.html
// However, supporting compiler detection is not complete. msgpack-c requires
// variadic macro arguments support. So BOOST_PP_VARIADICS is defined here explicitly.
#if !defined(MSGPACK_PP_VARIADICS)
#define MSGPACK_PP_VARIADICS
#endif

#include <msgpack/preprocessor.hpp>

#include "msgpack/versioning.hpp"

// for MSGPACK_ADD_ENUM
#include "msgpack/adaptor/int.hpp"

#define MSGPACK_DEFINE_ARRAY(...) \
    template <typename Packer> \
    void msgpack_pack(Packer& msgpack_pk) const \
    { \
        msgpack::type::make_define_array(__VA_ARGS__).msgpack_pack(msgpack_pk); \
    } \
    void msgpack_unpack(msgpack::object const& msgpack_o) \
    { \
        msgpack::type::make_define_array(__VA_ARGS__).msgpack_unpack(msgpack_o); \
    }\
    template <typename MSGPACK_OBJECT> \
    void msgpack_object(MSGPACK_OBJECT* msgpack_o, msgpack::zone& msgpack_z) const \
    { \
        msgpack::type::make_define_array(__VA_ARGS__).msgpack_object(msgpack_o, msgpack_z); \
    }

#define MSGPACK_BASE_ARRAY(base) (*const_cast<base *>(static_cast<base const*>(this)))
#define MSGPACK_NVP(name, value) (name) (value)

#define MSGPACK_DEFINE_MAP_EACH_PROC(r, data, elem) \
    MSGPACK_PP_IF( \
        MSGPACK_PP_IS_BEGIN_PARENS(elem), \
        elem, \
        (MSGPACK_PP_STRINGIZE(elem))(elem) \
    )

#define MSGPACK_DEFINE_MAP_IMPL(...) \
    MSGPACK_PP_SEQ_TO_TUPLE( \
        MSGPACK_PP_SEQ_FOR_EACH( \
            MSGPACK_DEFINE_MAP_EACH_PROC, \
            0, \
            MSGPACK_PP_VARIADIC_TO_SEQ(__VA_ARGS__) \
        ) \
    )

#define MSGPACK_DEFINE_MAP(...) \
    template <typename Packer> \
    void msgpack_pack(Packer& msgpack_pk) const \
    { \
        msgpack::type::make_define_map \
            MSGPACK_DEFINE_MAP_IMPL(__VA_ARGS__) \
            .msgpack_pack(msgpack_pk); \
    } \
    void msgpack_unpack(msgpack::object const& msgpack_o) \
    { \
        msgpack::type::make_define_map \
            MSGPACK_DEFINE_MAP_IMPL(__VA_ARGS__) \
            .msgpack_unpack(msgpack_o); \
    }\
    template <typename MSGPACK_OBJECT> \
    void msgpack_object(MSGPACK_OBJECT* msgpack_o, msgpack::zone& msgpack_z) const \
    { \
        msgpack::type::make_define_map \
            MSGPACK_DEFINE_MAP_IMPL(__VA_ARGS__) \
            .msgpack_object(msgpack_o, msgpack_z); \
    }

#define MSGPACK_BASE_MAP(base) \
    (MSGPACK_PP_STRINGIZE(base))(*const_cast<base *>(static_cast<base const*>(this)))

// MSGPACK_ADD_ENUM must be used in the global namespace.
#define MSGPACK_ADD_ENUM(enum_name) \
  namespace msgpack { \
  /** @cond */ \
  MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS) { \
  /** @endcond */ \
  namespace adaptor { \
    template<> \
    struct convert<enum_name> { \
      msgpack::object const& operator()(msgpack::object const& msgpack_o, enum_name& msgpack_v) const { \
        msgpack::underlying_type<enum_name>::type tmp; \
        msgpack::operator>>(msgpack_o, tmp);                   \
        msgpack_v = static_cast<enum_name>(tmp);   \
        return msgpack_o; \
      } \
    }; \
    template<> \
    struct object<enum_name> { \
      void operator()(msgpack::object& msgpack_o, const enum_name& msgpack_v) const { \
        msgpack::underlying_type<enum_name>::type tmp = static_cast<msgpack::underlying_type<enum_name>::type>(msgpack_v); \
        msgpack::operator<<(msgpack_o, tmp);                                    \
      } \
    }; \
    template<> \
    struct object_with_zone<enum_name> { \
      void operator()(msgpack::object::with_zone& msgpack_o, const enum_name& msgpack_v) const {  \
        msgpack::underlying_type<enum_name>::type tmp = static_cast<msgpack::underlying_type<enum_name>::type>(msgpack_v); \
        msgpack::operator<<(msgpack_o, tmp);                                    \
      } \
    }; \
    template <> \
    struct pack<enum_name> { \
      template <typename Stream> \
      msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& msgpack_o, const enum_name& msgpack_v) const { \
          return msgpack::operator<<(msgpack_o, static_cast<msgpack::underlying_type<enum_name>::type>(msgpack_v)); \
      } \
    }; \
  } \
  /** @cond */ \
  } \
  /** @endcond */ \
  }

#if defined(MSGPACK_USE_DEFINE_MAP)
#define MSGPACK_DEFINE MSGPACK_DEFINE_MAP
#define MSGPACK_BASE MSGPACK_BASE_MAP
#else  // defined(MSGPACK_USE_DEFINE_MAP)
#define MSGPACK_DEFINE MSGPACK_DEFINE_ARRAY
#define MSGPACK_BASE MSGPACK_BASE_ARRAY
#endif // defined(MSGPACK_USE_DEFINE_MAP)


#include "msgpack/v1/adaptor/define_decl.hpp"
#include "msgpack/v2/adaptor/define_decl.hpp"
#include "msgpack/v3/adaptor/define_decl.hpp"

#endif // MSGPACK_DEFINE_DECL_HPP
