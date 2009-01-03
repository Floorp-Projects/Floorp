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

#include <a.out.h>
#include <cstdarg>
#include <cstdlib>
#include <cxxabi.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <link.h>
#include <sys/mman.h>
#include <stab.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>

#include <functional>
#include <list>
#include <vector>
#include <string.h>

#include "common/linux/dump_symbols.h"
#include "common/linux/file_id.h"
#include "common/linux/guid_creator.h"
#include "processor/scoped_ptr.h"

// This namespace contains helper functions.
namespace {

// Infomation of a line.
struct LineInfo {
  // The index into string table for the name of the source file which
  // this line belongs to.
  // Load from stab symbol.
  uint32_t source_name_index;
  // Offset from start of the function.
  // Load from stab symbol.
  ElfW(Off) rva_to_func;
  // Offset from base of the loading binary.
  ElfW(Off) rva_to_base;
  // Size of the line.
  // It is the difference of the starting address of the line and starting
  // address of the next N_SLINE, N_FUN or N_SO.
  uint32_t size;
  // Line number.
  uint32_t line_num;
  // Id of the source file for this line.
  int source_id;
};

typedef std::list<struct LineInfo> LineInfoList;

// Information of a function.
struct FuncInfo {
  // Name of the function.
  const char *name;
  // Offset from the base of the loading address.
  ElfW(Off) rva_to_base;
  // Virtual address of the function.
  // Load from stab symbol.
  ElfW(Addr) addr;
  // Size of the function.
  // It is the difference of the starting address of the function and starting
  // address of the next N_FUN or N_SO.
  uint32_t size;
  // Total size of stack parameters.
  uint32_t stack_param_size;
  // Is there any lines included from other files?
  bool has_sol;
  // Line information array.
  LineInfoList line_info;
};

typedef std::list<struct FuncInfo> FuncInfoList;

// Information of a source file.
struct SourceFileInfo {
  // Name string index into the string table.
  uint32_t name_index;
  // Name of the source file.
  const char *name;
  // Starting address of the source file.
  ElfW(Addr) addr;
  // Id of the source file.
  int source_id;
  // Functions information.
  FuncInfoList func_info;
};

typedef std::list<struct SourceFileInfo> SourceFileInfoList;

// Information of a symbol table.
// This is the root of all types of symbol.
struct SymbolInfo {
  SourceFileInfoList source_file_info;

  // The next source id for newly found source file.
  int next_source_id;
};

// Stab section name.
static const char *kStabName = ".stab";

// Demangle using abi call.
// Older GCC may not support it.
static std::string Demangle(const char *mangled) {
  int status = 0;
  char *demangled = abi::__cxa_demangle(mangled, NULL, NULL, &status);
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
  ElfW(Word) base = reinterpret_cast<ElfW(Word)>(obj_base);
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

static bool WriteFormat(int fd, const char *fmt, ...) {
  va_list list;
  char buffer[4096];
  ssize_t expected, written;
  va_start(list, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, list);
  expected = strlen(buffer);
  written = write(fd, buffer, strlen(buffer));
  va_end(list);
  return expected == written;
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
      (char*)(strtab->sh_offset + sections[i].sh_name);
    if (!strncmp(name, section_name, name_len))
      return sections + i;
  }
  return NULL;
}

// TODO(liuli): Computer the stack parameter size.
// Expect parameter variables are immediately following the N_FUN symbol.
// Will need to parse the type information to get a correct size.
static int LoadStackParamSize(struct nlist *list,
                              struct nlist *list_end,
                              struct FuncInfo *func_info) {
  struct nlist *cur_list = list;
  assert(cur_list->n_type == N_FUN);
  ++cur_list;
  int step = 1;
  while (cur_list < list_end && cur_list->n_type == N_PSYM) {
    ++cur_list;
    ++step;
  }
  func_info->stack_param_size = 0;
  return step;
}

static int LoadLineInfo(struct nlist *list,
                        struct nlist *list_end,
                        const struct SourceFileInfo &source_file_info,
                        struct FuncInfo *func_info) {
  struct nlist *cur_list = list;
  func_info->has_sol = false;
  // Records which source file the following lines belongs. Default
  // to the file we are handling. This helps us handling inlined source.
  // When encountering N_SOL, we will change this to the source file
  // specified by N_SOL.
  int current_source_name_index = source_file_info.name_index;
  do {
    // Skip non line information.
    while (cur_list < list_end && cur_list->n_type != N_SLINE) {
      // Only exit when got another function, or source file.
      if (cur_list->n_type == N_FUN || cur_list->n_type == N_SO)
        return cur_list - list;
      // N_SOL means source lines following it will be from
      // another source file.
      if (cur_list->n_type == N_SOL) {
        func_info->has_sol = true;

        if (cur_list->n_un.n_strx > 0 &&
            cur_list->n_un.n_strx != current_source_name_index) {
          // The following lines will be from this source file.
          current_source_name_index = cur_list->n_un.n_strx;
        }
      }
      ++cur_list;
    }
    struct LineInfo line;
    while (cur_list < list_end && cur_list->n_type == N_SLINE) {
      line.source_name_index = current_source_name_index;
      line.rva_to_func = cur_list->n_value;
      // n_desc is a signed short
      line.line_num = (unsigned short)cur_list->n_desc;
      // Don't set it here.
      // Will be processed in later pass.
      line.source_id = -1;
      func_info->line_info.push_back(line);
      ++cur_list;
    }
  } while (list < list_end);

