// Copyright (c) 2006, Google Inc.
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

#include "common/windows/pdb_source_line_writer.h"

#include <assert.h>
#include <windows.h>
#include <winnt.h>
#include <atlbase.h>
#include <dia2.h>
#include <diacreate.h>
#include <ImageHlp.h>
#include <stdio.h>

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <utility>

#include "common/windows/dia_util.h"
#include "common/windows/guid_string.h"
#include "common/windows/string_utils-inl.h"

// This constant may be missing from DbgHelp.h.  See the documentation for
// IDiaSymbol::get_undecoratedNameEx.
#ifndef UNDNAME_NO_ECSU
#define UNDNAME_NO_ECSU 0x8000  // Suppresses enum/class/struct/union.
#endif  // UNDNAME_NO_ECSU

/*
 * Not defined in WinNT.h for some reason. Definitions taken from:
 * http://uninformed.org/index.cgi?v=4&a=1&p=13
 *
 */
typedef unsigned char UBYTE;

#if !defined(_WIN64)
#define UNW_FLAG_EHANDLER  0x01
#define UNW_FLAG_UHANDLER  0x02
#define UNW_FLAG_CHAININFO 0x04
#endif

union UnwindCode {
  struct {
    UBYTE offset_in_prolog;
    UBYTE unwind_operation_code : 4;
    UBYTE operation_info        : 4;
  };
  USHORT frame_offset;
};

enum UnwindOperationCodes {
  UWOP_PUSH_NONVOL = 0, /* info == register number */
  UWOP_ALLOC_LARGE,     /* no info, alloc size in next 2 slots */
  UWOP_ALLOC_SMALL,     /* info == size of allocation / 8 - 1 */
  UWOP_SET_FPREG,       /* no info, FP = RSP + UNWIND_INFO.FPRegOffset*16 */
  UWOP_SAVE_NONVOL,     /* info == register number, offset in next slot */
  UWOP_SAVE_NONVOL_FAR, /* info == register number, offset in next 2 slots */
  // XXX: these are missing from MSDN!
  // See: http://www.osronline.com/ddkx/kmarch/64bitamd_4rs7.htm
  UWOP_SAVE_XMM,
  UWOP_SAVE_XMM_FAR,
  UWOP_SAVE_XMM128,     /* info == XMM reg number, offset in next slot */
  UWOP_SAVE_XMM128_FAR, /* info == XMM reg number, offset in next 2 slots */
  UWOP_PUSH_MACHFRAME   /* info == 0: no error-code, 1: error-code */
};

// See: http://msdn.microsoft.com/en-us/library/ddssxxy8.aspx
// Note: some fields removed as we don't use them.
struct UnwindInfo {
  UBYTE version       : 3;
  UBYTE flags         : 5;
  UBYTE size_of_prolog;
  UBYTE count_of_codes;
  UBYTE frame_register : 4;
  UBYTE frame_offset   : 4;
  UnwindCode unwind_code[1];
};

namespace google_breakpad {

namespace {

using std::vector;

// The symbol (among possibly many) selected to represent an rva.
struct SelectedSymbol {
  SelectedSymbol(const CComPtr<IDiaSymbol>& symbol, bool is_public)
      : symbol(symbol), is_public(is_public), is_multiple(false) {}

  // The symbol to use for an rva.
  CComPtr<IDiaSymbol> symbol;
  // Whether this is a public or function symbol.
  bool is_public;
  // Whether the rva has multiple associated symbols. An rva will correspond to
  // multiple symbols in the case of linker identical symbol folding.
  bool is_multiple;
};

// Maps rva to the symbol to use for that address.
typedef std::map<DWORD, SelectedSymbol> SymbolMap;

// Record this in the map as the selected symbol for the rva if it satisfies the
// necessary conditions.
void MaybeRecordSymbol(DWORD rva,
                       const CComPtr<IDiaSymbol> symbol,
                       bool is_public,
                       SymbolMap* map) {
  SymbolMap::iterator loc = map->find(rva);
  if (loc == map->end()) {
    map->insert(std::make_pair(rva, SelectedSymbol(symbol, is_public)));
    return;
  }

  // Prefer function symbols to public symbols.
  if (is_public && !loc->second.is_public) {
    return;
  }

  loc->second.is_multiple = true;

  // Take the 'least' symbol by lexicographical order of the decorated name. We
  // use the decorated rather than undecorated name because computing the latter
  // is expensive.
  BSTR current_name, new_name;
  loc->second.symbol->get_name(&current_name);
  symbol->get_name(&new_name);
  if (wcscmp(new_name, current_name) < 0) {
    loc->second.symbol = symbol;
    loc->second.is_public = is_public;
  }
}

// A helper class to scope a PLOADED_IMAGE.
class AutoImage {
 public:
  explicit AutoImage(PLOADED_IMAGE img) : img_(img) {}
  ~AutoImage() {
    if (img_)
      ImageUnload(img_);
  }

  operator PLOADED_IMAGE() { return img_; }
  PLOADED_IMAGE operator->() { return img_; }

