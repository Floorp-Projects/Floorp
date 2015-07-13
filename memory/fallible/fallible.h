/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_fallible_h
#define mozilla_fallible_h

#if defined(__cplusplus)

/* Explicit fallible allocation
 *
 * Memory allocation (normally) defaults to abort in case of failed
 * allocation. That is, it never returns NULL, and crashes instead.
 *
 * Code can explicitely request for fallible memory allocation thanks
 * to the declarations below.
 *
 * The typical use of the mozilla::fallible const is with placement new,
 * like the following:
 *
 *     foo = new (mozilla::fallible) Foo();
 *
 * The following forms, or derivatives, are also possible but deprecated:
 *
 *     foo = new ((mozilla::fallible_t())) Foo();
 *
 *     const mozilla::fallible_t fallible = mozilla::fallible_t();
 *     bar = new (f) Bar();
 *
 * It is also possible to declare method overloads with fallible allocation
 * alternatives, like so:
 *
 *     class Foo {
 *     public:
 *       void Method(void *);
 *       void Method(void *, const mozilla::fallible_t&);
 *     };
 *
 *     Foo foo;
 *     foo.Method(nullptr, mozilla::fallible);
 *
 * If that last method call is in a method that itself takes a const
 * fallible_t& argument, it is recommended to propagate that argument
 * instead of using mozilla::fallible:
 *
 *     void Func(Foo &foo, const mozilla::fallible_t& aFallible) {
 *         foo.Method(nullptr, aFallible);
 *     }
 *
 */
namespace mozilla {

struct fallible_t { };

/* This symbol is kept unexported, such that in corner cases where the
 * compiler can't remove its use (essentially, cross compilation-unit
 * calls), the smallest machine code is used.
 * Depending how the linker packs symbols, it will consume between 1 and
 * 8 bytes of read-only data in each executable or shared library, but
 * only in those where it's actually not optimized out by the compiler.
 */
extern const fallible_t fallible;

} // namespace mozilla

#endif

#endif // mozilla_fallible_h
