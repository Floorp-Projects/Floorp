/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _DEBUGGERD_BACKTRACE_H
#define _DEBUGGERD_BACKTRACE_H

#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#include <corkscrew/ptrace.h>

/* Dumps a backtrace using a format similar to what Dalvik uses so that the result
 * can be intermixed in a bug report. */
void dump_backtrace(int fd, int amfd, pid_t pid, pid_t tid, bool* detach_failed,
        int* total_sleep_time_usec);

#endif // _DEBUGGERD_BACKTRACE_H
