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
#include <byteswap.h>

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
char reg_name[14][4] = { "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
        "cs", "ss", "ds", "es", "fs", "gs" };

char instr_name[][8] = { "unknown", "push", "add", "sub", "cmp", "mov", "j", "lea",
        "inc", "pop", "xor", "nop", "ret" };


enum eRegister {
    kNoReg = -1,
    keax = 0, kecx, kedx, kebx, kesp, kebp, kesi, kedi,
    kcs, kss, kds, kes, kfs, kgs
};

enum eInstruction {
    kunknown, kpush, kadd, ksub, kcmp, kmov, kjmp, klea,
    kinc, kpop, kxor, knop, kret,
};

const int kBaseFormatMask = (1<<5) -1;
enum eFormat {
    kfNone, kfreg, kfrm32, kfrm32_r32, kfr32_rm32, 
    kfmImm8     = 1 << 5,   // set on if imm8
    kfmImm32    = 1 << 6,   // set on if imm32
    kfmSize8    = 1 << 7,   // set on if 8 bit
    kfmSize16   = 1 << 8,   // set on if 16 bit
};

typedef unsigned char   uchar;


//********************************  Base OpCode Classes  **********************


struct CInstruction {
    CInstruction();

    int             fSize;  // size of the instruction in bytes

    eInstruction    fInstr;
    eFormat         fFormat;
    eRegister       fReg1;

    eRegister       fReg2;
    long            fDisp32;

    eRegister       fReg3;
    int             fScale;

    long            fImm32;

    virtual void    output_text( void );
    virtual void    optimize( void )    {}
    virtual int     generate_opcode( uchar *buffer );

    int             am_rm32( uchar *pCode, eFormat *format );
    int             am_rm32_reg( uchar *pCode, eFormat *format );
    int             am_encode( uchar *buffer, eRegister reg1, eFormat format );
};


CInstruction::CInstruction()
{
    fSize   = 1;
    fInstr   = kunknown;
    fFormat  = kfNone;
    fReg1    = kNoReg;
    fReg2    = kNoReg;
    fDisp32  = 0;
    fReg3    = kNoReg;
    fScale   = 1;
    fImm32   = 0;
}


struct CPop:CInstruction {
    CPop( eRegister reg )
    {
        fInstr = kpop;
        fFormat = kfreg;
        fReg1 = reg;
    }

    CPop( char imm8 )
    {
    }

    CPop( long imm32 )
    {
    }

    CPop( uchar *pCode, eFormat format )
    {
        fInstr = kpop;
        fSize += am_rm32( pCode, &format );
        fFormat = format;
    }

    virtual int     generate_opcode( uchar *buffer );
};


struct CPush:CInstruction {
    CPush( eRegister reg )
    {
        fInstr = kpush;
        fFormat = kfreg;
        fReg1 = reg;
    }

    CPush( char imm8 )
    {
    }

    CPush( long imm32 )
    {
    }

    CPush( uchar *pCode, eFormat format )
    {
        fInstr = kpush;
        fSize += am_rm32( pCode, &format );
        fFormat = format;
    }

    virtual int     generate_opcode( uchar *buffer );
};


struct CInc:CInstruction {
    CInc( uchar *pCode, eFormat format )
    {
        fInstr = kinc;
        fSize += am_rm32( pCode, &format );
        fFormat = format;
    }

    virtual int     generate_opcode( uchar *buffer );
};


struct CMov:CInstruction {
    CMov( uchar *pCode, eFormat format )
    {
        fInstr = kmov;
        fSize += am_rm32_reg( pCode, &format );
        fFormat = format;
    }

    virtual int     generate_opcode( uchar *buffer );
};


struct CCmp:CInstruction {
    CCmp( uchar *pCode, eFormat format )
    {
        fInstr = kcmp;
        fSize += am_rm32_reg( pCode, &format );
        fFormat = format;
    }

    virtual int     generate_opcode( uchar *buffer );
};


struct CAdd:CInstruction {
    CAdd( uchar *pCode, eFormat format )
    {
        fInstr = kadd;
        fSize += am_rm32_reg( pCode, &format );
        fFormat = format;
    }

    virtual int     generate_opcode( uchar *buffer );
};


struct CSub:CInstruction {
    CSub( uchar *pCode, eFormat format )
    {
        fInstr = ksub;
        fSize += am_rm32_reg( pCode, &format );
        fFormat = format;
    }

    virtual void    optimize( void );
    virtual int     generate_opcode( uchar *buffer );
};


struct CXor:CInstruction {
    CXor( uchar *pCode, eFormat format )
    {
        fInstr = kxor;
        fSize += am_rm32_reg( pCode, &format );
        fFormat = format;
    }

