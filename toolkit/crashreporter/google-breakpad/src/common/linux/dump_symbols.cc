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

#include <assert.h>
#include <cxxabi.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <link.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <list>
#include <map>
#include <string>
#include <vector>

#include "common/linux/dump_symbols.h"
#include "common/linux/file_id.h"
#include "common/linux/module.h"
#include "common/linux/stabs_reader.h"

// This namespace contains helper functions.
namespace {

using google_breakpad::Module;
using std::vector;

// Stab section name.
static const char *kStabName = ".stab";

// Demangle using abi call.
// Older GCC may not support it.
static std::string Demangle(const std::string &mangled) {
  int status = 0;
  char *demangled = abi::__cxa_demangle(mangled.c_str(), NULL, NULL, &status);
  if (status == 0 && demangled != NULL) {
    std::string str(demangled);
    free(demangled);
    return str;
  }
  return std::string(mangled);
}

// Fix offset into virtual address by adding the mapped base into offsets.
// Make life easier when want to find something by offset.
static void FixAddress(void *obj_base) {
  ElfW(Addr) base = reinterpret_cast<ElfW(Addr)>(obj_base);
  ElfW(Ehdr) *elf_header = static_cast<ElfW(Ehdr) *>(obj_base);
  elf_header->e_phoff += base;
  elf_header->e_shoff += base;
  ElfW(Shdr) *sections = reinterpret_cast<ElfW(Shdr) *>(elf_header->e_shoff);
  for (int i = 0; i < elf_header->e_shnum; ++i)
    sections[i].sh_offset += base;
}

// Find the prefered loading address of the binary.
static ElfW(Addr) GetLoadingAddress(const ElfW(Phdr) *program_headers,
                                    int nheader) {
  for (int i = 0; i < nheader; ++i) {
    const ElfW(Phdr) &header = program_headers[i];
    // For executable, it is the PT_LOAD segment with offset to zero.
    if (header.p_type == PT_LOAD &&
        header.p_offset == 0)
      return header.p_vaddr;
  }
  // For other types of ELF, return 0.
  return 0;
}

static bool IsValidElf(const ElfW(Ehdr) *elf_header) {
  return memcmp(elf_header, ELFMAG, SELFMAG) == 0;
}

static const ElfW(Shdr) *FindSectionByName(const char *name,
                                           const ElfW(Shdr) *sections,
                                           const ElfW(Shdr) *strtab,
                                           int nsection) {
  assert(name != NULL);
  assert(sections != NULL);
  assert(nsection > 0);

  int name_len = strlen(name);
  if (name_len == 0)
    return NULL;

  for (int i = 0; i < nsection; ++i) {
    const char *section_name =
      reinterpret_cast<char*>(strtab->sh_offset + sections[i].sh_name);
    if (!strncmp(name, section_name, name_len))
      return sections + i;
  }
  return NULL;
}

// Our handler class for STABS data.
class DumpStabsHandler: public google_breakpad::StabsHandler {
 public:
  DumpStabsHandler(Module *module) :
      module_(module),
      comp_unit_base_address_(0),
      current_function_(NULL),
      current_source_file_(NULL),
      current_source_file_name_(NULL) { }

  bool StartCompilationUnit(const char *name, uint64_t address,
                            const char *build_directory);
  bool EndCompilationUnit(uint64_t address);
  bool StartFunction(const std::string &name, uint64_t address);
  bool EndFunction(uint64_t address);
  bool Line(uint64_t address, const char *name, int number);

  // Do any final processing necessary to make module_ contain all the
  // data provided by the STABS reader.
  //
  // Because STABS does not provide reliable size information for
  // functions and lines, we need to make a pass over the data after
  // processing all the STABS to compute those sizes.  We take care of
  // that here.
  void Finalize();

 private:

  // An arbitrary, but very large, size to use for functions whose
  // size we can't compute properly.
  static const uint64_t kFallbackSize = 0x10000000;

  // The module we're contributing debugging info to.
  Module *module_;

  // The functions we've generated so far.  We don't add these to
  // module_ as we parse them.  Instead, we wait until we've computed
  // their ending address, and their lines' ending addresses.
  //
  // We could just stick them in module_ from the outset, but if
  // module_ already contains data gathered from other debugging
  // formats, that would complicate the size computation.
  vector<Module::Function *> functions_;

  // Boundary addresses.  STABS doesn't necessarily supply sizes for
  // functions and lines, so we need to compute them ourselves by
  // finding the next object.
  vector<Module::Address> boundaries_;

  // The base address of the current compilation unit.  We use this to
  // recognize functions we should omit from the symbol file.  (If you
  // know the details of why we omit these, please patch this
  // comment.)
  Module::Address comp_unit_base_address_;

  // The function we're currently contributing lines to.
  Module::Function *current_function_;

  // The last Module::File we got a line number in.
  Module::File *current_source_file_;