 private:
  PLOADED_IMAGE img_;
};

bool SymbolsMatch(IDiaSymbol* a, IDiaSymbol* b) {
  DWORD a_section, a_offset, b_section, b_offset;
  if (FAILED(a->get_addressSection(&a_section)) ||
      FAILED(a->get_addressOffset(&a_offset)) ||
      FAILED(b->get_addressSection(&b_section)) ||
      FAILED(b->get_addressOffset(&b_offset)))
    return false;
  return a_section == b_section && a_offset == b_offset;
}

bool CreateDiaDataSourceInstance(CComPtr<IDiaDataSource> &data_source) {
  if (SUCCEEDED(data_source.CoCreateInstance(CLSID_DiaSource))) {
    return true;
  }

  class DECLSPEC_UUID("B86AE24D-BF2F-4ac9-B5A2-34B14E4CE11D") DiaSource100;
  class DECLSPEC_UUID("761D3BCD-1304-41D5-94E8-EAC54E4AC172") DiaSource110;
  class DECLSPEC_UUID("3BFCEA48-620F-4B6B-81F7-B9AF75454C7D") DiaSource120;
  class DECLSPEC_UUID("E6756135-1E65-4D17-8576-610761398C3C") DiaSource140;

  // If the CoCreateInstance call above failed, msdia*.dll is not registered.
  // We can try loading the DLL corresponding to the #included DIA SDK, but
  // the DIA headers don't provide a version. Lets try to figure out which DIA
  // version we're compiling against by comparing CLSIDs.
  const wchar_t *msdia_dll = nullptr;
  if (CLSID_DiaSource == _uuidof(DiaSource100)) {
    msdia_dll = L"msdia100.dll";
  } else if (CLSID_DiaSource == _uuidof(DiaSource110)) {
    msdia_dll = L"msdia110.dll";
  } else if (CLSID_DiaSource == _uuidof(DiaSource120)) {
    msdia_dll = L"msdia120.dll";
  } else if (CLSID_DiaSource == _uuidof(DiaSource140)) {
    msdia_dll = L"msdia140.dll";
  }

  if (msdia_dll &&
      SUCCEEDED(NoRegCoCreate(msdia_dll, CLSID_DiaSource, IID_IDiaDataSource,
                              reinterpret_cast<void **>(&data_source)))) {
    return true;
  }

  return false;
}

}  // namespace

PDBSourceLineWriter::PDBSourceLineWriter() : output_(NULL) {
}

PDBSourceLineWriter::~PDBSourceLineWriter() {
}

bool PDBSourceLineWriter::SetCodeFile(const wstring &exe_file) {
  if (code_file_.empty()) {
    code_file_ = exe_file;
    return true;
  }
  // Setting a different code file path is an error.  It is success only if the
  // file paths are the same.
  return exe_file == code_file_;
}

bool PDBSourceLineWriter::Open(const wstring &file, FileFormat format) {
  Close();
  code_file_.clear();

  if (FAILED(CoInitialize(NULL))) {
    fprintf(stderr, "CoInitialize failed\n");
    return false;
  }

  CComPtr<IDiaDataSource> data_source;
  if (!CreateDiaDataSourceInstance(data_source)) {
    const int kGuidSize = 64;
    wchar_t classid[kGuidSize] = {0};
    StringFromGUID2(CLSID_DiaSource, classid, kGuidSize);
    fprintf(stderr, "CoCreateInstance CLSID_DiaSource %S failed "
            "(msdia*.dll unregistered?)\n", classid);
    return false;
  }

  switch (format) {
    case PDB_FILE:
      if (FAILED(data_source->loadDataFromPdb(file.c_str()))) {
        fprintf(stderr, "loadDataFromPdb failed for %ws\n", file.c_str());
        return false;
      }
      break;
    case EXE_FILE:
      if (FAILED(data_source->loadDataForExe(file.c_str(), NULL, NULL))) {
        fprintf(stderr, "loadDataForExe failed for %ws\n", file.c_str());
        return false;
      }
      code_file_ = file;
      break;
    case ANY_FILE:
      if (FAILED(data_source->loadDataFromPdb(file.c_str()))) {
        if (FAILED(data_source->loadDataForExe(file.c_str(), NULL, NULL))) {
          fprintf(stderr, "loadDataForPdb and loadDataFromExe failed for %ws\n",
                  file.c_str());
          return false;
        }
        code_file_ = file;
      }
      break;
    default:
      fprintf(stderr, "Unknown file format\n");
      return false;
  }

  if (FAILED(data_source->openSession(&session_))) {
    fprintf(stderr, "openSession failed\n");
  }

  return true;
}

bool PDBSourceLineWriter::PrintLines(IDiaEnumLineNumbers *lines) {
  // The line number format is:
  // <rva> <line number> <source file id>
  CComPtr<IDiaLineNumber> line;
  ULONG count;

  while (SUCCEEDED(lines->Next(1, &line, &count)) && count == 1) {
    DWORD rva;
    if (FAILED(line->get_relativeVirtualAddress(&rva))) {
      fprintf(stderr, "failed to get line rva\n");
      return false;
    }

    DWORD length;
    if (FAILED(line->get_length(&length))) {
      fprintf(stderr, "failed to get line code length\n");
      return false;
    }

    DWORD dia_source_id;
    if (FAILED(line->get_sourceFileId(&dia_source_id))) {
      fprintf(stderr, "failed to get line source file id\n");
      return false;
    }
    // duplicate file names are coalesced to share one ID
    DWORD source_id = GetRealFileID(dia_source_id);

    DWORD line_num;
    if (FAILED(line->get_lineNumber(&line_num))) {
      fprintf(stderr, "failed to get line number\n");
      return false;
    }

    AddressRangeVector ranges;
    MapAddressRange(image_map_, AddressRange(rva, length), &ranges);
    for (size_t i = 0; i < ranges.size(); ++i) {
      fprintf(output_, "%lx %lx %lu %lu\n", ranges[i].rva, ranges[i].length,
              line_num, source_id);
    }
    line.Release();
  }
  return true;
}

bool PDBSourceLineWriter::PrintFunction(IDiaSymbol *function,
                                        IDiaSymbol *block,
                                        bool has_multiple_symbols) {
  // The function format is:
  // FUNC <address> <length> <param_stack_size> <function>
  DWORD rva;
  if (FAILED(block->get_relativeVirtualAddress(&rva))) {
    fprintf(stderr, "couldn't get rva\n");
    return false;
  }

  ULONGLONG length;
  if (FAILED(block->get_length(&length))) {
    fprintf(stderr, "failed to get function length\n");
    return false;
  }

  if (length == 0) {
    // Silently ignore zero-length functions, which can infrequently pop up.
    return true;
  }

  CComBSTR name;
  int stack_param_size;
  if (!GetSymbolFunctionName(function, &name, &stack_param_size)) {
    return false;
  }

  // If the decorated name didn't give the parameter size, try to
  // calculate it.
  if (stack_param_size < 0) {
    stack_param_size = GetFunctionStackParamSize(function);
  }

  AddressRangeVector ranges;
  MapAddressRange(image_map_, AddressRange(rva, static_cast<DWORD>(length)),
                  &ranges);
  for (size_t i = 0; i < ranges.size(); ++i) {
    const char* optional_multiple_field = has_multiple_symbols ? "m " : "";
    fprintf(output_, "FUNC %s%lx %lx %x %ws\n", optional_multiple_field,
            ranges[i].rva, ranges[i].length, stack_param_size, name.m_str);
  }

  CComPtr<IDiaEnumLineNumbers> lines;
  if (FAILED(session_->findLinesByRVA(rva, DWORD(length), &lines))) {
    return false;
  }

  if (!PrintLines(lines)) {
    return false;
  }
  return true;
}

bool PDBSourceLineWriter::PrintSourceFiles() {
  CComPtr<IDiaSymbol> global;
  if (FAILED(session_->get_globalScope(&global))) {
    fprintf(stderr, "get_globalScope failed\n");
    return false;
  }

  CComPtr<IDiaEnumSymbols> compilands;
  if (FAILED(global->findChildren(SymTagCompiland, NULL,
                                  nsNone, &compilands))) {
    fprintf(stderr, "findChildren failed\n");
    return false;
  }

  CComPtr<IDiaSymbol> compiland;
  ULONG count;
  while (SUCCEEDED(compilands->Next(1, &compiland, &count)) && count == 1) {
    CComPtr<IDiaEnumSourceFiles> source_files;
    if (FAILED(session_->findFile(compiland, NULL, nsNone, &source_files))) {
      return false;
    }
    CComPtr<IDiaSourceFile> file;
    while (SUCCEEDED(source_files->Next(1, &file, &count)) && count == 1) {
      DWORD file_id;
      if (FAILED(file->get_uniqueId(&file_id))) {
        return false;
      }

      CComBSTR file_name;
      if (FAILED(file->get_fileName(&file_name))) {
        return false;
      }

      wstring file_name_string(file_name);
      if (!FileIDIsCached(file_name_string)) {
        // this is a new file name, cache it and output a FILE line.
        CacheFileID(file_name_string, file_id);
        fwprintf(output_, L"FILE %d %ws\n", file_id, file_name_string.c_str());
      } else {
        // this file name has already been seen, just save this
        // ID for later lookup.
        StoreDuplicateFileID(file_name_string, file_id);
      }
      file.Release();
    }
    compiland.Release();
  }
  return true;
}

bool PDBSourceLineWriter::PrintFunctions() {
  ULONG count = 0;
  DWORD rva = 0;
  CComPtr<IDiaSymbol> global;
  HRESULT hr;

  if (FAILED(session_->get_globalScope(&global))) {
    fprintf(stderr, "get_globalScope failed\n");
    return false;
  }

  CComPtr<IDiaEnumSymbols> symbols = NULL;

  // Find all function symbols first.
  SymbolMap rva_symbol;
  hr = global->findChildren(SymTagFunction, NULL, nsNone, &symbols);

  if (SUCCEEDED(hr)) {
    CComPtr<IDiaSymbol> symbol = NULL;

    while (SUCCEEDED(symbols->Next(1, &symbol, &count)) && count == 1) {
      if (SUCCEEDED(symbol->get_relativeVirtualAddress(&rva))) {
        // Potentially record this as the canonical symbol for this rva.
        MaybeRecordSymbol(rva, symbol, false, &rva_symbol);
      } else {
        fprintf(stderr, "get_relativeVirtualAddress failed on the symbol\n");
        return false;
      }

      symbol.Release();
    }

    symbols.Release();
  }

  // Find all public symbols and record public symbols that are not also private
  // symbols.
  hr = global->findChildren(SymTagPublicSymbol, NULL, nsNone, &symbols);

  if (SUCCEEDED(hr)) {
    CComPtr<IDiaSymbol> symbol = NULL;

    while (SUCCEEDED(symbols->Next(1, &symbol, &count)) && count == 1) {
      if (SUCCEEDED(symbol->get_relativeVirtualAddress(&rva))) {
        // Potentially record this as the canonical symbol for this rva.
        MaybeRecordSymbol(rva, symbol, true, &rva_symbol);
      } else {
        fprintf(stderr, "get_relativeVirtualAddress failed on the symbol\n");
        return false;
      }

      symbol.Release();
    }

    symbols.Release();
  }

  // For each rva, dump the selected symbol at the address.
  SymbolMap::iterator it;
  for (it = rva_symbol.begin(); it != rva_symbol.end(); ++it) {
    CComPtr<IDiaSymbol> symbol = it->second.symbol;
    // Only print public symbols if there is no function symbol for the address.
    if (!it->second.is_public) {
      if (!PrintFunction(symbol, symbol, it->second.is_multiple))
        return false;
    } else {
      if (!PrintCodePublicSymbol(symbol, it->second.is_multiple))
        return false;
    }
  }

  // When building with PGO, the compiler can split functions into
  // "hot" and "cold" blocks, and move the "cold" blocks out to separate
  // pages, so the function can be noncontiguous. To find these blocks,
  // we have to iterate over all the compilands, and then find blocks
  // that are children of them. We can then find the lexical parents
  // of those blocks and print out an extra FUNC line for blocks
  // that are not contained in their parent functions.
  CComPtr<IDiaEnumSymbols> compilands;
  if (FAILED(global->findChildren(SymTagCompiland, NULL,
                                  nsNone, &compilands))) {
    fprintf(stderr, "findChildren failed on the global\n");
    return false;
  }

  CComPtr<IDiaSymbol> compiland;
  while (SUCCEEDED(compilands->Next(1, &compiland, &count)) && count == 1) {
    CComPtr<IDiaEnumSymbols> blocks;
    if (FAILED(compiland->findChildren(SymTagBlock, NULL,
                                       nsNone, &blocks))) {
      fprintf(stderr, "findChildren failed on a compiland\n");
      return false;
    }

    CComPtr<IDiaSymbol> block;
    while (SUCCEEDED(blocks->Next(1, &block, &count)) && count == 1) {
      // find this block's lexical parent function
      CComPtr<IDiaSymbol> parent;
      DWORD tag;
      if (SUCCEEDED(block->get_lexicalParent(&parent)) &&
          SUCCEEDED(parent->get_symTag(&tag)) &&
          tag == SymTagFunction) {
        // now get the block's offset and the function's offset and size,
        // and determine if the block is outside of the function
        DWORD func_rva, block_rva;
        ULONGLONG func_length;
        if (SUCCEEDED(block->get_relativeVirtualAddress(&block_rva)) &&
            SUCCEEDED(parent->get_relativeVirtualAddress(&func_rva)) &&
            SUCCEEDED(parent->get_length(&func_length))) {
          if (block_rva < func_rva || block_rva > (func_rva + func_length)) {
            if (!PrintFunction(parent, block, false)) {
              return false;
            }
          }
        }
      }
      parent.Release();
      block.Release();
    }
    blocks.Release();
    compiland.Release();
  }

  global.Release();
  return true;
}

#undef max

bool PDBSourceLineWriter::PrintFrameDataUsingPDB() {
  // It would be nice if it were possible to output frame data alongside the
  // associated function, as is done with line numbers, but the DIA API
  // doesn't make it possible to get the frame data in that way.

  CComPtr<IDiaEnumFrameData> frame_data_enum;
  if (!FindTable(session_, &frame_data_enum))
    return false;

  DWORD last_type = std::numeric_limits<DWORD>::max();
  DWORD last_rva = std::numeric_limits<DWORD>::max();
  DWORD last_code_size = 0;
  DWORD last_prolog_size = std::numeric_limits<DWORD>::max();

  CComPtr<IDiaFrameData> frame_data;
  ULONG count = 0;
  while (SUCCEEDED(frame_data_enum->Next(1, &frame_data, &count)) &&
         count == 1) {
    DWORD type;
    if (FAILED(frame_data->get_type(&type)))
      return false;

    DWORD rva;
    if (FAILED(frame_data->get_relativeVirtualAddress(&rva)))
      return false;

    DWORD code_size;
    if (FAILED(frame_data->get_lengthBlock(&code_size)))
      return false;

    DWORD prolog_size;
    if (FAILED(frame_data->get_lengthProlog(&prolog_size)))
      return false;

    // parameter_size is the size of parameters passed on the stack.  If any
    // parameters are not passed on the stack (such as in registers), their
    // sizes will not be included in parameter_size.
    DWORD parameter_size;
    if (FAILED(frame_data->get_lengthParams(&parameter_size)))
      return false;

    DWORD saved_register_size;
    if (FAILED(frame_data->get_lengthSavedRegisters(&saved_register_size)))
      return false;

    DWORD local_size;
    if (FAILED(frame_data->get_lengthLocals(&local_size)))
      return false;

    // get_maxStack can return S_FALSE, just use 0 in that case.
    DWORD max_stack_size = 0;
    if (FAILED(frame_data->get_maxStack(&max_stack_size)))
      return false;

    // get_programString can return S_FALSE, indicating that there is no
    // program string.  In that case, check whether %ebp is used.
    HRESULT program_string_result;
    CComBSTR program_string;
    if (FAILED(program_string_result = frame_data->get_program(
        &program_string))) {
      return false;
    }

    // get_allocatesBasePointer can return S_FALSE, treat that as though
    // %ebp is not used.
    BOOL allocates_base_pointer = FALSE;
    if (program_string_result != S_OK) {
      if (FAILED(frame_data->get_allocatesBasePointer(
          &allocates_base_pointer))) {
        return false;
      }
    }

    // Only print out a line if type, rva, code_size, or prolog_size have
    // changed from the last line.  It is surprisingly common (especially in
    // system library PDBs) for DIA to return a series of identical
    // IDiaFrameData objects.  For kernel32.pdb from Windows XP SP2 on x86,
    // this check reduces the size of the dumped symbol file by a third.
    if (type != last_type || rva != last_rva || code_size != last_code_size ||
        prolog_size != last_prolog_size) {
      // The prolog and the code portions of the frame have to be treated
      // independently as they may have independently changed in size, or may
      // even have been split.
      // NOTE: If epilog size is ever non-zero, we have to do something
      //     similar with it.

      // Figure out where the prolog bytes have landed.
      AddressRangeVector prolog_ranges;
      if (prolog_size > 0) {
        MapAddressRange(image_map_, AddressRange(rva, prolog_size),
                        &prolog_ranges);
      }

      // And figure out where the code bytes have landed.
      AddressRangeVector code_ranges;
      MapAddressRange(image_map_,
                      AddressRange(rva + prolog_size,
                                   code_size - prolog_size),
                      &code_ranges);

      struct FrameInfo {
        DWORD rva;
        DWORD code_size;
        DWORD prolog_size;
      };
      std::vector<FrameInfo> frame_infos;

      // Special case: The prolog and the code bytes remain contiguous. This is
      // only done for compactness of the symbol file, and we could actually
      // be outputting independent frame info for the prolog and code portions.
      if (prolog_ranges.size() == 1 && code_ranges.size() == 1 &&
          prolog_ranges[0].end() == code_ranges[0].rva) {
        FrameInfo fi = { prolog_ranges[0].rva,
                         prolog_ranges[0].length + code_ranges[0].length,
                         prolog_ranges[0].length };
        frame_infos.push_back(fi);
      } else {
        // Otherwise we output the prolog and code frame info independently.
        for (size_t i = 0; i < prolog_ranges.size(); ++i) {
          FrameInfo fi = { prolog_ranges[i].rva,
                           prolog_ranges[i].length,
                           prolog_ranges[i].length };
          frame_infos.push_back(fi);
        }
        for (size_t i = 0; i < code_ranges.size(); ++i) {
          FrameInfo fi = { code_ranges[i].rva, code_ranges[i].length, 0 };
          frame_infos.push_back(fi);
        }
      }

      for (size_t i = 0; i < frame_infos.size(); ++i) {
        const FrameInfo& fi(frame_infos[i]);
        fprintf(output_, "STACK WIN %lx %lx %lx %lx %x %lx %lx %lx %lx %d ",
                type, fi.rva, fi.code_size, fi.prolog_size,
                0 /* epilog_size */, parameter_size, saved_register_size,
                local_size, max_stack_size, program_string_result == S_OK);
        if (program_string_result == S_OK) {
          fprintf(output_, "%ws\n", program_string.m_str);
        } else {
          fprintf(output_, "%d\n", allocates_base_pointer);
        }
      }

      last_type = type;
      last_rva = rva;
      last_code_size = code_size;
      last_prolog_size = prolog_size;
    }

    frame_data.Release();
  }

  return true;
}

bool PDBSourceLineWriter::PrintFrameDataUsingEXE() {
  AutoImage img(LoadImageForPEFile());
  if (!img) {
    return false;
  }

  DWORD exception_rva, exception_size;
  if (!Get64BitExceptionInformation(img, &exception_rva, &exception_size)) {
    fprintf(stderr, "Not a PE32+ image\n");
    return false;
  }

  PIMAGE_RUNTIME_FUNCTION_ENTRY funcs =
    static_cast<PIMAGE_RUNTIME_FUNCTION_ENTRY>(
        ImageRvaToVa(img->FileHeader,
                     img->MappedAddress,
                     exception_rva,
                     &img->LastRvaSection));
  for (DWORD i = 0; i < exception_size / sizeof(*funcs); i++) {
    DWORD unwind_rva = funcs[i].UnwindInfoAddress;
    // handle chaining
    while (unwind_rva & 0x1) {
      unwind_rva ^= 0x1;
      PIMAGE_RUNTIME_FUNCTION_ENTRY chained_func =
        static_cast<PIMAGE_RUNTIME_FUNCTION_ENTRY>(
            ImageRvaToVa(img->FileHeader,
                         img->MappedAddress,
                         unwind_rva,
                         &img->LastRvaSection));
      unwind_rva = chained_func->UnwindInfoAddress;
    }

    UnwindInfo *unwind_info = static_cast<UnwindInfo *>(
        ImageRvaToVa(img->FileHeader,
                     img->MappedAddress,
                     unwind_rva,
                     &img->LastRvaSection));

    DWORD stack_size = 8;  // minimal stack size is 8 for RIP
    DWORD rip_offset = 8;
    do {
      for (UBYTE c = 0; c < unwind_info->count_of_codes; c++) {
        UnwindCode *unwind_code = &unwind_info->unwind_code[c];
        switch (unwind_code->unwind_operation_code) {
          case UWOP_PUSH_NONVOL: {
            stack_size += 8;
            break;
          }
          case UWOP_ALLOC_LARGE: {
            if (unwind_code->operation_info == 0) {
              c++;
              if (c < unwind_info->count_of_codes)
                stack_size += (unwind_code + 1)->frame_offset * 8;
            } else {
              c += 2;
              if (c < unwind_info->count_of_codes)
                stack_size += (unwind_code + 1)->frame_offset |
                              ((unwind_code + 2)->frame_offset << 16);
            }
            break;
          }
          case UWOP_ALLOC_SMALL: {
            stack_size += unwind_code->operation_info * 8 + 8;
            break;
          }
          case UWOP_SET_FPREG:
          case UWOP_SAVE_XMM:
          case UWOP_SAVE_XMM_FAR:
            break;
          case UWOP_SAVE_NONVOL:
          case UWOP_SAVE_XMM128: {
            c++;  // skip slot with offset
            break;
          }
          case UWOP_SAVE_NONVOL_FAR:
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
        }
      }
      if (unwind_info->flags & UNW_FLAG_CHAININFO) {
        PIMAGE_RUNTIME_FUNCTION_ENTRY chained_func =
          reinterpret_cast<PIMAGE_RUNTIME_FUNCTION_ENTRY>(
              (unwind_info->unwind_code +
              ((unwind_info->count_of_codes + 1) & ~1)));

        unwind_info = static_cast<UnwindInfo *>(
            ImageRvaToVa(img->FileHeader,
                         img->MappedAddress,
                         chained_func->UnwindInfoAddress,
                         &img->LastRvaSection));
      } else {
        unwind_info = NULL;
      }
    } while (unwind_info);
    fprintf(output_, "STACK CFI INIT %lx %lx .cfa: $rsp .ra: .cfa %lu - ^\n",
            funcs[i].BeginAddress,
            funcs[i].EndAddress - funcs[i].BeginAddress, rip_offset);
    fprintf(output_, "STACK CFI %lx .cfa: $rsp %lu +\n",
            funcs[i].BeginAddress, stack_size);
  }

  return true;
}

namespace {

// See https://docs.microsoft.com/en-us/cpp/build/arm64-exception-handling
// for documentation on the particulars of the unwind format.

class UnwindInsn {
public:
  // Unwind codes for .xdata unwind records.  We store offsets for "save
  // register"-style codes as positive offsets always, despite the
  // documentation saying that offsets for *_x/*X codes are negative.
  enum class Code {
    kAllocS,
    kSaveR19R20X,
    kSaveFpLr,
    kSaveFpLrX,
    kAllocM,
    kSaveRegP,
    kSaveRegPX,
    kSaveReg,
    kSaveRegX,
    kSaveLrPair,
    kSaveFRegP,
    kSaveFRegPX,
    kSaveFReg,
    kSaveFRegX,
    kAllocL,
    kSetFP,
    kAddFP,
    kNop,
    kEnd,
    kEndC,
    kSaveNext,
    kArithmeticAdd,
    kArithmeticSub,
    kArithmeticEor,
    kArithmeticRol,
    kArithmeticRor,
    kCustomStack,
  };

