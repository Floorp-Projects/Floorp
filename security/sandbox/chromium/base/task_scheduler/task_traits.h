// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SCHEDULER_TASK_TRAITS_H_
#define BASE_TASK_SCHEDULER_TASK_TRAITS_H_

#include <stdint.h>

#include <iosfwd>

#include "base/base_export.h"
#include "build/build_config.h"

namespace base {

// Valid priorities supported by the task scheduler. Note: internal algorithms
// depend on priorities being expressed as a continuous zero-based list from
// lowest to highest priority. Users of this API shouldn't otherwise care about
// nor use the underlying values.
enum class TaskPriority {
  // This will always be equal to the lowest priority available.
  LOWEST = 0,
  // User won't notice if this task takes an arbitrarily long time to complete.
  BACKGROUND = LOWEST,
  // This task affects UI or responsiveness of future user interactions. It is
  // not an immediate response to a user interaction.
  // Examples:
  // - Updating the UI to reflect progress on a long task.
  // - Loading data that might be shown in the UI after a future user
  //   interaction.
  USER_VISIBLE,
  // This task affects UI immediately after a user interaction.
  // Example: Generating data shown in the UI immediately after a click.
  USER_BLOCKING,
  // This will always be equal to the highest priority available.
  HIGHEST = USER_BLOCKING,
};

// Valid shutdown behaviors supported by the task scheduler.
enum class TaskShutdownBehavior {
  // Tasks posted with this mode which have not started executing before
  // shutdown is initiated will never run. Tasks with this mode running at
  // shutdown will be ignored (the worker will not be joined).
  //
  // This option provides a nice way to post stuff you don't want blocking
  // shutdown. For example, you might be doing a slow DNS lookup and if it's
  // blocked on the OS, you may not want to stop shutdown, since the result
  // doesn't really matter at that point.
  //
  // However, you need to be very careful what you do in your callback when you
  // use this option. Since the thread will continue to run until the OS
  // terminates the process, the app can be in the process of tearing down when
  // you're running. This means any singletons or global objects you use may
  // suddenly become invalid out from under you. For this reason, it's best to
  // use this only for slow but simple operations like the DNS example.
  CONTINUE_ON_SHUTDOWN,

  // Tasks posted with this mode that have not started executing at
  // shutdown will never run. However, any task that has already begun
  // executing when shutdown is invoked will be allowed to continue and
  // will block shutdown until completion.
  //
  // Note: Because TaskScheduler::Shutdown() may block while these tasks are
  // executing, care must be taken to ensure that they do not block on the
  // thread that called TaskScheduler::Shutdown(), as this may lead to deadlock.
  SKIP_ON_SHUTDOWN,

  // Tasks posted with this mode before shutdown is complete will block shutdown
  // until they're executed. Generally, this should be used only to save
  // critical user data.
  //
  // Note: Tasks with BACKGROUND priority that block shutdown will be promoted
  // to USER_VISIBLE priority during shutdown.
  BLOCK_SHUTDOWN,
};

// Describes metadata for a single task or a group of tasks.
class BASE_EXPORT TaskTraits {
 public:
  // Constructs a default TaskTraits for tasks with
  //     (1) no I/O,
  //     (2) low priority, and
  //     (3) may block shutdown or be skipped on shutdown.
  // Tasks that require stricter guarantees should highlight those by requesting
  // explicit traits below.
  TaskTraits();
  TaskTraits(const TaskTraits& other) = default;
  TaskTraits& operator=(const TaskTraits& other) = default;
  ~TaskTraits();

  // Allows tasks with these traits to do file I/O.
  TaskTraits& WithFileIO();

  // Applies |priority| to tasks with these traits.
  TaskTraits& WithPriority(TaskPriority priority);

  // Applies |shutdown_behavior| to tasks with these traits.
  TaskTraits& WithShutdownBehavior(TaskShutdownBehavior shutdown_behavior);

  // Returns true if file I/O is allowed by these traits.
  bool with_file_io() const { return with_file_io_; }

  // Returns the priority of tasks with these traits.
  TaskPriority priority() const { return priority_; }

  // Returns the shutdown behavior of tasks with these traits.
  TaskShutdownBehavior shutdown_behavior() const { return shutdown_behavior_; }

 private:
  bool with_file_io_;
  TaskPriority priority_;
  TaskShutdownBehavior shutdown_behavior_;
};

// Returns string literals for the enums defined in this file. These methods
// should only be used for tracing and debugging.
BASE_EXPORT const char* TaskPriorityToString(TaskPriority task_priority);
BASE_EXPORT const char* TaskShutdownBehaviorToString(
    TaskShutdownBehavior task_priority);

// Stream operators so that the enums defined in this file can be used in
// DCHECK and EXPECT statements.
BASE_EXPORT std::ostream& operator<<(std::ostream& os,
                                     const TaskPriority& shutdown_behavior);
BASE_EXPORT std::ostream& operator<<(
    std::ostream& os,
    const TaskShutdownBehavior& shutdown_behavior);

}  // namespace base

#endif  // BASE_TASK_SCHEDULER_TASK_TRAITS_H_
