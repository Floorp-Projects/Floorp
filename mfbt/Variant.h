/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A template class for tagged unions. */

#include <new>
#include <stdint.h>

#include "mozilla/Assertions.h"
#include "mozilla/Move.h"
#include "mozilla/OperatorNewExtensions.h"
#include "mozilla/TemplateLib.h"
#include "mozilla/TypeTraits.h"

#ifndef mozilla_Variant_h
#define mozilla_Variant_h

namespace mozilla {

template<typename... Ts>
class Variant;

namespace detail {

template <typename...>
struct FirstTypeIsInRest;

template <typename First>
struct FirstTypeIsInRest<First> : FalseType {};

template <typename First, typename Second, typename... Rest>
struct FirstTypeIsInRest<First, Second, Rest...>
{
  static constexpr bool value =
    IsSame<First, Second>::value ||
    FirstTypeIsInRest<First, Rest...>::value;
};

template <typename...>
struct TypesAreDistinct;

template <>
struct TypesAreDistinct<> : TrueType { };

template<typename First, typename... Rest>
struct TypesAreDistinct<First, Rest...>
{
  static constexpr bool value =
    !FirstTypeIsInRest<First, Rest...>::value &&
    TypesAreDistinct<Rest...>::value;
};

// The `IsVariant` helper is used in conjunction with static_assert and
// `mozilla::EnableIf` to catch passing non-variant types to `Variant::is<T>()`
// and friends at compile time, rather than at runtime. It ensures that the
// given type `Needle` is one of the types in the set of types `Haystack`.

template<typename Needle, typename... Haystack>
struct IsVariant;

template<typename Needle>
struct IsVariant<Needle> : FalseType {};

template<typename Needle, typename... Haystack>
struct IsVariant<Needle, Needle, Haystack...> : TrueType {};

template<typename Needle, typename T, typename... Haystack>
struct IsVariant<Needle, T, Haystack...> : public IsVariant<Needle, Haystack...> { };

/// SelectVariantTypeHelper is used in the implementation of SelectVariantType.
template<typename T, typename... Variants>
struct SelectVariantTypeHelper;

template<typename T>
struct SelectVariantTypeHelper<T>
{ };

template<typename T, typename... Variants>
struct SelectVariantTypeHelper<T, T, Variants...>
{
  typedef T Type;
};

template<typename T, typename... Variants>
struct SelectVariantTypeHelper<T, const T, Variants...>
{
  typedef const T Type;
};

template<typename T, typename... Variants>
struct SelectVariantTypeHelper<T, const T&, Variants...>
{
  typedef const T& Type;
};

template<typename T, typename... Variants>
struct SelectVariantTypeHelper<T, T&&, Variants...>
{
  typedef T&& Type;
};

template<typename T, typename Head, typename... Variants>
struct SelectVariantTypeHelper<T, Head, Variants...>
  : public SelectVariantTypeHelper<T, Variants...>
{ };

/**
 * SelectVariantType takes a type T and a list of variant types Variants and
 * yields a type Type, selected from Variants, that can store a value of type T
 * or a reference to type T. If no such type was found, Type is not defined.
 */
template <typename T, typename... Variants>
struct SelectVariantType
  : public SelectVariantTypeHelper<typename RemoveConst<typename RemoveReference<T>::Type>::Type,
                                   Variants...>
{ };

// Compute a fast, compact type that can be used to hold integral values that
// distinctly map to every type in Ts.
template<typename... Ts>
struct VariantTag
{
private:
  static const size_t TypeCount = sizeof...(Ts);

public:
  using Type =
    typename Conditional<TypeCount < 3,
                         bool,
                         typename Conditional<TypeCount < (1 << 8),
                                              uint_fast8_t,
                                              size_t // stop caring past a certain point :-)
                                              >::Type
                         >::Type;
};

// TagHelper gets the given sentinel tag value for the given type T. This has to
// be split out from VariantImplementation because you can't nest a partial
// template specialization within a template class.

template<typename Tag, size_t N, typename T, typename U, typename Next, bool isMatch>
struct TagHelper;

// In the case where T != U, we continue recursion.
template<typename Tag, size_t N, typename T, typename U, typename Next>
struct TagHelper<Tag, N, T, U, Next, false>
{
  static Tag tag() { return Next::template tag<U>(); }
};

// In the case where T == U, return the tag number.
template<typename Tag, size_t N, typename T, typename U, typename Next>
struct TagHelper<Tag, N, T, U, Next, true>
{
  static Tag tag() { return Tag(N); }
};

// The VariantImplementation template provides the guts of mozilla::Variant.  We
// create a VariantImplementation for each T in Ts... which handles
// construction, destruction, etc for when the Variant's type is T.  If the
// Variant's type isn't T, it punts the request on to the next
// VariantImplementation.

template<typename Tag, size_t N, typename... Ts>
struct VariantImplementation;

// The singly typed Variant / recursion base case.
template<typename Tag, size_t N, typename T>
struct VariantImplementation<Tag, N, T>
{
  template<typename U>
  static Tag tag() {
    static_assert(mozilla::IsSame<T, U>::value,
                  "mozilla::Variant: tag: bad type!");
    return Tag(N);
  }