    virtual int     generate_opcode( uchar *buffer );
};


struct CJmp:CInstruction {
    CJmp( uchar imm8 )
    {
        fInstr = kjmp;
        fImm32 = imm8;
        fSize = 2;
    }

    virtual void    output_text( void );
};


struct CNop:CInstruction {
    CNop()
    {
        fInstr = knop;
        fSize = 1;
    }

    virtual int     generate_opcode( uchar *buffer ) { buffer[0] = 0x90; return 1; }
};


struct CRet:CInstruction {
    CRet()
    {
        fInstr = kret;
        fSize = 1;
    }

    virtual int     generate_opcode( uchar *buffer );
};


struct CLea:CInstruction {
    CLea( uchar *pCode, eFormat format )
    {
        fInstr = klea;
        fSize += am_rm32_reg( pCode, &format );
        fFormat = format;
    }
};


//*************************  Address Mode En/Decoding  ************************


/*
*   am_rm32 decodes the destination reg, and assumes that the src reg is a
*   selector or was set outside of this function
*/
int CInstruction::am_rm32( uchar *pCode, eFormat *format )
{
    unsigned char   reg = *pCode++;
    int             isize = 1;

    fReg2 = (eRegister)(reg & 0x07);

    if ( ((reg & 0x07) == 0x04) &&  // check for SIB
         ((reg & 0xC0) != 0xC0) )
    {
        pCode++;
        isize++;
    }

    if ((reg & 0xC0) == 0x80) // disp32
    {
        fDisp32 = bswap_32( *(long*)pCode );
        pCode += 4;
        isize += 4;
    }
    else if ((reg & 0xC0) == 0x40) // disp8
    {
        fDisp32 = *(char*)pCode;    // need it as a signed value
        pCode++;
        isize++;
    }
    else if ((reg & 0xC0) == 0x00) // no disp
    {
    }
    else // direct register
    {
    }

    if (*format & kfmImm8)
    {
        fImm32 = *(char*)pCode; // need it as a signed value
        pCode++;
        isize++;
    }

    if (*format & kfmImm32)
    {
        fImm32 = bswap_32( *(long*)pCode );
        pCode+=4;
        isize+=4;
    }

    return isize;
}


int CInstruction::am_rm32_reg( uchar *pCode, eFormat *format )
{
    fReg1 = (eRegister)((*pCode & 0x38) >> 3);
    return am_rm32( pCode, format );
}


int CInstruction::am_encode( uchar *buffer, eRegister reg1, eFormat format )
{
    int     isize = 0;

    if (fDisp32 == 0)
    {
        *buffer = 0xC0 | (reg1 << 3) | fReg2;
        isize = 1;
    }
    else if ( (fDisp32 >= -128) && (fDisp32 <= 127) )
    {
        *buffer = 0x40 | (reg1 << 3) | fReg2;
        *(buffer+1) = (uchar)fDisp32;
        isize = 2;
    }
    else
    {
        *buffer = 0x40 | (reg1 << 3) | fReg2;
        long bsDisp32 = bswap_32( fDisp32 );
        memcpy( buffer+1, &bsDisp32, 4 );
        isize = 5;
    }

    if (format & kfmImm8)
    {
        buffer[isize] = (uchar)fImm32;
        isize++;
    }
    else if (format & kfmImm32)
    {
        long bsImm32 = bswap_32( fImm32 );
        memcpy( buffer+isize, &bsImm32, 4 );
        isize += 4;
    }

    return isize;
}


//********************************** OpCode generators ************************


/*
*   returns the size of the generated opcode, which is in buffer
*/
int CInstruction::generate_opcode( uchar *buffer )
{
    buffer[0] = 0x90;   // make the default NOP
    return 0;
}


int CPop::generate_opcode( uchar *buffer )
{
    if (fFormat == kfreg)
    {
        buffer[0] = 0x58 | fReg1;
        return 1;
    }
}


int CPush::generate_opcode( uchar *buffer )
{
    if (fFormat == kfreg)
    {
        buffer[0] = 0x50 | fReg1;
        return 1;
    }
}


int CRet::generate_opcode( uchar *buffer )
{
    if (fFormat == kfNone)
    {
        buffer[0] = 0xC3;
        return 1;
    }
}


