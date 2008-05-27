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

//  dynamic_images.h
//
//    Implements most of the function of the dyld API, but allowing an
//    arbitrary task to be introspected, unlike the dyld API which
//    only allows operation on the current task.  The current implementation
//    is limited to use by 32-bit tasks.

#ifndef CLIENT_MAC_HANDLER_DYNAMIC_IMAGES_H__
#define CLIENT_MAC_HANDLER_DYNAMIC_IMAGES_H__

#include <mach/mach.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <sys/types.h>
#include <vector>

namespace google_breakpad {

using std::vector;

//==============================================================================
// The memory layout of this struct matches the dyld_image_info struct
// defined in "dyld_gdb.h" in the darwin source.
typedef struct dyld_image_info {
  struct mach_header        *load_address_;
  char                      *file_path_;
  uintptr_t                 file_mod_date_;
} dyld_image_info;

//==============================================================================
// This is as defined in "dyld_gdb.h" in the darwin source.
// _dyld_all_image_infos (in dyld) is a structure of this type
// which will be used to determine which dynamic code has been loaded.
typedef struct dyld_all_image_infos {
  uint32_t                      version;  // == 1 in Mac OS X 10.4
  uint32_t                      infoArrayCount;
  const struct dyld_image_info  *infoArray;
  void*                         notification;
  bool                          processDetachedFromSharedRegion;
} dyld_all_image_infos;

//==============================================================================
// A simple wrapper for a mach_header
//
// This could be fleshed out with some more interesting methods.
class MachHeader {
 public:
  explicit MachHeader(const mach_header &header) : header_(header) {}

  void Print() {
    printf("magic\t\t: %4x\n", header_.magic);
    printf("cputype\t\t: %d\n", header_.cputype);
    printf("cpusubtype\t: %d\n", header_.cpusubtype);
    printf("filetype\t: %d\n", header_.filetype);
    printf("ncmds\t\t: %d\n", header_.ncmds);
    printf("sizeofcmds\t: %d\n", header_.sizeofcmds);
    printf("flags\t\t: %d\n", header_.flags);
  }

  mach_header   header_;
};

//==============================================================================
// Represents a single dynamically loaded mach-o image
class DynamicImage {
 public:
  DynamicImage(mach_header *header,       // we take ownership
               int header_size,           // includes load commands
               mach_header *load_address,
               char *inFilePath,
               uintptr_t image_mod_date,
               mach_port_t task)
    : header_(header),
      header_size_(header_size),
      load_address_(load_address),
      file_mod_date_(image_mod_date),
      task_(task) {
    InitializeFilePath(inFilePath);
    CalculateMemoryInfo();
  }

  ~DynamicImage() {
    if (file_path_) {
      free(file_path_);
    }
    free(header_);
  }

  // Returns pointer to a local copy of the mach_header plus load commands
  mach_header *GetMachHeader() {return header_;}

  // Size of mach_header plus load commands
  int GetHeaderSize() const {return header_size_;}

  // Full path to mach-o binary
  char *GetFilePath() {return file_path_;}

  uintptr_t GetModDate() const {return file_mod_date_;}

  // Actual address where the image was loaded
  mach_header *GetLoadAddress() const {return load_address_;}

  // Address where the image should be loaded
  uint32_t GetVMAddr() const {return vmaddr_;}

  // Difference between GetLoadAddress() and GetVMAddr()
  ptrdiff_t GetVMAddrSlide() const {return slide_;}

  // Size of the image
  uint32_t GetVMSize() const {return vmsize_;}

  // Task owning this loaded image
  mach_port_t GetTask() {return task_;}

  // For sorting
  bool operator<(const DynamicImage &inInfo) {
    return GetLoadAddress() < inInfo.GetLoadAddress();
  }

  // Debugging
  void Print();
 
 private:
  friend class DynamicImages;

  // Sanity checking
  bool IsValid() {return GetVMSize() != 0;}

  // Makes local copy of file path to mach-o binary
  void InitializeFilePath(char *inFilePath) {
    if (inFilePath) {
      size_t path_size = 1 + strlen(inFilePath);
      file_path_ = reinterpret_cast<char*>(malloc(path_size));
      strlcpy(file_path_, inFilePath, path_size);
    } else {
      file_path_ = NULL;
    }
  }

