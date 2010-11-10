/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <stdio.h>
#include <stdlib.h>

#include "vpx_config.h"

#if defined(_MSC_VER)
#include <io.h>
#include <share.h>
#include "vpx/vpx_integer.h"
#else
#include <stdint.h>
#include <unistd.h>
#endif

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>

typedef enum
{
    OUTPUT_FMT_PLAIN,
    OUTPUT_FMT_RVDS,
    OUTPUT_FMT_GAS,
} output_fmt_t;

int log_msg(const char *fmt, ...)
{
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

int parse_macho(uint8_t *base_buf, size_t sz)
{
    int i, j;
    struct mach_header header;
    uint8_t *buf = base_buf;
    int base_data_section = 0;

    memcpy(&header, buf, sizeof(struct mach_header));
    buf += sizeof(struct mach_header);

    if (header.magic != MH_MAGIC)
    {
        log_msg("Bad magic number for object file. 0x%x expected, 0x%x found.\n",
                header.magic, MH_MAGIC);
        goto bail;
    }

    if (header.cputype != CPU_TYPE_ARM)
    {
        log_msg("Bad cputype for object file. Currently only tested for CPU_TYPE_ARM.\n");
        goto bail;
    }

    if (header.filetype != MH_OBJECT)
    {
        log_msg("Bad filetype for object file. Currently only tested for MH_OBJECT.\n");
        goto bail;
    }

    for (i = 0; i < header.ncmds; i++)
    {
        struct load_command lc;
        struct symtab_command sc;
        struct segment_command seg_c;

        memcpy(&lc, buf, sizeof(struct load_command));

        if (lc.cmd == LC_SEGMENT)
        {
            uint8_t *seg_buf = buf;
            struct section s;

            memcpy(&seg_c, buf, sizeof(struct segment_command));

            seg_buf += sizeof(struct segment_command);

            for (j = 0; j < seg_c.nsects; j++)
            {
                memcpy(&s, seg_buf + (j * sizeof(struct section)), sizeof(struct section));

                // Need to get this offset which is the start of the symbol table
                // before matching the strings up with symbols.
                base_data_section = s.offset;
            }
        }
        else if (lc.cmd == LC_SYMTAB)
        {
            uint8_t *sym_buf = base_buf;
            uint8_t *str_buf = base_buf;

            if (base_data_section != 0)
            {
                memcpy(&sc, buf, sizeof(struct symtab_command));

                if (sc.cmdsize != sizeof(struct symtab_command))
                    log_msg("Can't find symbol table!\n");

                sym_buf += sc.symoff;
                str_buf += sc.stroff;

                for (j = 0; j < sc.nsyms; j++)
                {
                    struct nlist nl;
                    int val;

                    memcpy(&nl, sym_buf + (j * sizeof(struct nlist)), sizeof(struct nlist));

                    val = *((int *)(base_buf + base_data_section + nl.n_value));

                    // Location of string is cacluated each time from the
                    // start of the string buffer.  On darwin the symbols
                    // are prefixed by "_".  On other platforms it is not
                    // so it needs to be removed.  That is the reason for
                    // the +1.
                    printf("%-40s EQU %5d\n", str_buf + nl.n_un.n_strx + 1, val);
                }
            }
        }

        buf += lc.cmdsize;
    }

    return 0;
bail:
    return 1;

}

int main(int argc, char **argv)
{
    int fd;
    char *f;
    struct stat stat_buf;
    uint8_t *file_buf;
    int res;

    if (argc < 2 || argc > 3)
    {
        fprintf(stderr, "Usage: %s [output format] <obj file>\n\n", argv[0]);
        fprintf(stderr, "  <obj file>\tMachO format object file to parse\n");
        fprintf(stderr, "Output Formats:\n");
        fprintf(stderr, "  gas  - compatible with GNU assembler\n");
        fprintf(stderr, "  rvds - compatible with armasm\n");
        goto bail;
    }

    f = argv[2];

    if (!((!strcmp(argv[1], "rvds")) || (!strcmp(argv[1], "gas"))))
        f = argv[1];

    fd = open(f, O_RDONLY);

    if (fd < 0)
    {
        perror("Unable to open file");
        goto bail;
    }

    if (fstat(fd, &stat_buf))
    {
        perror("stat");
        goto bail;
    }

    file_buf = malloc(stat_buf.st_size);

    if (!file_buf)
    {
        perror("malloc");
        goto bail;
    }

    if (read(fd, file_buf, stat_buf.st_size) != stat_buf.st_size)
    {
        perror("read");
        goto bail;
    }

    if (close(fd))
    {
        perror("close");
        goto bail;
    }

    res = parse_macho(file_buf, stat_buf.st_size);
    free(file_buf);

    if (!res)
        return EXIT_SUCCESS;

bail:
    return EXIT_FAILURE;
}

#else
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

typedef struct
{
    uint8_t     *buf; /* Buffer containing ELF data */
    size_t       sz;  /* Buffer size */
    int          le_data;   /* Data is little-endian */
    Elf32_Ehdr   hdr;
} elf_obj_t;

int parse_elf32_header(elf_obj_t *elf)
{
    int res;
    /* Verify ELF32 header */
    COPY_STRUCT(&elf->hdr, elf->buf, 0, elf->sz);
    res = elf->hdr.e_ident[EI_MAG0] == ELFMAG0;
    res &= elf->hdr.e_ident[EI_MAG1] == ELFMAG1;
    res &= elf->hdr.e_ident[EI_MAG2] == ELFMAG2;
    res &= elf->hdr.e_ident[EI_MAG3] == ELFMAG3;
    res &= elf->hdr.e_ident[EI_CLASS] == ELFCLASS32;
    res &= elf->hdr.e_ident[EI_DATA] == ELFDATA2LSB
           || elf->hdr.e_ident[EI_DATA] == ELFDATA2MSB;

    if (!res) goto bail;

    elf->le_data = elf->hdr.e_ident[EI_DATA] == ELFDATA2LSB;

    ENDIAN_ASSIGN_IN_PLACE(elf->hdr.e_type);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr.e_machine);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr.e_version);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr.e_entry);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr.e_phoff);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr.e_shoff);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr.e_flags);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr.e_ehsize);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr.e_phentsize);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr.e_phnum);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr.e_shentsize);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr.e_shnum);
    ENDIAN_ASSIGN_IN_PLACE(elf->hdr.e_shstrndx);
    return 0;