  UnwindInsn(const UnwindInsn&) = default;
  UnwindInsn& operator=(const UnwindInsn&) = default;
  UnwindInsn(UnwindInsn&&) = default;
  UnwindInsn& operator=(UnwindInsn&&) = default;

  Code InsnCode() const {
    return code_;
  }

  uint32_t InsnLength() const {
    return kInsnLengths[(int)code_];
  }

  uint32_t AllocAmount() const {
    assert(code_ == Code::kAllocS || code_ == Code::kAllocM || code_ == Code::kAllocL);
    return amount_;
  }

  static UnwindInsn AllocS(uint32_t amount) {
    assert(size < 512);
    return UnwindInsn(Code::kAllocS, amount);
  }

  static UnwindInsn SaveR19R20X(uint32_t offset) {
    assert(offset <= 248);
    return UnwindInsn(Code::kSaveR19R20X, offset);
  }

  static UnwindInsn SaveFpLr(uint32_t offset) {
    assert(offset <= 504);
    return UnwindInsn(Code::kSaveFpLr, offset);
  }

  static UnwindInsn SaveFpLrX(uint32_t offset) {
    assert(offset <= 512);
    return UnwindInsn(Code::kSaveFpLrX, offset);
  }

  static UnwindInsn AllocM(uint32_t amount) {
    assert(size < 65536);
    return UnwindInsn(Code::kAllocM, amount);
  }

  uint32_t RegPair() const {
    assert(code_ == Code::kSaveRegP ||
           code_ == Code::kSaveRegPX ||
           code_ == Code::kSaveFRegP ||
           code_ == Code::kSaveFRegPX);
    return reg_;
  }

  uint32_t Offset() const {
    assert(code_ == Code::kSaveFpLr ||
           code_ == Code::kSaveRegP ||
           code_ == Code::kSaveReg ||
           code_ == Code::kSaveLrPair ||
           code_ == Code::kSaveFRegP ||
           code_ == Code::kSaveFReg);
    return offset_;
  }

  uint32_t OffsetX() const {
    assert(code_ == Code::kSaveR19R20X ||
           code_ == Code::kSaveFpLrX ||
           code_ == Code::kSaveRegPX ||
           code_ == Code::kSaveRegX ||
           code_ == Code::kSaveFRegPX ||
           code_ == Code::kSaveFRegX);
    return offset_;
  }

  static UnwindInsn SaveRegP(uint32_t r1, uint32_t offset) {
    assert(size <= 504);
    return UnwindInsn(Code::kSaveRegP, offset, r1);
  }

  static UnwindInsn SaveRegPX(uint32_t r1, uint32_t offset) {
    assert(offset <= 512);
    return UnwindInsn(Code::kSaveRegPX, offset, r1);
  }

  uint32_t Reg() const {
    assert(code_ == Code::kSaveReg ||
           code_ == Code::kSaveRegX ||
           code_ == Code::kSaveFReg ||
           code_ == Code::kSaveFRegX ||
           code_ == Code::kSaveLrPair);
    return reg_;
  }

  static UnwindInsn SaveReg(uint32_t reg, uint32_t offset) {
    assert(offset <= 504);
    return UnwindInsn(Code::kSaveReg, offset, reg);
  }

  static UnwindInsn SaveRegX(uint32_t reg, uint32_t offset) {
    assert(offset <= 256);
    return UnwindInsn(Code::kSaveRegX, offset, reg);
  }

  uint32_t SaveRegLrPaired() const {
    assert(code_ == Code::kSaveLrPair);
    return reg_;
  }

  static UnwindInsn SaveLrPair(uint32_t reg, uint32_t offset) {
    assert(offset <= 504);
    return UnwindInsn(Code::kSaveLrPair, offset, reg);
  }

  static UnwindInsn SaveFRegP(uint32_t r1, uint32_t offset) {
    assert(size <= 504);
    return UnwindInsn(Code::kSaveFRegP, offset, r1);
  }

  static UnwindInsn SaveFRegPX(uint32_t r1, uint32_t offset) {
    assert(offset <= 512);
    return UnwindInsn(Code::kSaveFRegPX, offset, r1);
  }

  static UnwindInsn SaveFReg(uint32_t reg, uint32_t offset) {
    assert(offset <= 504);
    return UnwindInsn(Code::kSaveFReg, offset, reg);
  }

  static UnwindInsn SaveFRegX(uint32_t reg, uint32_t offset) {
    assert(offset <= 256);
    return UnwindInsn(Code::kSaveFRegX, offset, reg);
  }

  static UnwindInsn AllocL(uint32_t size) {
    assert(size < (1 << 28));
    return UnwindInsn(Code::kAllocL, size);
  }

  static UnwindInsn SetFP() {
    return UnwindInsn(Code::kSetFP);
  }

  static UnwindInsn AddFP(uint32_t amount) {
    assert(amount < (256 * 8));
    return UnwindInsn(Code::kAddFP, amount);
  }

  uint32_t FPAmount() const {
    assert(code_ == Code::kAddFP);
    return amount_;
  }

  static UnwindInsn Nop() { return UnwindInsn(Code::kNop); }
  static UnwindInsn End() { return UnwindInsn(Code::kEnd); }
  static UnwindInsn EndC() { return UnwindInsn(Code::kEndC); }
  static UnwindInsn SaveNext() { return UnwindInsn(Code::kSaveNext); }

