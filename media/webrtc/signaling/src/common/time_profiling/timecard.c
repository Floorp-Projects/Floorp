/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <string.h>
#include "timecard.h"
#include "mozilla/mozalloc.h"

Timecard* create_timecard() {
  Timecard* tc = moz_xcalloc(1, sizeof(Timecard));
  tc->entries_allocated = TIMECARD_INITIAL_TABLE_SIZE;
  tc->entries = moz_xcalloc(tc->entries_allocated, sizeof(TimecardEntry));
  tc->start_time = PR_Now();
  return tc;
}

void destroy_timecard(Timecard* tc) {
  free(tc->entries);
  free(tc);
}

void stamp_timecard(Timecard* tc, const char* event, const char* file,
                    unsigned int line, const char* function) {
  TimecardEntry* entry = NULL;

  /* Trim the path component from the filename */
  const char* last_slash = file;
  while (*file) {
    if (*file == '/' || *file == '\\') {
      last_slash = file;
    }
    file++;
  }
  file = last_slash;
  if (*file == '/' || *file == '\\') {
    file++;
  }

  /* Ensure there is enough space left in the entries list */
  if (tc->curr_entry == tc->entries_allocated) {
    tc->entries_allocated *= 2;
    tc->entries = moz_xrealloc(tc->entries,
                               tc->entries_allocated * sizeof(TimecardEntry));
  }

  /* Record the data into the timecard entry */
  entry = &tc->entries[tc->curr_entry];
  entry->timestamp = PR_Now();
  entry->event = event;
  entry->file = file;
  entry->line = line;
  entry->function = function;
  tc->curr_entry++;
}

void print_timecard(Timecard* tc) {
  size_t i;
  TimecardEntry* entry;
  size_t event_width = 5;
  size_t file_width = 4;
  size_t function_width = 8;
  size_t line_width;
  PRTime offset, delta;

  for (i = 0; i < tc->curr_entry; i++) {
    entry = &tc->entries[i];
    if (strlen(entry->event) > event_width) {
      event_width = strlen(entry->event);
    }
    if (strlen(entry->file) > file_width) {
      file_width = strlen(entry->file);
    }
    if (strlen(entry->function) > function_width) {
      function_width = strlen(entry->function);
    }
  }

  printf("\nTimecard created %4ld.%6.6ld\n\n",
         (long)(tc->start_time / PR_USEC_PER_SEC),
         (long)(tc->start_time % PR_USEC_PER_SEC));

  line_width =
      1 + 11 + 11 + event_width + file_width + 6 + function_width + (4 * 3);

  printf(" %-11s | %-11s | %-*s | %-*s | %-*s\n", "Timestamp", "Delta",
         (int)event_width, "Event", (int)file_width + 6, "File",
         (int)function_width, "Function");

  for (i = 0; i <= line_width; i++) {
    printf("=");
  }
  printf("\n");

  for (i = 0; i < tc->curr_entry; i++) {
    entry = &tc->entries[i];
    offset = entry->timestamp - tc->start_time;
    if (i > 0) {
      delta = entry->timestamp - tc->entries[i - 1].timestamp;
    } else {
      delta = entry->timestamp - tc->start_time;
    }
    printf(" %4ld.%6.6ld | %4ld.%6.6ld | %-*s | %*s:%-5d | %-*s\n",
           (long)(offset / PR_USEC_PER_SEC), (long)(offset % PR_USEC_PER_SEC),
           (long)(delta / PR_USEC_PER_SEC), (long)(delta % PR_USEC_PER_SEC),
           (int)event_width, entry->event, (int)file_width, entry->file,
           entry->line, (int)function_width, entry->function);
  }
  printf("\n");
}
