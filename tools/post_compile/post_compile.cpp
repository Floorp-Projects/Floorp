/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is foldelf.cpp, released November 28, 2000.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *    Edward Kandrot <kandrot@netscape.com>
 *    Chris Waterson <waterson@netscape.com>
 *
 * This program reads an ELF file and computes information about
 * redundancies.
 *
 * 
 */

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <elf.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>

//----------------------------------------------------------------------

bool	gDebug=false;

//----------------------------------------------------------------------

static void
hexdump(ostream& out, const char* bytes, size_t count)
{
    hex(out);

    size_t off = 0;
    while (off < count) {
        out.form("%08lx: ", off);

        const char* p = bytes + off;

        int j = 0;
        while (j < 16) {
            out.form("%02x", p[j++] & 0xff);
            if (j + off >= count)
                break;

            out.form("%02x ", p[j++] & 0xff);
            if (j + off >= count)
                break;
        }

        // Pad
        for (; j < 16; ++j)
            out << ((j%2) ? "   " : "  ");

        for (j = 0; j < 16; ++j) {
            if (j + off < count)
                out.put(isprint(p[j]) ? p[j] : '.');
        }

        out << endl;
        off += 16;
    }
}

//----------------------------------------------------------------------

int
verify_elf_header(const Elf32_Ehdr* hdr)
{
    if (hdr->e_ident[EI_MAG0] != ELFMAG0
        || hdr->e_ident[EI_MAG1] != ELFMAG1
        || hdr->e_ident[EI_MAG2] != ELFMAG2
        || hdr->e_ident[EI_MAG3] != ELFMAG3) {
        cerr << "not an elf file" << endl;
        return -1;
    }

    if (hdr->e_ident[EI_CLASS] != ELFCLASS32) {
        cerr << "not a 32-bit elf file" << endl;
        return -1;
    }

    if (hdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        cerr << "not a little endian elf file" << endl;
        return -1;
    }

    if (hdr->e_ident[EI_VERSION] != EV_CURRENT) {
        cerr << "incompatible version" << endl;
        return -1;
    }

    return 0;
}

//----------------------------------------------------------------------

class elf_symbol : public Elf32_Sym
{
public:
    elf_symbol(const Elf32_Sym& sym)
    { ::memcpy(static_cast<Elf32_Sym*>(this), &sym, sizeof(Elf32_Sym)); }

    friend bool operator==(const elf_symbol& lhs, const elf_symbol& rhs) {
        return 0 == ::memcmp(static_cast<const Elf32_Sym*>(&lhs),
                             static_cast<const Elf32_Sym*>(&rhs),
                             sizeof(Elf32_Sym)); }
};

//----------------------------------------------------------------------

static const char*
st_bind(unsigned char info)
{
    switch (ELF32_ST_BIND(info)) {
    case STB_LOCAL:      return "local";
    case STB_GLOBAL:     return "global";
    case STB_WEAK:       return "weak";
    default:             return "unknown";
    }
}

static const char*
st_type(unsigned char info)
{
    switch (ELF32_ST_TYPE(info)) {
    case STT_NOTYPE:     return "none";
    case STT_OBJECT:     return "object";
    case STT_FUNC:       return "func";
    case STT_SECTION:    return "section";
    case STT_FILE:       return "file";
    default:             return "unknown";
    }
}

static unsigned char
st_type(const char* type)
{
    if (strcmp(type, "none") == 0) {
        return STT_NOTYPE;
    }
    else if (strcmp(type, "object") == 0) {
        return STT_OBJECT;
    }
    else if (strcmp(type, "func") == 0) {
        return STT_FUNC;
    }
    else {
        return 0;
    }
}


//----------------------------------------------------------------------                      


