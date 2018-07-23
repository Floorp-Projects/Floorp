/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_CallFunction_h
#define mozilla_recordreplay_CallFunction_h

namespace mozilla {
namespace recordreplay {

// These macros define functions for calling a void* function pointer with
// a particular ABI and arbitrary arguments. In principle we could do this
// with varargs (i.e. cast to 'int (ABI *)(...)' before calling), but MSVC
// treats 'int (__stdcall *)(...)' as 'int (__cdecl *)(...)', unfortunately.
//
// After instantiating DefineAllCallFunctions, the resulting functions will
// be overloaded and have the form, for a given ABI:
//
// template <typename ReturnType>
// ReturnType CallFunctionABI(void* fn);
//
// template <typename ReturnType, typename T0>
// ReturnType CallFunctionABI(void* fn, T0 a0);
//
// template <typename ReturnType, typename T0, typename T1>
// ReturnType CallFunctionABI(void* fn, T0 a0, T1 a1);
//
// And so forth.
#define DefineCallFunction(aABI, aReturnType, aFormals, aFormalTypes, aActuals) \
  static inline aReturnType CallFunction ##aABI aFormals {              \
    MOZ_ASSERT(aFn);                                                    \
    return BitwiseCast<aReturnType (aABI *) aFormalTypes>(aFn) aActuals; \
  }
#define DefineAllCallFunctions(aABI)                                  \
  template <typename ReturnType>                                      \
  DefineCallFunction(aABI, ReturnType, (void* aFn), (), ())           \
  template <typename ReturnType, typename T0>                         \
  DefineCallFunction(aABI, ReturnType,                                \
                     (void* aFn, T0 a0), (T0), (a0))                  \
  template <typename ReturnType, typename T0, typename T1>            \
  DefineCallFunction(aABI, ReturnType,                                \
                     (void* aFn, T0 a0, T1 a1), (T0, T1), (a0, a1))   \
  template <typename ReturnType, typename T0, typename T1, typename T2> \
  DefineCallFunction(aABI, ReturnType,                                \
                     (void* aFn, T0 a0, T1 a1, T2 a2),                \
                     (T0, T1, T2), (a0, a1, a2))                      \
  template <typename ReturnType, typename T0, typename T1, typename T2, typename T3> \
  DefineCallFunction(aABI, ReturnType,                                \
                     (void* aFn, T0 a0, T1 a1, T2 a2, T3 a3),         \
                     (T0, T1, T2, T3),                                \
                     (a0, a1, a2, a3))                                \
  template <typename ReturnType, typename T0, typename T1, typename T2, typename T3, \
            typename T4>                                              \
  DefineCallFunction(aABI, ReturnType,                                \
                     (void* aFn, T0 a0, T1 a1, T2 a2, T3 a3, T4 a4),  \
                     (T0, T1, T2, T3, T4),                            \
                     (a0, a1, a2, a3, a4))                            \
  template <typename ReturnType, typename T0, typename T1, typename T2, typename T3, \
            typename T4, typename T5>                                 \
  DefineCallFunction(aABI, ReturnType,                                \
                     (void* aFn, T0 a0, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5), \
                     (T0, T1, T2, T3, T4, T5),                        \
                     (a0, a1, a2, a3, a4, a5))                        \
  template <typename ReturnType, typename T0, typename T1, typename T2, typename T3, \
            typename T4, typename T5, typename T6>                    \
  DefineCallFunction(aABI, ReturnType,                                \
                     (void* aFn, T0 a0, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, \
                      T6 a6),                                         \
                     (T0, T1, T2, T3, T4, T5, T6),                    \
                     (a0, a1, a2, a3, a4, a5, a6))                    \
  template <typename ReturnType, typename T0, typename T1, typename T2, typename T3, \
            typename T4, typename T5, typename T6, typename T7>       \
  DefineCallFunction(aABI, ReturnType,                                \
                     (void* aFn, T0 a0, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, \
                      T6 a6, T7 a7),                                  \
                     (T0, T1, T2, T3, T4, T5, T6, T7),                \
                     (a0, a1, a2, a3, a4, a5, a6, a7))                \
  template <typename ReturnType, typename T0, typename T1, typename T2, typename T3, \
            typename T4, typename T5, typename T6, typename T7,       \
            typename T8>                                              \
  DefineCallFunction(aABI, ReturnType,                                \
                     (void* aFn, T0 a0, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, \
                      T6 a6, T7 a7, T8 a8),                           \
                     (T0, T1, T2, T3, T4, T5, T6, T7, T8),            \
                     (a0, a1, a2, a3, a4, a5, a6, a7, a8))            \
  template <typename ReturnType, typename T0, typename T1, typename T2, typename T3, \
            typename T4, typename T5, typename T6, typename T7,       \
            typename T8, typename T9>                                 \
  DefineCallFunction(aABI, ReturnType,                                \
                     (void* aFn, T0 a0, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, \
                      T6 a6, T7 a7, T8 a8, T9 a9),                    \
                     (T0, T1, T2, T3, T4, T5, T6, T7, T8, T9),        \
                     (a0, a1, a2, a3, a4, a5, a6, a7, a8, a9))        \
  template <typename ReturnType, typename T0, typename T1, typename T2, typename T3, \
            typename T4, typename T5, typename T6, typename T7,       \
            typename T8, typename T9, typename T10>                   \
  DefineCallFunction(aABI, ReturnType,                                \
                     (void* aFn, T0 a0, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, \
                      T6 a6, T7 a7, T8 a8, T9 a9, T10 a10),           \
                     (T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10),   \
                     (a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10))   \
  template <typename ReturnType, typename T0, typename T1, typename T2, typename T3, \
            typename T4, typename T5, typename T6, typename T7,       \
            typename T8, typename T9, typename T10, typename T11>     \
  DefineCallFunction(aABI, ReturnType,                                \
                     (void* aFn, T0 a0, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, \
                      T6 a6, T7 a7, T8 a8, T9 a9, T10 a10, T11 a11),  \
                     (T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11), \
                     (a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11))

} // recordreplay
} // mozilla

#endif // mozilla_recordreplay_CallFunction_h
