/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is foldelf.cpp, released
 * November 28, 2000.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Edward Kandrot <kandrot@netscape.com>
 *   Chris Waterson <waterson@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * This program reads an ELF file and computes information about
 * redundancies.
 */
 
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <list>
#include <elf.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
//#include <byteswap.h>

//----------------------------------------------------------------------

bool	gDebug=false;
bool    gCompact=false;
bool    gOptimize=false;
bool    gAssembly=false;
bool    gfirst=false;

#define bswap_32(x) \
     ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) |           \
      (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24))

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

enum eRegister {
    kNoReg = -1,
    keax = 0, kecx, kedx, kebx, kesp, kebp, kesi, kedi,
    kcs, kss, kds, kes, kfs, kgs
};

char reg_name[14][4] = { "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
        "cs", "ss", "ds", "es", "fs", "gs" };


enum eInstruction {
    kunknown, kpush, kadd, ksub, kcmp, kmov, kjmp, klea,
    kinc, kpop, kxor, knop, kret, kcall, kand,  
};

char instr_name[][8] = { "unknown", "push", "add", "sub", "cmp", "mov", "j", "lea",
        "inc", "pop", "xor", "nop", "ret", "call", "and", };


char cond_name[][4] = { "o", "no", "b", "nb", "z", "nz", "be", "nbe",
        "s", "ns", "pe", "po", "l", "ge", "le", "g", "mp", "cxz" };

enum eCond {
    kjo, kjno, kjb, kjnb, kjz, kjnz, kjbe, kjnbe,
    kjs, kjns, kjpe, kjpo, kjl, kjge, kjle, kjg, kjt, kjcxz
};


const int kBaseFormatMask = (1<<5) -1;
enum eFormat {
    kfNone, kfreg, kfrm32, kfrm32_r32, kfr32_rm32, 
    kfmImm8     = 1 << 5,   // set on if imm8
    kfmImm32    = 1 << 6,   // set on if imm32
    kfmSize8    = 1 << 7,   // set on if 8 bit
    kfmSize16   = 1 << 8,   // set on if 16 bit
    kfmDeref    = 1 << 9,
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

    int             am_rm32( uchar *pCode );
    int             am_rm32_reg( uchar *pCode );
    int             am_encode( uchar *buffer, eRegister reg1 );
};


struct CFunction {
    CFunction( string funcName ){fName = funcName;}
    string              fName;
    list<CInstruction*> fInstructions;

    void            get_instructions( uchar *pCode, long codeSize );
    void            remove_nops();
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
        fFormat = format;
        fSize += am_rm32( pCode );
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
        fInstr = kpush;
        fFormat = kfreg;
        fImm32 = imm8;
    }

    CPush( long imm32 )
    {
        fInstr = kpush;
        fFormat = kfreg;
        fImm32 = imm32;
    }

    CPush( uchar *pCode, eFormat format )
    {
        fInstr = kpush;
        fFormat = format;
        fSize += am_rm32( pCode );
    }

    virtual int     generate_opcode( uchar *buffer );
};


struct CInc:CInstruction {
    CInc( eRegister reg )
    {
        fInstr = kinc;
        fFormat = kfreg;
        fReg1 = reg;
    }

    CInc( uchar *pCode, eFormat format )
    {
        fInstr = kinc;
        fFormat = format;
        fSize += am_rm32( pCode );
    }

    virtual int     generate_opcode( uchar *buffer );
};


struct CMov:CInstruction {
    CMov( eRegister reg, uchar *pCode, eFormat format )
    {
        fReg1 = reg;
        fInstr = kmov;
        fFormat = format;
        fSize += am_rm32( pCode );
    }

    CMov( uchar *pCode, eFormat format )
    {
        fInstr = kmov;
        fFormat = format;
        fSize += am_rm32_reg( pCode );
    }

    virtual void    optimize( void );
    virtual int     generate_opcode( uchar *buffer );
};


struct CCmp:CInstruction {
    CCmp( uchar *pCode, eFormat format )
    {
        fInstr = kcmp;
        fFormat = format;
        fSize += am_rm32_reg( pCode );
    }

    virtual int     generate_opcode( uchar *buffer );
};