  // The pointer in the .stabstr section of the name that
  // current_source_file_ is built from.  This allows us to quickly
  // recognize when the current line is in the same file as the
  // previous one (which it usually is).
  const char *current_source_file_name_;
};
    
bool DumpStabsHandler::StartCompilationUnit(const char *name, uint64_t address,
                                            const char *build_directory) {
  assert(! comp_unit_base_address_);
  current_source_file_name_ = name;
  current_source_file_ = module_->FindFile(name);
  comp_unit_base_address_ = address;
  boundaries_.push_back(static_cast<Module::Address>(address));
  return true;
}

bool DumpStabsHandler::EndCompilationUnit(uint64_t address) {
  assert(comp_unit_base_address_);
  comp_unit_base_address_ = 0;
  current_source_file_ = NULL;
  current_source_file_name_ = NULL;
  if (address)
    boundaries_.push_back(static_cast<Module::Address>(address));
  return true;
}

bool DumpStabsHandler::StartFunction(const std::string &name,
                                     uint64_t address) {
  assert(! current_function_);
  Module::Function *f = new Module::Function;
  f->name_ = Demangle(name);
  f->address_ = address;
  f->size_ = 0;           // We compute this in DumpStabsHandler::Finalize().
  f->parameter_size_ = 0; // We don't provide this information.
  current_function_ = f;
  boundaries_.push_back(static_cast<Module::Address>(address));
  return true;
}

bool DumpStabsHandler::EndFunction(uint64_t address) {
  assert(current_function_);
  // Functions in this compilation unit should have address bigger
  // than the compilation unit's starting address.  There may be a lot
  // of duplicated entries for functions in the STABS data; only one
  // entry can meet this requirement.
  //
  // (I don't really understand the above comment; just bringing it
  // along from the previous code, and leaving the behaivor unchanged.
  // If you know the whole story, please patch this comment.  --jimb)
  if (current_function_->address_ >= comp_unit_base_address_)
    functions_.push_back(current_function_);
  else
    delete current_function_;
  current_function_ = NULL;
  if (address)
    boundaries_.push_back(static_cast<Module::Address>(address));
  return true;
}

bool DumpStabsHandler::Line(uint64_t address, const char *name, int number) {
  assert(current_function_);
  assert(current_source_file_);
  if (name != current_source_file_name_) {
    current_source_file_ = module_->FindFile(name);
    current_source_file_name_ = name;
  }
  Module::Line line;
  line.address_ = address;
  line.size_ = 0;  // We compute this in DumpStabsHandler::Finalize().
  line.file_ = current_source_file_;
  line.number_ = number;
  current_function_->lines_.push_back(line);
  return true;
}

void DumpStabsHandler::Finalize() {
  // Sort our boundary list, so we can search it quickly.
  sort(boundaries_.begin(), boundaries_.end());
  // Sort all functions by address, just for neatness.
  sort(functions_.begin(), functions_.end(),
       Module::Function::CompareByAddress);
  for (vector<Module::Function *>::iterator func_it = functions_.begin();
       func_it != functions_.end();
       func_it++) {
    Module::Function *f = *func_it;
    // Compute the function f's size.
    vector<Module::Address>::iterator boundary
        = std::upper_bound(boundaries_.begin(), boundaries_.end(), f->address_);
    if (boundary != boundaries_.end())
      f->size_ = *boundary - f->address_;
    else
      // If this is the last function in the module, and the STABS
      // reader was unable to give us its ending address, then assign
      // it a bogus, very large value.  This will happen at most once
      // per module: since we've added all functions' addresses to the
      // boundary table, only one can be the last.
      f->size_ = kFallbackSize;

    // Compute sizes for each of the function f's lines --- if it has any.
    if (! f->lines_.empty()) {
      stable_sort(f->lines_.begin(), f->lines_.end(),
                  Module::Line::CompareByAddress);
      vector<Module::Line>::iterator last_line = f->lines_.end() - 1;
      for (vector<Module::Line>::iterator line_it = f->lines_.begin();
           line_it != last_line; line_it++)
        line_it[0].size_ = line_it[1].address_ - line_it[0].address_;
      // Compute the size of the last line from f's end address.
      last_line->size_ = (f->address_ + f->size_) - last_line->address_;
    }
  }
  // Now that everything has a size, add our functions to the module, and
  // dispose of our private list.
  module_->AddFunctions(functions_.begin(), functions_.end());
  functions_.clear();
}

static bool LoadSymbols(const ElfW(Shdr) *stab_section,
                        const ElfW(Shdr) *stabstr_section,
                        Module *module) {
  if (stab_section == NULL || stabstr_section == NULL)
    return false;

  // A callback object to handle data from the STABS reader.
  DumpStabsHandler handler(module);
  // Find the addresses of the STABS data, and create a STABS reader object.
  uint8_t *stabs = reinterpret_cast<uint8_t *>(stab_section->sh_offset);
  uint8_t *stabstr = reinterpret_cast<uint8_t *>(stabstr_section->sh_offset);
  google_breakpad::StabsReader reader(stabs, stab_section->sh_size,
                                      stabstr, stabstr_section->sh_size,
                                      &handler);
  // Read the STABS data, and do post-processing.
  if (! reader.Process())
    return false;
  handler.Finalize();
  return true;
}

static bool LoadSymbols(ElfW(Ehdr) *elf_header, Module *module) {
  // Translate all offsets in section headers into address.
  FixAddress(elf_header);
  ElfW(Addr) loading_addr = GetLoadingAddress(
      reinterpret_cast<ElfW(Phdr) *>(elf_header->e_phoff),
      elf_header->e_phnum);
  module->SetLoadAddress(loading_addr);

  const ElfW(Shdr) *sections =
    reinterpret_cast<ElfW(Shdr) *>(elf_header->e_shoff);
  const ElfW(Shdr) *strtab = sections + elf_header->e_shstrndx;
  const ElfW(Shdr) *stab_section =
    FindSectionByName(kStabName, sections, strtab, elf_header->e_shnum);
  if (stab_section == NULL) {
    fprintf(stderr, "Stab section not found.\n");
    return false;
  }
  const ElfW(Shdr) *stabstr_section = stab_section->sh_link + sections;

  // Load symbols.
  return LoadSymbols(stab_section, stabstr_section, module);
}

//
// FDWrapper
//
// Wrapper class to make sure opened file is closed.
//
class FDWrapper {
 public:
  explicit FDWrapper(int fd) :
    fd_(fd) {
    }
  ~FDWrapper() {
    if (fd_ != -1)
      close(fd_);
  }
  int get() {
    return fd_;
  }
  int release() {
    int fd = fd_;
    fd_ = -1;
    return fd;
  }
 private:
  int fd_;
};

//
// MmapWrapper
//
// Wrapper class to make sure mapped regions are unmapped.
//
class MmapWrapper {
  public:
   MmapWrapper(void *mapped_address, size_t mapped_size) :
     base_(mapped_address), size_(mapped_size) {
   }
   ~MmapWrapper() {
     if (base_ != NULL) {
       assert(size_ > 0);
       munmap(base_, size_);
     }
   }
   void release() {
     base_ = NULL;
     size_ = 0;
   }

