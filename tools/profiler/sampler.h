/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Benoit Girard <bgirard@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
 * 'm' - Timeline marker. Zero or more may appear before a 's' tag.
 * 'l' - Information about the program counter library and address. Post processing
 *         can include function and source line. If built with leaf data enabled
 *         this tag will describe the last 'c' tag.
 *
 * NOTE: File format is planned to be extended to include a dictionary to reduce size.
 */

#ifndef SAMPLER_H
#define SAMPLER_H

#if defined(_MSC_VER)
#define FULLFUNCTION __FUNCSIG__
#elif (__GNUC__ >= 4)
#define FULLFUNCTION __PRETTY_FUNCTION__
#else
#define FULLFUNCTION __FUNCTION__
#endif

// Initialize the sampler. Any other calls will be silently discarded
// before the sampler has been initialized (i.e. early start-up code)
#define SAMPLER_INIT()
#define SAMPLER_DEINIT()
#define SAMPLER_RESPONSIVENESS(time)
#define SAMPLE_CHECKPOINT(name_space, info)
#define SAMPLE_MARKER(info)

// Redefine the macros for platforms where SPS is supported.
#ifdef ANDROID

#include "sps_sampler.h"

#endif

#endif