  uint32_t CookieRegister() const {
    assert(code_ == Code::kArithmeticAdd ||
           code_ == Code::kArithmeticSub ||
           code_ == Code::kArithmeticEor ||
           code_ == Code::kArithmeticRol ||
           code_ == Code::kArithmeticRor);
    return cookie_reg_;
  }

  static UnwindInsn ArithmeticAdd(uint32_t cookie) {
    return UnwindInsn(Code::kArithmeticAdd, cookie);
  }

  static UnwindInsn ArithmeticSub(uint32_t cookie) {
    return UnwindInsn(Code::kArithmeticSub, cookie);
  }

  static UnwindInsn ArithmeticEor(uint32_t cookie) {
    return UnwindInsn(Code::kArithmeticEor, cookie);
  }

  static UnwindInsn ArithmeticRol(uint32_t cookie) {
    return UnwindInsn(Code::kArithmeticRol, cookie);
  }

  static UnwindInsn ArithmeticRor(uint32_t cookie) {
    return UnwindInsn(Code::kArithmeticRor, cookie);
  }

  static UnwindInsn CustomStack() {
    return UnwindInsn(Code::kCustomStack);
  }

  static std::vector<UnwindInsn> ParseInsns(const uint8_t* data,
                                            uint32_t n_bytes);

private:
  UnwindInsn(Code code)
    : UnwindInsn(code, 0, 0)
  {}

  UnwindInsn(Code code, uint32_t x)
    : UnwindInsn(code, x, 0)
  {}

  UnwindInsn(Code code, uint32_t x, uint32_t y)
    : code_(code), amount_(x), reg_(y)
  {}

  struct InsnParser {
    // Bits that match for a given instruction after applying `mask`.
    uint8_t match;
    // Bits for insn data and bits for insn codes are mixed in the same
    // byte, so we apply this mask to isolate the latter.
    uint8_t mask;
    // The total number of bytes occupied by this insn.
    uint8_t n_bytes;
    UnwindInsn (*func)(uint8_t unmasked_first, const uint8_t* codes);
  };

  static UnwindInsn ParseAllocS(uint8_t first, const uint8_t* rest);
  static UnwindInsn ParseSaveR19R20X(uint8_t first, const uint8_t* rest);
  static UnwindInsn ParseSaveFpLr(uint8_t first, const uint8_t* rest);
  static UnwindInsn ParseSaveFpLrX(uint8_t first, const uint8_t* rest);
  static UnwindInsn ParseAllocM(uint8_t first, const uint8_t* rest);
  static UnwindInsn ParseSaveRegP(uint8_t first, const uint8_t* rest);
  static UnwindInsn ParseSaveRegPX(uint8_t first, const uint8_t* rest);
  static UnwindInsn ParseSaveReg(uint8_t first, const uint8_t* rest);
  static UnwindInsn ParseSaveRegX(uint8_t first, const uint8_t* rest);
  static UnwindInsn ParseSaveLrPair(uint8_t first, const uint8_t* rest);
  static UnwindInsn ParseSaveFRegP(uint8_t first, const uint8_t* rest);
  static UnwindInsn ParseSaveFRegPX(uint8_t first, const uint8_t* rest);
  static UnwindInsn ParseSaveFReg(uint8_t first, const uint8_t* rest);
  static UnwindInsn ParseSaveFRegX(uint8_t first, const uint8_t* rest);
  static UnwindInsn ParseAllocL(uint8_t first, const uint8_t* rest);
  static UnwindInsn ParseSetFP(uint8_t first, const uint8_t* rest);
  static UnwindInsn ParseAddFP(uint8_t first, const uint8_t* rest);
  static UnwindInsn ParseNop(uint8_t first, const uint8_t* rest);
  static UnwindInsn ParseEnd(uint8_t first, const uint8_t* rest);
  static UnwindInsn ParseEndC(uint8_t first, const uint8_t* rest);
  static UnwindInsn ParseSaveNext(uint8_t first, const uint8_t* rest);
  static UnwindInsn ParseArithmetic(uint8_t first, const uint8_t* rest);
  static UnwindInsn ParseCustomStackReserved(uint8_t first, const uint8_t* rest);
  static UnwindInsn ParseReserved(uint8_t first, const uint8_t* rest);

  static const InsnParser kParsers[];

  static const uint8_t kInsnLengths[];