int DIGIT_MAP[256] = {
0, 0, 0, 0, 0, 0, 0, 0,     // 0x00 - 0x07
1, 1, 1, 1, 1, 1, 1, 1,
2, 2, 2, 2, 2, 2, 2, 2,
3, 3, 3, 3, 3, 3, 3, 3,
4, 4, 4, 4, 4, 4, 4, 4,
5, 5, 5, 5, 5, 5, 5, 5,
6, 6, 6, 6, 6, 6, 6, 6,
7, 7, 7, 7, 7, 7, 7, 7,     // 0x38 - 0x3f

0, 0, 0, 0, 0, 0, 0, 0,     // 0x40 - 0x47
1, 1, 1, 1, 1, 1, 1, 1,
2, 2, 2, 2, 2, 2, 2, 2,
3, 3, 3, 3, 3, 3, 3, 3,
4, 4, 4, 4, 4, 4, 4, 4,
5, 5, 5, 5, 5, 5, 5, 5,
6, 6, 6, 6, 6, 6, 6, 6,
7, 7, 7, 7, 7, 7, 7, 7,     // 0x78 - 0x7f

0, 0, 0, 0, 0, 0, 0, 0,     // 0x80 - 0x87
1, 1, 1, 1, 1, 1, 1, 1,
2, 2, 2, 2, 2, 2, 2, 2,
3, 3, 3, 3, 3, 3, 3, 3,
4, 4, 4, 4, 4, 4, 4, 4,
5, 5, 5, 5, 5, 5, 5, 5,
6, 6, 6, 6, 6, 6, 6, 6,
7, 7, 7, 7, 7, 7, 7, 7,     // 0xb8 - 0xbf

0, 0, 0, 0, 0, 0, 0, 0,     // 0xc0 - 0xc7
1, 1, 1, 1, 1, 1, 1, 1,
2, 2, 2, 2, 2, 2, 2, 2,
3, 3, 3, 3, 3, 3, 3, 3,
4, 4, 4, 4, 4, 4, 4, 4,
5, 5, 5, 5, 5, 5, 5, 5,
6, 6, 6, 6, 6, 6, 6, 6,
7, 7, 7, 7, 7, 7, 7, 7};     // 0xf8 - 0xff


/*
* Dceclation of the Instruction types
*/
char reg_name[8][4] = { "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi" };

char instr_name[][8] = { "unknown", "push", "add", "sub", "cmp", "mov", "j", "lea",
        "incr", "pop", "xor", "nop", "ret" };


enum eRegister {
    kNoReg = -1,
    keax = 0,
};

enum eInstruction {
    kunknown, kpush, kadd, ksub, kcmp, kmov, kjmp, klea,
    kincr, kpop, kxor, knop, kret,
};


struct CInstruction {
    int             isize;  // size of the instruction in bytes
    unsigned char   idata[8];

    eInstruction    instr;
    eRegister       dest;
    eRegister       src;

    long            destData;
    long            srcData;

    void            output( void );
    void            hexdump( void );
};


void am_rm32( CInstruction *instr, unsigned char *theCode )
{
    unsigned char   reg = *theCode;
    instr->isize = 2;
    instr->src = (eRegister)((reg & 0x31) >> 3);
    instr->dest = (eRegister)(reg & 0x07);
// ek need to add and set the address mode
// ek need to determine whether we are doing source/dest or dest/source
    if ((reg & 0x07) == 0x04)  // check for SIB
    {
    }

    if ((reg >= 0x80) && (reg < 0xC0)) // disp32
    {
        instr->destData = *(theCode +1);    // ek need to do a long here
        instr->isize += 4;
    }

    if ((reg >= 0x40) && (reg < 0x80)) // disp8
    {
        instr->destData = *(theCode +1);
        instr->isize++;
    }
}


/*
*   theCode points to the first register field
*/
CInstruction *am_rm32_imm32( eInstruction instr, unsigned char *theCode )
{
    CInstruction    *retInstr = new CInstruction;
    am_rm32( retInstr, theCode );
    retInstr->src = kNoReg;     // imm32 is the src
    retInstr->instr = instr;
    retInstr->srcData = *(long*)(theCode + retInstr->isize-1);
    retInstr->isize += 4;       // add one for the imm32
}


/*
*   theCode points to the first register field
*/
CInstruction *am_rm32_imm8( eInstruction instr, unsigned char *theCode )
{
    CInstruction    *retInstr = new CInstruction;
    am_rm32( retInstr, theCode );
    retInstr->src = kNoReg;     // imm8 is the src
    retInstr->instr = instr;
    retInstr->srcData = *(theCode + retInstr->isize-1);
    retInstr->isize += 1;       // add one for the imm8
}


CInstruction *am_rm32_reg( eInstruction instr, unsigned char *theCode )
{
    CInstruction    *retInstr = new CInstruction;
    am_rm32( retInstr, theCode );
    retInstr->instr = instr;
}