struct CAdd:CInstruction {
    CAdd( uchar *pCode, eFormat format )
    {
        fInstr = kadd;
        fFormat = format;
        fSize += am_rm32_reg( pCode );
    }

    virtual void    optimize( void );
    virtual int     generate_opcode( uchar *buffer );
};


struct CSub:CInstruction {
    CSub( uchar *pCode, eFormat format )
    {
        fInstr = ksub;
        fFormat = format;
        fSize += am_rm32_reg( pCode );
    }

    virtual void    optimize( void );
    virtual int     generate_opcode( uchar *buffer );
};


struct CAnd:CInstruction {
    CAnd( uchar *pCode, eFormat format )
    {
        fInstr = kand;
        fFormat = format;
        fSize += am_rm32_reg( pCode );
    }

    virtual int     generate_opcode( uchar *buffer );
};


struct CXor:CInstruction {
    CXor( uchar *pCode, eFormat format )
    {
        fInstr = kxor;
        fFormat = format;
        fSize += am_rm32_reg( pCode );
    }

    virtual int     generate_opcode( uchar *buffer );
};


struct CCall:CInstruction {
    CCall( long imm32 )
    {
        fInstr = kcall;
        fImm32 = imm32;
        fSize = 5;
    }

//    virtual void    output_text( void );
    virtual int     generate_opcode( uchar *buffer );
};


struct CJmp:CInstruction {
    eCond   fCond;

    CJmp( eCond cond, char imm8 )
    {
        fInstr = kjmp;
        fImm32 = imm8;
        fCond = cond;
        fSize = 2;
    }

    virtual void    output_text( void );
    virtual int     generate_opcode( uchar *buffer );
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
        fFormat = format;
        fSize += am_rm32_reg( pCode );
    }

    virtual void    optimize( void );
    virtual int     generate_opcode( uchar *buffer );
};


//*************************  Address Mode En/Decoding  ************************


/*
*   am_rm32 decodes the destination reg, and assumes that the src reg is a
*   selector or was set outside of this function
*/
int CInstruction::am_rm32( uchar *pCode )
{
    unsigned char   reg = *pCode++;
    int             isize = 1;

    fReg2 = (eRegister)(reg & 0x07);

    if ( ((reg & 0x07) == 0x04) &&  // check for SIB
         ((reg & 0xC0) != 0xC0) )
    {
        fFormat = (eFormat)(fFormat | kfmDeref);
        fReg2 = (eRegister)(*pCode & 0x07);
        fReg3 = (eRegister)((*pCode & 0x38) >> 3);
        if ((int)fReg3 = 0x04)  fReg3 = kNoReg;
        fScale = 1 << ((*pCode & 0xC0) >> 6);
        pCode++;
        isize++;
    }

    if ((reg & 0xC0) == 0x80) // disp32
    {
        fDisp32 = bswap_32( *(long*)pCode );
        fFormat = (eFormat)(fFormat | kfmDeref);
        pCode += 4;
        isize += 4;
    }
    else if ((reg & 0xC0) == 0x40) // disp8
    {
        fDisp32 = *(char*)pCode;    // need it as a signed value
        fFormat = (eFormat)(fFormat | kfmDeref);
        pCode++;
        isize++;
    }
    else if ((reg & 0xC0) == 0x00) // no disp
    {
        fFormat = (eFormat)(fFormat | kfmDeref);
		if (fReg2  == 0x05)	// absolute address
		{
			fReg2 = kNoReg;
	        fDisp32 = bswap_32( *(long*)pCode );
	        pCode += 4;
    	    isize += 4;
		}
    }

    if (fFormat & kfmImm8)
    {
        fImm32 = *(char*)pCode; // need it as a signed value
        pCode++;
        isize++;
    }

    if (fFormat & kfmImm32)
    {
        fImm32 = bswap_32( *(long*)pCode );
        pCode+=4;
        isize+=4;
    }

    return isize;
}


int CInstruction::am_rm32_reg( uchar *pCode )
{
    fReg1 = (eRegister)((*pCode & 0x38) >> 3);
    return am_rm32( pCode );
}


