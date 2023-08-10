/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Module_h
#define mozilla_Module_h

#include "nscore.h"

namespace mozilla {

namespace Module {
/**
 * This selector allows components to be marked so that they're only loaded
 * into certain kinds of processes. Selectors can be combined.
 */
// Note: This must be kept in sync with the selector matching in
// nsComponentManager.cpp.
enum ProcessSelector {
  ANY_PROCESS = 0,
  MAIN_PROCESS_ONLY = 1 << 0,
  CONTENT_PROCESS_ONLY = 1 << 1,

  /**
   * By default, modules are not loaded in the GPU, VR, Socket, RDD, Utility,
   * GMPlugin and IPDLUnitTest processes, even if ANY_PROCESS is specified.
   * This flag enables a module in the relevant process.
   *
   * NOTE: IPDLUnitTest does not have its own flag, and will only load a
   * module if it is enabled in all processes.
   */
  ALLOW_IN_GPU_PROCESS = 1 << 2,
  ALLOW_IN_VR_PROCESS = 1 << 3,
  ALLOW_IN_SOCKET_PROCESS = 1 << 4,
  ALLOW_IN_RDD_PROCESS = 1 << 5,
  ALLOW_IN_UTILITY_PROCESS = 1 << 6,
  ALLOW_IN_GMPLUGIN_PROCESS = 1 << 7,
  ALLOW_IN_GPU_AND_MAIN_PROCESS = ALLOW_IN_GPU_PROCESS | MAIN_PROCESS_ONLY,
  ALLOW_IN_GPU_AND_VR_PROCESS = ALLOW_IN_GPU_PROCESS | ALLOW_IN_VR_PROCESS,
  ALLOW_IN_GPU_AND_SOCKET_PROCESS =
      ALLOW_IN_GPU_PROCESS | ALLOW_IN_SOCKET_PROCESS,
  ALLOW_IN_GPU_VR_AND_SOCKET_PROCESS =
      ALLOW_IN_GPU_PROCESS | ALLOW_IN_VR_PROCESS | ALLOW_IN_SOCKET_PROCESS,
  ALLOW_IN_RDD_AND_SOCKET_PROCESS =
      ALLOW_IN_RDD_PROCESS | ALLOW_IN_SOCKET_PROCESS,
  ALLOW_IN_GPU_RDD_AND_SOCKET_PROCESS =
      ALLOW_IN_GPU_PROCESS | ALLOW_IN_RDD_PROCESS | ALLOW_IN_SOCKET_PROCESS,
  ALLOW_IN_GPU_RDD_SOCKET_AND_UTILITY_PROCESS =
      ALLOW_IN_GPU_PROCESS | ALLOW_IN_RDD_PROCESS | ALLOW_IN_SOCKET_PROCESS,
  ALLOW_IN_GPU_RDD_VR_AND_SOCKET_PROCESS =
      ALLOW_IN_GPU_PROCESS | ALLOW_IN_RDD_PROCESS | ALLOW_IN_VR_PROCESS |
      ALLOW_IN_SOCKET_PROCESS,
  ALLOW_IN_GPU_RDD_VR_SOCKET_AND_UTILITY_PROCESS =
      ALLOW_IN_GPU_PROCESS | ALLOW_IN_RDD_PROCESS | ALLOW_IN_VR_PROCESS |
      ALLOW_IN_SOCKET_PROCESS | ALLOW_IN_UTILITY_PROCESS,
  ALLOW_IN_GPU_RDD_VR_SOCKET_UTILITY_AND_GMPLUGIN_PROCESS =
      ALLOW_IN_GPU_PROCESS | ALLOW_IN_RDD_PROCESS | ALLOW_IN_VR_PROCESS |
      ALLOW_IN_SOCKET_PROCESS | ALLOW_IN_UTILITY_PROCESS |
      ALLOW_IN_GMPLUGIN_PROCESS
};

static constexpr size_t kMaxProcessSelector = size_t(
    ProcessSelector::ALLOW_IN_GPU_RDD_VR_SOCKET_UTILITY_AND_GMPLUGIN_PROCESS);

/**
 * This allows category entries to be marked so that they are or are
 * not loaded when in backgroundtask mode.
 */
// Note: This must be kept in sync with the selector matching in
// StaticComponents.cpp.in.
enum BackgroundTasksSelector {
  NO_TASKS = 0x0,
  ALL_TASKS = 0xFFFF,
};
};  // namespace Module

}  // namespace mozilla

#endif  // mozilla_Module_h