CInstruction *am_non_reg( eInstruction instr, eRegister reg )
{
    CInstruction    *retInstr = new CInstruction;
    retInstr->isize = 1;
    retInstr->instr = instr;
    retInstr->src = reg;
    retInstr->dest = kNoReg;
}


CInstruction *am_non_non( eInstruction instr ) 
{
    CInstruction    *retInstr = new CInstruction;
    retInstr->isize = 1;
    retInstr->instr = instr;
    retInstr->src = kNoReg;
    retInstr->dest = kNoReg;
}


CInstruction *am_imm8( eInstruction instr, unsigned char *theCode )
{
    CInstruction    *retInstr = new CInstruction;
    retInstr->isize = 2;
    retInstr->instr = instr;
    retInstr->src = kNoReg;
    retInstr->dest = kNoReg;
    // ek need to get the imm8 data into the data structure
}


CInstruction *am_reg_rm32( eInstruction instr, unsigned char *theCode )
{
    CInstruction    *retInstr = new CInstruction;
    am_rm32( retInstr, theCode );
    retInstr->instr = instr;
// ek need to reverse the src and dest
}


void CInstruction::output( void )
{
    bool commaNeeded = false;

    cout << instr_name[instr] << "\t";

    if (dest != kNoReg)
    {
        cout << reg_name[dest];
        commaNeeded = true;
    }
    if (src != kNoReg)
    {
        if (commaNeeded)    cout << ", ";
        cout << reg_name[src];
    }

    cout << "\n";
}


void CInstruction::hexdump( void )
{
    int instrSize = isize;
    while (instrSize)
    {
        cout.form("%02x ", idata[isize - instrSize]);
        instrSize--;
    }
    cout << "\t";
}


CInstruction* get_next_instruction( unsigned char *theCode )
{
    CInstruction    *retInstr;
    unsigned char   *reg = theCode+1;

    switch (*theCode)
    {
        case 0x01:
            retInstr = am_rm32_reg( kadd, reg );
            break;
        case 0x31:
            retInstr = am_rm32_reg( kxor, reg );
            break;
        case 0x50:
        case 0x51:
        case 0x52:
        case 0x53:
        case 0x54:
        case 0x55:
        case 0x56:
        case 0x57:
            retInstr = am_non_reg( kpush, (eRegister)(*theCode & 0x07) );
            break;
        case 0x58:
        case 0x59:
        case 0x5a:
        case 0x5b:
        case 0x5c:
        case 0x5d:
        case 0x5e:
        case 0x5f:
            retInstr = am_non_reg( kpop, (eRegister)(*theCode & 0x07) );
            break;
        case 0x83:
            switch (DIGIT_MAP[*reg])
            {
                case 5:
                    retInstr = am_rm32_imm8( ksub, reg );
                    break;
                case 7:
                    retInstr = am_rm32_imm8( kcmp, reg );
                    break;
                default:
                    retInstr = am_rm32_imm8( kunknown, reg );
                    break;
            }
            break;
        case 0x89:
            retInstr = am_rm32_reg( kmov, reg );
            break;
        case 0x88:
        case 0x8a:
            retInstr = am_rm32_imm8( kunknown, reg );
            break;
        case 0x8b:
            retInstr = am_reg_rm32( kmov, reg );
            break;
        case 0x8c:
        case 0x8e:
            retInstr = am_rm32_imm8( kunknown, reg );
            break;
        case 0x8d:
            retInstr = am_rm32_imm8( klea, reg );
       //     retInstr->isize++;  // ek need to handle the 16/32 instead of 8/32 for lea
            break;
        case 0x90:
            retInstr = am_non_non( knop );
            break;
        case 0xc3:
            retInstr = am_non_non( kret );
            break;
        case 0xc7:
            retInstr = am_rm32_imm32( kmov, reg );
            break;


        case 0x7e:
        case 0xeb:
            retInstr = am_imm8( kjmp, reg );
            break;
        case 0xff:
            switch (DIGIT_MAP[*reg])
            {
                case 0:     // ek check this out, since I believe the book is wrong
                    retInstr = am_reg_rm32( kincr, reg );
                    break;
                default:
                    retInstr = am_rm32_imm8( kunknown, reg );
                    break;
            } 
            break;

        default:
            retInstr = am_rm32_imm8( kunknown, reg);
            break;
    }

    memcpy( retInstr->idata, theCode, retInstr->isize );
    return retInstr;
}


