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

struct CInstruction {
    int             isize;  // size of the instruction in bytes
    unsigned char   idata[8];
    virtual void    output( void );
    void            hexdump( void );
};


void CInstruction::output( void )
{
    cout << "Dummy Instruction\n";
}


struct CPush:CInstruction {
                    CPush( unsigned char *theCode );
    virtual void    output( void ); 
};


struct CCmp:CInstruction {
                    CCmp( unsigned char *theCode );
    virtual void    output( void ); 
};


struct CSub:CInstruction {
                    CSub( unsigned char *theCode );
    virtual void    output( void ); 
};


struct CBadOpCode:CInstruction {
                    CBadOpCode( unsigned char *theCode );
    virtual void    output( void ); 
};


void CInstruction::hexdump( void )
{
    int instrSize = isize;
    while (instrSize)
    {
        cout.form("%02x ", idata[isize - instrSize]);
        instrSize--;
    }
    cout << "\n";
}


/* CPush */
CPush::CPush( unsigned char *theCode )
{
    switch( *theCode )
    {
        case 0x06:
            isize = 1;
            break;
        case 0x0e:
            isize = 1;
            break;
        case 0x0f:
            isize = 2;
            break;
        case 0x16:
            isize = 1;
            break;
        case 0x1e:
            isize = 1;
            break;
        case 0x68:
            isize = 5;
            break;
        case 0x6a:
            isize = 2;
            break;
        case 0x50:
        case 0x51:
        case 0x52:
        case 0x53:
        case 0x54:
        case 0x55:
        case 0x56:
        case 0x57:
            isize = 1;
            break;
        case 0xFF:
            isize = 2;  // at least 2!
            break;
    }
    memcpy( idata, theCode, isize );
}


void CPush::output( void )
{
    cout << "push r\n";
}


/* CCmp */
CCmp::CCmp( unsigned char *theCode )
{
    isize = 4;
    memcpy( idata, theCode, isize );
}


void CCmp::output( void )
{
    cout << "cmp imm8,off(r)\n";
}


/* CSub */
CSub::CSub( unsigned char *theCode )
{
    isize = 3;
    memcpy( idata, theCode, isize );
}


void CSub::output( void )
{
    cout << "sub r,imm8\n";
}


/* CBadOpCode */
CBadOpCode::CBadOpCode( unsigned char *theCode )
{
    isize = 2;
    memcpy( idata, theCode, isize );
}


void CBadOpCode::output( void )
{
    cout << "*** Bad OpCode ";
    cout.form( "%02X %02X ***\n", *idata, *(idata+1) );
}




CInstruction* get_next_instruction( unsigned char *theCode )
{
    CInstruction    *retInstr;

    switch (*theCode)
    {
        case 0x55:
            retInstr = new CPush( theCode );
            break;
        case 0x83:
            switch (DIGIT_MAP[*(theCode+1)])
            {
                case 5:
                    retInstr = new CSub( theCode );
                    break;
                case 7:
                    retInstr = new CCmp( theCode );
                    break;
                default:
                    retInstr = new CBadOpCode( theCode );
                    break;
            }
            break;
        case 0x88:
        case 0x89:
        case 0x8a:
        case 0x8b:
        case 0x8c:
        case 0x8e:
        case 0xc7:
          //  retInstr = new CMov( theCode );
            break;
        case 0x7e:
        case 0xeb:
          //  retInstr = new CJmp( theCode );
            break;

        default:
            retInstr = new CBadOpCode( theCode );
            break;
    }

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
