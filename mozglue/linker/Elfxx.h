/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Elfxx_h
#define Elfxx_h

/**
 * Android system headers have two different elf.h file. The one under linux/
 * is the most complete on older android API versions.
 */
#if defined(ANDROID) && __ANDROID_API__ < 21
#include <linux/elf.h>
#else
#include <elf.h>
#endif
#include <endian.h>

#if defined(__ARM_EABI__) && !defined(PT_ARM_EXIDX)
#define PT_ARM_EXIDX 0x70000001
#endif

/**
 * Generic ELF macros for the target system
 */
#ifdef __LP64__
#define Elf_(type) Elf64_ ## type
#define ELFCLASS ELFCLASS64
#define ELF_R_TYPE ELF64_R_TYPE
#define ELF_R_SYM ELF64_R_SYM
#ifndef ELF_ST_BIND
#define ELF_ST_BIND ELF64_ST_BIND
#endif
#else
#define Elf_(type) Elf32_ ## type
#define ELFCLASS ELFCLASS32
#define ELF_R_TYPE ELF32_R_TYPE
#define ELF_R_SYM ELF32_R_SYM
#ifndef ELF_ST_BIND
#define ELF_ST_BIND ELF32_ST_BIND
#endif
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
#ifndef DT_FLAGS_1
#define DT_FLAGS_1 0x6ffffffb
#endif
#ifndef DT_FLAGS
#define DT_FLAGS 30
#endif
#ifndef DF_SYMBOLIC
#define DF_SYMBOLIC 0x00000002
#endif
#ifndef DF_TEXTREL
#define DF_TEXTREL 0x00000004
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
typedef Elf_(Half) Half;

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

#endif /* Elfxx_h */