  // Initializes vmaddr_, vmsize_, and slide_
  void CalculateMemoryInfo();

#if 0   // currently not needed
  // Copy constructor: we don't want this to be invoked,
  // but here's the code in case we need to make it public some day.
  DynamicImage(DynamicImage &inInfo)
    : load_address_(inInfo.load_address_),
      vmaddr_(inInfo.vmaddr_),
      vmsize_(inInfo.vmsize_),
      slide_(inInfo.slide_),
      file_mod_date_(inInfo.file_mod_date_),
      task_(inInfo.task_) {
    // copy file path string
    InitializeFilePath(inInfo.GetFilePath());

    // copy mach_header and load commands
    header_ = reinterpret_cast<mach_header*>(malloc(inInfo.header_size_));
    memcpy(header_, inInfo.header_, inInfo.header_size_);
    header_size_ = inInfo.header_size_;
  }
#endif

  mach_header          *header_;        // our local copy of the header
  int                   header_size_;    // mach_header plus load commands
  mach_header          *load_address_;  // base address image is mapped into
  uint32_t             vmaddr_;
  uint32_t             vmsize_;
  ptrdiff_t            slide_;

  char                 *file_path_;     // path dyld used to load the image
  uintptr_t            file_mod_date_;  // time_t of image file

  mach_port_t          task_;
};

//==============================================================================
// DynamicImageRef is just a simple wrapper for a pointer to
// DynamicImage.  The reason we use it instead of a simple typedef is so
// that we can use stl::sort() on a vector of DynamicImageRefs
// and simple class pointers can't implement operator<().
//
class DynamicImageRef {
 public:
  explicit DynamicImageRef(DynamicImage *inP) : p(inP) {}
  DynamicImageRef(const DynamicImageRef &inRef) : p(inRef.p) {}  // STL required

  bool operator<(const DynamicImageRef &inRef) const {
    return (*const_cast<DynamicImageRef*>(this)->p)
      < (*const_cast<DynamicImageRef&>(inRef).p);
  }

  // Be just like DynamicImage*
  DynamicImage  *operator->() {return p;}
  operator DynamicImage*() {return p;}

 private:
  DynamicImage  *p;
};

//==============================================================================
// An object of type DynamicImages may be created to allow introspection of
// an arbitrary task's dynamically loaded mach-o binaries.  This makes the
// assumption that the current task has send rights to the target task.
class DynamicImages {
 public:
  explicit DynamicImages(mach_port_t task);

  ~DynamicImages() {
    for (int i = 0; i < (int)image_list_.size(); ++i) {
      delete image_list_[i];
    }
  }

  // Returns the number of dynamically loaded mach-o images.
  int GetImageCount() const {return image_list_.size();}

  // Returns an individual image.
  DynamicImage *GetImage(int i) {
    if (i < (int)image_list_.size()) {
      return image_list_[i];
    }
    return NULL;
  }

  // Returns the image corresponding to the main executable.
  DynamicImage *GetExecutableImage();
  int GetExecutableImageIndex();

  // Returns the task which we're looking at.
  mach_port_t GetTask() const {return task_;}

  // Debugging
  void Print() {
    for (int i = 0; i < (int)image_list_.size(); ++i) {
      image_list_[i]->Print();
    }
  }

  void TestPrint() {
    for (int i = 0; i < (int)image_list_.size(); ++i) {
      printf("dyld: %p: name = %s\n", _dyld_get_image_header(i),
        _dyld_get_image_name(i) );
      const mach_header *header = _dyld_get_image_header(i);
      MachHeader(*header).Print();
    }
  }

 private:
  bool IsOurTask() {return task_ == mach_task_self();}

  // Initialization
  void ReadImageInfoForTask();

  mach_port_t              task_;
  vector<DynamicImageRef>  image_list_;
};

// Returns a malloced block containing the contents of memory at a particular
// location in another task.
void* ReadTaskMemory(task_port_t target_task, const void* address, size_t len);

}   // namespace google_breakpad

#endif // CLIENT_MAC_HANDLER_DYNAMIC_IMAGES_H__