  template<typename Variant>
  static void copyConstruct(void* aLhs, const Variant& aRhs) {
    ::new (KnownNotNull, aLhs) T(aRhs.template as<T>());
  }

  template<typename Variant>
  static void moveConstruct(void* aLhs, Variant&& aRhs) {
    ::new (KnownNotNull, aLhs) T(aRhs.template extract<T>());
  }

  template<typename Variant>
  static void destroy(Variant& aV) {
    aV.template as<T>().~T();
  }

  template<typename Variant>
  static bool
  equal(const Variant& aLhs, const Variant& aRhs) {
      return aLhs.template as<T>() == aRhs.template as<T>();
  }

  template<typename Matcher, typename ConcreteVariant>
  static auto
  match(Matcher&& aMatcher, ConcreteVariant& aV)
    -> decltype(aMatcher.match(aV.template as<T>()))
  {
    return aMatcher.match(aV.template as<T>());
  }
};

// VariantImplementation for some variant type T.
template<typename Tag, size_t N, typename T, typename... Ts>
struct VariantImplementation<Tag, N, T, Ts...>
{
  // The next recursive VariantImplementation.
  using Next = VariantImplementation<Tag, N + 1, Ts...>;

  template<typename U>
  static Tag tag() {
    return TagHelper<Tag, N, T, U, Next, IsSame<T, U>::value>::tag();
  }

  template<typename Variant>
  static void copyConstruct(void* aLhs, const Variant& aRhs) {
    if (aRhs.template is<T>()) {
      ::new (KnownNotNull, aLhs) T(aRhs.template as<T>());
    } else {
      Next::copyConstruct(aLhs, aRhs);
    }
  }

  template<typename Variant>
  static void moveConstruct(void* aLhs, Variant&& aRhs) {
    if (aRhs.template is<T>()) {
      ::new (KnownNotNull, aLhs) T(aRhs.template extract<T>());
    } else {
      Next::moveConstruct(aLhs, Move(aRhs));
    }
  }

  template<typename Variant>
  static void destroy(Variant& aV) {
    if (aV.template is<T>()) {
      aV.template as<T>().~T();
    } else {
      Next::destroy(aV);
    }
  }

  template<typename Variant>
  static bool equal(const Variant& aLhs, const Variant& aRhs) {
    if (aLhs.template is<T>()) {
      MOZ_ASSERT(aRhs.template is<T>());
      return aLhs.template as<T>() == aRhs.template as<T>();
    } else {
      return Next::equal(aLhs, aRhs);
    }
  }

