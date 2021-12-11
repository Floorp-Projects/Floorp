/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if XP_WIN && HAVE_64BIT_BUILD

#  include "Win64ModuleUnwindMetadata.h"

#  include "MinidumpAnalyzerUtils.h"

#  include <windows.h>
#  include <winnt.h>
#  include <imagehlp.h>
#  include <iostream>
#  include <set>
#  include <sstream>
#  include <string>

namespace CrashReporter {

union UnwindCode {
  struct {
    uint8_t offset_in_prolog;
    uint8_t unwind_operation_code : 4;
    uint8_t operation_info : 4;
  };
  USHORT frame_offset;
};

enum UnwindOperationCodes {
  UWOP_PUSH_NONVOL = 0,      // info == register number
  UWOP_ALLOC_LARGE = 1,      // no info, alloc size in next 2 slots
  UWOP_ALLOC_SMALL = 2,      // info == size of allocation / 8 - 1
  UWOP_SET_FPREG = 3,        // no info, FP = RSP + UNWIND_INFO.FPRegOffset*16
  UWOP_SAVE_NONVOL = 4,      // info == register number, offset in next slot
  UWOP_SAVE_NONVOL_FAR = 5,  // info == register number, offset in next 2 slots
  UWOP_SAVE_XMM = 6,         // Version 1; undocumented
  UWOP_EPILOG = 6,           // Version 2; undocumented
  UWOP_SAVE_XMM_FAR = 7,     // Version 1; undocumented
  UWOP_SPARE = 7,            // Version 2; undocumented
  UWOP_SAVE_XMM128 = 8,      // info == XMM reg number, offset in next slot
  UWOP_SAVE_XMM128_FAR = 9,  // info == XMM reg number, offset in next 2 slots
  UWOP_PUSH_MACHFRAME = 10   // info == 0: no error-code, 1: error-code
};

struct UnwindInfo {
  uint8_t version : 3;
  uint8_t flags : 5;
  uint8_t size_of_prolog;
  uint8_t count_of_codes;
  uint8_t frame_register : 4;
  uint8_t frame_offset : 4;
  UnwindCode unwind_code[1];
};

ModuleUnwindParser::~ModuleUnwindParser() {
  if (mImg) {
    ImageUnload(mImg);
  }
}

void* ModuleUnwindParser::RvaToVa(ULONG aRva) {
  return ImageRvaToVa(mImg->FileHeader, mImg->MappedAddress, aRva,
                      &mImg->LastRvaSection);
}

ModuleUnwindParser::ModuleUnwindParser(const std::string& aPath)
    : mPath(aPath) {
  // Convert wchar to native charset because ImageLoad only takes
  // a PSTR as input.
  std::string code_file = UTF8ToMBCS(aPath);

  mImg = ImageLoad((PSTR)code_file.c_str(), NULL);
  if (!mImg || !mImg->FileHeader) {
    return;
  }

  PIMAGE_OPTIONAL_HEADER64 optional_header = &mImg->FileHeader->OptionalHeader;
  if (optional_header->Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
    return;
  }

  DWORD exception_rva =
      optional_header->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION]
          .VirtualAddress;

  DWORD exception_size =
      optional_header->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION].Size;

  auto funcs = (PIMAGE_RUNTIME_FUNCTION_ENTRY)RvaToVa(exception_rva);
  if (!funcs) {
    return;
  }

  for (DWORD i = 0; i < exception_size / sizeof(*funcs); i++) {
    mUnwindMap[funcs[i].BeginAddress] = &funcs[i];
  }
}