bail:
    return 1;
}

int parse_elf32_section(elf_obj_t *elf, int idx, Elf32_Shdr *hdr)
{
    if (idx >= elf->hdr.e_shnum)
        goto bail;

    COPY_STRUCT(hdr, elf->buf, elf->hdr.e_shoff + idx * elf->hdr.e_shentsize,
                elf->sz);
    ENDIAN_ASSIGN_IN_PLACE(hdr->sh_name);
    ENDIAN_ASSIGN_IN_PLACE(hdr->sh_type);
    ENDIAN_ASSIGN_IN_PLACE(hdr->sh_flags);
    ENDIAN_ASSIGN_IN_PLACE(hdr->sh_addr);
    ENDIAN_ASSIGN_IN_PLACE(hdr->sh_offset);
    ENDIAN_ASSIGN_IN_PLACE(hdr->sh_size);
    ENDIAN_ASSIGN_IN_PLACE(hdr->sh_link);
    ENDIAN_ASSIGN_IN_PLACE(hdr->sh_info);
    ENDIAN_ASSIGN_IN_PLACE(hdr->sh_addralign);
    ENDIAN_ASSIGN_IN_PLACE(hdr->sh_entsize);
    return 0;
bail:
    return 1;
}

char *parse_elf32_string_table(elf_obj_t *elf, int s_idx, int idx)
{
    Elf32_Shdr shdr;

    if (parse_elf32_section(elf, s_idx, &shdr))
    {
        log_msg("Failed to parse ELF string table: section %d, index %d\n",
                s_idx, idx);
        return "";
    }

    return (char *)(elf->buf + shdr.sh_offset + idx);
}

