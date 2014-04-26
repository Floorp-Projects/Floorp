/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS allows using a typed enum as bit flags. */

#ifndef mozilla_TypedEnumBits_h
#define mozilla_TypedEnumBits_h

#include "mozilla/IntegerTypeTraits.h"
#include "mozilla/TypedEnumInternal.h"

namespace mozilla {

#define MOZ_CASTABLETYPEDENUMRESULT_BINOP(Op, OtherType, ReturnType) \
template<typename E> \
MOZ_CONSTEXPR ReturnType \
operator Op(const OtherType& e, const CastableTypedEnumResult<E>& r) \
{ \
  return ReturnType(e Op OtherType(r)); \
} \
template<typename E> \
MOZ_CONSTEXPR ReturnType \
operator Op(const CastableTypedEnumResult<E>& r, const OtherType& e) \
{ \
  return ReturnType(OtherType(r) Op e); \
} \
template<typename E> \
MOZ_CONSTEXPR ReturnType \
operator Op(const CastableTypedEnumResult<E>& r1, \
            const CastableTypedEnumResult<E>& r2) \
{ \
  return ReturnType(OtherType(r1) Op OtherType(r2)); \
}

MOZ_CASTABLETYPEDENUMRESULT_BINOP(|, E, CastableTypedEnumResult<E>)
MOZ_CASTABLETYPEDENUMRESULT_BINOP(&, E, CastableTypedEnumResult<E>)
MOZ_CASTABLETYPEDENUMRESULT_BINOP(^, E, CastableTypedEnumResult<E>)
MOZ_CASTABLETYPEDENUMRESULT_BINOP(==, E, bool)
MOZ_CASTABLETYPEDENUMRESULT_BINOP(!=, E, bool)
MOZ_CASTABLETYPEDENUMRESULT_BINOP(||, bool, bool)
MOZ_CASTABLETYPEDENUMRESULT_BINOP(&&, bool, bool)

template <typename E>
MOZ_CONSTEXPR CastableTypedEnumResult<E>
operator ~(const CastableTypedEnumResult<E>& r)
{
  return CastableTypedEnumResult<E>(~(E(r)));
}

#define MOZ_CASTABLETYPEDENUMRESULT_COMPOUND_ASSIGN_OP(Op) \
template<typename E> \
E& \
operator Op(E& r1, \
            const CastableTypedEnumResult<E>& r2) \
{ \
  return r1 Op E(r2); \
}

MOZ_CASTABLETYPEDENUMRESULT_COMPOUND_ASSIGN_OP(&=)
MOZ_CASTABLETYPEDENUMRESULT_COMPOUND_ASSIGN_OP(|=)
MOZ_CASTABLETYPEDENUMRESULT_COMPOUND_ASSIGN_OP(^=)

#undef MOZ_CASTABLETYPEDENUMRESULT_COMPOUND_ASSIGN_OP

#undef MOZ_CASTABLETYPEDENUMRESULT_BINOP

#ifndef MOZ_HAVE_CXX11_STRONG_ENUMS

#define MOZ_CASTABLETYPEDENUMRESULT_BINOP_EXTRA_NON_CXX11(Op, ReturnType) \
template<typename E> \
MOZ_CONSTEXPR ReturnType \
operator Op(typename E::Enum e, const CastableTypedEnumResult<E>& r) \
{ \
  return ReturnType(e Op E(r)); \
} \
template<typename E> \
MOZ_CONSTEXPR ReturnType \
operator Op(const CastableTypedEnumResult<E>& r, typename E::Enum e) \
{ \
  return ReturnType(E(r) Op e); \
}

MOZ_CASTABLETYPEDENUMRESULT_BINOP_EXTRA_NON_CXX11(|, CastableTypedEnumResult<E>)
MOZ_CASTABLETYPEDENUMRESULT_BINOP_EXTRA_NON_CXX11(&, CastableTypedEnumResult<E>)
MOZ_CASTABLETYPEDENUMRESULT_BINOP_EXTRA_NON_CXX11(^, CastableTypedEnumResult<E>)
MOZ_CASTABLETYPEDENUMRESULT_BINOP_EXTRA_NON_CXX11(==, bool)
MOZ_CASTABLETYPEDENUMRESULT_BINOP_EXTRA_NON_CXX11(!=, bool)

#undef MOZ_CASTABLETYPEDENUMRESULT_BINOP_EXTRA_NON_CXX11

#endif // not MOZ_HAVE_CXX11_STRONG_ENUMS

namespace detail {
template<typename E>
struct UnsignedIntegerTypeForEnum
  : UnsignedStdintTypeForSize<sizeof(E)>
{};
}

} // namespace mozilla

