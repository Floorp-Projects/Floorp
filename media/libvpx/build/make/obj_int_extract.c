/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vpx_config.h"
#include "vpx/vpx_integer.h"

typedef enum {
  OUTPUT_FMT_PLAIN,
  OUTPUT_FMT_RVDS,
  OUTPUT_FMT_GAS,
  OUTPUT_FMT_C_HEADER,
} output_fmt_t;

int log_msg(const char *fmt, ...) {
  int res;
  va_list ap;
  va_start(ap, fmt);
  res = vfprintf(stderr, fmt, ap);
  va_end(ap);
  return res;
}

#if defined(__GNUC__) && __GNUC__
#if defined(__MACH__)

#include <mach-o/loader.h>
#include <mach-o/nlist.h>

int print_macho_equ(output_fmt_t mode, uint8_t* name, int val) {
  switch (mode) {
    case OUTPUT_FMT_RVDS:
      printf("%-40s EQU %5d\n", name, val);
      return 0;
    case OUTPUT_FMT_GAS:
      printf(".set %-40s, %5d\n", name, val);
      return 0;
    case OUTPUT_FMT_C_HEADER:
      printf("#define %-40s %5d\n", name, val);
      return 0;
    default:
      log_msg("Unsupported mode: %d", mode);
      return 1;
  }
}

int parse_macho(uint8_t *base_buf, size_t sz, output_fmt_t mode) {
  int i, j;
  struct mach_header header;
  uint8_t *buf = base_buf;
  int base_data_section = 0;
  int bits = 0;

  /* We can read in mach_header for 32 and 64 bit architectures
   * because it's identical to mach_header_64 except for the last
   * element (uint32_t reserved), which we don't use. Then, when
   * we know which architecture we're looking at, increment buf
   * appropriately.
   */
  memcpy(&header, buf, sizeof(struct mach_header));

  if (header.magic == MH_MAGIC) {
    if (header.cputype == CPU_TYPE_ARM
        || header.cputype == CPU_TYPE_X86) {
      bits = 32;
      buf += sizeof(struct mach_header);
    } else {
      log_msg("Bad cputype for object file. Currently only tested for CPU_TYPE_[ARM|X86].\n");
      goto bail;
    }
  } else if (header.magic == MH_MAGIC_64) {
    if (header.cputype == CPU_TYPE_X86_64) {
      bits = 64;
      buf += sizeof(struct mach_header_64);
    } else {
      log_msg("Bad cputype for object file. Currently only tested for CPU_TYPE_X86_64.\n");
      goto bail;
    }
  } else {
    log_msg("Bad magic number for object file. 0x%x or 0x%x expected, 0x%x found.\n",
            MH_MAGIC, MH_MAGIC_64, header.magic);
    goto bail;
  }

  if (header.filetype != MH_OBJECT) {
    log_msg("Bad filetype for object file. Currently only tested for MH_OBJECT.\n");
    goto bail;
  }

  for (i = 0; i < header.ncmds; i++) {
    struct load_command lc;

    memcpy(&lc, buf, sizeof(struct load_command));

    if (lc.cmd == LC_SEGMENT) {
      uint8_t *seg_buf = buf;
      struct section s;
      struct segment_command seg_c;

      memcpy(&seg_c, seg_buf, sizeof(struct segment_command));
      seg_buf += sizeof(struct segment_command);

      /* Although each section is given it's own offset, nlist.n_value
       * references the offset of the first section. This isn't
       * apparent without debug information because the offset of the
       * data section is the same as the first section. However, with
       * debug sections mixed in, the offset of the debug section
       * increases but n_value still references the first section.
       */
      if (seg_c.nsects < 1) {
        log_msg("Not enough sections\n");
        goto bail;
      }

      memcpy(&s, seg_buf, sizeof(struct section));
      base_data_section = s.offset;
    } else if (lc.cmd == LC_SEGMENT_64) {
      uint8_t *seg_buf = buf;
      struct section_64 s;
      struct segment_command_64 seg_c;

      memcpy(&seg_c, seg_buf, sizeof(struct segment_command_64));
      seg_buf += sizeof(struct segment_command_64);

      /* Explanation in LG_SEGMENT */
      if (seg_c.nsects < 1) {
        log_msg("Not enough sections\n");
        goto bail;
      }

      memcpy(&s, seg_buf, sizeof(struct section_64));
      base_data_section = s.offset;
    } else if (lc.cmd == LC_SYMTAB) {
      if (base_data_section != 0) {
        struct symtab_command sc;
        uint8_t *sym_buf = base_buf;
        uint8_t *str_buf = base_buf;

        memcpy(&sc, buf, sizeof(struct symtab_command));

        if (sc.cmdsize != sizeof(struct symtab_command)) {
          log_msg("Can't find symbol table!\n");
          goto bail;
        }

        sym_buf += sc.symoff;
        str_buf += sc.stroff;

        for (j = 0; j < sc.nsyms; j++) {
          /* Location of string is cacluated each time from the
           * start of the string buffer.  On darwin the symbols
           * are prefixed by "_", so we bump the pointer by 1.
           * The target value is defined as an int in *_asm_*_offsets.c,
           * which is 4 bytes on all targets we currently use.
           */
          if (bits == 32) {
            struct nlist nl;
            int val;

            memcpy(&nl, sym_buf, sizeof(struct nlist));
            sym_buf += sizeof(struct nlist);

            memcpy(&val, base_buf + base_data_section + nl.n_value,
                   sizeof(val));
            print_macho_equ(mode, str_buf + nl.n_un.n_strx + 1, val);
          } else { /* if (bits == 64) */
            struct nlist_64 nl;
            int val;

            memcpy(&nl, sym_buf, sizeof(struct nlist_64));
            sym_buf += sizeof(struct nlist_64);

            memcpy(&val, base_buf + base_data_section + nl.n_value,
                   sizeof(val));
            print_macho_equ(mode, str_buf + nl.n_un.n_strx + 1, val);
          }
        }
      }
    }

    buf += lc.cmdsize;
  }

  return 0;
bail:
  return 1;

}

