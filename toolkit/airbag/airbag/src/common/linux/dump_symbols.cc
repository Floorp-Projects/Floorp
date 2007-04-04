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

#include <functional>
#include <vector>

#include "common/linux/dump_symbols.h"
#include "common/linux/file_id.h"
#include "common/linux/guid_creator.h"
#include "processor/scoped_ptr.h"

// This namespace contains helper functions.
namespace {

// Infomation of a line.
struct LineInfo {
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
};

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
  // Is the function defined in included function?
  bool is_sol;
  // Line information array.
  std::vector<struct LineInfo> line_info;
};

// Information of a source file.
struct SourceFileInfo {
  // Name of the source file.
  const char *name;
  // Starting address of the source file.
  ElfW(Addr) addr;
  // Id of the source file.
  int source_id;
  // Functions information.
  std::vector<struct FuncInfo> func_info;
};

// Information of a symbol table.
// This is the root of all types of symbol.
struct SymbolInfo {
  std::vector<struct SourceFileInfo> source_file_info;
};

// Stab section name.
const char *kStabName = ".stab";

// Stab str section name.
const char *kStabStrName = ".stabstr";

// Demangle using abi call.
// Older GCC may not support it.
std::string Demangle(const char *mangled) {
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
void FixAddress(void *obj_base) {
  ElfW(Word) base = reinterpret_cast<ElfW(Word)>(obj_base);
  ElfW(Ehdr) *elf_header = static_cast<ElfW(Ehdr) *>(obj_base);
  elf_header->e_phoff += base;
  elf_header->e_shoff += base;
  ElfW(Shdr) *sections = reinterpret_cast<ElfW(Shdr) *>(elf_header->e_shoff);
  for (int i = 0; i < elf_header->e_shnum; ++i)
    sections[i].sh_offset += base;
}

// Find the prefered loading address of the binary.
ElfW(Addr) GetLoadingAddress(const ElfW(Phdr) *program_headers, int nheader) {
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

bool WriteFormat(int fd, const char *fmt, ...) {
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

bool IsValidElf(const ElfW(Ehdr) *elf_header) {
  return memcmp(elf_header, ELFMAG, SELFMAG) == 0;
}

const ElfW(Shdr) *FindSectionByName(const char *name,
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
int LoadStackParamSize(struct nlist *list,
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

int LoadLineInfo(struct nlist *list,
                 struct nlist *list_end,
                 struct FuncInfo *func_info) {
  struct nlist *cur_list = list;
  func_info->is_sol = false;
  do {
    // Skip non line information.
    while (cur_list < list_end && cur_list->n_type != N_SLINE) {
      // Only exit when got another function, or source file.
      if (cur_list->n_type == N_FUN || cur_list->n_type == N_SO)
        return cur_list - list;
      if (cur_list->n_type == N_SOL)
        func_info->is_sol = true;
      ++cur_list;
    }
    struct LineInfo line;
    while (cur_list < list_end && cur_list->n_type == N_SLINE) {
      line.rva_to_func = cur_list->n_value;
      line.line_num = cur_list->n_desc;
      func_info->line_info.push_back(line);
      ++cur_list;
    }
  } while (list < list_end);

  return cur_list - list;
}

int LoadFuncSymbols(struct nlist *list,
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
      memset(&func_info, 0, sizeof(func_info));
      func_info.name =
        reinterpret_cast<char *>(cur_list->n_un.n_strx +
                                 stabstr_section->sh_offset);
      func_info.addr = cur_list->n_value;
      // Stack parameter size.
      cur_list += LoadStackParamSize(cur_list, list_end, &func_info);
      // Line info.
      cur_list += LoadLineInfo(cur_list, list_end, &func_info);
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
bool CompareAddress(T1 *a, T2 *b) {
  return a->addr < b->addr;
}

// Sort the array into increasing ordered array based on the virtual address.
// Return vector of pointers to the elements in the incoming array. So caller
// should make sure the returned vector lives longer than the incoming vector.
template<class T>
std::vector<T *> SortByAddress(std::vector<T> *array) {
  std::vector<T *> sorted_array_ptr;
  sorted_array_ptr.reserve(array->size());
  for (size_t i = 0; i < array->size(); ++i)
    sorted_array_ptr.push_back(&(array->at(i)));
  std::sort(sorted_array_ptr.begin(),
            sorted_array_ptr.end(),
            std::ptr_fun(CompareAddress<T, T>));

  return sorted_array_ptr;
}

// Find the address of the next function or source file symbol in the symbol
// table. The address should be bigger than the current function's address.
ElfW(Addr) NextAddress(std::vector<struct FuncInfo *> *sorted_functions,
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

// Compute size and rva information based on symbols loaded from stab section.
bool ComputeSizeAndRVA(ElfW(Addr) loading_addr, struct SymbolInfo *symbols) {
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
      for (size_t k = 0; k < func_info.line_info.size(); ++k) {
        struct LineInfo &line_info = func_info.line_info[k];
        line_info.size = 0;
        if (k + 1 < func_info.line_info.size()) {
          line_info.size =
            func_info.line_info[k + 1].rva_to_func - line_info.rva_to_func;
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

bool LoadSymbols(const ElfW(Shdr) *stab_section,
                 const ElfW(Shdr) *stabstr_section,
                 ElfW(Addr) loading_addr,
                 struct SymbolInfo *symbols) {
  if (stab_section == NULL || stabstr_section == NULL)
    return false;

  struct nlist *lists =
    reinterpret_cast<struct nlist *>(stab_section->sh_offset);
  int nstab = stab_section->sh_size / sizeof(struct nlist);
  int source_id = 0;
  // First pass, load all symbols from the object file.
  for (int i = 0; i < nstab; ) {
    int step = 1;
    struct nlist *cur_list = lists + i;
    if (cur_list->n_type == N_SO) {
      // FUNC <address> <length> <param_stack_size> <function>
      struct SourceFileInfo source_file_info;
      source_file_info.name = reinterpret_cast<char *>(cur_list->n_un.n_strx +
                                 stabstr_section->sh_offset);
      source_file_info.addr = cur_list->n_value;
      if (strchr(source_file_info.name, '.'))
        source_file_info.source_id = source_id++;
      else
        source_file_info.source_id = -1;
      step = LoadFuncSymbols(cur_list, lists + nstab,
                             stabstr_section, &source_file_info);
      symbols->source_file_info.push_back(source_file_info);
    }
    i += step;
  }
  // Second pass, compute the size of functions and lines.
  return ComputeSizeAndRVA(loading_addr, symbols);
}

bool LoadSymbols(ElfW(Ehdr) *elf_header, struct SymbolInfo *symbols) {
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

bool WriteModuleInfo(int fd, ElfW(Half) arch, const std::string &obj_file) {
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
    return WriteFormat(fd, "MODULE Linux %s %s 1 %s\n", arch_name,
                       id_no_dash, filename.c_str());
  }
  return false;
}

bool WriteSourceFileInfo(int fd, const struct SymbolInfo &symbols) {
  for (size_t i = 0; i < symbols.source_file_info.size(); ++i) {
    if (symbols.source_file_info[i].source_id != -1) {
      const char *name = symbols.source_file_info[i].name;
      if (!WriteFormat(fd, "FILE %d %s\n",
                       symbols.source_file_info[i].source_id, name))
        return false;
    }
  }
  return true;
}

bool WriteOneFunction(int fd, int source_id,
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
    for (size_t i = 0; i < func_info.line_info.size(); ++i) {
      const struct LineInfo &line_info = func_info.line_info[i];
      if (!WriteFormat(fd, "%lx %lx %d %d\n",
                       line_info.rva_to_base,
                       line_info.size,
                       line_info.line_num,
                       source_id))
        return false;
    }
    return true;
  }
  return false;
}

bool WriteFunctionInfo(int fd, const struct SymbolInfo &symbols) {
  for (size_t i = 0; i < symbols.source_file_info.size(); ++i) {
    const struct SourceFileInfo &file_info = symbols.source_file_info[i];
    for (size_t j = 0; j < file_info.func_info.size(); ++j) {
      const struct FuncInfo &func_info = file_info.func_info[j];
      if (!WriteOneFunction(fd, file_info.source_id, func_info))
        return false;
    }
  }
  return true;
}

bool DumpStabSymbols(int fd, const struct SymbolInfo &symbols) {
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
                       const std::string &symbol_file) {
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
  if (!LoadSymbols(elf_header, &symbols))
     return false;
  // Write to symbol file.
  int sym_fd = open(symbol_file.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0666);
  if (sym_fd < 0)
    return false;
  FDWrapper sym_fd_wrapper(sym_fd);
  if (WriteModuleInfo(sym_fd, elf_header->e_machine, obj_file) &&
      DumpStabSymbols(sym_fd, symbols))
    return true;

  // Remove the symbol file if failed to write the symbols.
  unlink(symbol_file.c_str());
  return false;
}

}  // namespace google_breakpad
