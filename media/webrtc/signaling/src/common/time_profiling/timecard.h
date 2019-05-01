/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef timecard_h__
#define timecard_h__

#include <stdlib.h>
#include "prtime.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STAMP_TIMECARD(card, event)                                      \
  do {                                                                   \
    if (card) {                                                          \
      stamp_timecard((card), (event), __FILE__, __LINE__, __FUNCTION__); \
    }                                                                    \
  } while (0)

#define TIMECARD_INITIAL_TABLE_SIZE 16

/*
 * The "const char *" members of this structure point to static strings.
 * We do not own them, and should not attempt to deallocate them.
 */

typedef struct {
  PRTime timestamp;
  const char* event;
  const char* file;
  unsigned int line;
  const char* function;
} TimecardEntry;

typedef struct Timecard {
  size_t curr_entry;
  size_t entries_allocated;
  TimecardEntry* entries;
  PRTime start_time;
} Timecard;

/**
 * Creates a new Timecard structure for tracking events.
 */
Timecard* create_timecard();

/**
 * Frees the memory associated with a timecard. After returning, the
 * timecard pointed to by tc is no longer valid.
 */
void destroy_timecard(Timecard* tc);

/**
 * Records a new event in the indicated timecard. This should not be
 * called directly; code should instead use the STAMP_TIMECARD macro,
 * above.
 */
void stamp_timecard(Timecard* tc, const char* event, const char* file,
                    unsigned int line, const char* function);

/**
 * Formats and outputs the contents of a timecard onto stdout.
 */
void print_timecard(Timecard* tc);

#ifdef __cplusplus
}
#endif

#endif