  private:
   void *base_;
   size_t size_;
};

// Return the breakpad symbol file identifier for the architecture of
// ELF_HEADER.
const char *ElfArchitecture(const ElfW(Ehdr) *elf_header) {
  ElfW(Half) arch = elf_header->e_machine;
  if (arch == EM_386)
    return "x86";
  else if (arch == EM_X86_64)
    return "x86_64";
  else
    return NULL;
}

// Format the Elf file identifier in IDENTIFIER as a UUID with the
// dashes removed.
std::string FormatIdentifier(unsigned char identifier[16]) {
  char identifier_str[40];
  google_breakpad::FileID::ConvertIdentifierToString(
      identifier,
      identifier_str,
      sizeof(identifier_str));
  std::string id_no_dash;
  for (int i = 0; identifier_str[i] != '\0'; ++i)
    if (identifier_str[i] != '-')
      id_no_dash += identifier_str[i];
  // Add an extra "0" by the end.  PDB files on Windows have an 'age'
  // number appended to the end of the file identifier; this isn't
  // really used or necessary on other platforms, but let's preserve
  // the pattern.
  id_no_dash += '0';
  return id_no_dash;
}

// Return the non-directory portion of FILENAME: the portion after the
// last slash, or the whole filename if there are no slashes.
std::string BaseFileName(const std::string &filename) {
  // Lots of copies!  basename's behavior is less than ideal.
  char *c_filename = strdup(filename.c_str());
  std::string base = basename(c_filename);
  free(c_filename);
  return base;
}

}  // namespace

namespace google_breakpad {

bool DumpSymbols::WriteSymbolFile(const std::string &obj_file,
                                  FILE *sym_file) {
  int obj_fd = open(obj_file.c_str(), O_RDONLY);
  if (obj_fd < 0)
    return false;
  FDWrapper obj_fd_wrapper(obj_fd);
  struct stat st;
  if (fstat(obj_fd, &st) != 0 && st.st_size <= 0)
    return false;
  void *obj_base = mmap(NULL, st.st_size,
                        PROT_READ | PROT_WRITE, MAP_PRIVATE, obj_fd, 0);
  if (obj_base == MAP_FAILED)
    return false;
  MmapWrapper map_wrapper(obj_base, st.st_size);
  ElfW(Ehdr) *elf_header = reinterpret_cast<ElfW(Ehdr) *>(obj_base);
  if (!IsValidElf(elf_header))
    return false;

  unsigned char identifier[16];
  google_breakpad::FileID file_id(obj_file.c_str());
  if (! file_id.ElfFileIdentifier(identifier))
    return false;

  const char *architecture = ElfArchitecture(elf_header);
  if (! architecture)
    return false;

  std::string name = BaseFileName(obj_file);
  std::string os = "Linux";
  std::string id = FormatIdentifier(identifier);

  Module module(name, os, architecture, id);
  if (!LoadSymbols(elf_header, &module))
    return false;
  if (!module.Write(sym_file))
    return false;

  return true;
}

}  // namespace google_breakpad