  template<typename Matcher, typename ConcreteVariant>
  static auto
  match(Matcher&& aMatcher, ConcreteVariant& aV)
    -> decltype(aMatcher.match(aV.template as<T>()))
  {
    if (aV.template is<T>()) {
      return aMatcher.match(aV.template as<T>());
    } else {
      // If you're seeing compilation errors here like "no matching
      // function for call to 'match'" then that means that the
      // Matcher doesn't exhaust all variant types. There must exist a
      // Matcher::match(T&) for every variant type T.
      //
      // If you're seeing compilation errors here like "cannot
      // initialize return object of type <...> with an rvalue of type
      // <...>" then that means that the Matcher::match(T&) overloads
      // are returning different types. They must all return the same
      // Matcher::ReturnType type.
      return Next::match(aMatcher, aV);
    }
  }
};

/**
 * AsVariantTemporary stores a value of type T to allow construction of a
 * Variant value via type inference. Because T is copied and there's no
 * guarantee that the copy can be elided, AsVariantTemporary is best used with
 * primitive or very small types.
 */
template <typename T>
struct AsVariantTemporary
{
  explicit AsVariantTemporary(const T& aValue)
    : mValue(aValue)
  {}

  template<typename U>
  explicit AsVariantTemporary(U&& aValue)
    : mValue(Forward<U>(aValue))
  {}

  AsVariantTemporary(const AsVariantTemporary& aOther)
    : mValue(aOther.mValue)
  {}

  AsVariantTemporary(AsVariantTemporary&& aOther)
    : mValue(Move(aOther.mValue))
  {}

  AsVariantTemporary() = delete;
  void operator=(const AsVariantTemporary&) = delete;
  void operator=(AsVariantTemporary&&) = delete;

  typename RemoveConst<typename RemoveReference<T>::Type>::Type mValue;
};

} // namespace detail

/**
 * # mozilla::Variant
 *
 * A variant / tagged union / heterogenous disjoint union / sum-type template
 * class. Similar in concept to (but not derived from) `boost::variant`.
 *
 * Sometimes, you may wish to use a C union with non-POD types. However, this is
 * forbidden in C++ because it is not clear which type in the union should have
 * its constructor and destructor run on creation and deletion
 * respectively. This is the problem that `mozilla::Variant` solves.
 *
 * ## Usage
 *
 * A `mozilla::Variant` instance is constructed (via move or copy) from one of
 * its variant types (ignoring const and references). It does *not* support
 * construction from subclasses of variant types or types that coerce to one of
 * the variant types.
 *
 *     Variant<char, uint32_t> v1('a');
 *     Variant<UniquePtr<A>, B, C> v2(MakeUnique<A>());
 *
 * Because specifying the full type of a Variant value is often verbose,
 * AsVariant() can be used to construct a Variant value using type inference in
 * contexts such as expressions or when returning values from functions. Because
 * AsVariant() must copy or move the value into a temporary and this cannot
 * necessarily be elided by the compiler, it's mostly appropriate only for use
 * with primitive or very small types.
 *
 *
 *     Variant<char, uint32_t> Foo() { return AsVariant('x'); }
 *     // ...
 *     Variant<char, uint32_t> v1 = Foo();  // v1 holds char('x').
 *
 * All access to the contained value goes through type-safe accessors.
 *
 *     void
 *     Foo(Variant<A, B, C> v)
 *     {
 *       if (v.is<A>()) {
 *         A& ref = v.as<A>();
 *         ...
 *       } else {
 *         ...
 *       }
 *     }
 *
 * Attempting to use the contained value as type `T1` when the `Variant`
 * instance contains a value of type `T2` causes an assertion failure.
 *
 *     A a;
 *     Variant<A, B, C> v(a);
 *     v.as<B>(); // <--- Assertion failure!
 *
 * Trying to use a `Variant<Ts...>` instance as some type `U` that is not a
 * member of the set of `Ts...` is a compiler error.
 *
 *     A a;
 *     Variant<A, B, C> v(a);
 *     v.as<SomeRandomType>(); // <--- Compiler error!
 *
 * Additionally, you can turn a `Variant` that `is<T>` into a `T` by moving it
 * out of the containing `Variant` instance with the `extract<T>` method:
 *
 *     Variant<UniquePtr<A>, B, C> v(MakeUnique<A>());
 *     auto ptr = v.extract<UniquePtr<A>>();
 *
 * Finally, you can exhaustively match on the contained variant and branch into
 * different code paths depending which type is contained. This is preferred to
 * manually checking every variant type T with is<T>() because it provides
 * compile-time checking that you handled every type, rather than runtime
 * assertion failures.
 *
 *     // Bad!
 *     char* foo(Variant<A, B, C, D>& v) {
 *       if (v.is<A>()) {
 *         return ...;
 *       } else if (v.is<B>()) {
 *         return ...;
 *       } else {
 *         return doSomething(v.as<C>()); // Forgot about case D!
 *       }
 *     }
 *
 *     // Good!
 *     struct FooMatcher
 *     {
 *       // The return type of all matchers must be identical.
 *       char* match(A& a) { ... }
 *       char* match(B& b) { ... }
 *       char* match(C& c) { ... }
 *       char* match(D& d) { ... } // Compile-time error to forget D!
 *     }
 *     char* foo(Variant<A, B, C, D>& v) {
 *       return v.match(FooMatcher());
 *     }
 *
 * ## Examples
 *
 * A tree is either an empty leaf, or a node with a value and two children:
 *
 *     struct Leaf { };
 *
 *     template<typename T>
 *     struct Node
 *     {
 *       T value;
 *       Tree<T>* left;
 *       Tree<T>* right;
 *     };
 *
 *     template<typename T>
 *     using Tree = Variant<Leaf, Node<T>>;
 *
 * A copy-on-write string is either a non-owning reference to some existing
 * string, or an owning reference to our copy:
 *
 *     class CopyOnWriteString
 *     {
 *       Variant<const char*, UniquePtr<char[]>> string;
 *
 *       ...
 *     };
 *
 * Because Variant must be aligned suitable to hold any value stored within it,
 * and because |alignas| requirements don't affect platform ABI with respect to
 * how parameters are laid out in memory, Variant can't be used as the type of a
 * function parameter.  Pass Variant to functions by pointer or reference
 * instead.
 */
template<typename... Ts>
class MOZ_INHERIT_TYPE_ANNOTATIONS_FROM_TEMPLATE_ARGS MOZ_NON_PARAM Variant
{
  static_assert(detail::TypesAreDistinct<Ts...>::value,
                "Variant with duplicate types is not supported");

