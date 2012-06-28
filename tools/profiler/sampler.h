/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* *************** SPS Sampler Information ****************
 *
 * SPS is an always on profiler that takes fast and low overheads samples
 * of the program execution using only userspace functionity for portability.
 * The goal of this module is to provide performance data in a generic
 * cross platform way without requiring custom tools or kernel support.
 *
 * Non goals: Support features that are platform specific or replace
 *            platform specific profilers.
 *
 * Samples are collected to form a timeline with optional timeline event (markers)
 * used for filtering.
 *
 * SPS collects samples in a platform independant way by using a speudo stack abstraction
 * of the real program stack by using 'sample stack frames'. When a sample is collected
 * all active sample stack frames and the program counter are recorded.
 */

/* *************** SPS Sampler File Format ****************
 *
 * Simple new line seperated tag format:
 * S      -> BOF tags EOF
 * tags   -> tag tags
 * tag    -> CHAR - STRING
 *
 * Tags:
 * 's' - Sample tag followed by the first stack frame followed by 0 or more 'c' tags.
 * 'c' - Continue Sample tag gives remaining tag element. If a 'c' tag is seen without
 *         a preceding 's' tag it should be ignored. This is to support the behavior
 *         of circular buffers.
 *         If the 'stackwalk' feature is enabled this tag will have the format
 *         'l-<library name>@<hex address>' and will expect an external tool to translate
 *         the tag into something readable through a symbolication processing step.
 * 'm' - Timeline marker. Zero or more may appear before a 's' tag.
 * 'l' - Information about the program counter library and address. Post processing
 *         can include function and source line. If built with leaf data enabled
 *         this tag will describe the last 'c' tag.
 * 'r' - Responsiveness tag following an 's' tag. Gives an indication on how well the
 *          application is responding to the event loop. Lower is better.
 *
 * NOTE: File format is planned to be extended to include a dictionary to reduce size.
 */

#ifndef SAMPLER_H
#define SAMPLER_H

// Redefine the macros for platforms where SPS is supported.
#ifdef MOZ_ENABLE_PROFILER_SPS

#include "sps_sampler.h"

#else

// Initialize the sampler. Any other calls will be silently discarded
// before the sampler has been initialized (i.e. early start-up code)
#define SAMPLER_INIT()
#define SAMPLER_DEINIT()
#define SAMPLER_START(entries, interval, features, featureCount)
#define SAMPLER_STOP()
#define SAMPLER_IS_ACTIVE() false
#define SAMPLER_SAVE()
// Returned string must be free'ed
#define SAMPLER_GET_PROFILE() NULL
#define SAMPLER_GET_PROFILE_DATA(ctx) NULL
#define SAMPLER_RESPONSIVENESS(time) NULL
#define SAMPLER_GET_RESPONSIVENESS() NULL
#define SAMPLER_GET_FEATURES() NULL
#define SAMPLE_LABEL(name_space, info)
// Provide a default literal string to use if profiling is disabled
// and a printf argument to be computed if profiling is enabled.
// NOTE: This will store the formated string on the stack and consume
//       over 128 bytes on the stack.
#define SAMPLE_LABEL_PRINTF(name_space, info, format, ...)
#define SAMPLE_LABEL_FN(name_space, info)
#define SAMPLE_MARKER(info)

#endif

#endif // ifndef SAMPLER_H