  Code code_;
  union {
    uint32_t amount_;
    uint32_t offset_;
    uint32_t cookie_reg_;
  };
  union {
    uint32_t reg_;
  };
};

const uint8_t UnwindInsn::kInsnLengths[] = {
  1, // AllocS
  1, // SaveR19R20X
  1, // SaveFpLr
  1, // SaveFpLrX
  2, // AllocM
  2, // SaveRegP
  2, // SaveRegPX
  2, // SaveReg
  2, // SaveRegX
  2, // SaveLrPair
  2, // SaveFRegP
  2, // SaveFRegPX
  2, // SaveFReg
  2, // SaveFRegX
  4, // AllocL
  1, // SetFP
  2, // AddFP
  1, // Nop
  1, // End
  1, // EndC
  1, // SaveNext
  2, // ArithmeticAdd
  2, // ArithmeticSub
  2, // ArithmeticEor
  2, // ArithmeticRol
  2, // ArithmeticRor
  1, // CustomStack
};

static uint32_t
bitfield(uint32_t bits, uint32_t size, uint32_t bit) {
  return (bits >> bit) & ((1u << size) - 1);
}

UnwindInsn
UnwindInsn::ParseAllocS(uint8_t first, const uint8_t*) {
  uint32_t size = first * 16;
  return UnwindInsn::AllocS(size);
}

UnwindInsn
UnwindInsn::ParseSaveR19R20X(uint8_t first, const uint8_t*) {
  uint32_t offset = first * 8;
  return UnwindInsn::SaveR19R20X(offset);
}

UnwindInsn
UnwindInsn::ParseSaveFpLr(uint8_t first, const uint8_t*) {
  uint32_t offset = first * 8;
  return UnwindInsn::SaveFpLr(offset);
}

UnwindInsn
UnwindInsn::ParseSaveFpLrX(uint8_t first, const uint8_t*) {
  uint32_t offset = (first + 1) * 8;
  return UnwindInsn::SaveFpLrX(offset);
}

UnwindInsn
UnwindInsn::ParseAllocM(uint8_t first, const uint8_t* rest) {
  uint64_t size = ((uint64_t(first) << 8) | rest[0]) * 16;
  return UnwindInsn::AllocM(size);
}

UnwindInsn
UnwindInsn::ParseSaveRegP(uint8_t first, const uint8_t* rest) {
  uint8_t byte = rest[0];
  uint32_t r19offset = (first << 2) | (byte >> 6);
  uint32_t spoffset = (byte & 0x3f) * 8;
  return UnwindInsn::SaveRegP(19 + r19offset, spoffset);
}

UnwindInsn
UnwindInsn::ParseSaveRegPX(uint8_t first, const uint8_t* rest) {
  uint8_t byte = rest[0];
  uint32_t r19offset = (first << 2) | (byte >> 6);
  uint32_t spoffset = ((byte & 0x3f) + 1) * 8;
  return UnwindInsn::SaveRegPX(19 + r19offset, spoffset);
}

UnwindInsn
UnwindInsn::ParseSaveReg(uint8_t first, const uint8_t* rest) {
  uint8_t byte = rest[0];
  uint32_t r19offset = (first << 2) | (byte >> 6);
  uint32_t spoffset = (byte & 0x3f) * 8;
  return UnwindInsn::SaveReg(19 + r19offset, spoffset);
}

UnwindInsn
UnwindInsn::ParseSaveRegX(uint8_t first, const uint8_t* rest) {
  uint8_t byte = rest[0];
  uint32_t r19offset = (first << 3) | (byte >> 5);
  uint32_t spoffset = ((byte & 0x1f) + 1) * 8;
  return UnwindInsn::SaveRegX(19 + r19offset, spoffset);
}

UnwindInsn
UnwindInsn::ParseSaveLrPair(uint8_t first, const uint8_t* rest) {
  uint8_t byte = rest[0];
  uint32_t r19offset = ((first << 2) | (byte >> 6)) * 2;
  uint32_t spoffset = ((byte & 0x3f) + 1) * 8;
  return UnwindInsn::SaveLrPair(19 + r19offset, spoffset);
}

UnwindInsn
UnwindInsn::ParseSaveFRegP(uint8_t first, const uint8_t* rest) {
  uint8_t byte = rest[0];
  uint32_t d8offset = (first << 2) | (byte >> 6);
  uint32_t spoffset = (byte & 0x3f) * 8;
  return UnwindInsn::SaveFRegP(8 + d8offset, spoffset);
}

UnwindInsn
UnwindInsn::ParseSaveFRegPX(uint8_t first, const uint8_t* rest) {
  uint8_t byte = rest[0];
  uint32_t d8offset = (first << 2) | (byte >> 6);
  uint32_t spoffset = ((byte & 0x3f) + 1) * 8;
  return UnwindInsn::SaveFRegPX(8 + d8offset, spoffset);
}

UnwindInsn
UnwindInsn::ParseSaveFReg(uint8_t first, const uint8_t* rest) {
  uint8_t byte = rest[0];
  uint32_t d8offset = (first << 2) | (byte >> 6);
  uint32_t spoffset = (byte & 0x3f) * 8;
  return UnwindInsn::SaveFReg(8 + d8offset, spoffset);
}

UnwindInsn
UnwindInsn::ParseSaveFRegX(uint8_t, const uint8_t* rest) {
  uint8_t byte = rest[0];
  uint32_t d8offset = byte >> 3;
  uint32_t spoffset = ((byte & 0x1f) + 1) * 8;
  return UnwindInsn::SaveFRegX(8 + d8offset, spoffset);
}

UnwindInsn
UnwindInsn::ParseAllocL(uint8_t, const uint8_t* rest) {
  uint64_t size = ((rest[0] << 16) | (rest[1] << 8) | rest[2]) * 16;
  return UnwindInsn::AllocL(size);
}

UnwindInsn
UnwindInsn::ParseSetFP(uint8_t first, const uint8_t*) {
  return UnwindInsn::SetFP();
}

UnwindInsn
UnwindInsn::ParseAddFP(uint8_t first, const uint8_t* rest) {
  uint8_t amount = rest[0] * 8;
  return UnwindInsn::AddFP(amount);
}

UnwindInsn
UnwindInsn::ParseNop(uint8_t, const uint8_t*) {
  return UnwindInsn::Nop();
}

UnwindInsn
UnwindInsn::ParseEnd(uint8_t, const uint8_t*) {
  return UnwindInsn::End();
}

UnwindInsn
UnwindInsn::ParseEndC(uint8_t, const uint8_t*) {
  return UnwindInsn::EndC();
}

UnwindInsn
UnwindInsn::ParseSaveNext(uint8_t, const uint8_t*) {
  return UnwindInsn::SaveNext();
}

UnwindInsn
UnwindInsn::ParseArithmetic(uint8_t, const uint8_t* rest) {
  uint8_t code = rest[0];
  uint8_t op = (code & 0xe0) >> 5;

  enum Op {
    kAdd,
    kSub,
    kEor,
    kRol,
    kRor,
  };

  switch (op) {
  case kAdd: {
    uint32_t cookie = ((code & 0x10) >> 4) ? 28 : 31;
    return UnwindInsn::ArithmeticAdd(cookie);
  }
  case kSub: {
    uint32_t cookie = ((code & 0x10) >> 4) ? 28 : 31;
    return UnwindInsn::ArithmeticSub(cookie);
  }
  case kEor: {
    uint32_t cookie = ((code & 0x10) >> 4) ? 28 : 31;
    return UnwindInsn::ArithmeticEor(cookie);
  }
  case kRol: {
    // If this bit is not zero, the code is reserved.
    if (code & 0x10) {
      goto reserved;
    }
    uint32_t cookie = 28;
    return UnwindInsn::ArithmeticRol(cookie);
  }
  case kRor: {
    uint32_t cookie = ((code & 0x10) >> 4) ? 28 : 31;
    return UnwindInsn::ArithmeticRor(cookie);
  }
  default:
  reserved:
    // Reserved, error
    fprintf(stderr, "reserved arithmetic opcode found: %d\n", int(code));
    return UnwindInsn::Nop();
  }
}

UnwindInsn
UnwindInsn::ParseCustomStackReserved(uint8_t, const uint8_t*) {
  // XXX we really just want to crash here.
  return UnwindInsn::CustomStack();
}

UnwindInsn
UnwindInsn::ParseReserved(uint8_t, const uint8_t*) {
  // XXX we really just want to crash here.
  return UnwindInsn::Nop();
}

const UnwindInsn::InsnParser UnwindInsn::kParsers[] = {
  0x00, 0xe0, 1, ParseAllocS,
  0x20, 0xe0, 1, ParseSaveR19R20X,
  0x40, 0xc0, 1, ParseSaveFpLr,
  0x80, 0xc0, 1, ParseSaveFpLrX,
  0xc0, 0xf8, 2, ParseAllocM,
  0xc8, 0xfc, 2, ParseSaveRegP,
  0xcc, 0xfc, 2, ParseSaveRegPX,
  0xd0, 0xfc, 2, ParseSaveReg,
  0xd4, 0xfe, 2, ParseSaveRegX,
  0xd6, 0xfe, 2, ParseSaveLrPair,
  0xd8, 0xfe, 2, ParseSaveFRegP,
  0xda, 0xfe, 2, ParseSaveFRegPX,
  0xdc, 0xfe, 2, ParseSaveFReg,
  0xde, 0xff, 2, ParseSaveFRegX,
  0xe0, 0xff, 4, ParseAllocL,
  0xe1, 0xff, 1, ParseSetFP,
  0xe2, 0xff, 2, ParseAddFP,
  0xe3, 0xff, 1, ParseNop,
  0xe4, 0xff, 1, ParseEnd,
  0xe5, 0xff, 1, ParseEndC,
  0xe6, 0xff, 1, ParseSaveNext,
  0xe7, 0xff, 2, ParseArithmetic,
  0xe8, 0xf8, 1, ParseCustomStackReserved,
  0xf0, 0xf0, 1, ParseReserved,
};

std::vector<UnwindInsn>
UnwindInsn::ParseInsns(const uint8_t* data, uint32_t n_bytes) {
  std::vector<UnwindInsn> insns;

  // We should never try to match on bits we mask out.
  for (auto& parser : kParsers) {
    assert((parser.match & (~size_t(parser.mask))) == 0);
  }

  for (const uint8_t* end = data + n_bytes; data < end; ) {
    uint8_t first = data[0];
    const uint8_t* const rest = data + 1;

    auto b = std::begin(kParsers);
    auto e = std::end(kParsers);
    auto i = std::find_if(b, e, [&](const auto& p) {
        return (first & p.mask) == p.match;
      });

    assert(i != e);

    // Try to ensure parser functions aren't going to access more than they
    // are supposed to.
    const uint8_t* p = i->n_bytes == 1 ? nullptr : rest;
    insns.emplace_back(i->func(first & (~size_t(i->mask)), p));

    data += i->n_bytes;
  }

  return insns;
}

struct EpilogScope {
  uint32_t start_offset;
  uint32_t start_index;
};

struct UnwindInformation {
  uint32_t func_length;
  std::vector<EpilogScope> epilog_scopes;
  std::vector<UnwindInsn> unwind_insns;
};

bool
DecodePackedUnwindData(uint32_t packed, UnwindInformation* info) {
  uint32_t flag = bitfield(packed, 2, 0);
  uint32_t func_length = bitfield(packed, 11, 2) * 4;
  uint32_t reg_f = bitfield(packed, 3, 13);
  uint32_t reg_i = bitfield(packed, 4, 16);
  uint32_t h = bitfield(packed, 1, 20);
  uint32_t cr = bitfield(packed, 2, 21);
  uint32_t frame_size = bitfield(packed, 9, 23) * 16;

  // The steps below follow the table for decoding packed unwind data in the
  // ARM64 exception handling document referenced above.  Variable names are
  // as consistent as possible with the documentation as well.

  // Step 0.
  uint32_t intsz = reg_i * 8;
  if (cr == 0x1) {
    intsz += 8; // save lr
  }
  uint32_t fpsz = reg_f * 8;
  if (reg_f) {
    fpsz += 8;
  }

  // Documentation said `8 * h` here, but that seems completely wrong:
  // that would calculate the number of registers homed, not the size of
  // the area required to save them.
  uint32_t savsz = ((intsz + fpsz + 64 * h) + 0xf) & ~uint32_t(0xf);
  uint32_t locsz = frame_size - savsz;

  std::vector<UnwindInsn> unwind_insns;

  // Steps 1 and 2.
  if (reg_i > 0) {
    // XXX what happens when reg_i == 1 and cr == 0x1?  There's no
    // unwind insn for saving <r19,lr> with pre-indexed offset.
    if (reg_i == 1 && cr == 0x1) {
      fprintf(stderr, "No unwind insn for reg_i == 1 with saved lr\n");
      return false;
    }
    int32_t regs_to_save = reg_i;
    // Note that this is really SaveR19R20X in the regs_to_save >= 2 case, but
    // we do this so we have to worry less about how big `savsz` is.
    UnwindInsn (*insn)(uint32_t reg, uint32_t offset) =
      (regs_to_save >= 2
       ? UnwindInsn::SaveRegX
       : UnwindInsn::SaveRegPX);
    unwind_insns.emplace_back(insn(19, savsz));
    regs_to_save -= 2;
    uint32_t next_reg = 21;
    uint32_t reg_offset = 16;
    while (regs_to_save > 0) {
      if (regs_to_save >= 2) {
        insn = UnwindInsn::SaveRegP;
      } else if (cr == 0x1) {
        assert(regs_to_save == 1);
        insn = UnwindInsn::SaveLrPair;
      } else {
        assert(regs_to_save == 1);
        insn = UnwindInsn::SaveReg;
      }
      unwind_insns.emplace_back(insn(next_reg, reg_offset));
      next_reg += 2;
      reg_offset += 16;
      regs_to_save -= 2;
    }
  }

  // Step 2.  Folding the lr save in with an odd number of integer register
  // saves was handled above.
  if (cr == 0x1 && ((reg_i & 0x1) == 0)) {
    unwind_insns.emplace_back(UnwindInsn::SaveReg(30, intsz-8));
  }

  // Step 3.
  if (reg_f > 0) {
    int32_t regs_to_save = reg_f + 1;
    // Handle the special case of no integer save regs and no lr save.
    if (reg_i == 0 && cr == 0x0) {
      unwind_insns.emplace_back(UnwindInsn::SaveFRegPX(8, intsz));
    } else {
      unwind_insns.emplace_back(UnwindInsn::SaveFRegP(8, intsz));
    }
    regs_to_save -= 2;
    uint32_t next_reg = 10;
    uint32_t reg_offset = 16;
    while (regs_to_save > 0) {
      auto insn = regs_to_save < 2
        ? UnwindInsn::SaveFReg
        : UnwindInsn::SaveFRegP;
      unwind_insns.emplace_back(insn(next_reg, reg_offset));
      next_reg += 2;
      reg_offset += 16;
      regs_to_save -= 2;
    }
  }

  // Step 4.
  if (h == 1) {
    // TODO: we could record the saves of the argument registers in CFI data,
    // which might provide more informative unwinds.
    unwind_insns.emplace_back(UnwindInsn::Nop());
    unwind_insns.emplace_back(UnwindInsn::Nop());
    unwind_insns.emplace_back(UnwindInsn::Nop());
    unwind_insns.emplace_back(UnwindInsn::Nop());
  }

  // All the logic for allocating the stack for the substeps of step 5.
  auto alloc_stack = [&](uint32_t amount) {
      if (amount < 512) {
        unwind_insns.emplace_back(UnwindInsn::AllocS(amount));
      } else if (amount <= 4088) {
        unwind_insns.emplace_back(UnwindInsn::AllocM(amount));
      } else {
        unwind_insns.emplace_back(UnwindInsn::AllocM(4088));
        uint32_t remaining = amount - 4088;
        assert(remaining != 0);
        assert(remaining < amount);
        if (remaining < 512) {
          unwind_insns.emplace_back(UnwindInsn::AllocS(remaining));
        } else {
          unwind_insns.emplace_back(UnwindInsn::AllocM(remaining));
        }
      }
  };

  // Steps 5a - 5c.
  if (cr == 0x3) {
    if (locsz <= 512) {
      unwind_insns.emplace_back(UnwindInsn::SaveFpLrX(locsz));
      unwind_insns.emplace_back(UnwindInsn::SetFP());
    } else {
      alloc_stack(locsz);
      unwind_insns.emplace_back(UnwindInsn::SaveFpLr(0));
      unwind_insns.emplace_back(UnwindInsn::SetFP());
    }
  }

  // Steps 5d and 5e.
  if (cr == 0x0 || cr == 0x1) {
    alloc_stack(locsz);
  }

  // Signal the end of the prolog.
  unwind_insns.emplace_back(UnwindInsn::End());

  // TODO: if we ever decided to generate unwind information for epilogs,
  // we'd need to either make the unwind insns generated here consistent with
  // how they would appear in a .xdata record or do a bunch of gymnastics to
  // fixup our EpilogScopes vector.
  *info = UnwindInformation{func_length,
                            std::vector<EpilogScope>(),
                            std::move(unwind_insns)};
  return true;
}

bool
DecodeXDataRecord(void* ptr, UnwindInformation* info) {
  uint32_t* words = static_cast<uint32_t*>(ptr);
  uint32_t w = words[0];
  uint32_t func_length = bitfield(w, 18, 0) * 4;
  uint32_t vers = bitfield(w, 2, 18);
  uint32_t x = bitfield(w, 1, 20);
  uint32_t e = bitfield(w, 1, 21);
  uint32_t epilog_count = bitfield(w, 5, 22);
  uint32_t code_words = bitfield(w, 5, 27);

  assert(vers == 0);

  words++;

  if (epilog_count == 0 && code_words == 0) {
    uint32_t w = words[0];
    epilog_count = bitfield(w, 16, 0);
    code_words = bitfield(w, 8, 16);
    words++;
  }

  std::vector<EpilogScope> epilog_scopes;
  // When e == 1, the documentation claims that epilog_count actually
  // specifies the start index...but there's no documentation on how to
  // recover the offset.  Presumably what we would do is parse all the unwind
  // insns, and then calculate how long the epilog must be by examining the
  // insns, and then push our single epilog scope.  But other parts of the
  // documentation claim otherwise.
  //
  // Just punt here, we don't emit unwind information for epilogs anyway.
  if (e == 1) {
    ;
  } else if (epilog_count != 0) {
    for (size_t i = 0; i < epilog_count; ++i) {
      uint32_t w = words[i];
      uint32_t start_offset = bitfield(w, 18, 0) * 4;
      uint32_t res = bitfield(w, 4, 18);
      uint32_t start_index = bitfield(w, 10, 22);

      assert(res == 0);

      epilog_scopes.emplace_back(EpilogScope{start_offset, start_index});
    }

    words += epilog_count;
  }

  uint8_t* codes_ptr = reinterpret_cast<uint8_t*>(words);

  std::vector<UnwindInsn> unwind_insns =
    UnwindInsn::ParseInsns(codes_ptr, code_words * 4);

  // For unexplained reasons, presumably to increase sharing of unwind insns
  // between prolog and epilog sequences, the insns for unwinding the prolog
  // occur in the reverse order of their corresponding instructions in the
  // prolog.  That is, if the prolog looks like:
  //
  // stp x21, x22, [sp,#-0x30]!
  // stp x19, x20, [sp,#0x10]
  // stp fp, lr, [sp,#0x20]
  // add fp, sp, #0x20
  //
  // the unwind insns describing the prolog will appear in the unwind data as:
  //
  // AddFP
  // SaveRegP
  // SaveRegP
  // SaveRegPX
  // End
  //
  // This is somewhat inconvenient for using the insns to print STACK CFI lines,
  // below, so reverse the prolog insns.  This transformation means that we can
  // no longer generate STACK CFI lines for epilogs, as epilog unwind insns may
  // overlap the prolog unwind insns.  But since we don't generate said lines
  // for epilogs currently, this is not a huge restriction.
  auto end_insn = std::find_if(unwind_insns.begin(),
                               unwind_insns.end(),
                               [&](const UnwindInsn& insn) {
                                 return insn.InsnCode() == UnwindInsn::Code::kEnd;
                               });
  assert(end_insn != unwind_insns.end());
  std::reverse(unwind_insns.begin(), end_insn);

  *info = UnwindInformation{func_length,
                            std::move(epilog_scopes),
                            std::move(unwind_insns)};
  return true;
}

} // namespace

bool PDBSourceLineWriter::PrintFrameDataUsingArm64EXE() {
  AutoImage img(LoadImageForPEFile());
  if (!img) {
    return false;
  }

  DWORD exception_rva, exception_size;
  if (!Get64BitExceptionInformation(img, &exception_rva, &exception_size)) {
    fprintf(stderr, "Not a PE32+ image\n");
    return false;
  }

  PIMAGE_ARM64_RUNTIME_FUNCTION_ENTRY funcs =
    static_cast<PIMAGE_ARM64_RUNTIME_FUNCTION_ENTRY>(
        ImageRvaToVa(img->FileHeader,
                     img->MappedAddress,
                     exception_rva,
                     &img->LastRvaSection));

  for (DWORD i = 0; i < exception_size / sizeof(*funcs); ++i) {
    PIMAGE_ARM64_RUNTIME_FUNCTION_ENTRY func = &funcs[i];

    // Don't know what to do with the reserved data flag.
    if (func->Flag == 0x3) {
      fprintf(stderr, "Encountered reserved .pdata entry\n");
      return false;
    }

    // No prolog/epilog packed unwind data is a TODO.
    if (func->Flag == 0x2) {
      fprintf(stderr, "Don't handle no-prolog packed unwind data (%lx) yet\n",
              func->UnwindData);
      return false;
    }

    // Decode the bits we understand.
    UnwindInformation info;
    if (func->Flag == 0x1) {
      if (!DecodePackedUnwindData(func->UnwindData, &info)) {
        fprintf(stderr, "Couldn't decode packed unwind data (%lx)\n",
                func->UnwindData);
        return false;
      }
    } else {
      assert(func->Flag == 0);

      PVOID xdata_address = ImageRvaToVa(img->FileHeader,
                                         img->MappedAddress,
                                         func->UnwindData,
                                         nullptr);
      if (!DecodeXDataRecord(xdata_address, &info)) {
        fprintf(stderr, "Couldn't decode xdata record\n");
        return false;
      }
    }

    // Ouput CFI lines for the prolog only.  We fix the CFA at sp on function
    // entry, which means we just have to track updates to sp.

    // Whether we have seen a set of fp, for consistency checking.
    bool fp_set = false;
    bool saw_end = false;
    uint32_t stack_offset = 0;
    // This address is module-load relative, so it can be used for the
    // address required by STACK CFI records.
    uint32_t address = func->BeginAddress;
    fprintf(output_, "STACK CFI INIT %x %x .cfa: sp 0 + .ra: x30\n",
            address, info.func_length);

    // See the above comment in DecodeXDataRecord for an explanation of why
    // we can just iterate straight through here, and why this code needs to
    // change if we ever decide to handle epilogs here.
    for (const auto& insn : info.unwind_insns) {
      // Once we've seen the End insn, we know that we're done with the prolog.
      if (saw_end) {
        break;
      }

      // The unwind format is designed so that one unwind insn maps precisely
      // to one instruction.
      address += 4;

      // The *X insns adjust sp, which require updating the CFA.  We update
      // the CFA in the same line as describing the register saves, since
      // the unwind format doesn't permit multiple lines with the same
      // address.  Note that since the CFA is unchanging, even if the rule
      // used to calculate it changes, the register save descriptions are
      // identical whether the previous CFA rule is used, or the updated
      // CFA rule.
      //
      // The conceptual frame we're keeping track of here looks like this:
      //
      // CFA (sp at function entry) --> +-----
      //                                |
      //                                | ...
      //                                |
      //               stack_offset --> +-----
      //
      // Where `stack_offset` represents how far we've adjusted `sp` downwards.
      // We are always adding some quantity to `sp` or `fp` to find the CFA.
      //
      // For a *X instruction, which updates sp and then stores register(s),
      // the frame is going to look like:
      //
      // CFA (sp at function entry) --> +-----
      //                                |
      //                                | ...
      //                                |
      //               stack_offset --> +-----
      //                                | register(s)
      //   stack_offset + OffsetX() --> +-----
      //
      // since we always store OffsetX() as a positive quantity, despite the
      // actual ARM64 instruction being `str xnn, [sp, #-0xNN]!`.
      //
      // For other save register instructions, the frame looks more like:
      //
      // CFA (sp at function entry) --> +-----
      //                                |
      //                                | ...
      //                                +-----
      //                                | register(s)
      //    stack_offset - Offset() --> +-----
      //                                |
      //               stack_offset --> +-----
      //
      // Note that for a save pair operation, we need to subtract `Offset()`
      // to find the first slot, and then subtract another 8 bytes to find
      // the second slot.
      switch (insn.InsnCode()) {
      case UnwindInsn::Code::kAllocS:
      case UnwindInsn::Code::kAllocM:
      case UnwindInsn::Code::kAllocL:
        stack_offset += insn.AllocAmount();
        fprintf(output_, "STACK CFI %x .cfa: sp %u +\n",
                address, stack_offset);
        break;
      case UnwindInsn::Code::kSaveR19R20X: {
        uint32_t offset = stack_offset + insn.OffsetX();
        fprintf(output_, "STACK CFI %x .cfa: sp %u + x19: .cfa %u - ^ x20: .cfa %u - ^\n",
                address, offset, offset, offset - 8);
        stack_offset += insn.OffsetX();
        break;
      }
      case UnwindInsn::Code::kSaveFpLr: {
        uint32_t offset = stack_offset - insn.Offset();
        assert(offset < stack_offset);
        fprintf(output_, "STACK CFI %x x29: .cfa %u - ^ .ra: .cfa %u - ^\n",
                address, offset, offset - 8);
        break;
      }
      case UnwindInsn::Code::kSaveFpLrX: {
        uint32_t offset = stack_offset + insn.OffsetX();
        fprintf(output_, "STACK CFI %x .cfa: sp %u + x29: .cfa %u - ^ .ra: .cfa %u - ^\n",
                address, offset, offset, offset - 8);
        stack_offset += insn.OffsetX();
        break;
      }
      case UnwindInsn::Code::kSaveRegP: {
        uint32_t offset = stack_offset - insn.Offset();
        uint32_t r = insn.Reg();
        assert(offset < stack_offset);
        fprintf(output_, "STACK CFI %x x%d: .cfa %u - ^ x%d: .cfa %u - ^\n",
                address, r, offset, r + 1, offset - 8);
        break;
      }
      case UnwindInsn::Code::kSaveRegPX: {
        uint32_t offset = stack_offset + insn.OffsetX();
        uint32_t r = insn.Reg();
        fprintf(output_, "STACK CFI %x .cfa: sp %u + x%d: .cfa %u - ^ x%d: .cfa %u - ^\n",
                address, offset, r, offset, r + 1, offset - 8);
        stack_offset += insn.OffsetX();
        break;
      }
      case UnwindInsn::Code::kSaveReg: {
        uint32_t offset = stack_offset - insn.Offset();
        uint32_t r = insn.Reg();
        assert(offset < stack_offset);
        fprintf(output_, "STACK CFI %x x%d: .cfa %u - ^\n",
                address, r, offset);
        break;
      }
      case UnwindInsn::Code::kSaveRegX: {
        uint32_t offset = stack_offset + insn.OffsetX();
        uint32_t r = insn.Reg();
        fprintf(output_, "STACK CFI %x .cfa: sp %u + x%d: .cfa %u - ^\n",
                address, offset, r, offset);
        stack_offset += insn.OffsetX();
        break;
      }
      case UnwindInsn::Code::kSaveLrPair: {
        uint32_t offset = stack_offset - insn.Offset();
        uint32_t r = insn.Reg();
        assert(offset < stack_offset);
        fprintf(output_, "STACK CFI %x x%d: .cfa %u - ^ .ra: .cfa %u - ^\n",
                address, r, offset, offset - 8);
        break;
      }
      case UnwindInsn::Code::kSaveFRegP: {
        uint32_t offset = stack_offset - insn.Offset();
        uint32_t r = insn.Reg();
        assert(offset < stack_offset);
        fprintf(output_, "STACK CFI %x d%d: .cfa %u - ^ d%d: .cfa %u - ^\n",
                address, r, offset, r + 1, offset - 8);
        break;
      }
      case UnwindInsn::Code::kSaveFRegPX: {
        uint32_t offset = stack_offset + insn.OffsetX();
        uint32_t r = insn.Reg();
        fprintf(output_, "STACK CFI %x .cfa: sp %u - d%d: .cfa %u - ^ d%d: .cfa %u - ^\n",
                address, offset, r, offset, r + 1, offset - 8);
        stack_offset += insn.OffsetX();
        break;
      }
      case UnwindInsn::Code::kSaveFReg: {
        uint32_t offset = stack_offset - insn.Offset();
        uint32_t r = insn.Reg();
        assert(offset < stack_offset);
        fprintf(output_, "STACK CFI %x d%d: .cfa %u - ^\n",
                address, r, offset);
        break;
      }
      case UnwindInsn::Code::kSaveFRegX: {
        uint32_t offset = stack_offset + insn.OffsetX();
        uint32_t r = insn.Reg();
        fprintf(output_, "STACK CFI %x .cfa: sp %u - d%d: .cfa %u - ^\n",
                address, offset, r, offset);
        stack_offset += insn.OffsetX();
        break;
      }
      // AddFP/SetFP break in the presence of multiple instances of the
      // respective opcodes, but really that shouldn't happen...
      case UnwindInsn::Code::kAddFP:
        if (fp_set) {
          fprintf(stderr, "Can't handle multiple operations on fp\n");
          return false;
        }
        fp_set = true;
        fprintf(output_, "STACK CFI %x .cfa: x29 %u +\n",
                address, stack_offset + insn.FPAmount());
        break;
      case UnwindInsn::Code::kSetFP:
        if (fp_set) {
          fprintf(stderr, "Can't handle multiple operations on fp\n");
          return false;
        }
        fp_set = true;
        fprintf(output_, "STACK CFI %x .cfa: x29 %u +\n",
                address, stack_offset);
        break;
      case UnwindInsn::Code::kNop:
        break;
      case UnwindInsn::Code::kEnd:
        if (saw_end) {
          fprintf(stderr, "Can't handle multiple End opcodes\n");
          return false;
        }
        saw_end = true;
        break;
      case UnwindInsn::Code::kEndC:
        break;
      case UnwindInsn::Code::kSaveNext:
        fprintf(stderr, "Saving the next obvious register unimplemented\n");
        return false;
      case UnwindInsn::Code::kArithmeticAdd:
      case UnwindInsn::Code::kArithmeticSub:
      case UnwindInsn::Code::kArithmeticEor:
      case UnwindInsn::Code::kArithmeticRol:
      case UnwindInsn::Code::kArithmeticRor:
        fprintf(stderr, "Arithmetic cookie operations unimplemented\n");
        return false;
      case UnwindInsn::Code::kCustomStack:
        fprintf(stderr, "Custom stack operations unimplemented\n");
        return false;
      default:
        fprintf(stderr, "Unknown unwind opcode %d", int(insn.InsnCode()));
        return false;
      }
    }
  }

  return true;
}

bool PDBSourceLineWriter::PrintFrameData() {
  PDBModuleInfo info;
  if (!GetModuleInfo(&info)) {
    return PrintFrameDataUsingPDB();
  }
  if (info.cpu == L"x86_64") {
    return PrintFrameDataUsingEXE();
  }
  if (info.cpu == L"arm64") {
    return PrintFrameDataUsingArm64EXE();
  }
  return PrintFrameDataUsingPDB();
}

bool PDBSourceLineWriter::PrintCodePublicSymbol(IDiaSymbol *symbol,
                                                bool has_multiple_symbols) {
  BOOL is_code;
  if (FAILED(symbol->get_code(&is_code))) {
    return false;
  }
  if (!is_code) {
    return true;
  }

  DWORD rva;
  if (FAILED(symbol->get_relativeVirtualAddress(&rva))) {
    return false;
  }

  CComBSTR name;
  int stack_param_size;
  if (!GetSymbolFunctionName(symbol, &name, &stack_param_size)) {
    return false;
  }

  AddressRangeVector ranges;
  MapAddressRange(image_map_, AddressRange(rva, 1), &ranges);
  for (size_t i = 0; i < ranges.size(); ++i) {
    const char* optional_multiple_field = has_multiple_symbols ? "m " : "";
    fprintf(output_, "PUBLIC %s%lx %x %ws\n", optional_multiple_field,
            ranges[i].rva, stack_param_size > 0 ? stack_param_size : 0,
            name.m_str);
  }

  // Now walk the function in the original untranslated space, asking DIA
  // what function is at that location, stepping through OMAP blocks. If
  // we're still in the same function, emit another entry, because the
  // symbol could have been split into multiple pieces. If we've gotten to
  // another symbol in the original address space, then we're done for
  // this symbol. See https://crbug.com/678874.
  for (;;) {
    // This steps to the next block in the original image. Simply doing
    // rva++ would also be correct, but would emit tons of unnecessary
    // entries.
    rva = image_map_.subsequent_rva_block[rva];
    if (rva == 0)
      break;

    CComPtr<IDiaSymbol> next_sym = NULL;
    LONG displacement;
    if (FAILED(session_->findSymbolByRVAEx(rva, SymTagPublicSymbol, &next_sym,
                                           &displacement))) {
      break;
    }

    if (!SymbolsMatch(symbol, next_sym))
      break;

    AddressRangeVector next_ranges;
    MapAddressRange(image_map_, AddressRange(rva, 1), &next_ranges);
    for (size_t i = 0; i < next_ranges.size(); ++i) {
      fprintf(output_, "PUBLIC %lx %x %ws\n", next_ranges[i].rva,
              stack_param_size > 0 ? stack_param_size : 0, name.m_str);
    }
  }

  return true;
}

bool PDBSourceLineWriter::PrintPDBInfo() {
  PDBModuleInfo info;
  if (!GetModuleInfo(&info)) {
    return false;
  }

  // Hard-code "windows" for the OS because that's the only thing that makes
  // sense for PDB files.  (This might not be strictly correct for Windows CE
  // support, but we don't care about that at the moment.)
  fprintf(output_, "MODULE windows %ws %ws %ws\n",
          info.cpu.c_str(), info.debug_identifier.c_str(),
          info.debug_file.c_str());

  return true;
}

bool PDBSourceLineWriter::PrintPEInfo() {
  PEModuleInfo info;
  if (!GetPEInfo(&info)) {
    return false;
  }

  fprintf(output_, "INFO CODE_ID %ws %ws\n",
          info.code_identifier.c_str(),
          info.code_file.c_str());
  return true;
}

// wcstol_positive_strict is sort of like wcstol, but much stricter.  string
// should be a buffer pointing to a null-terminated string containing only
// decimal digits.  If the entire string can be converted to an integer
// without overflowing, and there are no non-digit characters before the
// result is set to the value and this function returns true.  Otherwise,
// this function returns false.  This is an alternative to the strtol, atoi,
// and scanf families, which are not as strict about input and in some cases
// don't provide a good way for the caller to determine if a conversion was
// successful.
static bool wcstol_positive_strict(wchar_t *string, int *result) {
  int value = 0;
  for (wchar_t *c = string; *c != '\0'; ++c) {
    int last_value = value;
    value *= 10;
    // Detect overflow.
    if (value / 10 != last_value || value < 0) {
      return false;
    }
    if (*c < '0' || *c > '9') {
      return false;
    }
    unsigned int c_value = *c - '0';
    last_value = value;
    value += c_value;
    // Detect overflow.
    if (value < last_value) {
      return false;
    }
    // Forbid leading zeroes unless the string is just "0".
    if (value == 0 && *(c+1) != '\0') {
      return false;
    }
  }
  *result = value;
  return true;
}

bool PDBSourceLineWriter::FindPEFile() {
  CComPtr<IDiaSymbol> global;
  if (FAILED(session_->get_globalScope(&global))) {
    fprintf(stderr, "get_globalScope failed\n");
    return false;
  }

  CComBSTR symbols_file;
  if (SUCCEEDED(global->get_symbolsFileName(&symbols_file))) {
    wstring file(symbols_file);

    // Look for an EXE or DLL file.
    const wchar_t *extensions[] = { L"exe", L"dll" };
    for (size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++) {
      size_t dot_pos = file.find_last_of(L".");
      if (dot_pos != wstring::npos) {
        file.replace(dot_pos + 1, wstring::npos, extensions[i]);
        // Check if this file exists.
        if (GetFileAttributesW(file.c_str()) != INVALID_FILE_ATTRIBUTES) {
          code_file_ = file;
          return true;
        }
      }
    }
  }

  return false;
}

bool PDBSourceLineWriter::Get64BitExceptionInformation(PLOADED_IMAGE img,
                                                       DWORD* rva,
                                                       DWORD* size) {
  PIMAGE_OPTIONAL_HEADER64 optional_header =
    &(reinterpret_cast<PIMAGE_NT_HEADERS64>(img->FileHeader))->OptionalHeader;
  if (optional_header->Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
    return false;
  }

  // Read Exception Directory
  *rva = optional_header->
    DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION].VirtualAddress;
  *size = optional_header->
    DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION].Size;
  return true;
}

