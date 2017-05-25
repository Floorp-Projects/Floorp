/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implements various macros meant to ease the use of variadic macros.
 */

#ifndef mozilla_MacroArgs_h
#define mozilla_MacroArgs_h

// Concatenates pre-processor tokens in a way that can be used with __LINE__.
#define MOZ_CONCAT2(x, y) x ## y
#define MOZ_CONCAT(x, y) MOZ_CONCAT2(x, y)

/*
 * MOZ_PASTE_PREFIX_AND_ARG_COUNT(aPrefix, ...) counts the number of variadic
 * arguments and prefixes it with |aPrefix|. For example:
 *
 *   MOZ_PASTE_PREFIX_AND_ARG_COUNT(, foo, 42) expands to 2
 *   MOZ_PASTE_PREFIX_AND_ARG_COUNT(A, foo, 42, bar) expands to A3
 *   MOZ_PASTE_PREFIX_AND_ARG_COUNT(A) expands to A0
 *   MOZ_PASTE_PREFIX_AND_ARG_COUNT() expands to 0, but MSVC warns there
 *   aren't enough arguments given.
 *
 * You must pass in between 0 and 50 (inclusive) variadic arguments, past
 * |aPrefix|.
 *
 * The `##__VA_ARGS__` form is a GCC extension that removes the comma if
 * __VA_ARGS__ is empty. It is supported by Clang too. MSVC ignores ##,
 * and its default behavior is already to strip the comma when __VA_ARGS__
 * is empty.
 *
 * So MOZ_MACROARGS_ARG_COUNT_HELPER(prefix) expands to
 *   (_, prefix50, prefix49, ...)
 * MOZ_MACROARGS_ARG_COUNT_HELPER(prefix, a) expands to
 *   (_, a, prefix50, prefix49, ...)
 * etc.
 */
#define MOZ_PASTE_PREFIX_AND_ARG_COUNT(aPrefix, ...) \
  MOZ_MACROARGS_ARG_COUNT_HELPER2( \
    MOZ_MACROARGS_ARG_COUNT_HELPER(aPrefix, ##__VA_ARGS__))

#define MOZ_MACROARGS_ARG_COUNT_HELPER(aPrefix, ...) (_, ##__VA_ARGS__, \
    aPrefix##50, aPrefix##49, aPrefix##48, aPrefix##47, aPrefix##46, \
    aPrefix##45, aPrefix##44, aPrefix##43, aPrefix##42, aPrefix##41, \
    aPrefix##40, aPrefix##39, aPrefix##38, aPrefix##37, aPrefix##36, \
    aPrefix##35, aPrefix##34, aPrefix##33, aPrefix##32, aPrefix##31, \
    aPrefix##30, aPrefix##29, aPrefix##28, aPrefix##27, aPrefix##26, \
    aPrefix##25, aPrefix##24, aPrefix##23, aPrefix##22, aPrefix##21, \
    aPrefix##20, aPrefix##19, aPrefix##18, aPrefix##17, aPrefix##16, \
    aPrefix##15, aPrefix##14, aPrefix##13, aPrefix##12, aPrefix##11, \
    aPrefix##10, aPrefix##9,  aPrefix##8,  aPrefix##7,  aPrefix##6,  \
    aPrefix##5,  aPrefix##4,  aPrefix##3,  aPrefix##2,  aPrefix##1, aPrefix##0)

#define MOZ_MACROARGS_ARG_COUNT_HELPER2(aArgs) \
  MOZ_MACROARGS_ARG_COUNT_HELPER3 aArgs

#define MOZ_MACROARGS_ARG_COUNT_HELPER3(a0, \
   a1,  a2,  a3,  a4,  a5,  a6,  a7,  a8,  a9, a10, \
  a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, \
  a21, a22, a23, a24, a25, a26, a27, a28, a29, a30, \
  a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, \
  a41, a42, a43, a44, a45, a46, a47, a48, a49, a50, \
  a51, ...) a51

/*
 * MOZ_ARGS_AFTER_N expands to its arguments excluding the first |N|
 * arguments. For example:
 *
 *   MOZ_ARGS_AFTER_2(a, b, c, d) expands to: c, d
 */
#define MOZ_ARGS_AFTER_1(a1, ...) __VA_ARGS__
#define MOZ_ARGS_AFTER_2(a1, a2, ...) __VA_ARGS__

/*
 * MOZ_ARG_N expands to its |N|th argument.
 */
#define MOZ_ARG_1(a1, ...) a1
#define MOZ_ARG_2(a1, a2, ...) a2

#endif /* mozilla_MacroArgs_h */
