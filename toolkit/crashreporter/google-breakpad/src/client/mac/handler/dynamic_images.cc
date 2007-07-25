// Copyright (c) 2007, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <mach-o/nlist.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>

#include "client/mac/handler/dynamic_images.h"

namespace google_breakpad {

//==============================================================================
// Reads an address range from another task.  A block of memory is malloced
// and should be freed by the caller.
void* ReadTaskMemory(task_port_t target_task,
                     const void* address,
                     size_t length) {
  void* result = NULL;
  vm_address_t page_address = reinterpret_cast<vm_address_t>(address) & (-4096);
  vm_address_t last_page_address =
    (reinterpret_cast<vm_address_t>(address) + length + 4095) & (-4096);
  vm_size_t page_size = last_page_address - page_address;
  uint8_t* local_start;
  uint32_t local_length;

  kern_return_t r = vm_read(target_task,
                            page_address,
                            page_size,
                            reinterpret_cast<vm_offset_t*>(&local_start),
                            &local_length);

  if (r == KERN_SUCCESS) {
    result = malloc(length);
    if (result != NULL) {
      memcpy(result, &local_start[(uint32_t)address - page_address], length);
    }
    vm_deallocate(mach_task_self(), (uintptr_t)local_start, local_length);
  }

  return result;
}

#pragma mark -

//==============================================================================
// Initializes vmaddr_, vmsize_, and slide_
void DynamicImage::CalculateMemoryInfo() {
  mach_header *header = GetMachHeader();

  const struct load_command *cmd =
    reinterpret_cast<const struct load_command *>(header + 1);

  for (unsigned int i = 0; cmd && (i < header->ncmds); ++i) {
    if (cmd->cmd == LC_SEGMENT) {
      const struct segment_command *seg =
        reinterpret_cast<const struct segment_command *>(cmd);

      if (!strcmp(seg->segname, "__TEXT")) {
        vmaddr_ = seg->vmaddr;
        vmsize_ = seg->vmsize;
        slide_ = 0;
        
        if (seg->fileoff == 0  &&  seg->filesize != 0) {
          slide_ = (uintptr_t)GetLoadAddress() - (uintptr_t)seg->vmaddr;
        }
        return;
      }
    }

    cmd = reinterpret_cast<const struct load_command *>
      (reinterpret_cast<const char *>(cmd) + cmd->cmdsize);
  }
  
  // we failed - a call to IsValid() will return false
  vmaddr_ = 0;
  vmsize_ = 0;
  slide_ = 0;
}

#pragma mark -

//==============================================================================
// Loads information about dynamically loaded code in the given task.
DynamicImages::DynamicImages(mach_port_t task)
  : task_(task) {
  ReadImageInfoForTask();
}

//==============================================================================
// This code was written using dyld_debug.c (from Darwin) as a guide.
void DynamicImages::ReadImageInfoForTask() {
  struct nlist l[8];
  memset(l, 0, sizeof(l) );
  
  // First we lookup the address of the "_dyld_all_image_infos" struct
  // which lives in "dyld".  This structure contains information about all
  // of the loaded dynamic images.
  struct nlist &list = l[0];
  list.n_un.n_name = "_dyld_all_image_infos";
  nlist("/usr/lib/dyld", &list);
  
  if (list.n_value) {
    // Read the structure inside of dyld that contains information about
    // loaded images.  We're reading from the desired task's address space.

    // Here we make the assumption that dyld loaded at the same address in
    // the crashed process vs. this one.  This is an assumption made in
    // "dyld_debug.c" and is said to be nearly always valid.
    dyld_all_image_infos *dyldInfo = reinterpret_cast<dyld_all_image_infos*>
      (ReadTaskMemory(task_,
                      reinterpret_cast<void*>(list.n_value),
                      sizeof(dyld_all_image_infos)));

    if (dyldInfo) {
      // number of loaded images
      int count = dyldInfo->infoArrayCount;

      // Read an array of dyld_image_info structures each containing
      // information about a loaded image.
      dyld_image_info *infoArray = reinterpret_cast<dyld_image_info*>
        (ReadTaskMemory(task_,
                        dyldInfo->infoArray,
                        count*sizeof(dyld_image_info)));

      image_list_.reserve(count);
      
      for (int i = 0; i < count; ++i) {
        dyld_image_info &info = infoArray[i];

        // First read just the mach_header from the image in the task.
        mach_header *header = reinterpret_cast<mach_header*>
          (ReadTaskMemory(task_, info.load_address_, sizeof(mach_header)));

        if (!header)
          break;   // bail on this dynamic image
        
        // Now determine the total amount we really want to read based on the
        // size of the load commands.  We need the header plus all of the 
        // load commands.
        unsigned int header_size = sizeof(mach_header) + header->sizeofcmds;
        free(header);

        header = reinterpret_cast<mach_header*>
          (ReadTaskMemory(task_, info.load_address_, header_size));
        
        // Read the file name from the task's memory space.
        char *file_path = NULL;
        if (info.file_path_) {
          // Although we're reading 0x2000 bytes, this is copied in the
          // the DynamicImage constructor below with the correct string length,
          // so it's not really wasting memory.
          file_path = reinterpret_cast<char*>
            (ReadTaskMemory(task_,
                            info.file_path_,
                            0x2000));
        }
        
        // Create an object representing this image and add it to our list.
        DynamicImage *new_image = new DynamicImage(header,
                                                   header_size,
                                                   info.load_address_,
                                                   file_path,
                                                   info.file_mod_date_,
                                                   task_);

        if (new_image->IsValid()) {
          image_list_.push_back(DynamicImageRef(new_image));
        } else {
          delete new_image;
        }
        
        if (file_path) {
          free(file_path);
        }
      }
      
      free(dyldInfo);
      free(infoArray);
      
      // sorts based on loading address
      sort(image_list_.begin(), image_list_.end() );
    }
  }
}

//==============================================================================
DynamicImage  *DynamicImages::GetExecutableImage() {
  int executable_index = GetExecutableImageIndex();
  
  if (executable_index >= 0) {
    return GetImage(executable_index);
  }
  
  return NULL;
}

//==============================================================================
// returns -1 if failure to find executable
int DynamicImages::GetExecutableImageIndex() {
  int image_count = GetImageCount();

  for (int i = 0; i < image_count; ++i) {
    DynamicImage  *image = GetImage(i);
    if (image->GetMachHeader()->filetype == MH_EXECUTE) {
      return i;
    }
  }

  return -1;
}

}  // namespace google_breakpad
