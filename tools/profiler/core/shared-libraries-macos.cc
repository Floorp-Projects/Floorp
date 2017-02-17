/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <AvailabilityMacros.h>
#include <mach-o/loader.h>
#include <mach-o/dyld_images.h>
#include <mach/task_info.h>
#include <mach/task.h>
#include <mach/mach_init.h>
#include <mach/mach_traps.h>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <sstream>

#include "shared-libraries.h"

// Architecture specific abstraction.
#ifdef __i386__
typedef mach_header platform_mach_header;
typedef segment_command mach_segment_command_type;
#define MACHO_MAGIC_NUMBER MH_MAGIC
#define CMD_SEGMENT LC_SEGMENT
#define seg_size uint32_t
#else
typedef mach_header_64 platform_mach_header;
typedef segment_command_64 mach_segment_command_type;
#define MACHO_MAGIC_NUMBER MH_MAGIC_64
#define CMD_SEGMENT LC_SEGMENT_64
#define seg_size uint64_t
#endif

static
void addSharedLibrary(const platform_mach_header* header, char *name, SharedLibraryInfo &info) {
  const struct load_command *cmd =
    reinterpret_cast<const struct load_command *>(header + 1);

  seg_size size = 0;
  unsigned long long start = reinterpret_cast<unsigned long long>(header);
  // Find the cmd segment in the macho image. It will contain the offset we care about.
  const uint8_t *uuid_bytes = nullptr;
  for (unsigned int i = 0;
       cmd && (i < header->ncmds) && (uuid_bytes == nullptr || size == 0);
       ++i) {
    if (cmd->cmd == CMD_SEGMENT) {
      const mach_segment_command_type *seg =
        reinterpret_cast<const mach_segment_command_type *>(cmd);

      if (!strcmp(seg->segname, "__TEXT")) {
        size = seg->vmsize;
      }
    } else if (cmd->cmd == LC_UUID) {
      const uuid_command *ucmd = reinterpret_cast<const uuid_command *>(cmd);
      uuid_bytes = ucmd->uuid;
    }

    cmd = reinterpret_cast<const struct load_command *>
      (reinterpret_cast<const char *>(cmd) + cmd->cmdsize);
  }

  std::stringstream uuid;
  uuid << std::hex << std::uppercase;
  if (uuid_bytes != nullptr) {
    for (int i = 0; i < 16; ++i) {
      uuid << ((uuid_bytes[i] & 0xf0) >> 4);
      uuid << (uuid_bytes[i] & 0xf);
    }
    uuid << '0';
  }

  info.AddSharedLibrary(SharedLibrary(start, start + size, 0, uuid.str(),
                                      name));
}

// Use dyld to inspect the macho image information. We can build the SharedLibraryEntry structure
// giving us roughtly the same info as /proc/PID/maps in Linux.
SharedLibraryInfo SharedLibraryInfo::GetInfoForSelf()
{
  SharedLibraryInfo sharedLibraryInfo;

  task_dyld_info_data_t task_dyld_info;
  mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
  if (task_info(mach_task_self (), TASK_DYLD_INFO, (task_info_t)&task_dyld_info,
                &count) != KERN_SUCCESS) {
    return sharedLibraryInfo;
  }

  struct dyld_all_image_infos* aii = (struct dyld_all_image_infos*)task_dyld_info.all_image_info_addr;
  size_t infoCount = aii->infoArrayCount;

  // Iterate through all dyld images (loaded libraries) to get their names
  // and offests.
  for (size_t i = 0; i < infoCount; ++i) {
    const dyld_image_info *info = &aii->infoArray[i];

    // If the magic number doesn't match then go no further
    // since we're not pointing to where we think we are.
    if (info->imageLoadAddress->magic != MACHO_MAGIC_NUMBER) {
      continue;
    }

    const platform_mach_header* header =
      reinterpret_cast<const platform_mach_header*>(info->imageLoadAddress);

    // Add the entry for this image.
    addSharedLibrary(header, (char*)info->imageFilePath, sharedLibraryInfo);

  }
  return sharedLibraryInfo;
}