int CInstruction::am_encode( uchar *buffer, eRegister reg1 )
{
    int     isize = 1;
    uchar   sib=0;
    bool    use_sib=false;
    eFormat format=fFormat;

    if ((fScale != 1) ||
        (fReg3 != kNoReg) ||
        ( (fReg2 == kesp) && (format & kfmDeref) ))
    {
        uchar   scale=1;
        switch (fScale)
        {
            case 1:
                scale = 0x00;
                break;
            case 2:
                scale = 0x40;
                break;
            case 4:
                scale = 0x80;
                break;
            case 8:
                scale = 0xC0;
                break;
        }
        if (fReg3 == kNoReg)
            sib = scale | (0x04 << 3) | fReg2;
        else
            sib = scale | (fReg3 << 3) | fReg2;
        use_sib = true;
*buffer = 0xff;     // ek   must fix the order of the SIB output -> opcode, sib, disp, imm
        buffer++;
        isize++;
    }

    if ( (fReg2 == kNoReg) && (fFormat & kfmDeref) )	// absolute address
	{
        *buffer = 0x00 | (reg1 << 3) | 0x05;
        long bsDisp32 = bswap_32( fDisp32 );
        memcpy( buffer+1, &bsDisp32, 4 );
        isize += 4;
	}
    else if (fDisp32 == 0)
    {
        if (format & kfmDeref)
        {
            *buffer = 0x00 | (reg1 << 3) | fReg2;
        }
        else
        {
            *buffer = 0xC0 | (reg1 << 3) | fReg2;
        }
    }
    else if ( (fDisp32 >= -128) && (fDisp32 <= 127) )
    {
        *buffer = 0x40 | (reg1 << 3) | fReg2;
        *(buffer+1) = (uchar)fDisp32;
        isize++;
    }
	else
    {
        *buffer = 0x40 | (reg1 << 3) | fReg2;
        long bsDisp32 = bswap_32( fDisp32 );
        memcpy( buffer+1, &bsDisp32, 4 );
        isize += 4;
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

    return 0;
}


int CPush::generate_opcode( uchar *buffer )
{
    if (fFormat == kfreg)
    {
        buffer[0] = 0x50 | fReg1;
        return 1;
    }

    return 0;
}


int CCall::generate_opcode( uchar *buffer )
{
    if (fFormat == kfNone)
    {
        buffer[0] = 0xE8;
        long bsImm32 = bswap_32( fImm32 );
        memcpy( buffer+1, &bsImm32, 4 );
        return 5;
    }

    return 0;
}


int CRet::generate_opcode( uchar *buffer )
{
    if (fFormat == kfNone)
    {
        buffer[0] = 0xC3;
        return 1;
    }

    return 0;
}


void CLea::optimize( void )
{
    eFormat format = (eFormat)(kBaseFormatMask & (int)fFormat);

    if ( (fDisp32 == 0) && (fReg1 == fReg2) && (fReg3 == kNoReg) )
        fInstr = knop;
} 


int CLea::generate_opcode( uchar *buffer )
{
    buffer[0] = 0x8D;
    return 1+am_encode( &buffer[1], fReg1 );
}


void CMov::optimize( void )
{
    if ( (fReg1 == fReg2) && (fReg3 == kNoReg) && !(fFormat & kfmDeref) )
        fInstr = knop;
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

    isize += am_encode( &buffer[1], fReg1 );
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
            isize += am_encode( &buffer[1], (eRegister)0 );
        }
        else if (kfmSize16 & fFormat)
        {
            buffer[0] = 0xFF;
            isize += am_encode( &buffer[1], (eRegister)0 );
        }
        else
        {
            buffer[0] = 0xFF;
            isize += am_encode( &buffer[1], (eRegister)6 );
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

        isize += am_encode( &buffer[1], (eRegister)7 );
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

    isize += am_encode( &buffer[1], fReg1 );
    return isize;
}


int CAnd::generate_opcode( uchar *buffer )
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

        isize += am_encode( &buffer[1], (eRegister)4 );
        return isize;
    }
    else if (format == kfrm32_r32)
    {
        if (fFormat & kfmSize8)
            buffer[0] = 0x20;
        else
            buffer[0] = 0x21;
    }
    else if (format == kfr32_rm32)
    {
        if (fFormat & kfmSize8)
            buffer[0] = 0x22;
        else
            buffer[0] = 0x23;
    }

    isize += am_encode( &buffer[1], fReg1 );
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

        isize += am_encode( &buffer[1], (eRegister)6 );
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

    isize += am_encode( &buffer[1], fReg1 );
    return isize;
}