#elif defined(__ELF__)
#include "elf.h"

#define COPY_STRUCT(dst, buf, ofst, sz) do {\
    if(ofst + sizeof((*(dst))) > sz) goto bail;\
    memcpy(dst, buf+ofst, sizeof((*(dst))));\
  } while(0)

#define ENDIAN_ASSIGN(val, memb) do {\
    if(!elf->le_data) {log_msg("Big Endian data not supported yet!\n");goto bail;}\
    (val) = (memb);\
  } while(0)

#define ENDIAN_ASSIGN_IN_PLACE(memb) do {\
    ENDIAN_ASSIGN(memb, memb);\
  } while(0)

typedef struct {
  uint8_t      *buf; /* Buffer containing ELF data */
  size_t        sz;  /* Buffer size */
  int           le_data; /* Data is little-endian */
  unsigned char e_ident[EI_NIDENT]; /* Magic number and other info */
  int           bits; /* 32 or 64 */
  Elf32_Ehdr    hdr32;
  Elf64_Ehdr    hdr64;
} elf_obj_t;

int parse_elf_header(elf_obj_t *elf) {
  int res;
  /* Verify ELF Magic numbers */
  COPY_STRUCT(&elf->e_ident, elf->buf, 0, elf->sz);
  res = elf->e_ident[EI_MAG0] == ELFMAG0;
  res &= elf->e_ident[EI_MAG1] == ELFMAG1;
  res &= elf->e_ident[EI_MAG2] == ELFMAG2;
  res &= elf->e_ident[EI_MAG3] == ELFMAG3;
  res &= elf->e_ident[EI_CLASS] == ELFCLASS32
         || elf->e_ident[EI_CLASS] == ELFCLASS64;
  res &= elf->e_ident[EI_DATA] == ELFDATA2LSB;

  if (!res) goto bail;

  elf->le_data = elf->e_ident[EI_DATA] == ELFDATA2LSB;

  /* Read in relevant values */
  if (elf->e_ident[EI_CLASS] == ELFCLASS32) {
    elf->bits = 32;
    COPY_STRUCT(&elf->hdr32, elf->buf, 0, elf->sz);

    ENDIAN_ASSIGN_IN_PLACE(elf->hdr32.e_type);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr32.e_machine);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr32.e_version);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr32.e_entry);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr32.e_phoff);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr32.e_shoff);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr32.e_flags);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr32.e_ehsize);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr32.e_phentsize);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr32.e_phnum);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr32.e_shentsize);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr32.e_shnum);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr32.e_shstrndx);
  } else { /* if (elf->e_ident[EI_CLASS] == ELFCLASS64) */
    elf->bits = 64;
    COPY_STRUCT(&elf->hdr64, elf->buf, 0, elf->sz);

    ENDIAN_ASSIGN_IN_PLACE(elf->hdr64.e_type);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr64.e_machine);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr64.e_version);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr64.e_entry);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr64.e_phoff);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr64.e_shoff);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr64.e_flags);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr64.e_ehsize);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr64.e_phentsize);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr64.e_phnum);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr64.e_shentsize);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr64.e_shnum);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr64.e_shstrndx);
  }

  return 0;