  return cur_list - list;
}

static int LoadFuncSymbols(struct nlist *list,
                           struct nlist *list_end,
                           const ElfW(Shdr) *stabstr_section,
                           struct SourceFileInfo *source_file_info) {
  struct nlist *cur_list = list;
  assert(cur_list->n_type == N_SO);
  ++cur_list;
  source_file_info->func_info.clear();
  while (cur_list < list_end) {
    // Go until the function symbol.
    while (cur_list < list_end && cur_list->n_type != N_FUN) {
      if (cur_list->n_type == N_SO) {
        return cur_list - list;
      }
      ++cur_list;
      continue;
    }
    if (cur_list->n_type == N_FUN) {
      struct FuncInfo func_info;
      func_info.name =
        reinterpret_cast<char *>(cur_list->n_un.n_strx +
                                 stabstr_section->sh_offset);
      func_info.addr = cur_list->n_value;
      func_info.rva_to_base = 0;
      func_info.size = 0;
      func_info.stack_param_size = 0;
      func_info.has_sol = 0;

      // Stack parameter size.
      cur_list += LoadStackParamSize(cur_list, list_end, &func_info);
      // Line info.
      cur_list += LoadLineInfo(cur_list,
                               list_end,
                               *source_file_info,
                               &func_info);

      // Functions in this module should have address bigger than the module
      // startring address.
      // There maybe a lot of duplicated entry for a function in the symbol,
      // only one of them can met this.
      if (func_info.addr >= source_file_info->addr) {
        source_file_info->func_info.push_back(func_info);
      }
    }
  }
  return cur_list - list;
}

// Comapre the address.
// The argument should have a memeber named "addr"
template<class T1, class T2>
static bool CompareAddress(T1 *a, T2 *b) {
  return a->addr < b->addr;
}

// Sort the array into increasing ordered array based on the virtual address.
// Return vector of pointers to the elements in the incoming array. So caller
// should make sure the returned vector lives longer than the incoming vector.
template<class Container>
static std::vector<typename Container::value_type *> SortByAddress(
    Container *container) {
  typedef typename Container::iterator It;
  typedef typename Container::value_type T;
  std::vector<T *> sorted_array_ptr;
  sorted_array_ptr.reserve(container->size());
  for (It it = container->begin(); it != container->end(); it++)
    sorted_array_ptr.push_back(&(*it));
  std::sort(sorted_array_ptr.begin(),
            sorted_array_ptr.end(),
            std::ptr_fun(CompareAddress<T, T>));

  return sorted_array_ptr;
}

// Find the address of the next function or source file symbol in the symbol
// table. The address should be bigger than the current function's address.
static ElfW(Addr) NextAddress(
    std::vector<struct FuncInfo *> *sorted_functions,
    std::vector<struct SourceFileInfo *> *sorted_files,
    const struct FuncInfo &func_info) {
  std::vector<struct FuncInfo *>::iterator next_func_iter =
    std::find_if(sorted_functions->begin(),
                 sorted_functions->end(),
                 std::bind1st(
                     std::ptr_fun(
                         CompareAddress<struct FuncInfo,
                                        struct FuncInfo>
                         ),
                     &func_info)
                );
  if (next_func_iter != sorted_functions->end())
    return (*next_func_iter)->addr;

  std::vector<struct SourceFileInfo *>::iterator next_file_iter =
    std::find_if(sorted_files->begin(),
                 sorted_files->end(),
                 std::bind1st(
                     std::ptr_fun(
                         CompareAddress<struct FuncInfo,
                                        struct SourceFileInfo>
                         ),
                     &func_info)
                );
  if (next_file_iter != sorted_files->end()) {
    return (*next_file_iter)->addr;
  }
  return 0;
}

static int FindFileByNameIdx(uint32_t name_index,
                             SourceFileInfoList &files) {
  for (SourceFileInfoList::iterator it = files.begin();
       it != files.end(); it++) {
    if (it->name_index == name_index)
      return it->source_id;
  }

  return -1;
}

// Add included file information.
// Also fix the source id for the line info.
static void AddIncludedFiles(struct SymbolInfo *symbols,
                             const ElfW(Shdr) *stabstr_section) {
  for (SourceFileInfoList::iterator source_file_it =
	 symbols->source_file_info.begin();
       source_file_it != symbols->source_file_info.end();
       ++source_file_it) {
    struct SourceFileInfo &source_file = *source_file_it;

    for (FuncInfoList::iterator func_info_it = source_file.func_info.begin(); 
	 func_info_it != source_file.func_info.end();
	 ++func_info_it) {
      struct FuncInfo &func_info = *func_info_it;

      for (LineInfoList::iterator line_info_it = func_info.line_info.begin(); 
	   line_info_it != func_info.line_info.end(); ++line_info_it) {
        struct LineInfo &line_info = *line_info_it;

        assert(line_info.source_name_index > 0);
        assert(source_file.name_index > 0);

        // Check if the line belongs to the source file by comparing the
        // name index into string table.
        if (line_info.source_name_index != source_file.name_index) {
          // This line is not from the current source file, check if this
          // source file has been added before.
          int found_source_id = FindFileByNameIdx(line_info.source_name_index,
                                                  symbols->source_file_info);
          if (found_source_id < 0) {
            // Got a new included file.
            // Those included files don't have address or line information.
            SourceFileInfo new_file;
            new_file.name_index = line_info.source_name_index;
            new_file.name = reinterpret_cast<char *>(new_file.name_index +
                                                     stabstr_section->sh_offset);
            new_file.addr = 0;
            new_file.source_id = symbols->next_source_id++;
            line_info.source_id = new_file.source_id;
            symbols->source_file_info.push_back(new_file);
          } else {
            // The file has been added.
            line_info.source_id = found_source_id;
          }
        } else {
          // The line belongs to the file.
          line_info.source_id = source_file.source_id;
        }
      }  // for each line.
    }  // for each function.
  } // for each source file.

}

// Compute size and rva information based on symbols loaded from stab section.
static bool ComputeSizeAndRVA(ElfW(Addr) loading_addr,
                              struct SymbolInfo *symbols) {
  std::vector<struct SourceFileInfo *> sorted_files =
    SortByAddress(&(symbols->source_file_info));
  for (size_t i = 0; i < sorted_files.size(); ++i) {
    struct SourceFileInfo &source_file = *sorted_files[i];
    std::vector<struct FuncInfo *> sorted_functions =
      SortByAddress(&(source_file.func_info));
    for (size_t j = 0; j < sorted_functions.size(); ++j) {
      struct FuncInfo &func_info = *sorted_functions[j];
      assert(func_info.addr >= loading_addr);
      func_info.rva_to_base = func_info.addr - loading_addr;
      func_info.size = 0;
      ElfW(Addr) next_addr = NextAddress(&sorted_functions,
                                         &sorted_files,
                                         func_info);
      // I've noticed functions with an address bigger than any other functions
      // and source files modules, this is probably the last function in the
      // module, due to limitions of Linux stab symbol, it is impossible to get
      // the exact size of this kind of function, thus we give it a default
      // very big value. This should be safe since this is the last function.
      // But it is a ugly hack.....
      // The following code can reproduce the case:
      // template<class T>
      // void Foo(T value) {
      // }
      //
      // int main(void) {
      //   Foo(10);
      //   Foo(std::string("hello"));
      //   return 0;
      // }
      // TODO(liuli): Find a better solution.
      static const int kDefaultSize = 0x10000000;
      static int no_next_addr_count = 0;
      if (next_addr != 0) {
        func_info.size = next_addr - func_info.addr;
      } else {
        if (no_next_addr_count > 1) {
          fprintf(stderr, "Got more than one funtion without the \
                  following symbol. Igore this function.\n");
          fprintf(stderr, "The dumped symbol may not correct.\n");
          assert(!"This should not happen!\n");
          func_info.size = 0;
          continue;
        }

        no_next_addr_count++;
        func_info.size = kDefaultSize;
      }
      // Compute line size.
      for (LineInfoList::iterator line_info_it = func_info.line_info.begin(); 
	   line_info_it != func_info.line_info.end(); line_info_it++) {
        struct LineInfo &line_info = *line_info_it;
	LineInfoList::iterator next_line_info_it = line_info_it;
	next_line_info_it++;
        line_info.size = 0;
        if (next_line_info_it != func_info.line_info.end()) {
          line_info.size =
            next_line_info_it->rva_to_func - line_info.rva_to_func;
        } else {
          // The last line in the function.
          // If we can find a function or source file symbol immediately
          // following the line, we can get the size of the line by computing
          // the difference of the next address to the starting address of this
          // line.
          // Otherwise, we need to set a default big enough value. This occurs
          // mostly because the this function is the last one in the module.
          if (next_addr != 0) {
            ElfW(Off) next_addr_offset = next_addr - func_info.addr;
            line_info.size = next_addr_offset - line_info.rva_to_func;
          } else {
            line_info.size = kDefaultSize;
          }
        }
        line_info.rva_to_base = line_info.rva_to_func + func_info.rva_to_base;
      }  // for each line.
    }  // for each function.
  } // for each source file.
  return true;
}

static bool LoadSymbols(const ElfW(Shdr) *stab_section,
                        const ElfW(Shdr) *stabstr_section,
                        ElfW(Addr) loading_addr,
                        struct SymbolInfo *symbols) {
  if (stab_section == NULL || stabstr_section == NULL)
    return false;