bool ModuleUnwindParser::GenerateCFIForFunction(
    IMAGE_RUNTIME_FUNCTION_ENTRY& aFunc, UnwindCFI& aRet) {
  DWORD unwind_rva = aFunc.UnwindInfoAddress;
  // Holds RVA to all visited IMAGE_RUNTIME_FUNCTION_ENTRY, to avoid
  // circular references.
  std::set<DWORD> visited;

  // Follow chained function entries
  while (unwind_rva & 0x1) {
    unwind_rva ^= 0x1;

    if (visited.end() != visited.find(unwind_rva)) {
      return false;
    }
    visited.insert(unwind_rva);

    auto chained_func = (PIMAGE_RUNTIME_FUNCTION_ENTRY)RvaToVa(unwind_rva);
    if (!chained_func) {
      return false;
    }
    unwind_rva = chained_func->UnwindInfoAddress;
  }

  visited.insert(unwind_rva);

  auto unwind_info = (UnwindInfo*)RvaToVa(unwind_rva);
  if (!unwind_info) {
    return false;
  }

  DWORD stack_size = 8;  // minimal stack size is 8 for RIP
  DWORD rip_offset = 8;
  do {
    for (uint8_t c = 0; c < unwind_info->count_of_codes; c++) {
      UnwindCode* unwind_code = &unwind_info->unwind_code[c];
      switch (unwind_code->unwind_operation_code) {
        case UWOP_PUSH_NONVOL: {
          stack_size += 8;
          break;
        }
        case UWOP_ALLOC_LARGE: {
          if (unwind_code->operation_info == 0) {
            c++;
            if (c < unwind_info->count_of_codes) {
              stack_size += (unwind_code + 1)->frame_offset * 8;
            }
          } else {
            c += 2;
            if (c < unwind_info->count_of_codes) {
              stack_size += (unwind_code + 1)->frame_offset |
                            ((unwind_code + 2)->frame_offset << 16);
            }
          }
          break;
        }
        case UWOP_ALLOC_SMALL: {
          stack_size += unwind_code->operation_info * 8 + 8;
          break;
        }
        case UWOP_SET_FPREG:
          // To correctly track RSP when it's been transferred to another
          // register, we would need to emit CFI records for every unwind op.
          // For simplicity, don't emit CFI records for this function as
          // we know it will be incorrect after this point.
          return false;
        case UWOP_SAVE_NONVOL:
        case UWOP_SAVE_XMM:  // also v2 UWOP_EPILOG
        case UWOP_SAVE_XMM128: {
          c++;  // skip slot with offset
          break;
        }
        case UWOP_SAVE_NONVOL_FAR:
        case UWOP_SAVE_XMM_FAR:  // also v2 UWOP_SPARE
        case UWOP_SAVE_XMM128_FAR: {
          c += 2;  // skip 2 slots with offset
          break;
        }
        case UWOP_PUSH_MACHFRAME: {
          if (unwind_code->operation_info) {
            stack_size += 88;
          } else {
            stack_size += 80;
          }
          rip_offset += 80;
          break;
        }
        default: {
          return false;
        }
      }
    }

    if (unwind_info->flags & UNW_FLAG_CHAININFO) {
      auto chained_func = (PIMAGE_RUNTIME_FUNCTION_ENTRY)((
          unwind_info->unwind_code + ((unwind_info->count_of_codes + 1) & ~1)));

      if (visited.end() != visited.find(chained_func->UnwindInfoAddress)) {
        return false;  // Circular reference
      }

      visited.insert(chained_func->UnwindInfoAddress);

      unwind_info = (UnwindInfo*)RvaToVa(chained_func->UnwindInfoAddress);
    } else {
      unwind_info = nullptr;
    }
  } while (unwind_info);

  aRet.beginAddress = aFunc.BeginAddress;
  aRet.size = aFunc.EndAddress - aFunc.BeginAddress;
  aRet.stackSize = stack_size;
  aRet.ripOffset = rip_offset;
  return true;
}

// For unit testing we sometimes need any address that's valid in this module.
// Just return the first address we know of.
DWORD
ModuleUnwindParser::GetAnyOffsetAddr() const {
  if (mUnwindMap.size() < 1) {
    return 0;
  }
  return mUnwindMap.begin()->first;
}

bool ModuleUnwindParser::GetCFI(DWORD aAddress, UnwindCFI& aRet) {
  // Figure out the begin address of the requested address.
  auto itUW = mUnwindMap.lower_bound(aAddress + 1);
  if (itUW == mUnwindMap.begin()) {
    return false;  // address before this module.
  }
  --itUW;

  // Ensure that the function entry is big enough to contain this address.
  IMAGE_RUNTIME_FUNCTION_ENTRY& func = *itUW->second;
  if (aAddress > func.EndAddress) {
    return false;
  }

  // Do we have CFI for this function already?
  auto itCFI = mCFIMap.find(aAddress);
  if (itCFI != mCFIMap.end()) {
    aRet = itCFI->second;
    return true;
  }

  // No, generate it.
  if (!GenerateCFIForFunction(func, aRet)) {
    return false;
  }

  mCFIMap[func.BeginAddress] = aRet;
  return true;
}

}  // namespace CrashReporter

#endif  // XP_WIN && HAVE_64BIT_BUILD