bail:
  log_msg("Failed to parse ELF file header");
  return 1;
}

int parse_elf_section(elf_obj_t *elf, int idx, Elf32_Shdr *hdr32, Elf64_Shdr *hdr64) {
  if (hdr32) {
    if (idx >= elf->hdr32.e_shnum)
      goto bail;

    COPY_STRUCT(hdr32, elf->buf, elf->hdr32.e_shoff + idx * elf->hdr32.e_shentsize,
                elf->sz);
    ENDIAN_ASSIGN_IN_PLACE(hdr32->sh_name);
    ENDIAN_ASSIGN_IN_PLACE(hdr32->sh_type);
    ENDIAN_ASSIGN_IN_PLACE(hdr32->sh_flags);
    ENDIAN_ASSIGN_IN_PLACE(hdr32->sh_addr);
    ENDIAN_ASSIGN_IN_PLACE(hdr32->sh_offset);
    ENDIAN_ASSIGN_IN_PLACE(hdr32->sh_size);
    ENDIAN_ASSIGN_IN_PLACE(hdr32->sh_link);
    ENDIAN_ASSIGN_IN_PLACE(hdr32->sh_info);
    ENDIAN_ASSIGN_IN_PLACE(hdr32->sh_addralign);
    ENDIAN_ASSIGN_IN_PLACE(hdr32->sh_entsize);
  } else { /* if (hdr64) */
    if (idx >= elf->hdr64.e_shnum)
      goto bail;

    COPY_STRUCT(hdr64, elf->buf, elf->hdr64.e_shoff + idx * elf->hdr64.e_shentsize,
                elf->sz);
    ENDIAN_ASSIGN_IN_PLACE(hdr64->sh_name);
    ENDIAN_ASSIGN_IN_PLACE(hdr64->sh_type);
    ENDIAN_ASSIGN_IN_PLACE(hdr64->sh_flags);
    ENDIAN_ASSIGN_IN_PLACE(hdr64->sh_addr);
    ENDIAN_ASSIGN_IN_PLACE(hdr64->sh_offset);
    ENDIAN_ASSIGN_IN_PLACE(hdr64->sh_size);
    ENDIAN_ASSIGN_IN_PLACE(hdr64->sh_link);
    ENDIAN_ASSIGN_IN_PLACE(hdr64->sh_info);
    ENDIAN_ASSIGN_IN_PLACE(hdr64->sh_addralign);
    ENDIAN_ASSIGN_IN_PLACE(hdr64->sh_entsize);
  }

  return 0;
bail:
  return 1;
}

const char *parse_elf_string_table(elf_obj_t *elf, int s_idx, int idx) {
  if (elf->bits == 32) {
    Elf32_Shdr shdr;

    if (parse_elf_section(elf, s_idx, &shdr, NULL)) {
      log_msg("Failed to parse ELF string table: section %d, index %d\n",
              s_idx, idx);
      return "";
    }

    return (char *)(elf->buf + shdr.sh_offset + idx);
  } else { /* if (elf->bits == 64) */
    Elf64_Shdr shdr;

    if (parse_elf_section(elf, s_idx, NULL, &shdr)) {
      log_msg("Failed to parse ELF string table: section %d, index %d\n",
              s_idx, idx);
      return "";
    }

    return (char *)(elf->buf + shdr.sh_offset + idx);
  }
}

