/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CustomElf_h
#define CustomElf_h

/**
 * Android system headers have two different elf.h file. The one under linux/
 * is the most complete.
 */
#ifdef ANDROID
#include <linux/elf.h>
#else
#include <elf.h>
#endif
#include <endian.h>
#include "ElfLoader.h"
#include "Logging.h"

/**
 * Generic ELF macros for the target system
 */
#ifdef HAVE_64BIT_OS
#define Elf_(type) Elf64_ ## type
#define ELFCLASS ELFCLASS64
#define ELF_R_TYPE ELF64_R_TYPE
#define ELF_R_SYM ELF64_R_SYM
#ifndef ELF_ST_BIND
#define ELF_ST_BIND ELF64_ST_BIND
#endif
#define PRIxAddr "lx"
#else
#define Elf_(type) Elf32_ ## type
#define ELFCLASS ELFCLASS32
#define ELF_R_TYPE ELF32_R_TYPE
#define ELF_R_SYM ELF32_R_SYM
#ifndef ELF_ST_BIND
#define ELF_ST_BIND ELF32_ST_BIND
#endif
#define PRIxAddr "x"
#endif

#ifndef __BYTE_ORDER
#error Cannot find endianness
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define ELFDATA ELFDATA2LSB
#elif __BYTE_ORDER == __BIG_ENDIAN
#define ELFDATA ELFDATA2MSB
#endif

#ifdef __linux__
#define ELFOSABI ELFOSABI_LINUX
#ifdef EI_ABIVERSION
#define ELFABIVERSION 0
#endif
#else
#error Unknown ELF OSABI
#endif

#if defined(__i386__)
#define ELFMACHINE EM_386

// Doing this way probably doesn't scale to other architectures
#define R_ABS R_386_32
#define R_GLOB_DAT R_386_GLOB_DAT
#define R_JMP_SLOT R_386_JMP_SLOT
#define R_RELATIVE R_386_RELATIVE
#define RELOC(n) DT_REL ## n
#define UNSUPPORTED_RELOC(n) DT_RELA ## n
#define STR_RELOC(n) "DT_REL" # n
#define Reloc Rel

#elif defined(__x86_64__)
#define ELFMACHINE EM_X86_64

#define R_ABS R_X86_64_64
#define R_GLOB_DAT R_X86_64_GLOB_DAT
#define R_JMP_SLOT R_X86_64_JUMP_SLOT
#define R_RELATIVE R_X86_64_RELATIVE
#define RELOC(n) DT_RELA ## n
#define UNSUPPORTED_RELOC(n) DT_REL ## n
#define STR_RELOC(n) "DT_RELA" # n
#define Reloc Rela

#elif defined(__arm__)
#define ELFMACHINE EM_ARM

#ifndef R_ARM_ABS32
#define R_ARM_ABS32 2
#endif
#ifndef R_ARM_GLOB_DAT
#define R_ARM_GLOB_DAT 21
#endif
#ifndef R_ARM_JUMP_SLOT
#define R_ARM_JUMP_SLOT 22
#endif
#ifndef R_ARM_RELATIVE
#define R_ARM_RELATIVE 23
#endif

#define R_ABS R_ARM_ABS32
#define R_GLOB_DAT R_ARM_GLOB_DAT
#define R_JMP_SLOT R_ARM_JUMP_SLOT
#define R_RELATIVE R_ARM_RELATIVE
#define RELOC(n) DT_REL ## n
#define UNSUPPORTED_RELOC(n) DT_RELA ## n
#define STR_RELOC(n) "DT_REL" # n
#define Reloc Rel

#else
#error Unknown ELF machine type
#endif

/**
 * Android system headers don't have all definitions
 */