int CMov::generate_opcode( uchar *buffer )
{
    int     isize = 1;
    eFormat format = (eFormat)(kBaseFormatMask & (int)fFormat);

    if (format == kfrm32)
    {
        if (fFormat & kfmSize8)
            buffer[0] = 0xC6;
        else
            buffer[0] = 0xC7;
    }
    else if (format == kfrm32_r32)
    {
        if (fFormat & kfmSize8)
            buffer[0] = 0x88;
        else
            buffer[0] = 0x89;
    }
    else if (format == kfr32_rm32)
    {
        if (fFormat & kfmSize8)
            buffer[0] = 0x8A;
        else
            buffer[0] = 0x8B;
    }

    isize += am_encode( &buffer[1], fReg1, fFormat );
    return isize;
}


int CInc::generate_opcode( uchar *buffer )
{
    if (fFormat == kfreg)
    {
        buffer[0] = 0x40 | fReg1;
        return 1;
    }

    int     isize = 1;
    eFormat format = (eFormat)(kBaseFormatMask & (int)fFormat);
 
    if (format == kfrm32)
    {
        if (kfmSize8 & fFormat)
        {
            buffer[0] = 0xFE;
            isize += am_encode( &buffer[1], (eRegister)0, fFormat );
        }
        else if (kfmSize16 & fFormat)
        {
            buffer[0] = 0xFF;
            isize += am_encode( &buffer[1], (eRegister)0, fFormat );
        }
        else
        {
            buffer[0] = 0xFF;
            isize += am_encode( &buffer[1], (eRegister)6, fFormat );
        }
        return isize;
    }
 
    return isize;
}


int CCmp::generate_opcode( uchar *buffer )
{
    int     isize = 1;
    eFormat format = (eFormat)(kBaseFormatMask & (int)fFormat);

    if (format == kfrm32)
    {
        if (fFormat & kfmSize8)
            buffer[0] = 0x80;
        else if (fFormat & kfmImm8)
            buffer[0] = 0x83;
        else
            buffer[0] = 0x81;

        isize += am_encode( &buffer[1], (eRegister)7, fFormat );
        return isize;
    }
    else if (format == kfrm32_r32)
    {
        if (fFormat & kfmSize8)
            buffer[0] = 0x38;
        else
            buffer[0] = 0x39;
    }
    else if (format == kfr32_rm32)
    {
        if (fFormat & kfmSize8)
            buffer[0] = 0x3A;
        else
            buffer[0] = 0x3B;
    }

    isize += am_encode( &buffer[1], fReg1, fFormat );
    return isize;
}


int CXor::generate_opcode( uchar *buffer )
{
    int     isize = 1;
    eFormat format = (eFormat)(kBaseFormatMask & (int)fFormat);

    if (format == kfrm32)
    {
        if (fFormat & kfmSize8)
            buffer[0] = 0x80;
        else if (fFormat & kfmImm8)
            buffer[0] = 0x83;
        else
            buffer[0] = 0x81;

        isize += am_encode( &buffer[1], (eRegister)6, fFormat );
        return isize;
    }
    else if (format == kfrm32_r32)
    {
        if (fFormat & kfmSize8)
            buffer[0] = 0x30;
        else
            buffer[0] = 0x31;
    }
    else if (format == kfr32_rm32)
    {
        if (fFormat & kfmSize8)
            buffer[0] = 0x32;
        else
            buffer[0] = 0x33;
    }

    isize += am_encode( &buffer[1], fReg1, fFormat );
    return isize;
}


int CAdd::generate_opcode( uchar *buffer )
{
    int     isize = 1;
    eFormat format = (eFormat)(kBaseFormatMask & (int)fFormat);

    if (format == kfrm32)
    {
        buffer[0] = 0x01;
        isize += am_encode( &buffer[1], (eRegister)0, fFormat );
        return isize;
    }

    return isize;
}


void CSub::optimize()
{
    eFormat format = (eFormat)(kBaseFormatMask & (int)fFormat);
    eFormat isize = (eFormat)((int)fFormat & (kfmSize8 | kfmSize16 ));
 
    if (format == kfrm32)
    {
        if (kfmSize8 & isize)
        {
            fFormat = (eFormat)(format | kfmImm8 | kfmSize8);
        }
        else
        {
            if ( (fImm32 >= -128) && (fImm32 <= 127) )
            {
                fFormat =(eFormat)(format | kfmImm8 | isize);
            }
            else
            {
                fFormat =(eFormat)(format | kfmImm32 | isize);
            }
        }
    }
}


int CSub::generate_opcode( uchar *buffer )
{
    int     isize = 1;
    eFormat format = (eFormat)(kBaseFormatMask & (int)fFormat);
 
    if (format == kfrm32)
    {
        if (kfmSize8 & fFormat)
        {
            buffer[0] = 0x80;
            isize += am_encode( &buffer[1], (eRegister)5, fFormat );
            return isize;
        }
        else
        {
            if (fFormat | kfmImm8)
                buffer[0] = 0x83;
            else
                buffer[0] = 0x81;
            isize += am_encode( &buffer[1], (eRegister)5, fFormat );
            return isize;
        }
    }

    return isize;
}


