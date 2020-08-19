/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/MacroArgs.h"

static_assert(MOZ_ARG_COUNT() == 0, "");
static_assert(MOZ_ARG_COUNT(a) == 1, "");
static_assert(MOZ_ARG_COUNT(a, b) == 2, "");
static_assert(MOZ_ARG_COUNT(a, b, c) == 3, "");

static_assert(MOZ_PASTE_PREFIX_AND_ARG_COUNT(100) == 1000, "");
static_assert(MOZ_PASTE_PREFIX_AND_ARG_COUNT(100, a) == 1001, "");
static_assert(MOZ_PASTE_PREFIX_AND_ARG_COUNT(100, a, b) == 1002, "");
static_assert(MOZ_PASTE_PREFIX_AND_ARG_COUNT(100, a, b, c) == 1003, "");

static_assert(MOZ_PASTE_PREFIX_AND_ARG_COUNT(, a, b, c) == 3, "");
static_assert(MOZ_PASTE_PREFIX_AND_ARG_COUNT(, a) == 1, "");
static_assert(MOZ_PASTE_PREFIX_AND_ARG_COUNT(, !a) == 1, "");
static_assert(MOZ_PASTE_PREFIX_AND_ARG_COUNT(, (a, b)) == 1, "");

static_assert(MOZ_PASTE_PREFIX_AND_ARG_COUNT(, MOZ_ARGS_AFTER_1(a, b, c)) == 2,
              "MOZ_ARGS_AFTER_1(a, b, c) should expand to 'b, c'");
static_assert(MOZ_ARGS_AFTER_2(a, b, 3) == 3,
              "MOZ_ARGS_AFTER_2(a, b, 3) should expand to '3'");

static_assert(MOZ_ARG_1(10, 20, 30, 40, 50, 60, 70, 80, 90) == 10, "");
static_assert(MOZ_ARG_2(10, 20, 30, 40, 50, 60, 70, 80, 90) == 20, "");
static_assert(MOZ_ARG_3(10, 20, 30, 40, 50, 60, 70, 80, 90) == 30, "");
static_assert(MOZ_ARG_4(10, 20, 30, 40, 50, 60, 70, 80, 90) == 40, "");
static_assert(MOZ_ARG_5(10, 20, 30, 40, 50, 60, 70, 80, 90) == 50, "");
static_assert(MOZ_ARG_6(10, 20, 30, 40, 50, 60, 70, 80, 90) == 60, "");
static_assert(MOZ_ARG_7(10, 20, 30, 40, 50, 60, 70, 80, 90) == 70, "");
static_assert(MOZ_ARG_8(10, 20, 30, 40, 50, 60, 70, 80, 90) == 80, "");

int main() { return 0; }
