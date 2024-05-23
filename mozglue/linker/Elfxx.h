/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Elfxx_h
#define Elfxx_h

#include "Utils.h"

#include <elf.h>
#include <endian.h>

/**
 * Generic ELF macros for the target system
 */
#ifdef __LP64__
#  define Elf_(type) Elf64_##type
#  define ELFCLASS ELFCLASS64
#  define ELF_R_TYPE ELF64_R_TYPE
#  define ELF_R_SYM ELF64_R_SYM
#  ifndef ELF_ST_BIND
#    define ELF_ST_BIND ELF64_ST_BIND
#  endif
#else
#  define Elf_(type) Elf32_##type
#  define ELFCLASS ELFCLASS32
#  define ELF_R_TYPE ELF32_R_TYPE
#  define ELF_R_SYM ELF32_R_SYM
#  ifndef ELF_ST_BIND
#    define ELF_ST_BIND ELF32_ST_BIND
#  endif
#endif

#ifndef __BYTE_ORDER
#  error Cannot find endianness
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN
#  define ELFDATA ELFDATA2LSB
#elif __BYTE_ORDER == __BIG_ENDIAN
#  define ELFDATA ELFDATA2MSB
#endif

#ifdef __linux__
#  define ELFOSABI ELFOSABI_LINUX
#  ifdef EI_ABIVERSION
#    define ELFABIVERSION 0
#  endif
#else
#  error Unknown ELF OSABI
#endif

#if defined(__i386__)
#  define ELFMACHINE EM_386

// Doing this way probably doesn't scale to other architectures
#  define R_ABS R_386_32
#  define R_GLOB_DAT R_386_GLOB_DAT
#  define R_JMP_SLOT R_386_JMP_SLOT
#  define R_RELATIVE R_386_RELATIVE
#  define RELOC(n) DT_REL##n
#  define UNSUPPORTED_RELOC(n) DT_RELA##n
#  define STR_RELOC(n) "DT_REL" #n
#  define Reloc Rel

#elif defined(__x86_64__)
#  define ELFMACHINE EM_X86_64

#  define R_ABS R_X86_64_64
#  define R_GLOB_DAT R_X86_64_GLOB_DAT
#  define R_JMP_SLOT R_X86_64_JUMP_SLOT
#  define R_RELATIVE R_X86_64_RELATIVE
#  define RELOC(n) DT_RELA##n
#  define UNSUPPORTED_RELOC(n) DT_REL##n
#  define STR_RELOC(n) "DT_RELA" #n
#  define Reloc Rela

#elif defined(__arm__)
#  define ELFMACHINE EM_ARM

#  ifndef R_ARM_ABS32
#    define R_ARM_ABS32 2
#  endif
#  ifndef R_ARM_GLOB_DAT
#    define R_ARM_GLOB_DAT 21
#  endif
#  ifndef R_ARM_JUMP_SLOT
#    define R_ARM_JUMP_SLOT 22
#  endif
#  ifndef R_ARM_RELATIVE
#    define R_ARM_RELATIVE 23
#  endif

#  define R_ABS R_ARM_ABS32
#  define R_GLOB_DAT R_ARM_GLOB_DAT
#  define R_JMP_SLOT R_ARM_JUMP_SLOT
#  define R_RELATIVE R_ARM_RELATIVE
#  define RELOC(n) DT_REL##n
#  define UNSUPPORTED_RELOC(n) DT_RELA##n
#  define STR_RELOC(n) "DT_REL" #n
#  define Reloc Rel

#elif defined(__aarch64__)
#  define ELFMACHINE EM_AARCH64

#  define R_ABS R_AARCH64_ABS64
#  define R_GLOB_DAT R_AARCH64_GLOB_DAT
#  define R_JMP_SLOT R_AARCH64_JUMP_SLOT
#  define R_RELATIVE R_AARCH64_RELATIVE
#  define RELOC(n) DT_RELA##n
#  define UNSUPPORTED_RELOC(n) DT_REL##n
#  define STR_RELOC(n) "DT_RELA" #n
#  define Reloc Rela

#else
#  error Unknown ELF machine type
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
struct Ehdr : public Elf_(Ehdr) {
  /**
   * Equivalent to reinterpret_cast<const Ehdr *>(buf), but additionally
   * checking that this is indeed an Elf header and that the Elf type
   * corresponds to that of the system
   */
  static const Ehdr* validate(const void* buf);
};

/**
 * Elf String table
 */
class Strtab : public UnsizedArray<const char> {
 public:
  /**
   * Returns the string at the given index in the table
   */
  const char* GetStringAt(off_t index) const {
    return &UnsizedArray<const char>::operator[](index);
  }
};

/**
 * Helper class around Elf relocation.
 */
struct Rel
    : public Elf_(Rel){/**
                        * Returns the addend for the relocation, which is the
                        * value stored at r_offset.
                        */
                       Addr GetAddend(void* base)
                           const {return *(reinterpret_cast<const Addr*>(
                               reinterpret_cast<const char*>(base) + r_offset));
}  // namespace Elf
}
;

/**
 * Helper class around Elf relocation with addend.
 */
struct Rela
    : public Elf_(Rela){/**
                         * Returns the addend for the relocation.
                         */
                        Addr GetAddend(void* base) const {return r_addend;
}
}
;

} /* namespace Elf */

#endif /* Elfxx_h */