int parse_elf32_symbol(elf_obj_t *elf, unsigned int ofst, Elf32_Sym *sym)
{
    COPY_STRUCT(sym, elf->buf, ofst, elf->sz);
    ENDIAN_ASSIGN_IN_PLACE(sym->st_name);
    ENDIAN_ASSIGN_IN_PLACE(sym->st_value);
    ENDIAN_ASSIGN_IN_PLACE(sym->st_size);
    ENDIAN_ASSIGN_IN_PLACE(sym->st_info);
    ENDIAN_ASSIGN_IN_PLACE(sym->st_other);
    ENDIAN_ASSIGN_IN_PLACE(sym->st_shndx);
    return 0;
bail:
    return 1;
}

int parse_elf32(uint8_t *buf, size_t sz, output_fmt_t mode)
{
    elf_obj_t  elf;
    Elf32_Shdr shdr;
    unsigned int ofst;
    int         i;
    Elf32_Off strtab_off;   /* save String Table offset for later use */

    memset(&elf, 0, sizeof(elf));
    elf.buf = buf;
    elf.sz = sz;

    /* Parse Header */
    if (parse_elf32_header(&elf))
    {
        log_msg("Parse error: File does not appear to be valid ELF32\n");
        return 1;
    }

    for (i = 0; i < elf.hdr.e_shnum; i++)
    {
        parse_elf32_section(&elf, i, &shdr);

        if (shdr.sh_type == SHT_STRTAB)
        {
            char strtsb_name[128];

            strcpy(strtsb_name, (char *)(elf.buf + shdr.sh_offset + shdr.sh_name));

            if (!(strcmp(strtsb_name, ".shstrtab")))
            {
                log_msg("found section: %s\n", strtsb_name);
                strtab_off = shdr.sh_offset;
                break;
            }
        }
    }

    /* Parse all Symbol Tables */
    for (i = 0; i < elf.hdr.e_shnum; i++)
    {

        parse_elf32_section(&elf, i, &shdr);

        if (shdr.sh_type == SHT_SYMTAB)
        {
            for (ofst = shdr.sh_offset;
                 ofst < shdr.sh_offset + shdr.sh_size;
                 ofst += shdr.sh_entsize)
            {
                Elf32_Sym sym;

                parse_elf32_symbol(&elf, ofst, &sym);

                /* For all OBJECTS (data objects), extract the value from the
                 * proper data segment.
                 */
                if (ELF32_ST_TYPE(sym.st_info) == STT_OBJECT && sym.st_name)
                    log_msg("found data object %s\n",
                            parse_elf32_string_table(&elf,
                                                     shdr.sh_link,
                                                     sym.st_name));

                if (ELF32_ST_TYPE(sym.st_info) == STT_OBJECT
                    && sym.st_size == 4)
                {
                    Elf32_Shdr dhdr;
                    int32_t      val;
                    char section_name[128];

                    parse_elf32_section(&elf, sym.st_shndx, &dhdr);

                    /* For explanition - refer to _MSC_VER version of code */
                    strcpy(section_name, (char *)(elf.buf + strtab_off + dhdr.sh_name));
                    log_msg("Section_name: %s, Section_type: %d\n", section_name, dhdr.sh_type);

                    if (!(strcmp(section_name, ".bss")))
                    {
                        val = 0;
                    }
                    else
                    {
                        memcpy(&val,
                               elf.buf + dhdr.sh_offset + sym.st_value,
                               sizeof(val));
                    }

                    if (!elf.le_data)
                    {
                        log_msg("Big Endian data not supported yet!\n");
                        goto bail;
                    }\

                    switch (mode)
                    {
                    case OUTPUT_FMT_RVDS:
                        printf("%-40s EQU %5d\n",
                               parse_elf32_string_table(&elf,
                                                        shdr.sh_link,
                                                        sym.st_name),
                               val);
                        break;
                    case OUTPUT_FMT_GAS:
                        printf(".equ %-40s, %5d\n",
                               parse_elf32_string_table(&elf,
                                                        shdr.sh_link,
                                                        sym.st_name),
                               val);
                        break;
                    default:
                        printf("%s = %d\n",
                               parse_elf32_string_table(&elf,
                                                        shdr.sh_link,
                                                        sym.st_name),
                               val);
                    }
                }
            }
        }
    }

    if (mode == OUTPUT_FMT_RVDS)
        printf("    END\n");

    return 0;
bail:
    log_msg("Parse error: File does not appear to be valid ELF32\n");
    return 1;
}