//*********************************** Mneumonic Outputers *********************


void CInstruction::output_text( void )
{
    cout << instr_name[fInstr] << "\t";

    eFormat format = (eFormat)(kBaseFormatMask & (int)fFormat);
    switch (format)
    {
        case kfreg:
            cout << reg_name[fReg1];
            break;
        case kfrm32:
            cout << reg_name[fReg2];
            break;
        case kfrm32_r32:
            cout << reg_name[fReg2] << ", ";
            cout << reg_name[fReg1];
            break;
        case kfr32_rm32:
            cout << reg_name[fReg1] << ", ";
            cout << reg_name[fReg2];
            break;
    }

    if (fFormat & kfmImm8)  cout.form( ", #0x%02X", fImm32 );
    if (fFormat & kfmImm32)  cout.form( ", #0x%08X", fImm32 );

    cout << "\n";
}


void CJmp::output_text( void )
{
    cout << "j\t";
    cout.form( ".+0x%02X\n", fImm32 );
}


void hexdump( CInstruction *instr )
{
    uchar   buffer[16];
    int     instrSize = instr->generate_opcode( buffer );
    for (int i=0; i<instrSize; i++)
    {
        cout.form("%02x ", buffer[i]);
    }
    cout << "\t";
}


CInstruction* get_next_instruction( uchar *pCode )
{
    CInstruction    *retInstr=NULL;
    uchar           *reg = pCode+1;

    switch (*pCode)
    {
        case 0x01:
            retInstr = new CAdd( reg, kfrm32_r32 );
            break;
        case 0x31:
            retInstr = new CXor( reg, kfrm32_r32 );
            break;
        case 0x50:
        case 0x51:
        case 0x52:
        case 0x53:
        case 0x54:
        case 0x55:
        case 0x56:
        case 0x57:
            retInstr = new CPush( (eRegister)(*pCode & 0x07) );
            break;
        case 0x58:
        case 0x59:
        case 0x5a:
        case 0x5b:
        case 0x5c:
        case 0x5d:
        case 0x5e:
        case 0x5f:
            retInstr = new CPop( (eRegister)(*pCode & 0x07) );
            break;
        case 0x83:
            switch (DIGIT_MAP[*reg])
            {
                case 0:
                    retInstr = new CAdd( reg, (eFormat)(kfrm32 | kfmImm8) );
                    break;
                case 5:
                    retInstr = new CSub( reg, (eFormat)(kfrm32 | kfmImm8) );
                    break;
                case 7:
                    retInstr = new CCmp( reg, (eFormat)(kfrm32 | kfmImm8) );
                    break;
                default:
//                    retInstr = am_rm32_imm8( kunknown, reg );
                    break;
            }
            break;
        case 0x89:
            retInstr = new CMov( reg, kfrm32_r32 );
            break;
        case 0x88:
        case 0x8a:
            break;
        case 0x8b:
            retInstr = new CMov( reg, kfr32_rm32 );
            break;
        case 0x8c:
        case 0x8e:
            break;
        case 0x8d:
            retInstr = new CLea( reg,  kfr32_rm32 );
            break;
        case 0x90:
            retInstr = new CNop();
            break;
        case 0xc3:
            retInstr = new CRet();
            break;
        case 0xc7:
            retInstr = new CMov( reg, (eFormat)(kfrm32 | kfmImm32) );
            break;


        case 0x7e:
        case 0xeb:
            retInstr = new CJmp( *reg );
            break;
        case 0xff:
            switch (DIGIT_MAP[*reg])
            {
                case 0:
                    retInstr = new CInc( reg, eFormat(kfrm32 | kfmSize16) );
                    break;
                case 6:
                    retInstr = new CPush( reg, kfrm32 );
                    break;
                default:
//                    retInstr = am_rm32_imm8( kunknown, reg );
                    break;
            } 
            break;

        default:
//            retInstr = am_rm32_imm8( kunknown, reg);
            break;
    }

// ek only until the table above is completed
    if (retInstr == NULL)   retInstr = new CInstruction();

    return retInstr;
}


/*
* Takes the compile intel code and tries to optimize it.
*
*   returns the number of bytes smaller the new code is.
*/

long process_function( uchar *pCode, long codeSize )
{
    long            saved=0;

    while (codeSize > 0)
    {
        CInstruction    *instr = get_next_instruction( pCode );
        int instrSize = instr->fSize;

        pCode += instrSize;
        codeSize -= instrSize;

        hexdump( instr );
        instr->output_text();
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