#ifndef STN_UNDEF
#define STN_UNDEF 0
#endif
#ifndef DT_INIT_ARRAY
#define DT_INIT_ARRAY 25
#endif
#ifndef DT_FINI_ARRAY
#define DT_FINI_ARRAY 26
#endif
#ifndef DT_INIT_ARRAYSZ
#define DT_INIT_ARRAYSZ 27
#endif
#ifndef DT_FINI_ARRAYSZ
#define DT_FINI_ARRAYSZ 28
#endif
#ifndef DT_RELACOUNT
#define DT_RELACOUNT 0x6ffffff9
#endif
#ifndef DT_RELCOUNT
#define DT_RELCOUNT 0x6ffffffa
#endif
#ifndef DT_VERSYM
#define DT_VERSYM 0x6ffffff0
#endif
#ifndef DT_VERDEF
#define DT_VERDEF 0x6ffffffc
#endif
#ifndef DT_VERDEFNUM
#define DT_VERDEFNUM 0x6ffffffd
#endif
#ifndef DT_VERNEED
#define DT_VERNEED 0x6ffffffe
#endif
#ifndef DT_VERNEEDNUM
#define DT_VERNEEDNUM 0x6fffffff
#endif
#ifndef DT_FLAGS
#define DT_FLAGS 30
#endif
#ifndef DF_SYMBOLIC
#define DF_SYMBOLIC 0x00000002
#endif

namespace Elf {

/**
 * Define a few basic Elf Types
 */
typedef Elf_(Phdr) Phdr;
typedef Elf_(Dyn) Dyn;
typedef Elf_(Sym) Sym;
typedef Elf_(Addr) Addr;
typedef Elf_(Word) Word;

/**
 * Helper class around the standard Elf header struct
 */
struct Ehdr: public Elf_(Ehdr)
{
  /**
   * Equivalent to reinterpret_cast<const Ehdr *>(buf), but additionally
   * checking that this is indeed an Elf header and that the Elf type
   * corresponds to that of the system
   */
  static const Ehdr *validate(const void *buf);
};

/**
 * Elf String table
 */
class Strtab: public UnsizedArray<const char>
{
public:
  /**
   * Returns the string at the given index in the table
   */
  const char *GetStringAt(off_t index) const
  {
    return &UnsizedArray<const char>::operator[](index);
  }
};

/**
 * Helper class around Elf relocation.
 */
struct Rel: public Elf_(Rel)
{
  /**
   * Returns the addend for the relocation, which is the value stored
   * at r_offset.
   */
  Addr GetAddend(void *base) const
  {
    return *(reinterpret_cast<const Addr *>(
                   reinterpret_cast<const char *>(base) + r_offset));
  }
};

/**
 * Helper class around Elf relocation with addend.
 */
struct Rela: public Elf_(Rela)
{
  /**
   * Returns the addend for the relocation.
   */
  Addr GetAddend(void *base) const
  {
    return r_addend;
  }
};

} /* namespace Elf */

class Mappable;

/**
 * Library Handle class for ELF libraries we don't let the system linker
 * handle.
 */
class CustomElf: public LibHandle, private ElfLoader::link_map
{
  friend class ElfLoader;
  friend class SEGVHandler;
public:
  /**
   * Returns a new CustomElf using the given file descriptor to map ELF
   * content. The file descriptor ownership is stolen, and it will be closed
   * in CustomElf's destructor if an instance is created, or by the Load
   * method otherwise. The path corresponds to the file descriptor, and flags
   * are the same kind of flags that would be given to dlopen(), though
   * currently, none are supported and the behaviour is more or less that of
   * RTLD_GLOBAL | RTLD_BIND_NOW.
   */
  static mozilla::TemporaryRef<LibHandle> Load(Mappable *mappable,
                                               const char *path, int flags);

  /**
   * Inherited from LibHandle
   */
  virtual ~CustomElf();
  virtual void *GetSymbolPtr(const char *symbol) const;
  virtual bool Contains(void *addr) const;