PLOADED_IMAGE PDBSourceLineWriter::LoadImageForPEFile() {
  if (code_file_.empty() && !FindPEFile()) {
    fprintf(stderr, "Couldn't locate EXE or DLL file.\n");
    return nullptr;
  }

  // Convert wchar to native charset because ImageLoad only takes
  // a PSTR as input.
  string code_file;
  if (!WindowsStringUtils::safe_wcstombs(code_file_, &code_file)) {
    return nullptr;
  }

  PLOADED_IMAGE img = ImageLoad((PSTR)code_file.c_str(), NULL);
  if (!img) {
    fprintf(stderr, "Failed to open PE file: %s\n", code_file.c_str());
    return nullptr;
  }
  
  return img;
}

static const DWORD kUndecorateOptions =
    UNDNAME_NO_MS_KEYWORDS | UNDNAME_NO_FUNCTION_RETURNS |
    UNDNAME_NO_ALLOCATION_MODEL | UNDNAME_NO_ALLOCATION_LANGUAGE |
    UNDNAME_NO_THISTYPE | UNDNAME_NO_ACCESS_SPECIFIERS |
    UNDNAME_NO_THROW_SIGNATURES | UNDNAME_NO_MEMBER_TYPE |
    UNDNAME_NO_RETURN_UDT_MODEL | UNDNAME_NO_ECSU;