void CAdd::optimize()
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


int CAdd::generate_opcode( uchar *buffer )
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

        isize += am_encode( &buffer[1], (eRegister)0 );
        return isize;
    }
    else if (format == kfrm32_r32)
    {
        if (fFormat & kfmSize8)
            buffer[0] = 0x00;
        else
            buffer[0] = 0x01;
    }
    else if (format == kfr32_rm32)
    {
        if (fFormat & kfmSize8)
            buffer[0] = 0x02;
        else
            buffer[0] = 0x03;
    }

    isize += am_encode( &buffer[1], fReg1 );
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
            isize += am_encode( &buffer[1], (eRegister)5 );
            return isize;
        }
        else
        {
            if (fFormat | kfmImm8)
                buffer[0] = 0x83;
            else
                buffer[0] = 0x81;
            isize += am_encode( &buffer[1], (eRegister)5 );
            return isize;
        }
    }

    return isize;
}


int CJmp::generate_opcode( uchar *buffer )
{
    int     isize = 1;

    if (fCond == kjt)
    {
        buffer[0] = 0xEB;
    }
    else if (fCond == kjcxz)
    {
        buffer[0] = 0xE3;
    }
    else
    {
        buffer[0] = fCond | 0x70;
    }

    buffer[1] = fImm32;
    isize++;

    return isize;
}



//*********************************** Mneumonic Outputers *********************


void CInstruction::output_text( void )
{
    if (fInstr == kunknown) return;


    cout << instr_name[fInstr] << "\t";

    eFormat format = (eFormat)(kBaseFormatMask & (int)fFormat);
    switch (format)
    {
        case kfreg:
            cout << reg_name[fReg1];
            break;
        case kfrm32:
            if (fDisp32) cout << fDisp32;
            if (fFormat & kfmDeref)  cout << "(";
            cout << reg_name[fReg2];
            if (fFormat & kfmDeref)  cout << ")";
            break;
        case kfrm32_r32:
            if (fDisp32) cout << fDisp32;
            if (fFormat & kfmDeref)  cout << "(";
            cout << reg_name[fReg2];
            if (fFormat & kfmDeref)  cout << ")";
            cout << ", ";
            cout << reg_name[fReg1];
            break;
        case kfr32_rm32:
            cout << reg_name[fReg1] << ", ";
            if (fDisp32) cout << fDisp32;
            if (fFormat & kfmDeref)  cout << "(";
			if (fReg2 != kNoReg)
	            cout << reg_name[fReg2];
			else
				cout.form( "0x%08X", (long)(bswap_32(fDisp32)) );
            if (fFormat & kfmDeref)  cout << ")";
            break;
    }

    if (fFormat & kfmImm8)  cout.form( ", #0x%02X", fImm32 );
    if (fFormat & kfmImm32)  cout.form( ", #0x%08X", fImm32 );

    cout << "\n";
}


void CJmp::output_text( void )
{
    cout << "j" << cond_name[fCond];
//    cout.form( "\t.+0x%02X\n", fImm32 );
    cout.form( "\t.+%ld\n", fImm32 );
}


//*********************  the rest of the code  ********************************


int hexdump( CInstruction *instr )
{
    uchar   buffer[16];
    int     instrSize = instr->generate_opcode( buffer );
    if (instrSize)
    {
        for (int i=0; i<instrSize; i++)
        {
            cout.form("%02x ", buffer[i]);
        }
        cout << "\t";
    }

    return instrSize;
}