/*
* Takes the compile intel code and tries to optimize it.
*
*   returns the number of bytes smaller the new code is.
*/

long process_function( unsigned char *theCode, long codeSize )
{
    long            saved=0;

    while (codeSize > 0)
    {
        CInstruction    *instr = get_next_instruction( theCode );
        int instrSize = instr->isize;

        theCode += instrSize;
        codeSize -= instrSize;

        instr->hexdump();
        instr->output();
        delete instr;
    }

    return 0;
}


//----------------------------------------------------------------------

typedef vector<elf_symbol> elf_symbol_table;
typedef map< basic_string<char>, elf_symbol_table > elf_text_map;

void
process_mapping(char* mapping, size_t size)
{
    const Elf32_Ehdr* ehdr = reinterpret_cast<Elf32_Ehdr*>(mapping);
    if (verify_elf_header(ehdr) < 0)
        return;
    // find the section headers
    const Elf32_Shdr* shdrs = reinterpret_cast<Elf32_Shdr*>(mapping + ehdr->e_shoff);

    // find the section header string table, .shstrtab
    const Elf32_Shdr* shstrtabsh = shdrs + ehdr->e_shstrndx;
    const char* shstrtab = mapping + shstrtabsh->sh_offset;

    // find the sections we care about
    const Elf32_Shdr *symtabsh, *strtabsh, *textsh;
    int textndx;
    for (int i = 0; i < ehdr->e_shnum; ++i)
	{
        basic_string<char> name(shstrtab + shdrs[i].sh_name);
        if (gDebug)	cout << "name = " << name << "\n";
        if (name == ".symtab") {
            symtabsh = shdrs + i;
        }
        else if (name == ".text") {
            textsh = shdrs + i;
            textndx = i;
        }
        else if (name == ".strtab") {
            strtabsh = shdrs + i;
        }
    }

    // find the .strtab
    char* strtab = mapping + strtabsh->sh_offset;

    // find the .text
    char* text = mapping + textsh->sh_offset;
    int textaddr = textsh->sh_addr;

    // find the symbol table
    int nentries = symtabsh->sh_size / sizeof(Elf32_Sym);
    Elf32_Sym* symtab = reinterpret_cast<Elf32_Sym*>(mapping + symtabsh->sh_offset);

    // look for code in the .text section
    elf_text_map textmap;
    long bytesSaved = 0;
    for (int i = 0; i < nentries; ++i) {
        const Elf32_Sym* sym = symtab + i;
        if ( sym->st_shndx == textndx && sym->st_size)
        {
            basic_string<char> funcname(sym->st_name + strtab, sym->st_size);
            basic_string<char> functext(text + sym->st_value - textaddr, sym->st_size);

            if (gDebug) cout << funcname << "\n\n";
            if (gDebug) hexdump(cout,functext.data(),sym->st_size);
            bytesSaved += process_function( (unsigned char *)functext.data(), sym->st_size );
        }
    }

    cout << "Code size reduction of " << bytesSaved << "\n";
}

void
process_file(const char* name)
{
	if (gDebug) cout << name << "\n";

    int fd = open(name, O_RDWR);
    if (fd < 0)
	{
		cerr << "***Failed to Open!***"<< "\n";
	}
	else
	{
        struct stat statbuf;
        if (fstat(fd, &statbuf) < 0)
		{
			cerr << "***Failed to Access File!***"<< "\n";
		}
		else
		{
            size_t size = statbuf.st_size;

            void* mapping = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
            if (mapping != MAP_FAILED)
			{
                process_mapping(static_cast<char*>(mapping), size);
                munmap(mapping, size);
            }
        }
        close(fd);
    }
}


int main(int argc, char* argv[])
{
	while(1)
	{
		char c = getopt( argc, argv, "d" );
		if (c == -1)	break;

		switch (c)
		{
			case 'd':
				gDebug = true;
				cout << "Debugging Info ON\n";
				break;
		}
	}

    for (int i = optind; i < argc; ++i)
        process_file(argv[i]);

    return 0;
}