static void FixupLlvmUniqueSymbol(BSTR *name) {
  wchar_t *suffix = wcsstr(*name, L".llvm.");

  if (suffix != nullptr) {
    *suffix = L'\0';

    const size_t undecorated_len = 1024;
    wchar_t undecorated[undecorated_len] = {};
    DWORD res = UnDecorateSymbolNameW(*name, undecorated, undecorated_len,
                                      kUndecorateOptions);
    if (res == 0) {
      fprintf(stderr, "failed to undecorate symbol %S\n", *name);
    } else {
      SysFreeString(*name);
      *name = SysAllocString(undecorated);
    }
  }
}

// static
bool PDBSourceLineWriter::GetSymbolFunctionName(IDiaSymbol *function,
                                                BSTR *name,
                                                int *stack_param_size) {
  *stack_param_size = -1;

  // Use get_undecoratedNameEx to get readable C++ names with arguments.
  if (function->get_undecoratedNameEx(kUndecorateOptions, name) != S_OK) {
    if (function->get_name(name) != S_OK) {
      fprintf(stderr, "failed to get function name\n");
      return false;
    }

    // It's possible for get_name to return an empty string, so
    // special-case that.
    if (wcscmp(*name, L"") == 0) {
      SysFreeString(*name);
      // dwarf_cu_to_module.cc uses "<name omitted>", so match that.
      *name = SysAllocString(L"<name omitted>");
      return true;
    }

    // If a name comes from get_name because no undecorated form existed,
    // it's already formatted properly to be used as output.  Don't do any
    // additional processing.
    //
    // MSVC7's DIA seems to not undecorate names in as many cases as MSVC8's.
    // This will result in calling get_name for some C++ symbols, so
    // all of the parameter and return type information may not be included in
    // the name string.
  } else {
    FixupLlvmUniqueSymbol(name);

    // C++ uses a bogus "void" argument for functions and methods that don't
    // take any parameters.  Take it out of the undecorated name because it's
    // ugly and unnecessary.
    const wchar_t *replace_string = L"(void)";
    const size_t replace_length = wcslen(replace_string);
    const wchar_t *replacement_string = L"()";
    size_t length = wcslen(*name);
    if (length >= replace_length) {
      wchar_t *name_end = *name + length - replace_length;
      if (wcscmp(name_end, replace_string) == 0) {
        WindowsStringUtils::safe_wcscpy(name_end, replace_length,
                                        replacement_string);
        length = wcslen(*name);
      }
    }

    // Undecorate names used for stdcall and fastcall.  These names prefix
    // the identifier with '_' (stdcall) or '@' (fastcall) and suffix it
    // with '@' followed by the number of bytes of parameters, in decimal.
    // If such a name is found, take note of the size and undecorate it.
    // Only do this for names that aren't C++, which is determined based on
    // whether the undecorated name contains any ':' or '(' characters.
    if (!wcschr(*name, ':') && !wcschr(*name, '(') &&
        (*name[0] == '_' || *name[0] == '@')) {
      wchar_t *last_at = wcsrchr(*name + 1, '@');
      if (last_at && wcstol_positive_strict(last_at + 1, stack_param_size)) {
        // If this function adheres to the fastcall convention, it accepts up
        // to the first 8 bytes of parameters in registers (%ecx and %edx).
        // We're only interested in the stack space used for parameters, so
        // so subtract 8 and don't let the size go below 0.
        if (*name[0] == '@') {
          if (*stack_param_size > 8) {
            *stack_param_size -= 8;
          } else {
            *stack_param_size = 0;
          }
        }

        // Undecorate the name by moving it one character to the left in its
        // buffer, and terminating it where the last '@' had been.
        WindowsStringUtils::safe_wcsncpy(*name, length,
                                         *name + 1, last_at - *name - 1);
     } else if (*name[0] == '_') {
        // This symbol's name is encoded according to the cdecl rules.  The
        // name doesn't end in a '@' character followed by a decimal positive
        // integer, so it's not a stdcall name.  Strip off the leading
        // underscore.
        WindowsStringUtils::safe_wcsncpy(*name, length, *name + 1, length);
      }
    }
  }

  return true;
}