int main(int argc, char **argv)
{
    int fd;
    output_fmt_t mode;
    char *f;
    struct stat stat_buf;
    uint8_t *file_buf;
    int res;

    if (argc < 2 || argc > 3)
    {
        fprintf(stderr, "Usage: %s [output format] <obj file>\n\n", argv[0]);
        fprintf(stderr, "  <obj file>\tELF format object file to parse\n");
        fprintf(stderr, "Output Formats:\n");
        fprintf(stderr, "  gas  - compatible with GNU assembler\n");
        fprintf(stderr, "  rvds - compatible with armasm\n");
        goto bail;
    }

    f = argv[2];

    if (!strcmp(argv[1], "rvds"))
        mode = OUTPUT_FMT_RVDS;
    else if (!strcmp(argv[1], "gas"))
        mode = OUTPUT_FMT_GAS;
    else
        f = argv[1];


    fd = open(f, O_RDONLY);

    if (fd < 0)
    {
        perror("Unable to open file");
        goto bail;
    }

    if (fstat(fd, &stat_buf))
    {
        perror("stat");
        goto bail;
    }

    file_buf = malloc(stat_buf.st_size);

    if (!file_buf)
    {
        perror("malloc");
        goto bail;
    }

    if (read(fd, file_buf, stat_buf.st_size) != stat_buf.st_size)
    {
        perror("read");
        goto bail;
    }

    if (close(fd))
    {
        perror("close");
        goto bail;
    }

    res = parse_elf32(file_buf, stat_buf.st_size, mode);
    //res = parse_coff(file_buf, stat_buf.st_size);
    free(file_buf);

    if (!res)
        return EXIT_SUCCESS;

bail:
    return EXIT_FAILURE;
}
#endif
#endif


#if defined(_MSC_VER)
/*  See "Microsoft Portable Executable and Common Object File Format Specification"
    for reference.
*/
#define get_le32(x) ((*(x)) | (*(x+1)) << 8 |(*(x+2)) << 16 | (*(x+3)) << 24 )
#define get_le16(x) ((*(x)) | (*(x+1)) << 8)