int parse_elf_symbol(elf_obj_t *elf, unsigned int ofst, Elf32_Sym *sym32, Elf64_Sym *sym64) {
  if (sym32) {
    COPY_STRUCT(sym32, elf->buf, ofst, elf->sz);
    ENDIAN_ASSIGN_IN_PLACE(sym32->st_name);
    ENDIAN_ASSIGN_IN_PLACE(sym32->st_value);
    ENDIAN_ASSIGN_IN_PLACE(sym32->st_size);
    ENDIAN_ASSIGN_IN_PLACE(sym32->st_info);
    ENDIAN_ASSIGN_IN_PLACE(sym32->st_other);
    ENDIAN_ASSIGN_IN_PLACE(sym32->st_shndx);
  } else { /* if (sym64) */
    COPY_STRUCT(sym64, elf->buf, ofst, elf->sz);
    ENDIAN_ASSIGN_IN_PLACE(sym64->st_name);
    ENDIAN_ASSIGN_IN_PLACE(sym64->st_value);
    ENDIAN_ASSIGN_IN_PLACE(sym64->st_size);
    ENDIAN_ASSIGN_IN_PLACE(sym64->st_info);
    ENDIAN_ASSIGN_IN_PLACE(sym64->st_other);
    ENDIAN_ASSIGN_IN_PLACE(sym64->st_shndx);
  }
  return 0;
bail:
  return 1;
}

