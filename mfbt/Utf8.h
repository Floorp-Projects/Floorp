/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * UTF-8-related functionality, including a type-safe structure representing a
 * UTF-8 code unit.
 */

#ifndef mozilla_Utf8_h
#define mozilla_Utf8_h

#include "mozilla/Types.h" // for MFBT_API

#include <limits.h> // for CHAR_BIT
#include <stddef.h> // for size_t
#include <stdint.h> // for uint8_t

namespace mozilla {

union Utf8Unit;

static_assert(CHAR_BIT == 8,
              "Utf8Unit won't work so well with non-octet chars");

/**
 * A code unit within a UTF-8 encoded string.  (A code unit is the smallest
 * unit within the Unicode encoding of a string.  For UTF-8 this is an 8-bit
 * number; for UTF-16 it would be a 16-bit number.)
 *
 * This is *not* the same as a single code point: in UTF-8, non-ASCII code
 * points are constituted by multiple code units.
 */
union Utf8Unit
{
private:
  // Utf8Unit is a union wrapping a raw |char|.  The C++ object model and C++
  // requirements as to how objects may be accessed with respect to their actual
  // types (almost?) uniquely compel this choice.
  //
  // Our requirements for a UTF-8 code unit representation are:
  //
  //   1. It must be "compatible" with C++ character/string literals that use
  //      the UTF-8 encoding.  Given a properly encoded C++ literal, you should
  //      be able to use |Utf8Unit| and friends to access it; given |Utf8Unit|
  //      and friends (particularly UnicodeData), you should be able to access
  //      C++ character types for their contents.
  //   2. |Utf8Unit| and friends must convert to/from |char| and |char*| only by
  //      explicit operation.
  //   3. |Utf8Unit| must participate in overload resolution and template type
  //      equivalence (that is, given |template<class> class X|, when |X<T>| and
  //      |X<U>| are the same type) distinctly from the C++ character types.
  //
  // And a few nice-to-haves (at least for the moment):
  //
  //   4. The representation should use unsigned numbers, to avoid undefined
  //      behavior that can arise with signed types, and because Unicode code
  //      points and code units are unsigned.
  //   5. |Utf8Unit| and friends should be convertible to/from |unsigned char|
  //      and |unsigned char*|, for APIs that (because of #4 above) use those
  //      types as the "natural" choice for UTF-8 data.
  //
  // #1 requires that |Utf8Unit| "incorporate" a C++ character type: one of
  // |{,{un,}signed} char|.[0]  |uint8_t| won't work because it might not be a
  // C++ character type.
  //
  // #2 and #3 mean that |Utf8Unit| can't *be* such a type (or a typedef to one:
  // typedefs don't generate *new* types, just type aliases).  This requires a
  // compound type.
  //
  // The ultimate representation (and character type in it) is constrained by
  // C++14 [basic.lval]p10 that defines how objects may be accessed, with
  // respect to the dynamic type in memory and the actual type used to access
  // them.  It reads:
  //
  //     If a program attempts to access the stored value of an object
  //     through a glvalue of other than one of the following types the
  //     behavior is undefined:
  //
  //       1. the dynamic type of the object,
  //       2. a cv-qualified version of the dynamic type of the object,
  //       ...other types irrelevant here...
  //       3. an aggregate or union type that includes one of the
  //          aforementioned types among its elements or non-static data
  //          members (including, recursively, an element or non-static
  //          data member of a subaggregate or contained union),
  //       ...more irrelevant types...
  //       4. a char or unsigned char type.
  //
  // Accessing (wrapped) UTF-8 data as |char|/|unsigned char| is allowed no
  // matter the representation by #4.  (Briefly set aside what values are seen.)
  // (And #2 allows |const| on either the dynamic type or the accessing type.)
  // (|signed char| is really only useful for small signed numbers, not
  // characters, so we ignore it.)
  //
  // If we interpret contents as |char|/|unsigned char| contrary to the actual
  // type stored there, what happens?  C++14 [basic.fundamental]p1 requires
  // character types be identically aligned/sized; C++14 [basic.fundamental]p3
  // requires |signed char| and |unsigned char| have the same value
  // representation.  C++ doesn't require identical bitwise representation, tho.
  // Practically we could assume it, but this verges on C++ spec bits best not
  // *relied* on for correctness, if possible.
  //
  // So we don't expose |Utf8Unit|'s contents as |unsigned char*|: only |char|
  // and |char*|.  Instead we safely expose |unsigned char| by fully-defined
  // *integral conversion* (C++14 [conv.integral]p2).  Integral conversion from
  // |unsigned char| → |char| has only implementation-defined behavior.  It'd be
  // better not to depend on that, but given twos-complement won, it should be
  // okay.  (Also |unsigned char*| is awkward enough to work with for strings
  // that it probably doesn't appear in string manipulation much anyway, only in
  // places that should really use |Utf8Unit| directly.)
  //
  // The opposite direction -- interpreting |char| or |char*| data through
  // |Utf8Unit| -- isn't tricky as long as |Utf8Unit| contains a |char| as
  // decided above, using #3.  An "aggregate or union" will work that contains a
  // |char|.  Oddly, an aggregate won't work: C++14 [dcl.init.aggr]p1 says
  // aggregates must have "no private or protected non-static data members", and
  // we want to keep the inner |char| hidden.  So a |struct| is out, and only
  // |union| remains.
  //
  // (Enums are not "an aggregate or union type", so [maybe surprisingly] we
  // can't make |Utf8Unit| an enum class with |char| underlying type, because we
  // are given no license to treat |char| memory as such an |enum|'s memory.)
  //
  // Therefore |Utf8Unit| is a union type with a |char| non-static data member.
  // This satisfies all our requirements.  It also supports the nice-to-haves of
  // creating a |Utf8Unit| from an |unsigned char|, and being convertible to
  // |unsigned char|.  It doesn't satisfy the nice-to-haves of using an
  // |unsigned char| internally, nor of letting us wrap an existing
  // |unsigned char| or pointer to one.  We probably *could* do these, if we
  // were willing to rely harder on implementation-defined behaviors, but for
  // now we privilege C++'s main character type over some conceptual purity.
  //
  // 0. There's a proposal for a UTF-8 character type distinct from the existing
  //    C++ narrow character types:
  //
  //      http://open-std.org/JTC1/SC22/WG21/docs/papers/2016/p0482r0.html
  //
  //    but it hasn't been standardized (and might never be), and none of the
  //    compilers we really care about have implemented it.  Maybe someday we
  //    can change our implementation to it without too much trouble, if we're
  //    lucky...
  char mValue;

public:
  explicit constexpr Utf8Unit(char aUnit)
    : mValue(aUnit)
  {}