#define MOZ_MAKE_ENUM_CLASS_BINOP_IMPL(Name, Op) \
   inline MOZ_CONSTEXPR mozilla::CastableTypedEnumResult<Name> \
   operator Op(Name a, Name b) \
   { \
     typedef mozilla::CastableTypedEnumResult<Name> Result; \
     typedef mozilla::detail::UnsignedIntegerTypeForEnum<Name>::Type U; \
     return Result(Name(U(a) Op U(b))); \
   } \
 \
   inline Name& \
   operator Op##=(Name& a, Name b) \
   { \
     return a = a Op b; \
   }

#define MOZ_MAKE_ENUM_CLASS_OPS_IMPL(Name) \
   MOZ_MAKE_ENUM_CLASS_BINOP_IMPL(Name, |) \
   MOZ_MAKE_ENUM_CLASS_BINOP_IMPL(Name, &) \
   MOZ_MAKE_ENUM_CLASS_BINOP_IMPL(Name, ^) \
   inline MOZ_CONSTEXPR mozilla::CastableTypedEnumResult<Name> \
   operator~(Name a) \
   { \
     typedef mozilla::CastableTypedEnumResult<Name> Result; \
     typedef mozilla::detail::UnsignedIntegerTypeForEnum<Name>::Type U; \
     return Result(Name(~(U(a)))); \
   }

#ifndef MOZ_HAVE_CXX11_STRONG_ENUMS
#  define MOZ_MAKE_ENUM_CLASS_BITWISE_BINOP_EXTRA_NON_CXX11(Name, Op) \
     inline MOZ_CONSTEXPR mozilla::CastableTypedEnumResult<Name> \
     operator Op(Name a, Name::Enum b) \
     { \
       return a Op Name(b); \
     } \
 \
     inline MOZ_CONSTEXPR mozilla::CastableTypedEnumResult<Name> \
     operator Op(Name::Enum a, Name b) \
     { \
       return Name(a) Op b; \
     } \
 \
     inline MOZ_CONSTEXPR mozilla::CastableTypedEnumResult<Name> \
     operator Op(Name::Enum a, Name::Enum b) \
     { \
       return Name(a) Op Name(b); \
     } \
 \
     inline Name& \
     operator Op##=(Name& a, Name::Enum b) \
     { \
       return a = a Op Name(b); \
    }

#  define MOZ_MAKE_ENUM_CLASS_OPS_EXTRA_NON_CXX11(Name) \
     MOZ_MAKE_ENUM_CLASS_BITWISE_BINOP_EXTRA_NON_CXX11(Name, |) \
     MOZ_MAKE_ENUM_CLASS_BITWISE_BINOP_EXTRA_NON_CXX11(Name, &) \
     MOZ_MAKE_ENUM_CLASS_BITWISE_BINOP_EXTRA_NON_CXX11(Name, ^) \
     inline MOZ_CONSTEXPR mozilla::CastableTypedEnumResult<Name> \
     operator~(Name::Enum a) \
     { \
       return ~(Name(a)); \
     }
#endif

/**
 * MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS generates standard bitwise operators
 * for the given enum type. Use this to enable using an enum type as bit-field.
 */
#ifdef MOZ_HAVE_CXX11_STRONG_ENUMS
#  define MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(Name) \
     MOZ_MAKE_ENUM_CLASS_OPS_IMPL(Name)
#else
#  define MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(Name) \
     MOZ_MAKE_ENUM_CLASS_OPS_IMPL(Name) \
     MOZ_MAKE_ENUM_CLASS_OPS_EXTRA_NON_CXX11(Name)
#endif

#endif // mozilla_TypedEnumBits_h