  using Tag = typename detail::VariantTag<Ts...>::Type;
  using Impl = detail::VariantImplementation<Tag, 0, Ts...>;

  static constexpr size_t RawDataAlignment = tl::Max<alignof(Ts)...>::value;
  static constexpr size_t RawDataSize = tl::Max<sizeof(Ts)...>::value;

  // Raw storage for the contained variant value.
  alignas(RawDataAlignment) unsigned char rawData[RawDataSize];

  // Each type is given a unique tag value that lets us keep track of the
  // contained variant value's type.
  Tag tag;

  // Some versions of GCC treat it as a -Wstrict-aliasing violation (ergo a
  // -Werror compile error) to reinterpret_cast<> |rawData| to |T*|, even
  // through |void*|.  Placing the latter cast in these separate functions
  // breaks the chain such that affected GCC versions no longer warn/error.
  void* ptr() {
    return rawData;
  }

  const void* ptr() const {
    return rawData;
  }

public:
  /** Perfect forwarding construction for some variant type T. */
  template<typename RefT,
           // RefT captures both const& as well as && (as intended, to support
           // perfect forwarding), so we have to remove those qualifiers here
           // when ensuring that T is a variant of this type, and getting T's
           // tag, etc.
           typename T = typename detail::SelectVariantType<RefT, Ts...>::Type>
  explicit Variant(RefT&& aT)
    : tag(Impl::template tag<T>())
  {
    ::new (KnownNotNull, ptr()) T(Forward<RefT>(aT));
  }

  /**
   * Constructs this Variant from an AsVariantTemporary<T> such that T can be
   * stored in one of the types allowable in this Variant. This is used in the
   * implementation of AsVariant().
   */
  template<typename RefT,
           typename T = typename detail::SelectVariantType<RefT, Ts...>::Type>
  MOZ_IMPLICIT Variant(detail::AsVariantTemporary<RefT>&& aValue)
    : tag(Impl::template tag<T>())
  {
    ::new (KnownNotNull, ptr()) T(Move(aValue.mValue));
  }

  /** Copy construction. */
  Variant(const Variant& aRhs)
    : tag(aRhs.tag)
  {
    Impl::copyConstruct(ptr(), aRhs);
  }