// static
int PDBSourceLineWriter::GetFunctionStackParamSize(IDiaSymbol *function) {
  // This implementation is highly x86-specific.

  // Gather the symbols corresponding to data.
  CComPtr<IDiaEnumSymbols> data_children;
  if (FAILED(function->findChildren(SymTagData, NULL, nsNone,
                                    &data_children))) {
    return 0;
  }

  // lowest_base is the lowest %ebp-relative byte offset used for a parameter.
  // highest_end is one greater than the highest offset (i.e. base + length).
  // Stack parameters are assumed to be contiguous, because in reality, they
  // are.
  int lowest_base = INT_MAX;
  int highest_end = INT_MIN;

  CComPtr<IDiaSymbol> child;
  DWORD count;
  while (SUCCEEDED(data_children->Next(1, &child, &count)) && count == 1) {
    // If any operation fails at this point, just proceed to the next child.
    // Use the next_child label instead of continue because child needs to
    // be released before it's reused.  Declare constructable/destructable
    // types early to avoid gotos that cross initializations.
    CComPtr<IDiaSymbol> child_type;

    // DataIsObjectPtr is only used for |this|.  Because |this| can be passed
    // as a stack parameter, look for it in addition to traditional
    // parameters.
    DWORD child_kind;
    if (FAILED(child->get_dataKind(&child_kind)) ||
        (child_kind != DataIsParam && child_kind != DataIsObjectPtr)) {
      goto next_child;
    }

    // Only concentrate on register-relative parameters.  Parameters may also
    // be enregistered (passed directly in a register), but those don't
    // consume any stack space, so they're not of interest.
    DWORD child_location_type;
    if (FAILED(child->get_locationType(&child_location_type)) ||
        child_location_type != LocIsRegRel) {
      goto next_child;
    }

    // Of register-relative parameters, the only ones that make any sense are
    // %ebp- or %esp-relative.  Note that MSVC's debugging information always
    // gives parameters as %ebp-relative even when a function doesn't use a
    // traditional frame pointer and stack parameters are accessed relative to
    // %esp, so just look for %ebp-relative parameters.  If you wanted to
    // access parameters, you'd probably want to treat these %ebp-relative
    // offsets as if they were relative to %esp before a function's prolog
    // executed.
    DWORD child_register;
    if (FAILED(child->get_registerId(&child_register)) ||
        child_register != CV_REG_EBP) {
      goto next_child;
    }

    LONG child_register_offset;
    if (FAILED(child->get_offset(&child_register_offset))) {
      goto next_child;
    }

    // IDiaSymbol::get_type can succeed but still pass back a NULL value.
    if (FAILED(child->get_type(&child_type)) || !child_type) {
      goto next_child;
    }

    ULONGLONG child_length;
    if (FAILED(child_type->get_length(&child_length))) {
      goto next_child;
    }

    // Extra scope to avoid goto jumping over variable initialization
    {
      int child_end = child_register_offset + static_cast<ULONG>(child_length);
      if (child_register_offset < lowest_base) {
        lowest_base = child_register_offset;
      }
      if (child_end > highest_end) {
        highest_end = child_end;
      }
    }

next_child:
    child.Release();
  }

  int param_size = 0;
  // Make sure lowest_base isn't less than 4, because [%esp+4] is the lowest
  // possible address to find a stack parameter before executing a function's
  // prolog (see above).  Some optimizations cause parameter offsets to be
  // lower than 4, but we're not concerned with those because we're only
  // looking for parameters contained in addresses higher than where the
  // return address is stored.
  if (lowest_base < 4) {
    lowest_base = 4;
  }
  if (highest_end > lowest_base) {
    // All stack parameters are pushed as at least 4-byte quantities.  If the
    // last type was narrower than 4 bytes, promote it.  This assumes that all
    // parameters' offsets are 4-byte-aligned, which is always the case.  Only
    // worry about the last type, because we're not summing the type sizes,
    // just looking at the lowest and highest offsets.
    int remainder = highest_end % 4;
    if (remainder) {
      highest_end += 4 - remainder;
    }

    param_size = highest_end - lowest_base;
  }

  return param_size;
}

bool PDBSourceLineWriter::WriteMap(FILE *map_file) {
  output_ = map_file;

  // Load the OMAP information, and disable auto-translation of addresses in
  // preference of doing it ourselves.
  OmapData omap_data;
  if (!GetOmapDataAndDisableTranslation(session_, &omap_data))
    return false;
  BuildImageMap(omap_data, &image_map_);

  bool ret = PrintPDBInfo();
  // This is not a critical piece of the symbol file.
  PrintPEInfo();
  ret = ret &&
      PrintSourceFiles() &&
      PrintFunctions() &&
      PrintFrameData();

  output_ = NULL;
  return ret;
}

void PDBSourceLineWriter::Close() {
  session_.Release();
}

bool PDBSourceLineWriter::GetModuleInfo(PDBModuleInfo *info) {
  if (!info) {
    return false;
  }

  info->debug_file.clear();
  info->debug_identifier.clear();
  info->cpu.clear();

  CComPtr<IDiaSymbol> global;
  if (FAILED(session_->get_globalScope(&global))) {
    return false;
  }

  DWORD machine_type;
  // get_machineType can return S_FALSE.
  if (global->get_machineType(&machine_type) == S_OK) {
    // The documentation claims that get_machineType returns a value from
    // the CV_CPU_TYPE_e enumeration, but that's not the case.
    // Instead, it returns one of the IMAGE_FILE_MACHINE values as
    // defined here:
    // http://msdn.microsoft.com/en-us/library/ms680313%28VS.85%29.aspx
    switch (machine_type) {
      case IMAGE_FILE_MACHINE_I386:
        info->cpu = L"x86";
        break;
      case IMAGE_FILE_MACHINE_AMD64:
        info->cpu = L"x86_64";
        break;
      case IMAGE_FILE_MACHINE_ARM64:
        info->cpu = L"arm64";
        break;
      default:
        info->cpu = L"unknown";
        break;
    }
  } else {
    // Unexpected, but handle gracefully.
    info->cpu = L"unknown";
  }

  // DWORD* and int* are not compatible.  This is clean and avoids a cast.
  DWORD age;
  if (FAILED(global->get_age(&age))) {
    return false;
  }

  bool uses_guid;
  if (!UsesGUID(&uses_guid)) {
    return false;
  }

  if (uses_guid) {
    GUID guid;
    if (FAILED(global->get_guid(&guid))) {
      return false;
    }

    // Use the same format that the MS symbol server uses in filesystem
    // hierarchies.
    wchar_t age_string[9];
    swprintf(age_string, sizeof(age_string) / sizeof(age_string[0]),
             L"%x", age);

    // remove when VC++7.1 is no longer supported
    age_string[sizeof(age_string) / sizeof(age_string[0]) - 1] = L'\0';

    info->debug_identifier = GUIDString::GUIDToSymbolServerWString(&guid);
    info->debug_identifier.append(age_string);
  } else {
    DWORD signature;
    if (FAILED(global->get_signature(&signature))) {
      return false;
    }

    // Use the same format that the MS symbol server uses in filesystem
    // hierarchies.
    wchar_t identifier_string[17];
    swprintf(identifier_string,
             sizeof(identifier_string) / sizeof(identifier_string[0]),
             L"%08X%x", signature, age);

    // remove when VC++7.1 is no longer supported
    identifier_string[sizeof(identifier_string) /
                      sizeof(identifier_string[0]) - 1] = L'\0';

    info->debug_identifier = identifier_string;
  }

  CComBSTR debug_file_string;
  if (FAILED(global->get_symbolsFileName(&debug_file_string))) {
    return false;
  }
  info->debug_file =
      WindowsStringUtils::GetBaseName(wstring(debug_file_string));

  return true;
}

bool PDBSourceLineWriter::GetPEInfo(PEModuleInfo *info) {
  if (!info) {
    return false;
  }

  AutoImage img(LoadImageForPEFile());
  if (!img) {
    return false;
  }

  info->code_file = WindowsStringUtils::GetBaseName(code_file_);

  // The date and time that the file was created by the linker.
  DWORD TimeDateStamp = img->FileHeader->FileHeader.TimeDateStamp;
  // The size of the file in bytes, including all headers.
  DWORD SizeOfImage = 0;
  PIMAGE_OPTIONAL_HEADER64 opt =
    &((PIMAGE_NT_HEADERS64)img->FileHeader)->OptionalHeader;
  if (opt->Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
    // 64-bit PE file.
    SizeOfImage = opt->SizeOfImage;
  } else {
    // 32-bit PE file.
    SizeOfImage = img->FileHeader->OptionalHeader.SizeOfImage;
  }
  wchar_t code_identifier[32];
  swprintf(code_identifier,
      sizeof(code_identifier) / sizeof(code_identifier[0]),
      L"%08X%X", TimeDateStamp, SizeOfImage);
  info->code_identifier = code_identifier;

  return true;
}

bool PDBSourceLineWriter::UsesGUID(bool *uses_guid) {
  if (!uses_guid)
    return false;

  CComPtr<IDiaSymbol> global;
  if (FAILED(session_->get_globalScope(&global)))
    return false;

  GUID guid;
  if (FAILED(global->get_guid(&guid)))
    return false;

  DWORD signature;
  if (FAILED(global->get_signature(&signature)))
    return false;

  // There are two possibilities for guid: either it's a real 128-bit GUID
  // as identified in a code module by a new-style CodeView record, or it's
  // a 32-bit signature (timestamp) as identified by an old-style record.
  // See MDCVInfoPDB70 and MDCVInfoPDB20 in minidump_format.h.
  //
  // Because DIA doesn't provide a way to directly determine whether a module
  // uses a GUID or a 32-bit signature, this code checks whether the first 32
  // bits of guid are the same as the signature, and if the rest of guid is
  // zero.  If so, then with a pretty high degree of certainty, there's an
  // old-style CodeView record in use.  This method will only falsely find an
  // an old-style CodeView record if a real 128-bit GUID has its first 32
  // bits set the same as the module's signature (timestamp) and the rest of
  // the GUID is set to 0.  This is highly unlikely.

  GUID signature_guid = {signature};  // 0-initializes other members
  *uses_guid = !IsEqualGUID(guid, signature_guid);
  return true;
}

}  // namespace google_breakpad
