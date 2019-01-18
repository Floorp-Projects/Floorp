/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Win64ModuleUnwindMetadata_h
#define Win64ModuleUnwindMetadata_h

#if XP_WIN && HAVE_64BIT_BUILD

#  include <functional>
#  include <map>
#  include <string>

#  include <windows.h>
#  include <winnt.h>
#  include <imagehlp.h>

namespace CrashReporter {

struct UnwindCFI {
  uint32_t beginAddress;
  uint32_t size;
  uint32_t stackSize;
  uint32_t ripOffset;
};

// Does lazy-parsing of unwind info.
class ModuleUnwindParser {
  PLOADED_IMAGE mImg;
  std::string mPath;

  // Maps begin address to exception record.
  // Populated upon construction.
  std::map<DWORD, PIMAGE_RUNTIME_FUNCTION_ENTRY> mUnwindMap;

  // Maps begin address to CFI.
  // Populated as needed.
  std::map<DWORD, UnwindCFI> mCFIMap;

  bool GenerateCFIForFunction(IMAGE_RUNTIME_FUNCTION_ENTRY& aFunc,
                              UnwindCFI& aRet);
  void* RvaToVa(ULONG aRva);

 public:
  explicit ModuleUnwindParser(const std::string& aPath);
  ~ModuleUnwindParser();
  bool GetCFI(DWORD aAddress, UnwindCFI& aRet);
  DWORD GetAnyOffsetAddr() const;
};

}  // namespace CrashReporter

#endif  // XP_WIN && HAVE_64BIT_BUILD

#endif  // Win64ModuleUnwindMetadata_h