int parse_coff(unsigned __int8 *buf, size_t sz)
{
    unsigned int nsections, symtab_ptr, symtab_sz, strtab_ptr;
    unsigned int sectionrawdata_ptr;
    unsigned int i;
    unsigned __int8 *ptr;
    unsigned __int32 symoffset;
    FILE *fp;

    char **sectionlist;  //this array holds all section names in their correct order.
    //it is used to check if the symbol is in .bss or .data section.

    nsections = get_le16(buf + 2);
    symtab_ptr = get_le32(buf + 8);
    symtab_sz = get_le32(buf + 12);
    strtab_ptr = symtab_ptr + symtab_sz * 18;

    if (nsections > 96)
        goto bail;

    sectionlist = malloc(nsections * sizeof * sectionlist);

    //log_msg("COFF: Found %u symbols in %u sections.\n", symtab_sz, nsections);

    /*
    The size of optional header is always zero for an obj file. So, the section header
    follows the file header immediately.
    */

    ptr = buf + 20;     //section header

    for (i = 0; i < nsections; i++)
    {
        char sectionname[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
        strncpy(sectionname, ptr, 8);
        //log_msg("COFF: Parsing section %s\n",sectionname);

        sectionlist[i] = malloc(strlen(sectionname) + 1);
        strcpy(sectionlist[i], sectionname);

        if (!strcmp(sectionname, ".data")) sectionrawdata_ptr = get_le32(ptr + 20);

        ptr += 40;
    }

    //log_msg("COFF: Symbol table at offset %u\n", symtab_ptr);
    //log_msg("COFF: raw data pointer ofset for section .data is %u\n", sectionrawdata_ptr);

    fp = fopen("vpx_asm_offsets.asm", "w");

    if (fp == NULL)
    {
        perror("open file");
        goto bail;
    }

    /*  The compiler puts the data with non-zero offset in .data section, but puts the data with
        zero offset in .bss section. So, if the data in in .bss section, set offset=0.
        Note from Wiki: In an object module compiled from C, the bss section contains
        the local variables (but not functions) that were declared with the static keyword,
        except for those with non-zero initial values. (In C, static variables are initialized
        to zero by default.) It also contains the non-local (both extern and static) variables
        that are also initialized to zero (either explicitly or by default).
        */
    //move to symbol table
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

    for (i = 0; i < symtab_sz; i++)
    {
        __int16 section = get_le16(ptr + 12); //section number

        if (section > 0 && ptr[16] == 2)
        {
            //if(section > 0 && ptr[16] == 3 && get_le32(ptr+8)) {

            if (get_le32(ptr))
            {
                char name[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
                strncpy(name, ptr, 8);
                //log_msg("COFF: Parsing symbol %s\n",name);
                fprintf(fp, "%-40s EQU ", name);
            }
            else
            {
                //log_msg("COFF: Parsing symbol %s\n",
                //        buf + strtab_ptr + get_le32(ptr+4));
                fprintf(fp, "%-40s EQU ", buf + strtab_ptr + get_le32(ptr + 4));
            }

            if (!(strcmp(sectionlist[section-1], ".bss")))
            {
                symoffset = 0;
            }
            else
            {
                symoffset = get_le32(buf + sectionrawdata_ptr + get_le32(ptr + 8));
            }

            //log_msg("      Section: %d\n",section);
            //log_msg("      Class:   %d\n",ptr[16]);
            //log_msg("      Address: %u\n",get_le32(ptr+8));
            //log_msg("      Offset: %u\n", symoffset);

            fprintf(fp, "%5d\n", symoffset);
        }

        ptr += 18;
    }

    fprintf(fp, "    END\n");
    fclose(fp);

    for (i = 0; i < nsections; i++)
    {
        free(sectionlist[i]);
    }

    free(sectionlist);

    return 0;
bail:

    for (i = 0; i < nsections; i++)
    {
        free(sectionlist[i]);
    }

    free(sectionlist);

    return 1;
}

int main(int argc, char **argv)
{
    int fd;
    output_fmt_t mode;
    const char *f;
    struct _stat stat_buf;
    unsigned __int8 *file_buf;
    int res;

    if (argc < 2 || argc > 3)
    {
        fprintf(stderr, "Usage: %s [output format] <obj file>\n\n", argv[0]);
        fprintf(stderr, "  <obj file>\tELF format object file to parse\n");
        fprintf(stderr, "Output Formats:\n");
        fprintf(stderr, "  gas  - compatible with GNU assembler\n");
        fprintf(stderr, "  rvds - compatible with armasm\n");
        goto bail;
    }

    f = argv[2];

    if (!strcmp(argv[1], "rvds"))
        mode = OUTPUT_FMT_RVDS;
    else if (!strcmp(argv[1], "gas"))
        mode = OUTPUT_FMT_GAS;
    else
        f = argv[1];

    if (_sopen_s(&fd, f, _O_BINARY, _SH_DENYNO, _S_IREAD | _S_IWRITE))
    {
        perror("Unable to open file");
        goto bail;
    }

    if (_fstat(fd, &stat_buf))
    {
        perror("stat");
        goto bail;
    }

    file_buf = malloc(stat_buf.st_size);

    if (!file_buf)
    {
        perror("malloc");
        goto bail;
    }

    if (_read(fd, file_buf, stat_buf.st_size) != stat_buf.st_size)
    {
        perror("read");
        goto bail;
    }

    if (_close(fd))
    {
        perror("close");
        goto bail;
    }

    res = parse_coff(file_buf, stat_buf.st_size);

    free(file_buf);

    if (!res)
        return EXIT_SUCCESS;

bail:
    return EXIT_FAILURE;
}
#endif