  /**
   * Shows some stats about the Mappable instance. The when argument is to be
   * used by the caller to give an identifier of the when the stats call is
   * made.
   */
  void stats(const char *when) const;

private:
  /**
   * Returns a pointer to the Elf Symbol in the Dynamic Symbol table
   * corresponding to the given symbol name (with a pre-computed hash).
   */
  const Elf::Sym *GetSymbol(const char *symbol, unsigned long hash) const;

  /**
   * Returns the address corresponding to the given symbol name (with a
   * pre-computed hash).
   */
  void *GetSymbolPtr(const char *symbol, unsigned long hash) const;

  /**
   * Scan dependent libraries to find the address corresponding to the
   * given symbol name. This is used to find symbols that are undefined
   * in the Elf object.
   */
  void *GetSymbolPtrInDeps(const char *symbol) const;

  /**
   * Private constructor
   */
  CustomElf(Mappable *mappable, const char *path)
  : LibHandle(path), mappable(mappable), init(0), fini(0), initialized(false)
  { }

  /**
   * Returns a pointer relative to the base address where the library is
   * loaded.
   */
  void *GetPtr(const Elf::Addr offset) const
  {
    return base + offset;
  }

  /**
   * Like the above, but returns a typed (const) pointer
   */
  template <typename T>
  const T *GetPtr(const Elf::Addr offset) const
  {
    return reinterpret_cast<const T *>(base + offset);
  }

  /**
   * Loads an Elf segment defined by the given PT_LOAD header.
   * Returns whether this succeeded or failed.
   */
  bool LoadSegment(const Elf::Phdr *pt_load) const;

  /**
   * Initializes the library according to information found in the given
   * PT_DYNAMIC header.
   * Returns whether this succeeded or failed.
   */
  bool InitDyn(const Elf::Phdr *pt_dyn);

  /**
   * Apply .rel.dyn/.rela.dyn relocations.
   * Returns whether this succeeded or failed.
   */
  bool Relocate();

  /**
   * Apply .rel.plt/.rela.plt relocations.
   * Returns whether this succeeded or failed.
   */
  bool RelocateJumps();

  /**
   * Call initialization functions (.init/.init_array)
   * Returns true;
   */
  bool CallInit();

  /**
   * Call destructor functions (.fini_array/.fini)
   * Returns whether this succeeded or failed.
   */
  void CallFini();

  /**
   * Call a function given a pointer to its location.
   */
  void CallFunction(void *ptr) const
  {
    /* C++ doesn't allow direct conversion between pointer-to-object
     * and pointer-to-function. */
    union {
      void *ptr;
      void (*func)(void);
    } f;
    f.ptr = ptr;
    debug("%s: Calling function @%p", GetPath(), ptr);
    f.func();
  }

  /**
   * Call a function given a an address relative to the library base
   */
  void CallFunction(Elf::Addr addr) const
  {
    return CallFunction(GetPtr(addr));
  }

  /* Appropriated Mappable */
  Mappable *mappable;

  /* Base address where the library is loaded */
  MappedPtr base;

  /* String table */
  Elf::Strtab strtab;

  /* Symbol table */
  UnsizedArray<Elf::Sym> symtab;

  /* Buckets and chains for the System V symbol hash table */
  Array<Elf::Word> buckets;
  UnsizedArray<Elf::Word> chains;

  /* List of dependent libraries */
  std::vector<mozilla::RefPtr<LibHandle> > dependencies;

  /* List of .rel.dyn/.rela.dyn relocations */
  Array<Elf::Reloc> relocations;

  /* List of .rel.plt/.rela.plt relocation */
  Array<Elf::Reloc> jumprels;

  /* Relative address of the initialization and destruction functions
   * (.init/.fini) */
  Elf::Addr init, fini;

  /* List of initialization and destruction functions
   * (.init_array/.fini_array) */
  Array<void *> init_array, fini_array;

  bool initialized;
};

#endif /* CustomElf_h */