  /** Move construction. */
  Variant(Variant&& aRhs)
    : tag(aRhs.tag)
  {
    Impl::moveConstruct(ptr(), Move(aRhs));
  }

  /** Copy assignment. */
  Variant& operator=(const Variant& aRhs) {
    MOZ_ASSERT(&aRhs != this, "self-assign disallowed");
    this->~Variant();
    ::new (KnownNotNull, this) Variant(aRhs);
    return *this;
  }

  /** Move assignment. */
  Variant& operator=(Variant&& aRhs) {
    MOZ_ASSERT(&aRhs != this, "self-assign disallowed");
    this->~Variant();
    ::new (KnownNotNull, this) Variant(Move(aRhs));
    return *this;
  }

  /** Move assignment from AsVariant(). */
  template <typename T>
  Variant& operator=(detail::AsVariantTemporary<T>&& aValue)
  {
    this->~Variant();
    ::new (KnownNotNull, this) Variant(Move(aValue));
    return *this;
  }

  ~Variant()
  {
    Impl::destroy(*this);
  }

  /** Check which variant type is currently contained. */
  template<typename T>
  bool is() const {
    static_assert(detail::IsVariant<T, Ts...>::value,
                  "provided a type not found in this Variant's type list");
    return Impl::template tag<T>() == tag;
  }

  /**
   * Operator == overload that defers to the variant type's operator==
   * implementation if the rhs is tagged as the same type as this one.
   */
  bool operator==(const Variant& aRhs) const {
    return tag == aRhs.tag && Impl::equal(*this, aRhs);
  }

  /**
   * Operator != overload that defers to the negation of the variant type's
   * operator== implementation if the rhs is tagged as the same type as this
   * one.
   */
  bool operator!=(const Variant& aRhs) const {
    return !(*this == aRhs);
  }

  // Accessors for working with the contained variant value.

  /** Mutable reference. */
  template<typename T>
  T& as() {
    static_assert(detail::IsVariant<T, Ts...>::value,
                  "provided a type not found in this Variant's type list");
    MOZ_RELEASE_ASSERT(is<T>());
    return *static_cast<T*>(ptr());
  }

  /** Immutable const reference. */
  template<typename T>
  const T& as() const {
    static_assert(detail::IsVariant<T, Ts...>::value,
                  "provided a type not found in this Variant's type list");
    MOZ_RELEASE_ASSERT(is<T>());
    return *static_cast<const T*>(ptr());
  }

  /**
   * Extract the contained variant value from this container into a temporary
   * value.  On completion, the value in the variant will be in a
   * safely-destructible state, as determined by the behavior of T's move
   * constructor when provided the variant's internal value.
   */
  template<typename T>
  T extract() {
    static_assert(detail::IsVariant<T, Ts...>::value,
                  "provided a type not found in this Variant's type list");
    MOZ_ASSERT(is<T>());
    return T(Move(as<T>()));
  }

  // Exhaustive matching of all variant types on the contained value.

  /** Match on an immutable const reference. */
  template<typename Matcher>
  auto
  match(Matcher&& aMatcher) const
    -> decltype(Impl::match(aMatcher, *this))
  {
    return Impl::match(aMatcher, *this);
  }

  /** Match on a mutable non-const reference. */
  template<typename Matcher>
  auto
  match(Matcher&& aMatcher)
    -> decltype(Impl::match(aMatcher, *this))
  {
    return Impl::match(aMatcher, *this);
  }
};

/*
 * AsVariant() is used to construct a Variant<T,...> value containing the
 * provided T value using type inference. It can be used to construct Variant
 * values in expressions or return them from functions without specifying the
 * entire Variant type.
 *
 * Because AsVariant() must copy or move the value into a temporary and this
 * cannot necessarily be elided by the compiler, it's mostly appropriate only
 * for use with primitive or very small types.
 *
 * AsVariant() returns a AsVariantTemporary value which is implicitly
 * convertible to any Variant that can hold a value of type T.
 */
template<typename T>
detail::AsVariantTemporary<T>
AsVariant(T&& aValue)
{
  return detail::AsVariantTemporary<T>(Forward<T>(aValue));
}

} // namespace mozilla

#endif /* mozilla_Variant_h */