CInstruction* get_2byte_instruction( uchar *pCode )
{
    CInstruction    *retInstr=NULL;
    uchar           *reg = pCode+1;

	switch (*pCode)
	{
		case 0xBE:
            retInstr = new CMov( reg, (eFormat)(kfr32_rm32 | kfmSize8) );
			break;
		case 0xBF:
            retInstr = new CMov( reg, (eFormat)(kfr32_rm32 | kfmSize16) );
			break;
	}

// ek only until the table above is completed
    if (retInstr == NULL)   retInstr = new CInstruction();

	retInstr->fSize++;
	return retInstr;
}


CInstruction* get_next_instruction( uchar *pCode )
{
    CInstruction    *retInstr=NULL;
    uchar           *reg = pCode+1;

    switch (*pCode)
    {
        case 0x00:
            retInstr = new CAdd( reg, (eFormat)(kfrm32_r32 | kfmSize8) );
            break;
        case 0x01:
            retInstr = new CAdd( reg, kfrm32_r32 );
            break;
        case 0x02:
            retInstr = new CAdd( reg, (eFormat)(kfr32_rm32 | kfmSize8) );
            break;
        case 0x03:
            retInstr = new CAdd( reg, kfr32_rm32 );
            break;
        case 0x06:
            retInstr = new CPush( kes );
            break;
		case 0x0F:
			retInstr = get_2byte_instruction( reg );
			break;
        case 0x25:
            retInstr = new CAnd( reg, kfr32_rm32 );
            break;
        case 0x28:
            retInstr = new CSub( reg, (eFormat)(kfrm32_r32 | kfmSize8) );
            break;
        case 0x29:
            retInstr = new CSub( reg, kfrm32_r32 );
            break;
        case 0x2A:
            retInstr = new CSub( reg, (eFormat)(kfr32_rm32 | kfmSize8) );
            break;
        case 0x2B:
            retInstr = new CSub( reg, kfr32_rm32 );
            break;
        case 0x31:
            retInstr = new CXor( reg, kfrm32_r32 );
            break;
        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43:
        case 0x44:
        case 0x45:
        case 0x46:
        case 0x47:
            retInstr = new CInc( (eRegister)(*pCode & 0x07) );
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
        case 0x6A:
            retInstr = new CPush( (char)(*reg) );
            break;
        case 0x70:
        case 0x71:
        case 0x72:
        case 0x73:
        case 0x74:
        case 0x75:
        case 0x76:
        case 0x77:
        case 0x78:
        case 0x79:
        case 0x7A:
        case 0x7B:
        case 0x7C:
        case 0x7D:
        case 0x7E:
        case 0x7F:
            retInstr = new CJmp( (eCond)(*pCode - 0x70), (char)*reg );
            break;
        case 0x80:
            switch (DIGIT_MAP[*reg])
            {
                case 0:
                    retInstr = new CAdd( reg, (eFormat)(kfrm32 | kfmImm8 | kfmSize8) );
                    break;
                case 5:
                    retInstr = new CSub( reg, (eFormat)(kfrm32 | kfmImm8 | kfmSize8) );
                    break;
                case 7:
                    retInstr = new CCmp( reg, (eFormat)(kfrm32 | kfmImm8 | kfmSize8) );
                    break;
                default:
//                    retInstr = am_rm32_imm8( kunknown, reg );
                    break;
            }
            break;
        case 0x81:
            switch (DIGIT_MAP[*reg])
            {
                case 0:
                    retInstr = new CAdd( reg, (eFormat)(kfrm32 | kfmImm32) );
                    break;
                case 5:
                    retInstr = new CSub( reg, (eFormat)(kfrm32 | kfmImm32) );
                    break;
                case 7:
                    retInstr = new CCmp( reg, (eFormat)(kfrm32 | kfmImm32) );
                    break;
                default:
//                    retInstr = am_rm32_imm8( kunknown, reg );
                    break;
            }
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
        case 0x8D:
            retInstr = new CLea( reg,  kfr32_rm32 );
            break;
        case 0x90:
            retInstr = new CNop();
            break;
        case 0xB8:
        case 0xB9:
        case 0xBA:
        case 0xBB:
        case 0xBC:
        case 0xBD:
        case 0xBE:
        case 0xBF:
            retInstr = new CMov( (eRegister)(*pCode & 0x07), reg, (eFormat)(kfrm32 | kfmImm32) );
            break;
        case 0xC3:
            retInstr = new CRet();
            break;
        case 0xC7:
            retInstr = new CMov( reg, (eFormat)(kfrm32 | kfmImm32) );
            break;


        case 0xE8:
            retInstr = new CCall( (long)bswap_32( *(long*)reg ));
            break;
        case 0xEB:
            retInstr = new CJmp( kjt, *reg );
            break;
        case 0xFF:
            switch (DIGIT_MAP[*reg])
            {
                case 0:
                    retInstr = new CInc( reg, (eFormat)(kfrm32 | kfmSize16) );
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
    if (retInstr == NULL)
    {
        if (!gfirst)
        {
            cout << (unsigned long)*pCode << " was the first\n";
            cout << (unsigned long)*(pCode+1) << "\n";
        }
        gfirst = true;
        retInstr = new CInstruction();
    }

    return retInstr;
}


//*************************  Function Level Code ******************************


/*
* Takes the compile intel code and tries to optimize it.
*
*   returns the number of bytes smaller the new code is.
*/

void CFunction::get_instructions( uchar *pCode, long codeSize )
{
    long            saved=0;

    while (codeSize > 0)
    {
        CInstruction    *instr = get_next_instruction( pCode );
        int instrSize = instr->fSize;

        pCode += instrSize;
        codeSize -= instrSize;

        fInstructions.push_back( instr );
    }
}

void CFunction::remove_nops( void )
{
    for( list<CInstruction*>::iterator    p = fInstructions.begin();
        p != fInstructions.end(); ++p )
    {
        if ( (*p)->fInstr == knop )
        {
            list<CInstruction*>::iterator   s = p;
            --p;
            delete *s;
            fInstructions.erase(s);
        }
    }
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
cout << name << " size = " << (shdrs+i)->sh_size << "\n";
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
    long newSize = 0, oldSize = 0;
    long    numUnknowns=0;
    for (int i = 0; i < nentries; ++i) {
        const Elf32_Sym* sym = symtab + i;
        if ( sym->st_shndx == textndx && sym->st_size)
        {
//            basic_string<char> funcname(sym->st_name + strtab, sym->st_size);
            string      funcname(sym->st_name + strtab, sym->st_size);
            basic_string<char> functext(text + sym->st_value - textaddr, sym->st_size);

            CFunction *func = new CFunction( funcname );

            if (gDebug) cout << funcname << "\n\n";
            if (gDebug) (void)hexdump(cout,functext.data(),sym->st_size);
            oldSize += sym->st_size;
            func->get_instructions( (unsigned char *)functext.data(), sym->st_size );

            if (gOptimize)
            {
                for( list<CInstruction*>::iterator    p = func->fInstructions.begin();
                    p != func->fInstructions.end(); ++p )
                {
                     (*p)->optimize();
                }
            }

            if (gCompact) func->remove_nops();

            for( list<CInstruction*>::iterator    p = func->fInstructions.begin();
                p != func->fInstructions.end(); ++p )
            {
                //if ( (*p)->fInstr == kunknown)  numUnknowns++;
                uchar   buffer[16];
                int     instrSize = (*p)->generate_opcode( buffer );
                if (instrSize == 0)  numUnknowns++;
                newSize += instrSize;

                if (gAssembly)
                {
                    (void)hexdump( *p );
                    (*p)->output_text();
                }

                delete *p;
            }
            delete func;
        }
    }

    cout << "Code size reduction of " << oldSize-newSize -numUnknowns << " bytes out of "
         << oldSize << " bytes.\n";
    if (numUnknowns)    cout << "*** Unknowns found:  " << numUnknowns << "\n";
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
		char c = getopt( argc, argv, "dcoa" );
		if (c == -1)	break;

		switch (c)
		{
			case 'a':
				gAssembly = true;
                break;
			case 'c':
				gCompact = true;
				cout << "Compacting Dead Code ON\n";
				break;
			case 'd':
				gDebug = true;
				cout << "Debugging Info ON\n";
				break;
			case 'o':
				gOptimize = true;
				cout << "Instrustion Optimization ON\n";
				break;
		}
	}

    for (int i = optind; i < argc; ++i)
        process_file(argv[i]);

    return 0;
}
