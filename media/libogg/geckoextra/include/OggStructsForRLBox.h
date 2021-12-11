// -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vim: set ts=2 et sw=2 tw=80:
// This Source Code is subject to the terms of the Mozilla Public License
// version 2.0 (the "License"). You can obtain a copy of the License at
// http://mozilla.org/MPL/2.0/.

#ifndef OggStructsForRLBox_h__
#define OggStructsForRLBox_h__

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#elif defined(__GNUC__) || defined(__GNUG__)
// Can't turn off the variadic macro warning emitted from -pedantic
#  pragma GCC system_header
#elif defined(_MSC_VER)
// Doesn't seem to emit the warning
#else
// Don't know the compiler... just let it go through
#endif

#define sandbox_fields_reflection_ogg_class_ogg_packet(f, g, ...) \
  f(unsigned char *, packet    , FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(long           , bytes     , FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(long           , b_o_s     , FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(long           , e_o_s     , FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(long long      , granulepos, FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(long long      , packetno  , FIELD_NORMAL, ##__VA_ARGS__) g()

#define sandbox_fields_reflection_ogg_class_ogg_sync_state(f, g, ...) \
  f(unsigned char *, data       , FIELD_NORMAL, ##__VA_ARGS__) g()    \
  f(int            , storage    , FIELD_NORMAL, ##__VA_ARGS__) g()    \
  f(int            , fill       , FIELD_NORMAL, ##__VA_ARGS__) g()    \
  f(int            , returned   , FIELD_NORMAL, ##__VA_ARGS__) g()    \
  f(int            , unsynced   , FIELD_NORMAL, ##__VA_ARGS__) g()    \
  f(int            , headerbytes, FIELD_NORMAL, ##__VA_ARGS__) g()    \
  f(int            , bodybytes  , FIELD_NORMAL, ##__VA_ARGS__) g()

#define sandbox_fields_reflection_ogg_class_ogg_page(f, g, ...)   \
  f(unsigned char *, header    , FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(long           , header_len, FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(unsigned char *, body      , FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(long           , body_len  , FIELD_NORMAL, ##__VA_ARGS__) g()

#define sandbox_fields_reflection_ogg_class_ogg_stream_state(f, g, ...)    \
  f(unsigned char *    , body_data      , FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(long               , body_storage   , FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(long               , body_fill      , FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(long               , body_returned  , FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int *              , lacing_vals    , FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(ogg_int64_t *      , granule_vals   , FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(long               , lacing_storage , FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(long               , lacing_fill    , FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(long               , lacing_packet  , FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(long               , lacing_returned, FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(unsigned char [282], header         , FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int                , header_fill    , FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int                , e_o_s          , FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int                , b_o_s          , FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(long               , serialno       , FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(long               , pageno         , FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(ogg_int64_t        , packetno       , FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(ogg_int64_t        , granulepos     , FIELD_NORMAL, ##__VA_ARGS__) g()

#define sandbox_fields_reflection_ogg_allClasses(f, ...) \
  f(ogg_packet      , ogg, ##__VA_ARGS__)                \
  f(ogg_sync_state  , ogg, ##__VA_ARGS__)                \
  f(ogg_page        , ogg, ##__VA_ARGS__)                \
  f(ogg_stream_state, ogg, ##__VA_ARGS__)

#if defined(__clang__)
#  pragma clang diagnostic pop
#elif defined(__GNUC__) || defined(__GNUG__)
#elif defined(_MSC_VER)
#else
#endif

#endif