  explicit constexpr Utf8Unit(unsigned char aUnit)
    : mValue(static_cast<char>(aUnit))
  {
    // Per the above comment, the prior cast is integral conversion with
    // implementation-defined semantics, and we regretfully but unavoidably
    // assume the conversion does what we want it to.
  }

  constexpr bool operator==(const Utf8Unit& aOther) const
  {
    return mValue == aOther.mValue;
  }

  constexpr bool operator!=(const Utf8Unit& aOther) const
  {
    return !(*this == aOther);
  }

  /** Convert a UTF-8 code unit to a raw char. */
  constexpr char toChar() const
  {
    // Only a |char| is ever permitted to be written into this location, so this
    // is both permissible and returns the desired value.
    return mValue;
  }

  /** Convert a UTF-8 code unit to a raw unsigned char. */
  constexpr unsigned char toUnsignedChar() const
  {
    // Per the above comment, this is well-defined integral conversion.
    return static_cast<unsigned char>(mValue);
  }

  /** Convert a UTF-8 code unit to a uint8_t. */
  constexpr uint8_t toUint8() const
  {
    // Per the above comment, this is well-defined integral conversion.
    return static_cast<uint8_t>(mValue);
  }

  // We currently don't expose |&mValue|.  |UnicodeData| sort of does, but
  // that's a somewhat separate concern, justified in different comments in
  // that other code.
};

/**
 * Returns true if the given length-delimited memory consists of a valid UTF-8
 * string, false otherwise.
 *
 * A valid UTF-8 string contains no overlong-encoded code points (as one would
 * expect) and contains no code unit sequence encoding a UTF-16 surrogate.  The
 * string *may* contain U+0000 NULL code points.
 */
extern MFBT_API bool
IsValidUtf8(const void* aCodeUnits, size_t aCount);

} // namespace mozilla

#endif /* mozilla_Utf8_h */