  struct nlist *lists =
    reinterpret_cast<struct nlist *>(stab_section->sh_offset);
  int nstab = stab_section->sh_size / sizeof(struct nlist);
  // First pass, load all symbols from the object file.
  for (int i = 0; i < nstab; ) {
    int step = 1;
    struct nlist *cur_list = lists + i;
    if (cur_list->n_type == N_SO) {
      // FUNC <address> <length> <param_stack_size> <function>
      struct SourceFileInfo source_file_info;
      source_file_info.name_index = cur_list->n_un.n_strx;
      source_file_info.name = reinterpret_cast<char *>(cur_list->n_un.n_strx +
                                 stabstr_section->sh_offset);
      source_file_info.addr = cur_list->n_value;
      if (strchr(source_file_info.name, '.'))
        source_file_info.source_id = symbols->next_source_id++;
      else
        source_file_info.source_id = -1;
      step = LoadFuncSymbols(cur_list, lists + nstab,
                             stabstr_section, &source_file_info);
      symbols->source_file_info.push_back(source_file_info);
    }
    i += step;
  }

  // Second pass, compute the size of functions and lines.
  if (ComputeSizeAndRVA(loading_addr, symbols)) {
    // Third pass, check for included source code, especially for header files.
    // Until now, we only have compiling unit information, but they can
    // have code from include files, add them here.
    AddIncludedFiles(symbols, stabstr_section);
    return true;
  }
  return false;
}

static bool LoadSymbols(ElfW(Ehdr) *elf_header, struct SymbolInfo *symbols) {
  // Translate all offsets in section headers into address.
  FixAddress(elf_header);
  ElfW(Addr) loading_addr = GetLoadingAddress(
      reinterpret_cast<ElfW(Phdr) *>(elf_header->e_phoff),
      elf_header->e_phnum);

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
  return LoadSymbols(stab_section, stabstr_section, loading_addr, symbols);
}

static bool WriteModuleInfo(int fd,
                            ElfW(Half) arch,
                            const std::string &obj_file) {
  const char *arch_name = NULL;
  if (arch == EM_386)
    arch_name = "x86";
  else if (arch == EM_X86_64)
    arch_name = "x86_64";
  else
    return false;

  unsigned char identifier[16];
  google_breakpad::FileID file_id(obj_file.c_str());
  if (file_id.ElfFileIdentifier(identifier)) {
    char identifier_str[40];
    file_id.ConvertIdentifierToString(identifier,
                                      identifier_str, sizeof(identifier_str));
    char id_no_dash[40];
    int id_no_dash_len = 0;
    memset(id_no_dash, 0, sizeof(id_no_dash));
    for (int i = 0; identifier_str[i] != '\0'; ++i)
      if (identifier_str[i] != '-')
        id_no_dash[id_no_dash_len++] = identifier_str[i];
    // Add an extra "0" by the end.
    id_no_dash[id_no_dash_len++] = '0';
    std::string filename = obj_file;
    size_t slash_pos = obj_file.find_last_of("/");
    if (slash_pos != std::string::npos)
      filename = obj_file.substr(slash_pos + 1);
    return WriteFormat(fd, "MODULE Linux %s %s %s\n", arch_name,
                       id_no_dash, filename.c_str());
  }
  return false;
}

static bool WriteSourceFileInfo(int fd, const struct SymbolInfo &symbols) {
  for (SourceFileInfoList::const_iterator it =
	 symbols.source_file_info.begin();
       it != symbols.source_file_info.end(); it++) {
    if (it->source_id != -1) {
      const char *name = it->name;
      if (!WriteFormat(fd, "FILE %d %s\n", it->source_id, name))
        return false;
    }
  }
  return true;
}

static bool WriteOneFunction(int fd,
                             const struct FuncInfo &func_info){
  // Discard the ending part of the name.
  std::string func_name(func_info.name);
  std::string::size_type last_colon = func_name.find_last_of(':');
  if (last_colon != std::string::npos)
    func_name = func_name.substr(0, last_colon);
  func_name = Demangle(func_name.c_str());

  if (func_info.size <= 0)
    return true;

  if (WriteFormat(fd, "FUNC %lx %lx %d %s\n",
                  func_info.rva_to_base,
                  func_info.size,
                  func_info.stack_param_size,
                  func_name.c_str())) {
    for (LineInfoList::const_iterator it = func_info.line_info.begin();
	 it != func_info.line_info.end(); it++) {
      const struct LineInfo &line_info = *it;
      if (!WriteFormat(fd, "%lx %lx %d %d\n",
                       line_info.rva_to_base,
                       line_info.size,
                       line_info.line_num,
                       line_info.source_id))
        return false;
    }
    return true;
  }
  return false;
}

static bool WriteFunctionInfo(int fd, const struct SymbolInfo &symbols) {
  for (SourceFileInfoList::const_iterator it =
	 symbols.source_file_info.begin();
       it != symbols.source_file_info.end(); it++) {
    const struct SourceFileInfo &file_info = *it;
    for (FuncInfoList::const_iterator fiIt = file_info.func_info.begin(); 
	 fiIt != file_info.func_info.end(); fiIt++) {
      const struct FuncInfo &func_info = *fiIt;
      if (!WriteOneFunction(fd, func_info))
        return false;
    }
  }
  return true;
}

static bool DumpStabSymbols(int fd, const struct SymbolInfo &symbols) {
  return WriteSourceFileInfo(fd, symbols) &&
    WriteFunctionInfo(fd, symbols);
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

}  // namespace

namespace google_breakpad {

bool DumpSymbols::WriteSymbolFile(const std::string &obj_file,
                                  int sym_fd) {
  int obj_fd = open(obj_file.c_str(), O_RDONLY);
  if (obj_fd < 0)
    return false;
  FDWrapper obj_fd_wrapper(obj_fd);
  struct stat st;
  if (fstat(obj_fd, &st) != 0 && st.st_size <= 0)
    return false;
  void *obj_base = mmap(NULL, st.st_size,
                        PROT_READ | PROT_WRITE, MAP_PRIVATE, obj_fd, 0);
  if (!obj_base)
    return false;
  MmapWrapper map_wrapper(obj_base, st.st_size);
  ElfW(Ehdr) *elf_header = reinterpret_cast<ElfW(Ehdr) *>(obj_base);
  if (!IsValidElf(elf_header))
    return false;
  struct SymbolInfo symbols;
  symbols.next_source_id = 0;

  if (!LoadSymbols(elf_header, &symbols))
     return false;
  // Write to symbol file.
  if (WriteModuleInfo(sym_fd, elf_header->e_machine, obj_file) &&
      DumpStabSymbols(sym_fd, symbols))
    return true;

  return false;
}

}  // namespace google_breakpad