int parse_elf(uint8_t *buf, size_t sz, output_fmt_t mode) {
  elf_obj_t    elf;
  unsigned int ofst;
  int          i;
  Elf32_Off    strtab_off32;
  Elf64_Off    strtab_off64; /* save String Table offset for later use */

  memset(&elf, 0, sizeof(elf));
  elf.buf = buf;
  elf.sz = sz;

  /* Parse Header */
  if (parse_elf_header(&elf))
    goto bail;

  if (elf.bits == 32) {
    Elf32_Shdr shdr;
    for (i = 0; i < elf.hdr32.e_shnum; i++) {
      parse_elf_section(&elf, i, &shdr, NULL);

      if (shdr.sh_type == SHT_STRTAB) {
        char strtsb_name[128];

        strcpy(strtsb_name, (char *)(elf.buf + shdr.sh_offset + shdr.sh_name));

        if (!(strcmp(strtsb_name, ".shstrtab"))) {
          /* log_msg("found section: %s\n", strtsb_name); */
          strtab_off32 = shdr.sh_offset;
          break;
        }
      }
    }
  } else { /* if (elf.bits == 64) */
    Elf64_Shdr shdr;
    for (i = 0; i < elf.hdr64.e_shnum; i++) {
      parse_elf_section(&elf, i, NULL, &shdr);

      if (shdr.sh_type == SHT_STRTAB) {
        char strtsb_name[128];

        strcpy(strtsb_name, (char *)(elf.buf + shdr.sh_offset + shdr.sh_name));

        if (!(strcmp(strtsb_name, ".shstrtab"))) {
          /* log_msg("found section: %s\n", strtsb_name); */
          strtab_off64 = shdr.sh_offset;
          break;
        }
      }
    }
  }

  /* Parse all Symbol Tables */
  if (elf.bits == 32) {
    Elf32_Shdr shdr;
    for (i = 0; i < elf.hdr32.e_shnum; i++) {
      parse_elf_section(&elf, i, &shdr, NULL);

      if (shdr.sh_type == SHT_SYMTAB) {
        for (ofst = shdr.sh_offset;
             ofst < shdr.sh_offset + shdr.sh_size;
             ofst += shdr.sh_entsize) {
          Elf32_Sym sym;

          parse_elf_symbol(&elf, ofst, &sym, NULL);

          /* For all OBJECTS (data objects), extract the value from the
           * proper data segment.
           */
          /* if (ELF32_ST_TYPE(sym.st_info) == STT_OBJECT && sym.st_name)
              log_msg("found data object %s\n",
                      parse_elf_string_table(&elf,
                                             shdr.sh_link,
                                             sym.st_name));
           */

          if (ELF32_ST_TYPE(sym.st_info) == STT_OBJECT
              && sym.st_size == 4) {
            Elf32_Shdr dhdr;
            int val = 0;
            char section_name[128];

            parse_elf_section(&elf, sym.st_shndx, &dhdr, NULL);

            /* For explanition - refer to _MSC_VER version of code */
            strcpy(section_name, (char *)(elf.buf + strtab_off32 + dhdr.sh_name));
            /* log_msg("Section_name: %s, Section_type: %d\n", section_name, dhdr.sh_type); */

            if (strcmp(section_name, ".bss")) {
              if (sizeof(val) != sym.st_size) {
                /* The target value is declared as an int in
                 * *_asm_*_offsets.c, which is 4 bytes on all
                 * targets we currently use. Complain loudly if
                 * this is not true.
                 */
                log_msg("Symbol size is wrong\n");
                goto bail;
              }

              memcpy(&val,
                     elf.buf + dhdr.sh_offset + sym.st_value,
                     sym.st_size);
            }

            if (!elf.le_data) {
              log_msg("Big Endian data not supported yet!\n");
              goto bail;
            }

            switch (mode) {
              case OUTPUT_FMT_RVDS:
                printf("%-40s EQU %5d\n",
                       parse_elf_string_table(&elf,
                                              shdr.sh_link,
                                              sym.st_name),
                       val);
                break;
              case OUTPUT_FMT_GAS:
                printf(".equ %-40s, %5d\n",
                       parse_elf_string_table(&elf,
                                              shdr.sh_link,
                                              sym.st_name),
                       val);
                break;
              case OUTPUT_FMT_C_HEADER:
                printf("#define %-40s %5d\n",
                       parse_elf_string_table(&elf,
                                              shdr.sh_link,
                                              sym.st_name),
                       val);
                break;
              default:
                printf("%s = %d\n",
                       parse_elf_string_table(&elf,
                                              shdr.sh_link,
                                              sym.st_name),
                       val);
            }
          }
        }
      }
    }
  } else { /* if (elf.bits == 64) */
    Elf64_Shdr shdr;
    for (i = 0; i < elf.hdr64.e_shnum; i++) {
      parse_elf_section(&elf, i, NULL, &shdr);

      if (shdr.sh_type == SHT_SYMTAB) {
        for (ofst = shdr.sh_offset;
             ofst < shdr.sh_offset + shdr.sh_size;
             ofst += shdr.sh_entsize) {
          Elf64_Sym sym;

          parse_elf_symbol(&elf, ofst, NULL, &sym);

          /* For all OBJECTS (data objects), extract the value from the
           * proper data segment.
           */
          /* if (ELF64_ST_TYPE(sym.st_info) == STT_OBJECT && sym.st_name)
              log_msg("found data object %s\n",
                      parse_elf_string_table(&elf,
                                             shdr.sh_link,
                                             sym.st_name));
           */

          if (ELF64_ST_TYPE(sym.st_info) == STT_OBJECT
              && sym.st_size == 4) {
            Elf64_Shdr dhdr;
            int val = 0;
            char section_name[128];

            parse_elf_section(&elf, sym.st_shndx, NULL, &dhdr);

            /* For explanition - refer to _MSC_VER version of code */
            strcpy(section_name, (char *)(elf.buf + strtab_off64 + dhdr.sh_name));
            /* log_msg("Section_name: %s, Section_type: %d\n", section_name, dhdr.sh_type); */

            if ((strcmp(section_name, ".bss"))) {
              if (sizeof(val) != sym.st_size) {
                /* The target value is declared as an int in
                 * *_asm_*_offsets.c, which is 4 bytes on all
                 * targets we currently use. Complain loudly if
                 * this is not true.
                 */
                log_msg("Symbol size is wrong\n");
                goto bail;
              }

              memcpy(&val,
                     elf.buf + dhdr.sh_offset + sym.st_value,
                     sym.st_size);
            }

            if (!elf.le_data) {
              log_msg("Big Endian data not supported yet!\n");
              goto bail;
            }

            switch (mode) {
              case OUTPUT_FMT_RVDS:
                printf("%-40s EQU %5d\n",
                       parse_elf_string_table(&elf,
                                              shdr.sh_link,
                                              sym.st_name),
                       val);
                break;
              case OUTPUT_FMT_GAS:
                printf(".equ %-40s, %5d\n",
                       parse_elf_string_table(&elf,
                                              shdr.sh_link,
                                              sym.st_name),
                       val);
                break;
              default:
                printf("%s = %d\n",
                       parse_elf_string_table(&elf,
                                              shdr.sh_link,
                                              sym.st_name),
                       val);
            }
          }
        }
      }
    }
  }

  if (mode == OUTPUT_FMT_RVDS)
    printf("    END\n");

  return 0;
bail:
  log_msg("Parse error: File does not appear to be valid ELF32 or ELF64\n");
  return 1;
}

#endif
#endif /* defined(__GNUC__) && __GNUC__ */


#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__CYGWIN__)
/*  See "Microsoft Portable Executable and Common Object File Format Specification"
    for reference.
*/
#define get_le32(x) ((*(x)) | (*(x+1)) << 8 |(*(x+2)) << 16 | (*(x+3)) << 24 )
#define get_le16(x) ((*(x)) | (*(x+1)) << 8)

int parse_coff(uint8_t *buf, size_t sz) {
  unsigned int nsections, symtab_ptr, symtab_sz, strtab_ptr;
  unsigned int sectionrawdata_ptr;
  unsigned int i;
  uint8_t *ptr;
  uint32_t symoffset;

  char **sectionlist;  // this array holds all section names in their correct order.
  // it is used to check if the symbol is in .bss or .rdata section.

  nsections = get_le16(buf + 2);
  symtab_ptr = get_le32(buf + 8);
  symtab_sz = get_le32(buf + 12);
  strtab_ptr = symtab_ptr + symtab_sz * 18;

  if (nsections > 96) {
    log_msg("Too many sections\n");
    return 1;
  }

  sectionlist = malloc(nsections * sizeof(sectionlist));

  if (sectionlist == NULL) {
    log_msg("Allocating first level of section list failed\n");
    return 1;
  }

  // log_msg("COFF: Found %u symbols in %u sections.\n", symtab_sz, nsections);

  /*
  The size of optional header is always zero for an obj file. So, the section header
  follows the file header immediately.
  */

  ptr = buf + 20;     // section header

  for (i = 0; i < nsections; i++) {
    char sectionname[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    strncpy(sectionname, ptr, 8);
    // log_msg("COFF: Parsing section %s\n",sectionname);

    sectionlist[i] = malloc(strlen(sectionname) + 1);

    if (sectionlist[i] == NULL) {
      log_msg("Allocating storage for %s failed\n", sectionname);
      goto bail;
    }
    strcpy(sectionlist[i], sectionname);

    // check if it's .rdata and is not a COMDAT section.
    if (!strcmp(sectionname, ".rdata") &&
        (get_le32(ptr + 36) & 0x1000) == 0) {
      sectionrawdata_ptr = get_le32(ptr + 20);
    }

    ptr += 40;
  }

  // log_msg("COFF: Symbol table at offset %u\n", symtab_ptr);
  // log_msg("COFF: raw data pointer ofset for section .rdata is %u\n", sectionrawdata_ptr);

  /*  The compiler puts the data with non-zero offset in .rdata section, but puts the data with
      zero offset in .bss section. So, if the data in in .bss section, set offset=0.
      Note from Wiki: In an object module compiled from C, the bss section contains
      the local variables (but not functions) that were declared with the static keyword,
      except for those with non-zero initial values. (In C, static variables are initialized
      to zero by default.) It also contains the non-local (both extern and static) variables
      that are also initialized to zero (either explicitly or by default).
      */
  // move to symbol table
  /* COFF symbol table:
      offset      field
      0           Name(*)
      8           Value
      12          SectionNumber
      14          Type
      16          StorageClass
      17          NumberOfAuxSymbols
      */
  ptr = buf + symtab_ptr;

  for (i = 0; i < symtab_sz; i++) {
    int16_t section = get_le16(ptr + 12); // section number

    if (section > 0 && ptr[16] == 2) {
      // if(section > 0 && ptr[16] == 3 && get_le32(ptr+8)) {

      if (get_le32(ptr)) {
        char name[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
        strncpy(name, ptr, 8);
        // log_msg("COFF: Parsing symbol %s\n",name);
        /* The 64bit Windows compiler doesn't prefix with an _.
         * Check what's there, and bump if necessary
         */
        if (name[0] == '_')
          printf("%-40s EQU ", name + 1);
        else
          printf("%-40s EQU ", name);
      } else {
        // log_msg("COFF: Parsing symbol %s\n",
        //        buf + strtab_ptr + get_le32(ptr+4));
        if ((buf + strtab_ptr + get_le32(ptr + 4))[0] == '_')
          printf("%-40s EQU ",
                 buf + strtab_ptr + get_le32(ptr + 4) + 1);
        else
          printf("%-40s EQU ", buf + strtab_ptr + get_le32(ptr + 4));
      }

      if (!(strcmp(sectionlist[section - 1], ".bss"))) {
        symoffset = 0;
      } else {
        symoffset = get_le32(buf + sectionrawdata_ptr + get_le32(ptr + 8));
      }

      // log_msg("      Section: %d\n",section);
      // log_msg("      Class:   %d\n",ptr[16]);
      // log_msg("      Address: %u\n",get_le32(ptr+8));
      // log_msg("      Offset: %u\n", symoffset);

      printf("%5d\n", symoffset);
    }

    ptr += 18;
  }

  printf("    END\n");

  for (i = 0; i < nsections; i++) {
    free(sectionlist[i]);
  }

  free(sectionlist);

  return 0;
bail:

  for (i = 0; i < nsections; i++) {
    free(sectionlist[i]);
  }

  free(sectionlist);

  return 1;
}
#endif /* defined(_MSC_VER) || defined(__MINGW32__) || defined(__CYGWIN__) */

int main(int argc, char **argv) {
  output_fmt_t mode = OUTPUT_FMT_PLAIN;
  const char *f;
  uint8_t *file_buf;
  int res;
  FILE *fp;
  long int file_size;

  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Usage: %s [output format] <obj file>\n\n", argv[0]);
    fprintf(stderr, "  <obj file>\tobject file to parse\n");
    fprintf(stderr, "Output Formats:\n");
    fprintf(stderr, "  gas  - compatible with GNU assembler\n");
    fprintf(stderr, "  rvds - compatible with armasm\n");
    fprintf(stderr, "  cheader - c/c++ header file\n");
    goto bail;
  }

  f = argv[2];

  if (!strcmp(argv[1], "rvds"))
    mode = OUTPUT_FMT_RVDS;
  else if (!strcmp(argv[1], "gas"))
    mode = OUTPUT_FMT_GAS;
  else if (!strcmp(argv[1], "cheader"))
    mode = OUTPUT_FMT_C_HEADER;
  else
    f = argv[1];

  fp = fopen(f, "rb");

  if (!fp) {
    perror("Unable to open file");
    goto bail;
  }

  if (fseek(fp, 0, SEEK_END)) {
    perror("stat");
    goto bail;
  }

  file_size = ftell(fp);
  file_buf = malloc(file_size);

  if (!file_buf) {
    perror("malloc");
    goto bail;
  }

  rewind(fp);

  if (fread(file_buf, sizeof(char), file_size, fp) != file_size) {
    perror("read");
    goto bail;
  }

  if (fclose(fp)) {
    perror("close");
    goto bail;
  }

#if defined(__GNUC__) && __GNUC__
#if defined(__MACH__)
  res = parse_macho(file_buf, file_size, mode);
#elif defined(__ELF__)
  res = parse_elf(file_buf, file_size, mode);
#endif
#endif
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__CYGWIN__)
  res = parse_coff(file_buf, file_size);
#endif

  free(file_buf);

  if (!res)
    return EXIT_SUCCESS;

bail:
  return EXIT_FAILURE;
}
