/* pnggccrd.c - mixed C/assembler version of utilities to read a PNG file
 *
 * For Intel x86 CPU (Pentium-MMX or later) and GNU C compiler.
 *
 *     See http://www.intel.com/drg/pentiumII/appnotes/916/916.htm
 *     and http://www.intel.com/drg/pentiumII/appnotes/923/923.htm
 *     for Intel's performance analysis of the MMX vs. non-MMX code.
 *
 * libpng version 1.0.8 - July 24, 2000
 * For conditions of distribution and use, see copyright notice in png.h
 * Copyright (c) 1998, 1999, 2000 Glenn Randers-Pehrson
 * Copyright (c) 1998, Intel Corporation
 *
 * Based on MSVC code contributed by Nirav Chhatrapati, Intel Corp., 1998.
 * Interface to libpng contributed by Gilles Vollant, 1999.
 * GNU C port by Greg Roelofs, 1999.
 *
 * Lines 2350-4300 converted in place with intel2gas 1.3.1:
 *
 *   intel2gas -mdI pnggccrd.c.partially-msvc -o pnggccrd.c
 *
 * and then cleaned up by hand.  See http://hermes.terminal.at/intel2gas/ .
 *
 * NOTE:  A sufficiently recent version of GNU as (or as.exe under DOS/Windows)
 *        is required to assemble the newer MMX instructions such as movq.
 *        For djgpp, see
 *
 *           ftp://ftp.simtel.net/pub/simtelnet/gnu/djgpp/v2gnu/bnu281b.zip
 *
 *        (or a later version in the same directory).  For Linux, check your
 *        distribution's web site(s) or try these links:
 *
 *           http://rufus.w3.org/linux/RPM/binutils.html
 *           http://www.debian.org/Packages/stable/devel/binutils.html
 *           ftp://ftp.slackware.com/pub/linux/slackware/slackware/slakware/d1/
 *             binutils.tgz
 *
 *        For other platforms, see the main GNU site:
 *
 *           ftp://ftp.gnu.org/pub/gnu/binutils/
 *
 *        Version 2.5.2l.15 is definitely too old...
 */

/*
 * NOTES (mostly by Greg Roelofs)
 * =====
 *
 * 19991006:
 *  - fixed sign error in post-MMX cleanup code (16- & 32-bit cases)
 *
 * 19991007:
 *  - additional optimizations (possible or definite):
 *     x [DONE] write MMX code for 64-bit case (pixel_bytes == 8) [not tested]
 *     - write MMX code for 48-bit case (pixel_bytes == 6)
 *     - figure out what's up with 24-bit case (pixel_bytes == 3):
 *        why subtract 8 from width_mmx in the pass 4/5 case?
 *        (only width_mmx case)
 *     x [DONE] replace pixel_bytes within each block with the true
 *        constant value (or are compilers smart enough to do that?)
 *     - rewrite all MMX interlacing code so it's aligned with
 *        the *beginning* of the row buffer, not the end.  This
 *        would not only allow one to eliminate half of the memory
 *        writes for odd passes (i.e., pass == odd), it may also
 *        eliminate some unaligned-data-access exceptions (assuming
 *        there's a penalty for not aligning 64-bit accesses on
 *        64-bit boundaries).  The only catch is that the "leftover"
 *        pixel(s) at the end of the row would have to be saved,
 *        but there are enough unused MMX registers in every case,
 *        so this is not a problem.  A further benefit is that the
 *        post-MMX cleanup code (C code) in at least some of the
 *        cases could be done within the assembler block.
 *  x [DONE] the "v3 v2 v1 v0 v7 v6 v5 v4" comments are confusing,
 *     inconsistent, and don't match the MMX Programmer's Reference
 *     Manual conventions anyway.  They should be changed to
 *     "b7 b6 b5 b4 b3 b2 b1 b0," where b0 indicates the byte that
 *     was lowest in memory (e.g., corresponding to a left pixel)
 *     and b7 is the byte that was highest (e.g., a right pixel).
 *
 * 19991016:
 *  - Brennan's Guide notwithstanding, gcc under Linux does *not*
 *     want globals prefixed by underscores when referencing them--
 *     i.e., if the variable is const4, then refer to it as const4,
 *     not _const4.  This seems to be a djgpp-specific requirement.
 *     Also, such variables apparently *must* be declared outside
 *     of functions; neither static nor automatic variables work if
 *     defined within the scope of a single function, but both
 *     static and truly global (multi-module) variables work fine.
 *
 * 19991023:
 *  - fixed png_combine_row() non-MMX replication bug (odd passes only?)
 *  - switched from string-concatenation-with-macros to cleaner method of
 *     renaming global variables for djgpp--i.e., always use prefixes in
 *     inlined assembler code (== strings) and conditionally rename the
 *     variables, not the other way around.  Hence _const4, _mask8_0, etc.
 *
 * 19991024:
 *  - fixed mmxsupport()/png_do_interlace() first-row bug
 *     This one was severely weird:  even though mmxsupport() doesn't touch
 *     ebx (where "row" pointer was stored), it nevertheless managed to zero
 *     the register (even in static/non-fPIC code--see below), which in turn
 *     caused png_do_interlace() to return prematurely on the first row of
 *     interlaced images (i.e., without expanding the interlaced pixels).
 *     Inspection of the generated assembly code didn't turn up any clues,
 *     although it did point at a minor optimization (i.e., get rid of
 *     mmx_supported_local variable and just use eax).  Possibly the CPUID
 *     instruction is more destructive than it looks?  (Not yet checked.)
 *  - "info gcc" was next to useless, so compared fPIC and non-fPIC assembly
 *     listings...  Apparently register spillage has to do with ebx, since
 *     it's used to index the global offset table.  Commenting it out of the
 *     input-reg lists in png_combine_row() eliminated compiler barfage, so
 *     ifdef'd with __PIC__ macro:  if defined, use a global for unmask
 *
 * 19991107:
 *  - verified CPUID clobberage:  12-char string constant ("GenuineIntel",
 *     "AuthenticAMD", etc.) placed in EBX:ECX:EDX.  Still need to polish.
 *
 * 19991120:
 *  - made "diff" variable (now "_dif") global to simplify conversion of
 *     filtering routines (running out of regs, sigh).  "diff" is still used
 *     in interlacing routines, however.
 *  - fixed up both versions of mmxsupport() (ORIG_THAT_USED_TO_CLOBBER_EBX
 *     macro determines which is used); original not yet tested.
 *
 * 20000213:
 *  - When compiling with gcc, be sure to use  -fomit-frame-pointer
 *
 * 20000319:
 *  - fixed a register-name typo in png_do_read_interlace(), default (MMX) case,
 *     pass == 4 or 5, that caused visible corruption of interlaced images
 *
 * 20000623:
 *  -  Various problems were reported with gcc 2.95.2 in the Cygwin environment,
 *     many of the form "forbidden register 0 (ax) was spilled for class AREG."
 *     This is explained at http://gcc.gnu.org/fom_serv/cache/23.html, and
 *     Chuck Wilson supplied a patch involving dummy output registers.  See
 *     http://sourceforge.net/bugs/?func=detailbug&bug_id=108741&group_id=5624
 *     for the original (anonymous) SourceForge bug report.
 *
 * 20000706:
 *  - Chuck Wilson passed along these remaining gcc 2.95.2 errors:
 *       pnggccrd.c: In function `png_combine_row':
 *       pnggccrd.c:525: more than 10 operands in `asm'
 *       pnggccrd.c:669: more than 10 operands in `asm'
 *       pnggccrd.c:828: more than 10 operands in `asm'
 *       pnggccrd.c:994: more than 10 operands in `asm'
 *       pnggccrd.c:1177: more than 10 operands in `asm'
 *     They are all the same problem and can be worked around by using the
 *     global _unmask variable unconditionally, not just in the -fPIC case.
 *     Apparently earlier versions of gcc also have the problem with more than
 *     10 operands; they just don't report it.  Much strangeness ensues, etc.
 */

#define PNG_INTERNAL
#include "png.h"

#if defined(PNG_ASSEMBLER_CODE_SUPPORTED) && defined(PNG_USE_PNGGCCRD)

int mmxsupport(void);

static int mmx_supported = 2;

#ifdef PNG_USE_LOCAL_ARRAYS
static const int png_pass_start[7] = {0, 4, 0, 2, 0, 1, 0};
static const int png_pass_inc[7]   = {8, 8, 4, 4, 2, 2, 1};
static const int png_pass_width[7] = {8, 4, 4, 2, 2, 1, 1};
#endif

// djgpp, Win32, and Cygwin add their own underscores to global variables,
// so define them without:
#if defined(__DJGPP__) || defined(WIN32) || defined(__CYGWIN__)
#  define _unmask      unmask
#  define _const4      const4
#  define _const6      const6
#  define _mask8_0     mask8_0  
#  define _mask16_1    mask16_1 
#  define _mask16_0    mask16_0 
#  define _mask24_2    mask24_2 
#  define _mask24_1    mask24_1 
#  define _mask24_0    mask24_0 
#  define _mask32_3    mask32_3 
#  define _mask32_2    mask32_2 
#  define _mask32_1    mask32_1 
#  define _mask32_0    mask32_0 
#  define _mask48_5    mask48_5 
#  define _mask48_4    mask48_4 
#  define _mask48_3    mask48_3 
#  define _mask48_2    mask48_2 
#  define _mask48_1    mask48_1 
#  define _mask48_0    mask48_0 
#  define _FullLength  FullLength
#  define _MMXLength   MMXLength
#  define _dif         dif
#endif

/* These constants are used in the inlined MMX assembly code.
   Ignore gcc's "At top level: defined but not used" warnings. */

/* GRR 20000706:  originally _unmask was needed only when compiling with -fPIC,
 *  since that case uses the %ebx register for indexing the Global Offset Table
 *  and there were no other registers available.  But gcc 2.95 and later emit
 *  "more than 10 operands in `asm'" errors when %ebx is used to preload unmask
 *  in the non-PIC case, so we'll just use the global unconditionally now.
 */
static int _unmask;

static unsigned long long _mask8_0  = 0x0102040810204080LL;

static unsigned long long _mask16_1 = 0x0101020204040808LL;
static unsigned long long _mask16_0 = 0x1010202040408080LL;

static unsigned long long _mask24_2 = 0x0101010202020404LL;
static unsigned long long _mask24_1 = 0x0408080810101020LL;
static unsigned long long _mask24_0 = 0x2020404040808080LL;

static unsigned long long _mask32_3 = 0x0101010102020202LL;
static unsigned long long _mask32_2 = 0x0404040408080808LL;
static unsigned long long _mask32_1 = 0x1010101020202020LL;
static unsigned long long _mask32_0 = 0x4040404080808080LL;

static unsigned long long _mask48_5 = 0x0101010101010202LL;
static unsigned long long _mask48_4 = 0x0202020204040404LL;
static unsigned long long _mask48_3 = 0x0404080808080808LL;
static unsigned long long _mask48_2 = 0x1010101010102020LL;
static unsigned long long _mask48_1 = 0x2020202040404040LL;
static unsigned long long _mask48_0 = 0x4040808080808080LL;

static unsigned long long _const4   = 0x0000000000FFFFFFLL;
//static unsigned long long _const5 = 0x000000FFFFFF0000LL;     // NOT USED
static unsigned long long _const6   = 0x00000000000000FFLL;

// These are used in the row-filter routines and should/would be local
//  variables if not for gcc addressing limitations.

static png_uint_32  _FullLength;
static png_uint_32  _MMXLength;
static int          _dif;


void /* PRIVATE */
png_read_filter_row_c(png_structp png_ptr, png_row_infop row_info,
   png_bytep row, png_bytep prev_row, int filter);


#if defined(PNG_HAVE_ASSEMBLER_COMBINE_ROW)

/* Combines the row recently read in with the previous row.
   This routine takes care of alpha and transparency if requested.
   This routine also handles the two methods of progressive display
   of interlaced images, depending on the mask value.
   The mask value describes which pixels are to be combined with
   the row.  The pattern always repeats every 8 pixels, so just 8
   bits are needed.  A one indicates the pixel is to be combined; a
   zero indicates the pixel is to be skipped.  This is in addition
   to any alpha or transparency value associated with the pixel.
   If you want all pixels to be combined, pass 0xff (255) in mask. */

/* Use this routine for the x86 platform - it uses a faster MMX routine
   if the machine supports MMX. */

void /* PRIVATE */
png_combine_row(png_structp png_ptr, png_bytep row, int mask)
{
   png_debug(1,"in png_combine_row_asm\n");

   if (mmx_supported == 2)
       mmx_supported = mmxsupport();

/*
fprintf(stderr, "GRR DEBUG:  png_combine_row() pixel_depth = %d, mask = 0x%02x, unmask = 0x%02x\n", png_ptr->row_info.pixel_depth, mask, ~mask);
fflush(stderr);
 */
   if (mask == 0xff)
   {
      png_memcpy(row, png_ptr->row_buf + 1,
       (png_size_t)((png_ptr->width * png_ptr->row_info.pixel_depth + 7) >> 3));
   }
   /* GRR:  add "else if (mask == 0)" case?
    *       or does png_combine_row() not even get called in that case? */
   else
   {
      switch (png_ptr->row_info.pixel_depth)
      {
         case 1:        // png_ptr->row_info.pixel_depth
         {
            png_bytep sp;
            png_bytep dp;
            int s_inc, s_start, s_end;
            int m;
            int shift;
            png_uint_32 i;

            sp = png_ptr->row_buf + 1;
            dp = row;
            m = 0x80;
#if defined(PNG_READ_PACKSWAP_SUPPORTED)
            if (png_ptr->transformations & PNG_PACKSWAP)
            {
                s_start = 0;
                s_end = 7;
                s_inc = 1;
            }
            else
#endif
            {
                s_start = 7;
                s_end = 0;
                s_inc = -1;
            }

            shift = s_start;

            for (i = 0; i < png_ptr->width; i++)
            {
               if (m & mask)
               {
                  int value;

                  value = (*sp >> shift) & 0x1;
                  *dp &= (png_byte)((0x7f7f >> (7 - shift)) & 0xff);
                  *dp |= (png_byte)(value << shift);
               }

               if (shift == s_end)
               {
                  shift = s_start;
                  sp++;
                  dp++;
               }
               else
                  shift += s_inc;

               if (m == 1)
                  m = 0x80;
               else
                  m >>= 1;
            }
            break;
         }

         case 2:        // png_ptr->row_info.pixel_depth
         {
            png_bytep sp;
            png_bytep dp;
            int s_start, s_end, s_inc;
            int m;
            int shift;
            png_uint_32 i;
            int value;

            sp = png_ptr->row_buf + 1;
            dp = row;
            m = 0x80;
#if defined(PNG_READ_PACKSWAP_SUPPORTED)
            if (png_ptr->transformations & PNG_PACKSWAP)
            {
               s_start = 0;
               s_end = 6;
               s_inc = 2;
            }
            else
#endif
            {
               s_start = 6;
               s_end = 0;
               s_inc = -2;
            }

            shift = s_start;

            for (i = 0; i < png_ptr->width; i++)
            {
               if (m & mask)
               {
                  value = (*sp >> shift) & 0x3;
                  *dp &= (png_byte)((0x3f3f >> (6 - shift)) & 0xff);
                  *dp |= (png_byte)(value << shift);
               }

               if (shift == s_end)
               {
                  shift = s_start;
                  sp++;
                  dp++;
               }
               else
                  shift += s_inc;
               if (m == 1)
                  m = 0x80;
               else
                  m >>= 1;
            }
            break;
         }

         case 4:        // png_ptr->row_info.pixel_depth
         {
            png_bytep sp;
            png_bytep dp;
            int s_start, s_end, s_inc;
            int m;
            int shift;
            png_uint_32 i;
            int value;

            sp = png_ptr->row_buf + 1;
            dp = row;
            m = 0x80;
#if defined(PNG_READ_PACKSWAP_SUPPORTED)
            if (png_ptr->transformations & PNG_PACKSWAP)
            {
               s_start = 0;
               s_end = 4;
               s_inc = 4;
            }
            else
#endif
            {
               s_start = 4;
               s_end = 0;
               s_inc = -4;
            }
            shift = s_start;

            for (i = 0; i < png_ptr->width; i++)
            {
               if (m & mask)
               {
                  value = (*sp >> shift) & 0xf;
                  *dp &= (png_byte)((0xf0f >> (4 - shift)) & 0xff);
                  *dp |= (png_byte)(value << shift);
               }

               if (shift == s_end)
               {
                  shift = s_start;
                  sp++;
                  dp++;
               }
               else
                  shift += s_inc;
               if (m == 1)
                  m = 0x80;
               else
                  m >>= 1;
            }
            break;
         }

         case 8:        // png_ptr->row_info.pixel_depth
         {
            png_bytep srcptr;
            png_bytep dstptr;

            if (mmx_supported)
            {
               png_uint_32 len;
               int diff;
               int dummy_value_a;   // fix 'forbidden register spilled' error
               int dummy_value_d;
               int dummy_value_c;
               int dummy_value_S;
               int dummy_value_D;
               _unmask = ~mask;            // global variable for -fPIC version
               srcptr = png_ptr->row_buf + 1;
               dstptr = row;
               len  = png_ptr->width &~7;  // reduce to multiple of 8
               diff = png_ptr->width & 7;  // amount lost

               __asm__ __volatile__ (
                  "movd      _unmask, %%mm7  \n\t" // load bit pattern
                  "psubb     %%mm6, %%mm6    \n\t" // zero mm6
                  "punpcklbw %%mm7, %%mm7    \n\t"
                  "punpcklwd %%mm7, %%mm7    \n\t"
                  "punpckldq %%mm7, %%mm7    \n\t" // fill reg with 8 masks

                  "movq      _mask8_0, %%mm0 \n\t"
                  "pand      %%mm7, %%mm0    \n\t" // nonzero if keep byte
                  "pcmpeqb   %%mm6, %%mm0    \n\t" // zeros->1s, v versa

// preload        "movl      len, %%ecx      \n\t" // load length of line
// preload        "movl      srcptr, %%esi   \n\t" // load source
// preload        "movl      dstptr, %%edi   \n\t" // load dest

                  "cmpl      $0, %%ecx       \n\t" // len == 0 ?
                  "je        mainloop8end    \n\t"

                "mainloop8:                  \n\t"
                  "movq      (%%esi), %%mm4  \n\t" // *srcptr
                  "pand      %%mm0, %%mm4    \n\t"
                  "movq      %%mm0, %%mm6    \n\t"
                  "pandn     (%%edi), %%mm6  \n\t" // *dstptr
                  "por       %%mm6, %%mm4    \n\t"
                  "movq      %%mm4, (%%edi)  \n\t"
                  "addl      $8, %%esi       \n\t" // inc by 8 bytes processed
                  "addl      $8, %%edi       \n\t"
                  "subl      $8, %%ecx       \n\t" // dec by 8 pixels processed
                  "ja        mainloop8       \n\t"

                "mainloop8end:               \n\t"
// preload        "movl      diff, %%ecx     \n\t" // (diff is in eax)
                  "movl      %%eax, %%ecx    \n\t"
                  "cmpl      $0, %%ecx       \n\t"
                  "jz        end8            \n\t"
// preload        "movl      mask, %%edx     \n\t"
                  "sall      $24, %%edx      \n\t" // make low byte, high byte

                "secondloop8:                \n\t"
                  "sall      %%edx           \n\t" // move high bit to CF
                  "jnc       skip8           \n\t" // if CF = 0
                  "movb      (%%esi), %%al   \n\t"
                  "movb      %%al, (%%edi)   \n\t"

                "skip8:                      \n\t"
                  "incl      %%esi           \n\t"
                  "incl      %%edi           \n\t"
                  "decl      %%ecx           \n\t"
                  "jnz       secondloop8     \n\t"

                "end8:                       \n\t"
                  "EMMS                      \n\t"  // DONE

                  : "=a" (dummy_value_a),           // output regs (dummy)
                    "=d" (dummy_value_d),
                    "=c" (dummy_value_c),
                    "=S" (dummy_value_S),
                    "=D" (dummy_value_D)

                  : "3" (srcptr),      // esi       // input regs
                    "4" (dstptr),      // edi
                    "0" (diff),        // eax
// was (unmask)     "b"    RESERVED    // ebx       // Global Offset Table idx
                    "2" (len),         // ecx
                    "1" (mask)         // edx

//                  :          // clobber list
#if 0  /* MMX regs (%mm0, etc.) not supported by gcc 2.7.2.3 or egcs 1.1 */
                  : "%mm0", "%mm4", "%mm6", "%mm7"
#endif
               );
            }
            else /* mmx _not supported - Use modified C routine */
            {
               register png_uint_32 i;
               png_uint_32 initial_val = png_pass_start[png_ptr->pass];
                 // png.c:  png_pass_start[] = {0, 4, 0, 2, 0, 1, 0};
               register int stride = png_pass_inc[png_ptr->pass];
                 // png.c:  png_pass_inc[] = {8, 8, 4, 4, 2, 2, 1};
               register int rep_bytes = png_pass_width[png_ptr->pass];
                 // png.c:  png_pass_width[] = {8, 4, 4, 2, 2, 1, 1};
               register png_uint_32 final_val = png_ptr->width;

               srcptr = png_ptr->row_buf + 1 + initial_val;
               dstptr = row + initial_val;

               for (i = initial_val; i < final_val; i += stride)
               {
                  png_memcpy(dstptr, srcptr, rep_bytes);
                  srcptr += stride;
                  dstptr += stride;
               } 
            } /* end of else */

            break;
         }       // end 8 bpp

         case 16:       // png_ptr->row_info.pixel_depth
         {
            png_bytep srcptr;
            png_bytep dstptr;

            if (mmx_supported)
            {
               png_uint_32 len;
               int diff;
               int dummy_value_a;   // fix 'forbidden register spilled' error
               int dummy_value_d;
               int dummy_value_c;
               int dummy_value_S;
               int dummy_value_D;
               _unmask = ~mask;            // global variable for -fPIC version
               srcptr = png_ptr->row_buf + 1;
               dstptr = row;
               len  = png_ptr->width &~7;  // reduce to multiple of 8
               diff = png_ptr->width & 7;  // amount lost

               __asm__ __volatile__ (
                  "movd      _unmask, %%mm7   \n\t" // load bit pattern
                  "psubb     %%mm6, %%mm6     \n\t" // zero mm6
                  "punpcklbw %%mm7, %%mm7     \n\t"
                  "punpcklwd %%mm7, %%mm7     \n\t"
                  "punpckldq %%mm7, %%mm7     \n\t" // fill reg with 8 masks

                  "movq      _mask16_0, %%mm0 \n\t"
                  "movq      _mask16_1, %%mm1 \n\t"

                  "pand      %%mm7, %%mm0     \n\t"
                  "pand      %%mm7, %%mm1     \n\t"

                  "pcmpeqb   %%mm6, %%mm0     \n\t"
                  "pcmpeqb   %%mm6, %%mm1     \n\t"

// preload        "movl      len, %%ecx       \n\t" // load length of line
// preload        "movl      srcptr, %%esi    \n\t" // load source
// preload        "movl      dstptr, %%edi    \n\t" // load dest

                  "cmpl      $0, %%ecx        \n\t"
                  "jz        mainloop16end    \n\t"

                "mainloop16:                  \n\t"
                  "movq      (%%esi), %%mm4   \n\t"
                  "pand      %%mm0, %%mm4     \n\t"
                  "movq      %%mm0, %%mm6     \n\t"
                  "movq      (%%edi), %%mm7   \n\t"
                  "pandn     %%mm7, %%mm6     \n\t"
                  "por       %%mm6, %%mm4     \n\t"
                  "movq      %%mm4, (%%edi)   \n\t"

                  "movq      8(%%esi), %%mm5  \n\t"
                  "pand      %%mm1, %%mm5     \n\t"
                  "movq      %%mm1, %%mm7     \n\t"
                  "movq      8(%%edi), %%mm6  \n\t"
                  "pandn     %%mm6, %%mm7     \n\t"
                  "por       %%mm7, %%mm5     \n\t"
                  "movq      %%mm5, 8(%%edi)  \n\t"

                  "addl      $16, %%esi       \n\t" // inc by 16 bytes processed
                  "addl      $16, %%edi       \n\t"
                  "subl      $8, %%ecx        \n\t" // dec by 8 pixels processed
                  "ja        mainloop16       \n\t"

                "mainloop16end:               \n\t"
// preload        "movl      diff, %%ecx      \n\t" // (diff is in eax)
                  "movl      %%eax, %%ecx     \n\t"
                  "cmpl      $0, %%ecx        \n\t"
                  "jz        end16            \n\t"
// preload        "movl      mask, %%edx      \n\t"
                  "sall      $24, %%edx       \n\t" // make low byte, high byte

                "secondloop16:                \n\t"
                  "sall      %%edx            \n\t" // move high bit to CF
                  "jnc       skip16           \n\t" // if CF = 0
                  "movw      (%%esi), %%ax    \n\t"
                  "movw      %%ax, (%%edi)    \n\t"

                "skip16:                      \n\t"
                  "addl      $2, %%esi        \n\t"
                  "addl      $2, %%edi        \n\t"
                  "decl      %%ecx            \n\t"
                  "jnz       secondloop16     \n\t"

                "end16:                       \n\t"
                  "EMMS                       \n\t" // DONE

                  : "=a" (dummy_value_a),              // output regs (dummy)
                    "=d" (dummy_value_d),
                    "=c" (dummy_value_c),
                    "=S" (dummy_value_S),
                    "=D" (dummy_value_D)

                  : "3" (srcptr),      // esi       // input regs
                    "4" (dstptr),      // edi
                    "0" (diff),        // eax
// was (unmask)     "b"    RESERVED    // ebx       // Global Offset Table idx
                    "2" (len),         // ecx
                    "1" (mask)         // edx

//                  :          // clobber list
#if 0  /* MMX regs (%mm0, etc.) not supported by gcc 2.7.2.3 or egcs 1.1 */
                  : "%mm0", "%mm1",
                    "%mm4", "%mm5", "%mm6", "%mm7"
#endif
               );
            }
            else /* mmx _not supported - Use modified C routine */
            {
               register png_uint_32 i;
               png_uint_32 initial_val = 2 * png_pass_start[png_ptr->pass];
                 // png.c:  png_pass_start[] = {0, 4, 0, 2, 0, 1, 0};
               register int stride = 2 * png_pass_inc[png_ptr->pass];
                 // png.c:  png_pass_inc[] = {8, 8, 4, 4, 2, 2, 1};
               register int rep_bytes = 2 * png_pass_width[png_ptr->pass];
                 // png.c:  png_pass_width[] = {8, 4, 4, 2, 2, 1, 1};
               register png_uint_32 final_val = 2 * png_ptr->width;

               srcptr = png_ptr->row_buf + 1 + initial_val;
               dstptr = row + initial_val;

               for (i = initial_val; i < final_val; i += stride)
               {
                  png_memcpy(dstptr, srcptr, rep_bytes);
                  srcptr += stride;
                  dstptr += stride;
               } 
            } /* end of else */

            break;
         }       // end 16 bpp

         case 24:       // png_ptr->row_info.pixel_depth
         {
            png_bytep srcptr;
            png_bytep dstptr;

            if (mmx_supported)
            {
               png_uint_32 len;
               int diff;
               int dummy_value_a;   // fix 'forbidden register spilled' error
               int dummy_value_d;
               int dummy_value_c;
               int dummy_value_S;
               int dummy_value_D;
               _unmask = ~mask;            // global variable for -fPIC version
               srcptr = png_ptr->row_buf + 1;
               dstptr = row;
               len  = png_ptr->width &~7;  // reduce to multiple of 8
               diff = png_ptr->width & 7;  // amount lost

               __asm__ __volatile__ (
                  "movd      _unmask, %%mm7   \n\t" // load bit pattern
                  "psubb     %%mm6, %%mm6     \n\t" // zero mm6
                  "punpcklbw %%mm7, %%mm7     \n\t"
                  "punpcklwd %%mm7, %%mm7     \n\t"
                  "punpckldq %%mm7, %%mm7     \n\t" // fill reg with 8 masks

                  "movq      _mask24_0, %%mm0 \n\t"
                  "movq      _mask24_1, %%mm1 \n\t"
                  "movq      _mask24_2, %%mm2 \n\t"

                  "pand      %%mm7, %%mm0     \n\t"
                  "pand      %%mm7, %%mm1     \n\t"
                  "pand      %%mm7, %%mm2     \n\t"

                  "pcmpeqb   %%mm6, %%mm0     \n\t"
                  "pcmpeqb   %%mm6, %%mm1     \n\t"
                  "pcmpeqb   %%mm6, %%mm2     \n\t"

// preload        "movl      len, %%ecx       \n\t" // load length of line
// preload        "movl      srcptr, %%esi    \n\t" // load source
// preload        "movl      dstptr, %%edi    \n\t" // load dest

                  "cmpl      $0, %%ecx        \n\t"
                  "jz        mainloop24end    \n\t"

                "mainloop24:                  \n\t"
                  "movq      (%%esi), %%mm4   \n\t"
                  "pand      %%mm0, %%mm4     \n\t"
                  "movq      %%mm0, %%mm6     \n\t"
                  "movq      (%%edi), %%mm7   \n\t"
                  "pandn     %%mm7, %%mm6     \n\t"
                  "por       %%mm6, %%mm4     \n\t"
                  "movq      %%mm4, (%%edi)   \n\t"

                  "movq      8(%%esi), %%mm5  \n\t"
                  "pand      %%mm1, %%mm5     \n\t"
                  "movq      %%mm1, %%mm7     \n\t"
                  "movq      8(%%edi), %%mm6  \n\t"
                  "pandn     %%mm6, %%mm7     \n\t"
                  "por       %%mm7, %%mm5     \n\t"
                  "movq      %%mm5, 8(%%edi)  \n\t"

                  "movq      16(%%esi), %%mm6 \n\t"
                  "pand      %%mm2, %%mm6     \n\t"
                  "movq      %%mm2, %%mm4     \n\t"
                  "movq      16(%%edi), %%mm7 \n\t"
                  "pandn     %%mm7, %%mm4     \n\t"
                  "por       %%mm4, %%mm6     \n\t"
                  "movq      %%mm6, 16(%%edi) \n\t"

                  "addl      $24, %%esi       \n\t" // inc by 24 bytes processed
                  "addl      $24, %%edi       \n\t"
                  "subl      $8, %%ecx        \n\t" // dec by 8 pixels processed

                  "ja        mainloop24       \n\t"

                "mainloop24end:               \n\t"
// preload        "movl      diff, %%ecx      \n\t" // (diff is in eax)
                  "movl      %%eax, %%ecx     \n\t"
                  "cmpl      $0, %%ecx        \n\t"
                  "jz        end24            \n\t"
// preload        "movl      mask, %%edx      \n\t"
                  "sall      $24, %%edx       \n\t" // make low byte, high byte

                "secondloop24:                \n\t"
                  "sall      %%edx            \n\t" // move high bit to CF
                  "jnc       skip24           \n\t" // if CF = 0
                  "movw      (%%esi), %%ax    \n\t"
                  "movw      %%ax, (%%edi)    \n\t"
                  "xorl      %%eax, %%eax     \n\t"
                  "movb      2(%%esi), %%al   \n\t"
                  "movb      %%al, 2(%%edi)   \n\t"

                "skip24:                      \n\t"
                  "addl      $3, %%esi        \n\t"
                  "addl      $3, %%edi        \n\t"
                  "decl      %%ecx            \n\t"
                  "jnz       secondloop24     \n\t"

                "end24:                       \n\t"
                  "EMMS                       \n\t" // DONE

                  : "=a" (dummy_value_a),              // output regs (dummy)
                    "=d" (dummy_value_d),
                    "=c" (dummy_value_c),
                    "=S" (dummy_value_S),
                    "=D" (dummy_value_D)

                  : "3" (srcptr),      // esi       // input regs
                    "4" (dstptr),      // edi
                    "0" (diff),        // eax
// was (unmask)     "b"    RESERVED    // ebx       // Global Offset Table idx
                    "2" (len),         // ecx
                    "1" (mask)         // edx

//                  :          // clobber list
#if 0  /* MMX regs (%mm0, etc.) not supported by gcc 2.7.2.3 or egcs 1.1 */
                  : "%mm0", "%mm1", "%mm2",
                    "%mm4", "%mm5", "%mm6", "%mm7"
#endif
               );
            }
            else /* mmx _not supported - Use modified C routine */
            {
               register png_uint_32 i;
               png_uint_32 initial_val = 3 * png_pass_start[png_ptr->pass];
                 // png.c:  png_pass_start[] = {0, 4, 0, 2, 0, 1, 0};
               register int stride = 3 * png_pass_inc[png_ptr->pass];
                 // png.c:  png_pass_inc[] = {8, 8, 4, 4, 2, 2, 1};
               register int rep_bytes = 3 * png_pass_width[png_ptr->pass];
                 // png.c:  png_pass_width[] = {8, 4, 4, 2, 2, 1, 1};
               register png_uint_32 final_val = 3 * png_ptr->width;

               srcptr = png_ptr->row_buf + 1 + initial_val;
               dstptr = row + initial_val;

               for (i = initial_val; i < final_val; i += stride)
               {
                  png_memcpy(dstptr, srcptr, rep_bytes);
                  srcptr += stride;
                  dstptr += stride;
               } 
            } /* end of else */

            break;
         }       // end 24 bpp

         case 32:       // png_ptr->row_info.pixel_depth
         {
            png_bytep srcptr;
            png_bytep dstptr;

            if (mmx_supported)
            {
               png_uint_32 len;
               int diff;
               int dummy_value_a;   // fix 'forbidden register spilled' error
               int dummy_value_d;
               int dummy_value_c;
               int dummy_value_S;
               int dummy_value_D;
               _unmask = ~mask;            // global variable for -fPIC version
               srcptr = png_ptr->row_buf + 1;
               dstptr = row;
               len  = png_ptr->width &~7;  // reduce to multiple of 8
               diff = png_ptr->width & 7;  // amount lost

               __asm__ __volatile__ (
                  "movd      _unmask, %%mm7   \n\t" // load bit pattern
                  "psubb     %%mm6, %%mm6     \n\t" // zero mm6
                  "punpcklbw %%mm7, %%mm7     \n\t"
                  "punpcklwd %%mm7, %%mm7     \n\t"
                  "punpckldq %%mm7, %%mm7     \n\t" // fill reg with 8 masks

                  "movq      _mask32_0, %%mm0 \n\t"
                  "movq      _mask32_1, %%mm1 \n\t"
                  "movq      _mask32_2, %%mm2 \n\t"
                  "movq      _mask32_3, %%mm3 \n\t"

                  "pand      %%mm7, %%mm0     \n\t"
                  "pand      %%mm7, %%mm1     \n\t"
                  "pand      %%mm7, %%mm2     \n\t"
                  "pand      %%mm7, %%mm3     \n\t"

                  "pcmpeqb   %%mm6, %%mm0     \n\t"
                  "pcmpeqb   %%mm6, %%mm1     \n\t"
                  "pcmpeqb   %%mm6, %%mm2     \n\t"
                  "pcmpeqb   %%mm6, %%mm3     \n\t"

// preload        "movl      len, %%ecx       \n\t" // load length of line
// preload        "movl      srcptr, %%esi    \n\t" // load source
// preload        "movl      dstptr, %%edi    \n\t" // load dest

                  "cmpl      $0, %%ecx        \n\t" // lcr
                  "jz        mainloop32end    \n\t"

                "mainloop32:                  \n\t"
                  "movq      (%%esi), %%mm4   \n\t"
                  "pand      %%mm0, %%mm4     \n\t"
                  "movq      %%mm0, %%mm6     \n\t"
                  "movq      (%%edi), %%mm7   \n\t"
                  "pandn     %%mm7, %%mm6     \n\t"
                  "por       %%mm6, %%mm4     \n\t"
                  "movq      %%mm4, (%%edi)   \n\t"

                  "movq      8(%%esi), %%mm5  \n\t"
                  "pand      %%mm1, %%mm5     \n\t"
                  "movq      %%mm1, %%mm7     \n\t"
                  "movq      8(%%edi), %%mm6  \n\t"
                  "pandn     %%mm6, %%mm7     \n\t"
                  "por       %%mm7, %%mm5     \n\t"
                  "movq      %%mm5, 8(%%edi)  \n\t"

                  "movq      16(%%esi), %%mm6 \n\t"
                  "pand      %%mm2, %%mm6     \n\t"
                  "movq      %%mm2, %%mm4     \n\t"
                  "movq      16(%%edi), %%mm7 \n\t"
                  "pandn     %%mm7, %%mm4     \n\t"
                  "por       %%mm4, %%mm6     \n\t"
                  "movq      %%mm6, 16(%%edi) \n\t"

                  "movq      24(%%esi), %%mm7 \n\t"
                  "pand      %%mm3, %%mm7     \n\t"
                  "movq      %%mm3, %%mm5     \n\t"
                  "movq      24(%%edi), %%mm4 \n\t"
                  "pandn     %%mm4, %%mm5     \n\t"
                  "por       %%mm5, %%mm7     \n\t"
                  "movq      %%mm7, 24(%%edi) \n\t"

                  "addl      $32, %%esi       \n\t" // inc by 32 bytes processed
                  "addl      $32, %%edi       \n\t"
                  "subl      $8, %%ecx        \n\t" // dec by 8 pixels processed
                  "ja        mainloop32       \n\t"

                "mainloop32end:               \n\t"
// preload        "movl      diff, %%ecx      \n\t" // (diff is in eax)
                  "movl      %%eax, %%ecx     \n\t"
                  "cmpl      $0, %%ecx        \n\t"
                  "jz        end32            \n\t"
// preload        "movl      mask, %%edx      \n\t"
                  "sall      $24, %%edx       \n\t" // low byte => high byte

                "secondloop32:                \n\t"
                  "sall      %%edx            \n\t" // move high bit to CF
                  "jnc       skip32           \n\t" // if CF = 0
                  "movl      (%%esi), %%eax   \n\t"
                  "movl      %%eax, (%%edi)   \n\t"

                "skip32:                      \n\t"
                  "addl      $4, %%esi        \n\t"
                  "addl      $4, %%edi        \n\t"
                  "decl      %%ecx            \n\t"
                  "jnz       secondloop32     \n\t"

                "end32:                       \n\t"
                  "EMMS                       \n\t" // DONE

                  : "=a" (dummy_value_a),              // output regs (dummy)
                    "=d" (dummy_value_d),
                    "=c" (dummy_value_c),
                    "=S" (dummy_value_S),
                    "=D" (dummy_value_D)

                  : "3" (srcptr),      // esi       // input regs
                    "4" (dstptr),      // edi
                    "0" (diff),        // eax
// was (unmask)     "b"    RESERVED    // ebx       // Global Offset Table idx
                    "2" (len),         // ecx
                    "1" (mask)         // edx

//                  :          // clobber list
#if 0  /* MMX regs (%mm0, etc.) not supported by gcc 2.7.2.3 or egcs 1.1 */
                  : "%mm0", "%mm1", "%mm2", "%mm3",
                    "%mm4", "%mm5", "%mm6", "%mm7"
#endif
               );
            }
            else /* mmx _not supported - Use modified C routine */
            {
               register png_uint_32 i;
               png_uint_32 initial_val = 4 * png_pass_start[png_ptr->pass];
                 // png.c:  png_pass_start[] = {0, 4, 0, 2, 0, 1, 0};
               register int stride = 4 * png_pass_inc[png_ptr->pass];
                 // png.c:  png_pass_inc[] = {8, 8, 4, 4, 2, 2, 1};
               register int rep_bytes = 4 * png_pass_width[png_ptr->pass];
                 // png.c:  png_pass_width[] = {8, 4, 4, 2, 2, 1, 1};
               register png_uint_32 final_val = 4 * png_ptr->width;

               srcptr = png_ptr->row_buf + 1 + initial_val;
               dstptr = row + initial_val;

               for (i = initial_val; i < final_val; i += stride)
               {
                  png_memcpy(dstptr, srcptr, rep_bytes);
                  srcptr += stride;
                  dstptr += stride;
               }
            } /* end of else */

            break;
         }       // end 32 bpp

         case 48:       // png_ptr->row_info.pixel_depth
         {
            png_bytep srcptr;
            png_bytep dstptr;

            if (mmx_supported)
            {
               png_uint_32 len;
               int diff;
               int dummy_value_a;   // fix 'forbidden register spilled' error
               int dummy_value_d;
               int dummy_value_c;
               int dummy_value_S;
               int dummy_value_D;
               _unmask = ~mask;            // global variable for -fPIC version
               srcptr = png_ptr->row_buf + 1;
               dstptr = row;
               len  = png_ptr->width &~7;  // reduce to multiple of 8
               diff = png_ptr->width & 7;  // amount lost

               __asm__ __volatile__ (
                  "movd      _unmask, %%mm7   \n\t" // load bit pattern
                  "psubb     %%mm6, %%mm6     \n\t" // zero mm6
                  "punpcklbw %%mm7, %%mm7     \n\t"
                  "punpcklwd %%mm7, %%mm7     \n\t"
                  "punpckldq %%mm7, %%mm7     \n\t" // fill reg with 8 masks

                  "movq      _mask48_0, %%mm0 \n\t"
                  "movq      _mask48_1, %%mm1 \n\t"
                  "movq      _mask48_2, %%mm2 \n\t"
                  "movq      _mask48_3, %%mm3 \n\t"
                  "movq      _mask48_4, %%mm4 \n\t"
                  "movq      _mask48_5, %%mm5 \n\t"

                  "pand      %%mm7, %%mm0     \n\t"
                  "pand      %%mm7, %%mm1     \n\t"
                  "pand      %%mm7, %%mm2     \n\t"
                  "pand      %%mm7, %%mm3     \n\t"
                  "pand      %%mm7, %%mm4     \n\t"
                  "pand      %%mm7, %%mm5     \n\t"

                  "pcmpeqb   %%mm6, %%mm0     \n\t"
                  "pcmpeqb   %%mm6, %%mm1     \n\t"
                  "pcmpeqb   %%mm6, %%mm2     \n\t"
                  "pcmpeqb   %%mm6, %%mm3     \n\t"
                  "pcmpeqb   %%mm6, %%mm4     \n\t"
                  "pcmpeqb   %%mm6, %%mm5     \n\t"

// preload        "movl      len, %%ecx       \n\t" // load length of line
// preload        "movl      srcptr, %%esi    \n\t" // load source
// preload        "movl      dstptr, %%edi    \n\t" // load dest

                  "cmpl      $0, %%ecx        \n\t"
                  "jz        mainloop48end    \n\t"

                "mainloop48:                  \n\t"
                  "movq      (%%esi), %%mm7   \n\t"
                  "pand      %%mm0, %%mm7     \n\t"
                  "movq      %%mm0, %%mm6     \n\t"
                  "pandn     (%%edi), %%mm6   \n\t"
                  "por       %%mm6, %%mm7     \n\t"
                  "movq      %%mm7, (%%edi)   \n\t"

                  "movq      8(%%esi), %%mm6  \n\t"
                  "pand      %%mm1, %%mm6     \n\t"
                  "movq      %%mm1, %%mm7     \n\t"
                  "pandn     8(%%edi), %%mm7  \n\t"
                  "por       %%mm7, %%mm6     \n\t"
                  "movq      %%mm6, 8(%%edi)  \n\t"

                  "movq      16(%%esi), %%mm6 \n\t"
                  "pand      %%mm2, %%mm6     \n\t"
                  "movq      %%mm2, %%mm7     \n\t"
                  "pandn     16(%%edi), %%mm7 \n\t"
                  "por       %%mm7, %%mm6     \n\t"
                  "movq      %%mm6, 16(%%edi) \n\t"

                  "movq      24(%%esi), %%mm7 \n\t"
                  "pand      %%mm3, %%mm7     \n\t"
                  "movq      %%mm3, %%mm6     \n\t"
                  "pandn     24(%%edi), %%mm6 \n\t"
                  "por       %%mm6, %%mm7     \n\t"
                  "movq      %%mm7, 24(%%edi) \n\t"

                  "movq      32(%%esi), %%mm6 \n\t"
                  "pand      %%mm4, %%mm6     \n\t"
                  "movq      %%mm4, %%mm7     \n\t"
                  "pandn     32(%%edi), %%mm7 \n\t"
                  "por       %%mm7, %%mm6     \n\t"
                  "movq      %%mm6, 32(%%edi) \n\t"

                  "movq      40(%%esi), %%mm7 \n\t"
                  "pand      %%mm5, %%mm7     \n\t"
                  "movq      %%mm5, %%mm6     \n\t"
                  "pandn     40(%%edi), %%mm6 \n\t"
                  "por       %%mm6, %%mm7     \n\t"
                  "movq      %%mm7, 40(%%edi) \n\t"

                  "addl      $48, %%esi       \n\t" // inc by 48 bytes processed
                  "addl      $48, %%edi       \n\t"
                  "subl      $8, %%ecx        \n\t" // dec by 8 pixels processed

                  "ja        mainloop48       \n\t"

                "mainloop48end:               \n\t"
// preload        "movl      diff, %%ecx      \n\t" // (diff is in eax)
                  "movl      %%eax, %%ecx     \n\t"
                  "cmpl      $0, %%ecx        \n\t"
                  "jz        end48            \n\t"
// preload        "movl      mask, %%edx      \n\t"
                  "sall      $24, %%edx       \n\t" // make low byte, high byte

                "secondloop48:                \n\t"
                  "sall      %%edx            \n\t" // move high bit to CF
                  "jnc       skip48           \n\t" // if CF = 0
                  "movl      (%%esi), %%eax   \n\t"
                  "movl      %%eax, (%%edi)   \n\t"

                "skip48:                      \n\t"
                  "addl      $4, %%esi        \n\t"
                  "addl      $4, %%edi        \n\t"
                  "decl      %%ecx            \n\t"
                  "jnz       secondloop48     \n\t"

                "end48:                       \n\t"
                  "EMMS                       \n\t" // DONE

                  : "=a" (dummy_value_a),              // output regs (dummy)
                    "=d" (dummy_value_d),
                    "=c" (dummy_value_c),
                    "=S" (dummy_value_S),
                    "=D" (dummy_value_D)

                  : "3" (srcptr),      // esi       // input regs
                    "4" (dstptr),      // edi
                    "0" (diff),        // eax
// was (unmask)     "b"    RESERVED    // ebx       // Global Offset Table idx
                    "2" (len),         // ecx
                    "1" (mask)         // edx

//                  :         // clobber list
#if 0  /* MMX regs (%mm0, etc.) not supported by gcc 2.7.2.3 or egcs 1.1 */
                  : "%mm0", "%mm1", "%mm2", "%mm3",
                    "%mm4", "%mm5", "%mm6", "%mm7"
#endif
               );
            }
            else /* mmx _not supported - Use modified C routine */
            {
               register png_uint_32 i;
               png_uint_32 initial_val = 6 * png_pass_start[png_ptr->pass];
                 // png.c:  png_pass_start[] = {0, 4, 0, 2, 0, 1, 0};
               register int stride = 6 * png_pass_inc[png_ptr->pass];
                 // png.c:  png_pass_inc[] = {8, 8, 4, 4, 2, 2, 1};
               register int rep_bytes = 6 * png_pass_width[png_ptr->pass];
                 // png.c:  png_pass_width[] = {8, 4, 4, 2, 2, 1, 1};
               register png_uint_32 final_val = 6 * png_ptr->width;

               srcptr = png_ptr->row_buf + 1 + initial_val;
               dstptr = row + initial_val;

               for (i = initial_val; i < final_val; i += stride)
               {
                  png_memcpy(dstptr, srcptr, rep_bytes);
                  srcptr += stride;
                  dstptr += stride;
               } 
            } /* end of else */

            break;
         }       // end 48 bpp

         case 64:       // png_ptr->row_info.pixel_depth
         {
            png_bytep srcptr;
            png_bytep dstptr;
            register png_uint_32 i;
            png_uint_32 initial_val = 8 * png_pass_start[png_ptr->pass];
              // png.c:  png_pass_start[] = {0, 4, 0, 2, 0, 1, 0};
            register int stride = 8 * png_pass_inc[png_ptr->pass];
              // png.c:  png_pass_inc[] = {8, 8, 4, 4, 2, 2, 1};
            register int rep_bytes = 8 * png_pass_width[png_ptr->pass];
              // png.c:  png_pass_width[] = {8, 4, 4, 2, 2, 1, 1};
            register png_uint_32 final_val = 8 * png_ptr->width;

            srcptr = png_ptr->row_buf + 1 + initial_val;
            dstptr = row + initial_val;

            for (i = initial_val; i < final_val; i += stride)
            {
               png_memcpy(dstptr, srcptr, rep_bytes);
               srcptr += stride;
               dstptr += stride;
            } 
            break;
         }       // end 64 bpp

         default:   // png_ptr->row_info.pixel_depth != 1,2,4,8,16,24,32,48,64
         {
            // this should never happen
            fprintf(stderr,
              "libpng internal error:  png_ptr->row_info.pixel_depth = %d\n",
              png_ptr->row_info.pixel_depth);
            fflush(stderr);
            break;
         }
      } /* end switch (png_ptr->row_info.pixel_depth) */

   } /* end if (non-trivial mask) */

} /* end png_combine_row() */

#endif /* PNG_HAVE_ASSEMBLER_COMBINE_ROW */



#if defined(PNG_READ_INTERLACING_SUPPORTED)
#if defined(PNG_HAVE_ASSEMBLER_READ_INTERLACE)

/* png_do_read_interlace() is called after any 16-bit to 8-bit conversion
 * has taken place.  [GRR: what other steps come before and/or after?]
 */

void /* PRIVATE */
png_do_read_interlace(png_row_infop row_info, png_bytep row, int pass,
   png_uint_32 transformations)
{
/*
fprintf(stderr, "GRR DEBUG:  entering png_do_read_interlace()\n");
if (row == NULL) fprintf(stderr, "GRR DEBUG:  row == NULL\n");
if (row_info == NULL) fprintf(stderr, "GRR DEBUG:  row_info == NULL\n");
fflush(stderr);
 */
   png_debug(1,"in png_do_read_interlace\n");

   if (mmx_supported == 2)
       mmx_supported = mmxsupport();
/*
{
fprintf(stderr, "GRR DEBUG:  calling mmxsupport()\n");
fprintf(stderr, "GRR DEBUG:  done with mmxsupport() (mmx_supported = %d)\n", mmx_supported);
}
 */

/*
this one happened on first row due to weirdness with mmxsupport():
if (row == NULL) fprintf(stderr, "GRR DEBUG:  now row == NULL!!!\n");
  row was in ebx, and even though nothing touched ebx, it still got wiped...
  [weird side effect of CPUID instruction?]
if (row_info == NULL) fprintf(stderr, "GRR DEBUG:  now row_info == NULL!!!\n");
 */
   if (row != NULL && row_info != NULL)
   {
      png_uint_32 final_width;

      final_width = row_info->width * png_pass_inc[pass];

/*
fprintf(stderr, "GRR DEBUG:  png_do_read_interlace() row_info->width = %d, final_width = %d\n", row_info->width, final_width);
fprintf(stderr, "GRR DEBUG:  png_do_read_interlace() pixel_depth = %d\n", row_info->pixel_depth);
fflush(stderr);
 */
      switch (row_info->pixel_depth)
      {
         case 1:
         {
            png_bytep sp, dp;
            int sshift, dshift;
            int s_start, s_end, s_inc;
            png_byte v;
            png_uint_32 i;
            int j;

            sp = row + (png_size_t)((row_info->width - 1) >> 3);
            dp = row + (png_size_t)((final_width - 1) >> 3);
#if defined(PNG_READ_PACKSWAP_SUPPORTED)
            if (transformations & PNG_PACKSWAP)
            {
               sshift = (int)((row_info->width + 7) & 7);
               dshift = (int)((final_width + 7) & 7);
               s_start = 7;
               s_end = 0;
               s_inc = -1;
            }
            else
#endif
            {
               sshift = 7 - (int)((row_info->width + 7) & 7);
               dshift = 7 - (int)((final_width + 7) & 7);
               s_start = 0;
               s_end = 7;
               s_inc = 1;
            }

            for (i = row_info->width; i; i--)
            {
               v = (png_byte)((*sp >> sshift) & 0x1);
               for (j = 0; j < png_pass_inc[pass]; j++)
               {
                  *dp &= (png_byte)((0x7f7f >> (7 - dshift)) & 0xff);
                  *dp |= (png_byte)(v << dshift);
                  if (dshift == s_end)
                  {
                     dshift = s_start;
                     dp--;
                  }
                  else
                     dshift += s_inc;
               }
               if (sshift == s_end)
               {
                  sshift = s_start;
                  sp--;
               }
               else
                  sshift += s_inc;
            }
            break;
         }

         case 2:
         {
            png_bytep sp, dp;
            int sshift, dshift;
            int s_start, s_end, s_inc;
            png_uint_32 i;

            sp = row + (png_size_t)((row_info->width - 1) >> 2);
            dp = row + (png_size_t)((final_width - 1) >> 2);
#if defined(PNG_READ_PACKSWAP_SUPPORTED)
            if (transformations & PNG_PACKSWAP)
            {
               sshift = (png_size_t)(((row_info->width + 3) & 3) << 1);
               dshift = (png_size_t)(((final_width + 3) & 3) << 1);
               s_start = 6;
               s_end = 0;
               s_inc = -2;
            }
            else
#endif
            {
               sshift = (png_size_t)((3 - ((row_info->width + 3) & 3)) << 1);
               dshift = (png_size_t)((3 - ((final_width + 3) & 3)) << 1);
               s_start = 0;
               s_end = 6;
               s_inc = 2;
            }

            for (i = row_info->width; i; i--)
            {
               png_byte v;
               int j;

               v = (png_byte)((*sp >> sshift) & 0x3);
               for (j = 0; j < png_pass_inc[pass]; j++)
               {
                  *dp &= (png_byte)((0x3f3f >> (6 - dshift)) & 0xff);
                  *dp |= (png_byte)(v << dshift);
                  if (dshift == s_end)
                  {
                     dshift = s_start;
                     dp--;
                  }
                  else
                     dshift += s_inc;
               }
               if (sshift == s_end)
               {
                  sshift = s_start;
                  sp--;
               }
               else
                  sshift += s_inc;
            }
            break;
         }

         case 4:
         {
            png_bytep sp, dp;
            int sshift, dshift;
            int s_start, s_end, s_inc;
            png_uint_32 i;

            sp = row + (png_size_t)((row_info->width - 1) >> 1);
            dp = row + (png_size_t)((final_width - 1) >> 1);
#if defined(PNG_READ_PACKSWAP_SUPPORTED)
            if (transformations & PNG_PACKSWAP)
            {
               sshift = (png_size_t)(((row_info->width + 1) & 1) << 2);
               dshift = (png_size_t)(((final_width + 1) & 1) << 2);
               s_start = 4;
               s_end = 0;
               s_inc = -4;
            }
            else
#endif
            {
               sshift = (png_size_t)((1 - ((row_info->width + 1) & 1)) << 2);
               dshift = (png_size_t)((1 - ((final_width + 1) & 1)) << 2);
               s_start = 0;
               s_end = 4;
               s_inc = 4;
            }

            for (i = row_info->width; i; i--)
            {
               png_byte v;
               int j;

               v = (png_byte)((*sp >> sshift) & 0xf);
               for (j = 0; j < png_pass_inc[pass]; j++)
               {
                  *dp &= (png_byte)((0xf0f >> (4 - dshift)) & 0xff);
                  *dp |= (png_byte)(v << dshift);
                  if (dshift == s_end)
                  {
                     dshift = s_start;
                     dp--;
                  }
                  else
                     dshift += s_inc;
               }
               if (sshift == s_end)
               {
                  sshift = s_start;
                  sp--;
               }
               else
                  sshift += s_inc;
            }
            break;
         }

         //====================================================================

         default:  // 8-bit or larger (this is where the routine is modified)
         {
//          static unsigned long long _const4 = 0x0000000000FFFFFFLL;  no good
//          static unsigned long long const4 = 0x0000000000FFFFFFLL;   no good
//          unsigned long long _const4 = 0x0000000000FFFFFFLL;         no good
//          unsigned long long const4 = 0x0000000000FFFFFFLL;          no good
            png_bytep sptr, dp;
            png_uint_32 i;
            png_size_t pixel_bytes;
            int width = row_info->width;

            pixel_bytes = (row_info->pixel_depth >> 3);

            // point sptr at the last pixel in the pre-expanded row:
            sptr = row + (width - 1) * pixel_bytes;

            // point dp at the last pixel position in the expanded row:
            dp = row + (final_width - 1) * pixel_bytes;

            // New code by Nirav Chhatrapati - Intel Corporation

            if (mmx_supported)  // use MMX code if machine supports it
            {
               //--------------------------------------------------------------
               if (pixel_bytes == 3)
               {
                  if (((pass == 0) || (pass == 1)) && width)
                  {
                     int dummy_value_c;   // fix 'forbidden register spilled'
                     int dummy_value_S;
                     int dummy_value_D;
                     __asm__ __volatile__ (
                        "subl $21, %%edi         \n\t"
                                     // (png_pass_inc[pass] - 1)*pixel_bytes

                     ".loop3_pass0:              \n\t"
                        "movd (%%esi), %%mm0     \n\t" // x x x x x 2 1 0
                        "pand _const4, %%mm0     \n\t" // z z z z z 2 1 0
                        "movq %%mm0, %%mm1       \n\t" // z z z z z 2 1 0
                        "psllq $16, %%mm0        \n\t" // z z z 2 1 0 z z
                        "movq %%mm0, %%mm2       \n\t" // z z z 2 1 0 z z
                        "psllq $24, %%mm0        \n\t" // 2 1 0 z z z z z
                        "psrlq $8, %%mm1         \n\t" // z z z z z z 2 1
                        "por %%mm2, %%mm0        \n\t" // 2 1 0 2 1 0 z z
                        "por %%mm1, %%mm0        \n\t" // 2 1 0 2 1 0 2 1
                        "movq %%mm0, %%mm3       \n\t" // 2 1 0 2 1 0 2 1
                        "psllq $16, %%mm0        \n\t" // 0 2 1 0 2 1 z z
                        "movq %%mm3, %%mm4       \n\t" // 2 1 0 2 1 0 2 1
                        "punpckhdq %%mm0, %%mm3  \n\t" // 0 2 1 0 2 1 0 2
                        "movq %%mm4, 16(%%edi)   \n\t"
                        "psrlq $32, %%mm0        \n\t" // z z z z 0 2 1 0
                        "movq %%mm3, 8(%%edi)    \n\t"
                        "punpckldq %%mm4, %%mm0  \n\t" // 1 0 2 1 0 2 1 0
                        "subl $3, %%esi          \n\t"
                        "movq %%mm0, (%%edi)     \n\t"
                        "subl $24, %%edi         \n\t"
                        "decl %%ecx              \n\t"
                        "jnz .loop3_pass0        \n\t"
                        "EMMS                    \n\t" // DONE

                        : "=c" (dummy_value_c),           // output regs (dummy)
                          "=S" (dummy_value_S),
                          "=D" (dummy_value_D)

                        : "1" (sptr),      // esi      // input regs
                          "2" (dp),        // edi
                          "0" (width)      // ecx
// doesn't work           "i" (0x0000000000FFFFFFLL)   // %1 (a.k.a. _const4)

//                        :        // clobber list
#if 0  /* %mm0, ..., %mm4 not supported by gcc 2.7.2.3 or egcs 1.1 */
                        : "%mm0", "%mm1", "%mm2", "%mm3", "%mm4"
#endif
                     );
                  }
                  else if (((pass == 2) || (pass == 3)) && width)
                  {
                     int dummy_value_c;   // fix 'forbidden register spilled'
                     int dummy_value_S;
                     int dummy_value_D;
                     __asm__ __volatile__ (
                        "subl $9, %%edi          \n\t"
                                     // (png_pass_inc[pass] - 1)*pixel_bytes

                     ".loop3_pass2:              \n\t"
                        "movd (%%esi), %%mm0     \n\t" // x x x x x 2 1 0
                        "pand _const4, %%mm0     \n\t" // z z z z z 2 1 0
                        "movq %%mm0, %%mm1       \n\t" // z z z z z 2 1 0
                        "psllq $16, %%mm0        \n\t" // z z z 2 1 0 z z
                        "movq %%mm0, %%mm2       \n\t" // z z z 2 1 0 z z
                        "psllq $24, %%mm0        \n\t" // 2 1 0 z z z z z
                        "psrlq $8, %%mm1         \n\t" // z z z z z z 2 1
                        "por %%mm2, %%mm0        \n\t" // 2 1 0 2 1 0 z z
                        "por %%mm1, %%mm0        \n\t" // 2 1 0 2 1 0 2 1
                        "movq %%mm0, 4(%%edi)    \n\t"
                        "psrlq $16, %%mm0        \n\t" // z z 2 1 0 2 1 0
                        "subl $3, %%esi          \n\t"
                        "movd %%mm0, (%%edi)     \n\t"
                        "subl $12, %%edi         \n\t"
                        "decl %%ecx              \n\t"
                        "jnz .loop3_pass2        \n\t"
                        "EMMS                    \n\t" // DONE

                        : "=c" (dummy_value_c),           // output regs (dummy)
                          "=S" (dummy_value_S),
                          "=D" (dummy_value_D)

                        : "1" (sptr),      // esi      // input regs
                          "2" (dp),        // edi
                          "0" (width)      // ecx

//                        :        // clobber list
#if 0  /* %mm0, ..., %mm2 not supported by gcc 2.7.2.3 or egcs 1.1 */
                        : "%mm0", "%mm1", "%mm2"
#endif
                     );
                  }
                  else if (width) /* && ((pass == 4) || (pass == 5)) */
                  {
                     int width_mmx = ((width >> 1) << 1) - 8;   // GRR:  huh?
                     if (width_mmx < 0)
                         width_mmx = 0;
                     width -= width_mmx;        // 8 or 9 pix, 24 or 27 bytes
                     if (width_mmx)
                     {
                        // png_pass_inc[] = {8, 8, 4, 4, 2, 2, 1};
                        // sptr points at last pixel in pre-expanded row
                        // dp points at last pixel position in expanded row
                        int dummy_value_c;  // fix 'forbidden register spilled'
                        int dummy_value_S;
                        int dummy_value_D;
                        __asm__ __volatile__ (
                           "subl $3, %%esi          \n\t"
                           "subl $9, %%edi          \n\t"
                                        // (png_pass_inc[pass] + 1)*pixel_bytes

                        ".loop3_pass4:              \n\t"
                           "movq (%%esi), %%mm0     \n\t" // x x 5 4 3 2 1 0
                           "movq %%mm0, %%mm1       \n\t" // x x 5 4 3 2 1 0
                           "movq %%mm0, %%mm2       \n\t" // x x 5 4 3 2 1 0
                           "psllq $24, %%mm0        \n\t" // 4 3 2 1 0 z z z
                           "pand _const4, %%mm1     \n\t" // z z z z z 2 1 0
                           "psrlq $24, %%mm2        \n\t" // z z z x x 5 4 3
                           "por %%mm1, %%mm0        \n\t" // 4 3 2 1 0 2 1 0
                           "movq %%mm2, %%mm3       \n\t" // z z z x x 5 4 3
                           "psllq $8, %%mm2         \n\t" // z z x x 5 4 3 z
                           "movq %%mm0, (%%edi)     \n\t"
                           "psrlq $16, %%mm3        \n\t" // z z z z z x x 5
                           "pand _const6, %%mm3     \n\t" // z z z z z z z 5
                           "por %%mm3, %%mm2        \n\t" // z z x x 5 4 3 5
                           "subl $6, %%esi          \n\t"
                           "movd %%mm2, 8(%%edi)    \n\t"
                           "subl $12, %%edi         \n\t"
                           "subl $2, %%ecx          \n\t"
                           "jnz .loop3_pass4        \n\t"
                           "EMMS                    \n\t" // DONE

                           : "=c" (dummy_value_c),           // output regs (dummy)
                             "=S" (dummy_value_S),
                             "=D" (dummy_value_D)

                           : "1" (sptr),      // esi      // input regs
                             "2" (dp),        // edi
                             "0" (width_mmx)  // ecx

//                           :        // clobber list
#if 0  /* %mm0, ..., %mm3 not supported by gcc 2.7.2.3 or egcs 1.1 */
                           : "%mm0", "%mm1", "%mm2", "%mm3"
#endif
                        );
                     }

                     sptr -= width_mmx*3;
                     dp -= width_mmx*6;
                     for (i = width; i; i--)
                     {
                        png_byte v[8];
                        int j;

                        png_memcpy(v, sptr, 3);
                        for (j = 0; j < png_pass_inc[pass]; j++)
                        {
                           png_memcpy(dp, v, 3);
                           dp -= 3;
                        }
                        sptr -= 3;
                     }
                  }
               } /* end of pixel_bytes == 3 */

               //--------------------------------------------------------------
               else if (pixel_bytes == 1)
               {
                  if (((pass == 0) || (pass == 1)) && width)
                  {
                     int width_mmx = ((width >> 2) << 2);
                     width -= width_mmx;        // 0-3 pixels => 0-3 bytes
                     if (width_mmx)
                     {
                        int dummy_value_c;  // fix 'forbidden register spilled'
                        int dummy_value_S;
                        int dummy_value_D;
                        __asm__ __volatile__ (
                           "subl $3, %%esi          \n\t"
                           "subl $31, %%edi         \n\t"

                        ".loop1_pass0:              \n\t"
                           "movd (%%esi), %%mm0     \n\t" // x x x x 3 2 1 0
                           "movq %%mm0, %%mm1       \n\t" // x x x x 3 2 1 0
                           "punpcklbw %%mm0, %%mm0  \n\t" // 3 3 2 2 1 1 0 0
                           "movq %%mm0, %%mm2       \n\t" // 3 3 2 2 1 1 0 0
                           "punpcklwd %%mm0, %%mm0  \n\t" // 1 1 1 1 0 0 0 0
                           "movq %%mm0, %%mm3       \n\t" // 1 1 1 1 0 0 0 0
                           "punpckldq %%mm0, %%mm0  \n\t" // 0 0 0 0 0 0 0 0
                           "punpckhdq %%mm3, %%mm3  \n\t" // 1 1 1 1 1 1 1 1
                           "movq %%mm0, (%%edi)     \n\t"
                           "punpckhwd %%mm2, %%mm2  \n\t" // 3 3 3 3 2 2 2 2
                           "movq %%mm3, 8(%%edi)    \n\t"
                           "movq %%mm2, %%mm4       \n\t" // 3 3 3 3 2 2 2 2
                           "punpckldq %%mm2, %%mm2  \n\t" // 2 2 2 2 2 2 2 2
                           "punpckhdq %%mm4, %%mm4  \n\t" // 3 3 3 3 3 3 3 3
                           "movq %%mm2, 16(%%edi)   \n\t"
                           "subl $4, %%esi          \n\t"
                           "movq %%mm4, 24(%%edi)   \n\t"
                           "subl $32, %%edi         \n\t"
                           "subl $4, %%ecx          \n\t"
                           "jnz .loop1_pass0        \n\t"
                           "EMMS                    \n\t" // DONE

                           : "=c" (dummy_value_c),           // output regs (dummy)
                             "=S" (dummy_value_S),
                             "=D" (dummy_value_D)

                           : "1" (sptr),      // esi      // input regs
                             "2" (dp),        // edi
                             "0" (width_mmx)  // ecx

//                           :       // clobber list
#if 0  /* %mm0, ..., %mm4 not supported by gcc 2.7.2.3 or egcs 1.1 */
                           : "%mm0", "%mm1", "%mm2", "%mm3", "%mm4"
#endif
                        );
                     }

                     sptr -= width_mmx;
                     dp -= width_mmx*8;
                     for (i = width; i; i--)
                     {
                        int j;

                       /* I simplified this part in version 1.0.4e
                        * here and in several other instances where
                        * pixel_bytes == 1  -- GR-P
                        *
                        * Original code:
                        *
                        * png_byte v[8];
                        * png_memcpy(v, sptr, pixel_bytes);
                        * for (j = 0; j < png_pass_inc[pass]; j++)
                        * {
                        *    png_memcpy(dp, v, pixel_bytes);
                        *    dp -= pixel_bytes;
                        * }
                        * sptr -= pixel_bytes;
                        *
                        * Replacement code is in the next three lines:
                        */

                        for (j = 0; j < png_pass_inc[pass]; j++)
                           *dp-- = *sptr;
                        --sptr;
                     }
                  }
                  else if (((pass == 2) || (pass == 3)) && width)
                  {
                     int width_mmx = ((width >> 2) << 2);
                     width -= width_mmx;        // 0-3 pixels => 0-3 bytes
                     if (width_mmx)
                     {
                        int dummy_value_c;  // fix 'forbidden register spilled'
                        int dummy_value_S;
                        int dummy_value_D;
                        __asm__ __volatile__ (
                           "subl $3, %%esi          \n\t"
                           "subl $15, %%edi         \n\t"

                        ".loop1_pass2:              \n\t"
                           "movd (%%esi), %%mm0     \n\t" // x x x x 3 2 1 0
                           "punpcklbw %%mm0, %%mm0  \n\t" // 3 3 2 2 1 1 0 0
                           "movq %%mm0, %%mm1       \n\t" // 3 3 2 2 1 1 0 0
                           "punpcklwd %%mm0, %%mm0  \n\t" // 1 1 1 1 0 0 0 0
                           "punpckhwd %%mm1, %%mm1  \n\t" // 3 3 3 3 2 2 2 2
                           "movq %%mm0, (%%edi)     \n\t"
                           "subl $4, %%esi          \n\t"
                           "movq %%mm1, 8(%%edi)    \n\t"
                           "subl $16, %%edi         \n\t"
                           "subl $4, %%ecx          \n\t"
                           "jnz .loop1_pass2        \n\t"
                           "EMMS                    \n\t" // DONE

                           : "=c" (dummy_value_c),           // output regs (dummy)
                             "=S" (dummy_value_S),
                             "=D" (dummy_value_D)

                           : "1" (sptr),      // esi      // input regs
                             "2" (dp),        // edi
                             "0" (width_mmx)  // ecx

//                           :        // clobber list
#if 0  /* %mm0, %mm1 not supported by gcc 2.7.2.3 or egcs 1.1 */
                           : "%mm0", "%mm1"
#endif
                        );
                     }

                     sptr -= width_mmx;
                     dp -= width_mmx*4;
                     for (i = width; i; i--)
                     {
                        int j;

                        for (j = 0; j < png_pass_inc[pass]; j++)
                           *dp-- = *sptr;
                        --sptr;
                     }
                  }
                  else if (width)  /* && ((pass == 4) || (pass == 5)) */
                  {
                     int width_mmx = ((width >> 3) << 3);
                     width -= width_mmx;        // 0-3 pixels => 0-3 bytes
                     if (width_mmx)
                     {
                        int dummy_value_c;  // fix 'forbidden register spilled'
                        int dummy_value_S;
                        int dummy_value_D;
                        __asm__ __volatile__ (
                           "subl $7, %%esi          \n\t"
                           "subl $15, %%edi         \n\t"

                        ".loop1_pass4:              \n\t"
                           "movq (%%esi), %%mm0     \n\t" // 7 6 5 4 3 2 1 0
                           "movq %%mm0, %%mm1       \n\t" // 7 6 5 4 3 2 1 0
                           "punpcklbw %%mm0, %%mm0  \n\t" // 3 3 2 2 1 1 0 0
                           "punpckhbw %%mm1, %%mm1  \n\t" // 7 7 6 6 5 5 4 4
                           "movq %%mm1, 8(%%edi)    \n\t"
                           "subl $8, %%esi          \n\t"
                           "movq %%mm0, (%%edi)     \n\t"
                           "subl $16, %%edi         \n\t"
                           "subl $8, %%ecx          \n\t"
                           "jnz .loop1_pass4        \n\t"
                           "EMMS                    \n\t" // DONE

                           : "=c" (dummy_value_c),           // output regs (none)
                             "=S" (dummy_value_S),
                             "=D" (dummy_value_D)

                           : "1" (sptr),      // esi      // input regs
                             "2" (dp),        // edi
                             "0" (width_mmx)  // ecx

//                           :        // clobber list
#if 0  /* %mm0, %mm1 not supported by gcc 2.7.2.3 or egcs 1.1 */
                           : "%mm0", "%mm1"
#endif
                        );
                     }

                     sptr -= width_mmx;
                     dp -= width_mmx*2;
                     for (i = width; i; i--)
                     {
                        int j;

                        for (j = 0; j < png_pass_inc[pass]; j++)
                           *dp-- = *sptr;
                        --sptr;
                     }
                  }
               } /* end of pixel_bytes == 1 */

               //--------------------------------------------------------------
               else if (pixel_bytes == 2)
               {
                  if (((pass == 0) || (pass == 1)) && width)
                  {
                     int width_mmx = ((width >> 1) << 1);
                     width -= width_mmx;        // 0,1 pixels => 0,2 bytes
                     if (width_mmx)
                     {
                        int dummy_value_c;  // fix 'forbidden register spilled'
                        int dummy_value_S;
                        int dummy_value_D;
                        __asm__ __volatile__ (
                           "subl $2, %%esi          \n\t"
                           "subl $30, %%edi         \n\t"

                        ".loop2_pass0:              \n\t"
                           "movd (%%esi), %%mm0     \n\t" // x x x x 3 2 1 0
                           "punpcklwd %%mm0, %%mm0  \n\t" // 3 2 3 2 1 0 1 0
                           "movq %%mm0, %%mm1       \n\t" // 3 2 3 2 1 0 1 0
                           "punpckldq %%mm0, %%mm0  \n\t" // 1 0 1 0 1 0 1 0
                           "punpckhdq %%mm1, %%mm1  \n\t" // 3 2 3 2 3 2 3 2
                           "movq %%mm0, (%%edi)     \n\t"
                           "movq %%mm0, 8(%%edi)    \n\t"
                           "movq %%mm1, 16(%%edi)   \n\t"
                           "subl $4, %%esi          \n\t"
                           "movq %%mm1, 24(%%edi)   \n\t"
                           "subl $32, %%edi         \n\t"
                           "subl $2, %%ecx          \n\t"
                           "jnz .loop2_pass0        \n\t"
                           "EMMS                    \n\t" // DONE

                           : "=c" (dummy_value_c),           // output regs (dummy)
                             "=S" (dummy_value_S),
                             "=D" (dummy_value_D)

                           : "1" (sptr),      // esi      // input regs
                             "2" (dp),        // edi
                             "0" (width_mmx)  // ecx

//                           :        // clobber list
#if 0  /* %mm0, %mm1 not supported by gcc 2.7.2.3 or egcs 1.1 */
                           : "%mm0", "%mm1"
#endif
                        );
                     }

                     sptr -= (width_mmx*2 - 2); // sign fixed
                     dp -= (width_mmx*16 - 2);  // sign fixed
                     for (i = width; i; i--)
                     {
                        png_byte v[8];
                        int j;
                        sptr -= 2;
                        png_memcpy(v, sptr, 2);
                        for (j = 0; j < png_pass_inc[pass]; j++)
                        {
                           dp -= 2;
                           png_memcpy(dp, v, 2);
                        }
                     }
                  }
                  else if (((pass == 2) || (pass == 3)) && width)
                  {
                     int width_mmx = ((width >> 1) << 1) ;
                     width -= width_mmx;        // 0,1 pixels => 0,2 bytes
                     if (width_mmx)
                     {
                        int dummy_value_c;  // fix 'forbidden register spilled'
                        int dummy_value_S;
                        int dummy_value_D;
                        __asm__ __volatile__ (
                           "subl $2, %%esi          \n\t"
                           "subl $14, %%edi         \n\t"

                        ".loop2_pass2:              \n\t"
                           "movd (%%esi), %%mm0     \n\t" // x x x x 3 2 1 0
                           "punpcklwd %%mm0, %%mm0  \n\t" // 3 2 3 2 1 0 1 0
                           "movq %%mm0, %%mm1       \n\t" // 3 2 3 2 1 0 1 0
                           "punpckldq %%mm0, %%mm0  \n\t" // 1 0 1 0 1 0 1 0
                           "punpckhdq %%mm1, %%mm1  \n\t" // 3 2 3 2 3 2 3 2
                           "movq %%mm0, (%%edi)     \n\t"
                           "subl $4, %%esi          \n\t"
                           "movq %%mm1, 8(%%edi)    \n\t"
                           "subl $16, %%edi         \n\t"
                           "subl $2, %%ecx          \n\t"
                           "jnz .loop2_pass2        \n\t"
                           "EMMS                    \n\t" // DONE

                           : "=c" (dummy_value_c),           // output regs (dummy)
                             "=S" (dummy_value_S),
                             "=D" (dummy_value_D)

                           : "1" (sptr),      // esi      // input regs
                             "2" (dp),        // edi
                             "0" (width_mmx)  // ecx

//                           :        // clobber list
#if 0  /* %mm0, %mm1 not supported by gcc 2.7.2.3 or egcs 1.1 */
                           : "%mm0", "%mm1"
#endif
                        );
                     }

                     sptr -= (width_mmx*2 - 2); // sign fixed
                     dp -= (width_mmx*8 - 2);   // sign fixed
                     for (i = width; i; i--)
                     {
                        png_byte v[8];
                        int j;
                        sptr -= 2;
                        png_memcpy(v, sptr, 2);
                        for (j = 0; j < png_pass_inc[pass]; j++)
                        {
                           dp -= 2;
                           png_memcpy(dp, v, 2);
                        }
                     }
                  }
                  else if (width)  // pass == 4 or 5
                  {
                     int width_mmx = ((width >> 1) << 1) ;
                     width -= width_mmx;        // 0,1 pixels => 0,2 bytes
                     if (width_mmx)
                     {
                        int dummy_value_c;  // fix 'forbidden register spilled'
                        int dummy_value_S;
                        int dummy_value_D;
                        __asm__ __volatile__ (
                           "subl $2, %%esi          \n\t"
                           "subl $6, %%edi          \n\t"

                        ".loop2_pass4:              \n\t"
                           "movd (%%esi), %%mm0     \n\t" // x x x x 3 2 1 0
                           "punpcklwd %%mm0, %%mm0  \n\t" // 3 2 3 2 1 0 1 0
                           "subl $4, %%esi          \n\t"
                           "movq %%mm0, (%%edi)     \n\t"
                           "subl $8, %%edi          \n\t"
                           "subl $2, %%ecx          \n\t"
                           "jnz .loop2_pass4        \n\t"
                           "EMMS                    \n\t" // DONE

                           : "=c" (dummy_value_c),           // output regs (dummy)
                             "=S" (dummy_value_S),
                             "=D" (dummy_value_D)

                           : "1" (sptr),      // esi      // input regs
                             "2" (dp),        // edi
                             "0" (width_mmx)  // ecx

//                           :        // clobber list
#if 0  /* %mm0 not supported by gcc 2.7.2.3 or egcs 1.1 */
                           : "%mm0"
#endif
                        );
                     }

                     sptr -= (width_mmx*2 - 2); // sign fixed
                     dp -= (width_mmx*4 - 2);   // sign fixed
                     for (i = width; i; i--)
                     {
                        png_byte v[8];
                        int j;
                        sptr -= 2;
                        png_memcpy(v, sptr, 2);
                        for (j = 0; j < png_pass_inc[pass]; j++)
                        {
                           dp -= 2;
                           png_memcpy(dp, v, 2);
                        }
                     }
                  }
               } /* end of pixel_bytes == 2 */

               //--------------------------------------------------------------
               else if (pixel_bytes == 4)
               {
                  if (((pass == 0) || (pass == 1)) && width)
                  {
                     int width_mmx = ((width >> 1) << 1);
                     width -= width_mmx;        // 0,1 pixels => 0,4 bytes
/*
fprintf(stderr, "GRR DEBUG:  png_do_read_interlace() pass = %d, width_mmx = %d, width = %d\n", pass, width_mmx, width);
fprintf(stderr, "            sptr = 0x%08lx, dp = 0x%08lx\n", (unsigned long)sptr, (unsigned long)dp);
fflush(stderr);
 */
                     if (width_mmx)
                     {
                        int dummy_value_c;  // fix 'forbidden register spilled'
                        int dummy_value_S;
                        int dummy_value_D;
#ifdef GRR_DEBUG
                        FILE *junk = fopen("junk.4bytes", "wb");
                        if (junk)
                           fclose(junk);
#endif /* GRR_DEBUG */
                        __asm__ __volatile__ (
                           "subl $4, %%esi          \n\t"
                           "subl $60, %%edi         \n\t"

                        ".loop4_pass0:              \n\t"
                           "movq (%%esi), %%mm0     \n\t" // 7 6 5 4 3 2 1 0
                           "movq %%mm0, %%mm1       \n\t" // 7 6 5 4 3 2 1 0
                           "punpckldq %%mm0, %%mm0  \n\t" // 3 2 1 0 3 2 1 0
                           "punpckhdq %%mm1, %%mm1  \n\t" // 7 6 5 4 7 6 5 4
                           "movq %%mm0, (%%edi)     \n\t"
                           "movq %%mm0, 8(%%edi)    \n\t"
                           "movq %%mm0, 16(%%edi)   \n\t"
                           "movq %%mm0, 24(%%edi)   \n\t"
                           "movq %%mm1, 32(%%edi)   \n\t"
                           "movq %%mm1, 40(%%edi)   \n\t"
                           "movq %%mm1, 48(%%edi)   \n\t"
                           "subl $8, %%esi          \n\t"
                           "movq %%mm1, 56(%%edi)   \n\t"
                           "subl $64, %%edi         \n\t"
                           "subl $2, %%ecx          \n\t"
                           "jnz .loop4_pass0        \n\t"
                           "EMMS                    \n\t" // DONE

                           : "=c" (dummy_value_c),           // output regs (dummy)
                             "=S" (dummy_value_S),
                             "=D" (dummy_value_D)
 
                           : "1" (sptr),      // esi      // input regs
                             "2" (dp),        // edi
                             "0" (width_mmx)  // ecx

//                           :        // clobber list
#if 0  /* %mm0, %mm1 not supported by gcc 2.7.2.3 or egcs 1.1 */
                           : "%mm0", "%mm1"
#endif
                        );
                     }

                     sptr -= (width_mmx*4 - 4); // sign fixed
                     dp -= (width_mmx*32 - 4);  // sign fixed
                     for (i = width; i; i--)
                     {
                        png_byte v[8];
                        int j;
                        sptr -= 4;
                        png_memcpy(v, sptr, 4);
                        for (j = 0; j < png_pass_inc[pass]; j++)
                        {
                           dp -= 4;
                           png_memcpy(dp, v, 4);
                        }
                     }
                  }
                  else if (((pass == 2) || (pass == 3)) && width)
                  {
                     int width_mmx = ((width >> 1) << 1);
                     width -= width_mmx;        // 0,1 pixels => 0,4 bytes
                     if (width_mmx)
                     {
                        int dummy_value_c;  // fix 'forbidden register spilled'
                        int dummy_value_S;
                        int dummy_value_D;
                        __asm__ __volatile__ (
                           "subl $4, %%esi          \n\t"
                           "subl $28, %%edi         \n\t"

                        ".loop4_pass2:              \n\t"
                           "movq (%%esi), %%mm0     \n\t" // 7 6 5 4 3 2 1 0
                           "movq %%mm0, %%mm1       \n\t" // 7 6 5 4 3 2 1 0
                           "punpckldq %%mm0, %%mm0  \n\t" // 3 2 1 0 3 2 1 0
                           "punpckhdq %%mm1, %%mm1  \n\t" // 7 6 5 4 7 6 5 4
                           "movq %%mm0, (%%edi)     \n\t"
                           "movq %%mm0, 8(%%edi)    \n\t"
                           "movq %%mm1, 16(%%edi)   \n\t"
                           "movq %%mm1, 24(%%edi)   \n\t"
                           "subl $8, %%esi          \n\t"
                           "subl $32, %%edi         \n\t"
                           "subl $2, %%ecx          \n\t"
                           "jnz .loop4_pass2        \n\t"
                           "EMMS                    \n\t" // DONE

                           : "=c" (dummy_value_c),           // output regs (dummy)
                             "=S" (dummy_value_S),
                             "=D" (dummy_value_D)
 
                           : "1" (sptr),      // esi      // input regs
                             "2" (dp),        // edi
                             "0" (width_mmx)  // ecx

//                           :        // clobber list
#if 0  /* %mm0, %mm1 not supported by gcc 2.7.2.3 or egcs 1.1 */
                           : "%mm0", "%mm1"
#endif
                        );
                     }

                     sptr -= (width_mmx*4 - 4); // sign fixed
                     dp -= (width_mmx*16 - 4);  // sign fixed
                     for (i = width; i; i--)
                     {
                        png_byte v[8];
                        int j;
                        sptr -= 4;
                        png_memcpy(v, sptr, 4);
                        for (j = 0; j < png_pass_inc[pass]; j++)
                        {
                           dp -= 4;
                           png_memcpy(dp, v, 4);
                        }
                     }
                  }
                  else if (width)  // pass == 4 or 5
                  {
                     int width_mmx = ((width >> 1) << 1) ;
                     width -= width_mmx;        // 0,1 pixels => 0,4 bytes
                     if (width_mmx)
                     {
                        int dummy_value_c;  // fix 'forbidden register spilled'
                        int dummy_value_S;
                        int dummy_value_D;
                        __asm__ __volatile__ (
                           "subl $4, %%esi          \n\t"
                           "subl $12, %%edi         \n\t"

                        ".loop4_pass4:              \n\t"
                           "movq (%%esi), %%mm0     \n\t" // 7 6 5 4 3 2 1 0
                           "movq %%mm0, %%mm1       \n\t" // 7 6 5 4 3 2 1 0
                           "punpckldq %%mm0, %%mm0  \n\t" // 3 2 1 0 3 2 1 0
                           "punpckhdq %%mm1, %%mm1  \n\t" // 7 6 5 4 7 6 5 4
                           "movq %%mm0, (%%edi)     \n\t"
                           "subl $8, %%esi          \n\t"
                           "movq %%mm1, 8(%%edi)    \n\t"
                           "subl $16, %%edi         \n\t"
                           "subl $2, %%ecx          \n\t"
                           "jnz .loop4_pass4        \n\t"
                           "EMMS                    \n\t" // DONE

                           : "=c" (dummy_value_c),           // output regs (dummy)
                             "=S" (dummy_value_S),
                             "=D" (dummy_value_D)

                           : "1" (sptr),      // esi      // input regs
                             "2" (dp),        // edi
                             "0" (width_mmx)  // ecx

//                           :        // clobber list
#if 0  /* %mm0, %mm1 not supported by gcc 2.7.2.3 or egcs 1.1 */
                           : "%mm0", "%mm1"
#endif
                        );
                     }

                     sptr -= (width_mmx*4 - 4); // sign fixed
                     dp -= (width_mmx*8 - 4);   // sign fixed
                     for (i = width; i; i--)
                     {
                        png_byte v[8];
                        int j;
                        sptr -= 4;
                        png_memcpy(v, sptr, 4);
                        for (j = 0; j < png_pass_inc[pass]; j++)
                        {
                           dp -= 4;
                           png_memcpy(dp, v, 4);
                        }
                     }
                  }
               } /* end of pixel_bytes == 4 */

#define STILL_WORKING_ON_THIS
#ifdef STILL_WORKING_ON_THIS  // GRR: should work, but needs testing
                              //      (special 64-bit version of rpng2)

               //--------------------------------------------------------------
               else if (pixel_bytes == 8)
               {
                  // GRR NOTE:  no need to combine passes here!
                  if (((pass == 0) || (pass == 1)) && width)
                  {
                     // source is 8-byte RRGGBBAA
                     // dest is 64-byte RRGGBBAA RRGGBBAA RRGGBBAA RRGGBBAA ...
                        int dummy_value_c;  // fix 'forbidden register spilled'
                        int dummy_value_S;
                        int dummy_value_D;
#ifdef GRR_DEBUG
                        FILE *junk = fopen("junk.8bytes", "wb");
                        if (junk)
                            fclose(junk);
#endif /* GRR_DEBUG */
                        __asm__ __volatile__ (
                           "subl $56, %%edi         \n\t" // start of last block

                        ".loop8_pass0:              \n\t"
                           "movq (%%esi), %%mm0     \n\t" // 7 6 5 4 3 2 1 0
                           "movq %%mm0, (%%edi)     \n\t"
                           "movq %%mm0, 8(%%edi)    \n\t"
                           "movq %%mm0, 16(%%edi)   \n\t"
                           "movq %%mm0, 24(%%edi)   \n\t"
                           "movq %%mm0, 32(%%edi)   \n\t"
                           "movq %%mm0, 40(%%edi)   \n\t"
                           "movq %%mm0, 48(%%edi)   \n\t"
                           "subl $8, %%esi          \n\t"
                           "movq %%mm0, 56(%%edi)   \n\t"
                           "subl $64, %%edi         \n\t"
                           "decl %%ecx              \n\t"
                           "jnz .loop8_pass0        \n\t"
                           "EMMS                    \n\t" // DONE

                           : "=c" (dummy_value_c),           // output regs (dummy)
                             "=S" (dummy_value_S),
                             "=D" (dummy_value_D)

                           : "1" (sptr),      // esi      // input regs
                             "2" (dp),        // edi
                             "0" (width)      // ecx

//                           :        // clobber list
#if 0  /* %mm0 not supported by gcc 2.7.2.3 or egcs 1.1 */
                           : "%mm0"
#endif
                        );
                  }
                  else if (((pass == 2) || (pass == 3)) && width)
                  {
                     // source is 8-byte RRGGBBAA
                     // dest is 32-byte RRGGBBAA RRGGBBAA RRGGBBAA RRGGBBAA
                     int width_mmx = ((width >> 1) << 1) ;
                     width -= width_mmx;
                     if (width_mmx)
                     {
                        int dummy_value_c;  // fix 'forbidden register spilled'
                        int dummy_value_S;
                        int dummy_value_D;
                        __asm__ __volatile__ (
                           "subl $24, %%edi         \n\t" // start of last block

                        ".loop8_pass2:              \n\t"
                           "movq (%%esi), %%mm0     \n\t" // 7 6 5 4 3 2 1 0
                           "movq %%mm0, (%%edi)     \n\t"
                           "movq %%mm0, 8(%%edi)    \n\t"
                           "movq %%mm0, 16(%%edi)   \n\t"
                           "subl $8, %%esi          \n\t"
                           "movq %%mm0, 24(%%edi)   \n\t"
                           "subl $32, %%edi         \n\t"
                           "decl %%ecx              \n\t"
                           "jnz .loop8_pass2        \n\t"
                           "EMMS                    \n\t" // DONE

                           : "=c" (dummy_value_c),           // output regs (dummy)
                             "=S" (dummy_value_S),
                             "=D" (dummy_value_D)

                           : "1" (sptr),      // esi      // input regs
                             "2" (dp),        // edi
                             "0" (width)      // ecx

//                           :        // clobber list
#if 0  /* %mm0 not supported by gcc 2.7.2.3 or egcs 1.1 */
                           : "%mm0"
#endif
                        );
                     }
                  }
                  else if (width)  // pass == 4 or 5
                  {
                     // source is 8-byte RRGGBBAA
                     // dest is 16-byte RRGGBBAA RRGGBBAA
                     int width_mmx = ((width >> 1) << 1) ;
                     width -= width_mmx;
                     if (width_mmx)
                     {
                        int dummy_value_c;  // fix 'forbidden register spilled'
                        int dummy_value_S;
                        int dummy_value_D;
                        __asm__ __volatile__ (
                           "subl $8, %%edi          \n\t" // start of last block

                        ".loop8_pass4:              \n\t"
                           "movq (%%esi), %%mm0     \n\t" // 7 6 5 4 3 2 1 0
                           "movq %%mm0, (%%edi)     \n\t"
                           "subl $8, %%esi          \n\t"
                           "movq %%mm0, 8(%%edi)    \n\t"
                           "subl $16, %%edi         \n\t"
                           "decl %%ecx              \n\t"
                           "jnz .loop8_pass4        \n\t"
                           "EMMS                    \n\t" // DONE

                           : "=c" (dummy_value_c),           // output regs (dummy)
                             "=S" (dummy_value_S),
                             "=D" (dummy_value_D)

                           : "1" (sptr),      // esi      // input regs
                             "2" (dp),        // edi
                             "0" (width)      // ecx

//                           :        // clobber list
#if 0  /* %mm0 not supported by gcc 2.7.2.3 or egcs 1.1 */
                           : "%mm0"
#endif
                        );
                     }
                  }

               } /* end of pixel_bytes == 8 */

#endif /* STILL_WORKING_ON_THIS */

               //--------------------------------------------------------------
               else if (pixel_bytes == 6)
               {
                  for (i = width; i; i--)
                  {
                     png_byte v[8];
                     int j;
                     png_memcpy(v, sptr, 6);
                     for (j = 0; j < png_pass_inc[pass]; j++)
                     {
                        png_memcpy(dp, v, 6);
                        dp -= 6;
                     }
                     sptr -= 6;
                  }
               } /* end of pixel_bytes == 6 */

               //--------------------------------------------------------------
               else
               {
                  for (i = width; i; i--)
                  {
                     png_byte v[8];
                     int j;
                     png_memcpy(v, sptr, pixel_bytes);
                     for (j = 0; j < png_pass_inc[pass]; j++)
                     {
                        png_memcpy(dp, v, pixel_bytes);
                        dp -= pixel_bytes;
                     }
                     sptr-= pixel_bytes;
                  }
               }
            } // end of mmx_supported =========================================

            else /* MMX not supported:  use modified C code - takes advantage
                  *   of inlining of memcpy for a constant */
                 /* GRR 19991007:  does it?  or should pixel_bytes in each
                  *   block be replaced with immediate value (e.g., 1)? */
                 /* GRR 19991017:  replaced with constants in each case */
            {
               if (pixel_bytes == 1)
               {
                  for (i = width; i; i--)
                  {
                     int j;
                     for (j = 0; j < png_pass_inc[pass]; j++)
                        *dp-- = *sptr;
                     --sptr;
                  }
               }
               else if (pixel_bytes == 3)
               {
                  for (i = width; i; i--)
                  {
                     png_byte v[8];
                     int j;
                     png_memcpy(v, sptr, 3);
                     for (j = 0; j < png_pass_inc[pass]; j++)
                     {
                        png_memcpy(dp, v, 3);
                        dp -= 3;
                     }
                     sptr -= 3;
                  }
               }
               else if (pixel_bytes == 2)
               {
                  for (i = width; i; i--)
                  {
                     png_byte v[8];
                     int j;
                     png_memcpy(v, sptr, 2);
                     for (j = 0; j < png_pass_inc[pass]; j++)
                     {
                        png_memcpy(dp, v, 2);
                        dp -= 2;
                     }
                     sptr -= 2;
                  }
               }
               else if (pixel_bytes == 4)
               {
                  for (i = width; i; i--)
                  {
                     png_byte v[8];
                     int j;
                     png_memcpy(v, sptr, 4);
                     for (j = 0; j < png_pass_inc[pass]; j++)
                     {
                        png_memcpy(dp, v, 4);
                        dp -= 4;
                     }
                     sptr -= 4;
                  }
               }
               else if (pixel_bytes == 6)
               {
                  for (i = width; i; i--)
                  {
                     png_byte v[8];
                     int j;
                     png_memcpy(v, sptr, 6);
                     for (j = 0; j < png_pass_inc[pass]; j++)
                     {
                        png_memcpy(dp, v, 6);
                        dp -= 6;
                     }
                     sptr -= 6;
                  }
               }
               else if (pixel_bytes == 8)
               {
                  for (i = width; i; i--)
                  {
                     png_byte v[8];
                     int j;
                     png_memcpy(v, sptr, 8);
                     for (j = 0; j < png_pass_inc[pass]; j++)
                     {
                        png_memcpy(dp, v, 8);
                        dp -= 8;
                     }
                     sptr -= 8;
                  }
               }
               else     // GRR:  should never be reached
               {
                  for (i = width; i; i--)
                  {
                     png_byte v[8];
                     int j;
                     png_memcpy(v, sptr, pixel_bytes);
                     for (j = 0; j < png_pass_inc[pass]; j++)
                     {
                        png_memcpy(dp, v, pixel_bytes);
                        dp -= pixel_bytes;
                     }
                     sptr -= pixel_bytes;
                  }
               }

            } /* end if (MMX not supported) */
            break;
         }
      } /* end switch (row_info->pixel_depth) */

      row_info->width = final_width;
      row_info->rowbytes = ((final_width *
         (png_uint_32)row_info->pixel_depth + 7) >> 3);
   }

} /* end png_do_read_interlace() */

#endif /* PNG_HAVE_ASSEMBLER_READ_INTERLACE */
#endif /* PNG_READ_INTERLACING_SUPPORTED */


// These variables are utilized in the functions below.  They are declared
// globally here to ensure alignment on 8-byte boundaries.

union uAll {
   long long use;
   double  align;
} LBCarryMask = {0x0101010101010101LL},
  HBClearMask = {0x7f7f7f7f7f7f7f7fLL},
  ActiveMask, ActiveMask2, ActiveMaskEnd, ShiftBpp, ShiftRem;


// Optimized code for PNG Average filter decoder
void /* PRIVATE */
png_read_filter_row_mmx_avg(png_row_infop row_info, png_bytep row,
                            png_bytep prev_row)
{
   int bpp;
   int dummy_value_c;   // fix 'forbidden register 2 (cx) was spilled' error
   int dummy_value_S;
   int dummy_value_D;
// int diff;  GRR: global now (shortened to dif/_dif)

   bpp = (row_info->pixel_depth + 7) >> 3;  // Get # bytes per pixel
   _FullLength  = row_info->rowbytes;        // # of bytes to filter
   __asm__ __volatile__ (
      // Init address pointers and offset
//GRR "movl row, %%edi             \n\t" // edi ==> Avg(x)
      "xorl %%ebx, %%ebx           \n\t" // ebx ==> x
      "movl %%edi, %%edx           \n\t"
//GRR "movl prev_row, %%esi        \n\t" // esi ==> Prior(x)
//GRR "subl bpp, %%edx             \n\t" // (bpp is preloaded into ecx)
      "subl %%ecx, %%edx           \n\t" // edx ==> Raw(x-bpp)

      "xorl %%eax,%%eax            \n\t"

      // Compute the Raw value for the first bpp bytes
      //    Raw(x) = Avg(x) + (Prior(x)/2)
   "avg_rlp:                       \n\t"
      "movb (%%esi,%%ebx,),%%al    \n\t" // Load al with Prior(x)
      "incl %%ebx                  \n\t"
      "shrb %%al                   \n\t" // divide by 2
      "addb -1(%%edi,%%ebx,),%%al  \n\t" // add Avg(x); -1 to offset inc ebx
//GRR "cmpl bpp, %%ebx             \n\t" // (bpp is preloaded into ecx)
      "cmpl %%ecx, %%ebx           \n\t"
      "movb %%al,-1(%%edi,%%ebx,)  \n\t" // write Raw(x); -1 to offset inc ebx
      "jb avg_rlp                  \n\t" // mov does not affect flags

      // get # of bytes to alignment
      "movl %%edi, _dif            \n\t" // take start of row
      "addl %%ebx, _dif            \n\t" // add bpp
      "addl $0xf, _dif             \n\t" // add 7+8 to incr past alignment bdry
      "andl $0xfffffff8, _dif      \n\t" // mask to alignment boundary
      "subl %%edi, _dif            \n\t" // subtract from start => value ebx at alignment
      "jz avg_go                   \n\t"

      // fix alignment
      // Compute the Raw value for the bytes up to the alignment boundary
      //    Raw(x) = Avg(x) + ((Raw(x-bpp) + Prior(x))/2)
      "xorl %%ecx, %%ecx           \n\t"
   "avg_lp1:                       \n\t"
      "xorl %%eax, %%eax           \n\t"
      "movb (%%esi,%%ebx,), %%cl   \n\t" // load cl with Prior(x)
      "movb (%%edx,%%ebx,), %%al   \n\t" // load al with Raw(x-bpp)
      "addw %%cx, %%ax             \n\t"
      "incl %%ebx                  \n\t"
      "shrw %%ax                   \n\t" // divide by 2
      "addb -1(%%edi,%%ebx,), %%al \n\t" // add Avg(x); -1 to offset inc ebx
      "cmpl _dif, %%ebx            \n\t" // check if at alignment boundary
      "movb %%al, -1(%%edi,%%ebx,) \n\t" // write Raw(x); -1 to offset inc ebx
      "jb avg_lp1                  \n\t" // repeat until at alignment boundary

   "avg_go:                        \n\t"
      "movl _FullLength, %%eax     \n\t"
      "movl %%eax, %%ecx           \n\t"
      "subl %%ebx, %%eax           \n\t" // subtract alignment fix
      "andl $0x00000007, %%eax     \n\t" // calc bytes over mult of 8
      "subl %%eax, %%ecx           \n\t" // drop over bytes from original length
      "movl %%ecx, _MMXLength      \n\t"

      : "=c" (dummy_value_c), // output regs/vars here, e.g., "=m" (_MMXLength) instead of final instr
        "=S" (dummy_value_S),
        "=D" (dummy_value_D)

      : "1" (prev_row),  // esi          // input regs
        "2" (row),       // edi
        "0" (bpp)        // ecx

      : "%eax", "%ebx",          // clobber list
        "%edx"
// GRR: INCLUDE "memory" as clobbered? (_dif, _MMXLength)     PROBABLY
   );

#ifdef GRR_GCC_MMX_CONVERTED
   // Now do the math for the rest of the row
   switch ( bpp )
   {
      case 3:
      {
         ActiveMask.use  = 0x0000000000ffffff;
         ShiftBpp.use = 24;    // == 3 * 8
         ShiftRem.use = 40;    // == 64 - 24
         __asm__ (
            // Re-init address pointers and offset
            "movq $ActiveMask, %%mm7     \n\t"
            "movl _dif, %%ebx            \n\t" // ebx ==> x = offset to alignment boundary
            "movq $LBCarryMask, %%mm5    \n\t"
            "movl row, %%edi             \n\t" // edi ==> Avg(x)
            "movq $HBClearMask, %%mm4    \n\t"
            "movl prev_row, %%esi        \n\t" // esi ==> Prior(x)
            // PRIME the pump (load the first Raw(x-bpp) data set)
            "movq -8(%%edi,%%ebx,), %%mm2 \n\t" // Load previous aligned 8 bytes
                                          // (we correct position in loop below)
         "avg_3lp:                       \n\t"
            "movq (%%edi,%%ebx,), %%mm0  \n\t" // Load mm0 with Avg(x)
            // Add (Prev_row/2) to Average
            "movq %%mm5, %%mm3           \n\t"
            "psrlq $ShiftRem, %%mm2      \n\t" // Correct position Raw(x-bpp) data
            "movq (%%esi,%%ebx,), %%mm1  \n\t" // Load mm1 with Prior(x)
            "movq %%mm7, %%mm6           \n\t"
            "pand %%mm1, %%mm3           \n\t" // get lsb for each prev_row byte
            "psrlq $1, %%mm1             \n\t" // divide prev_row bytes by 2
            "pand  %%mm4, %%mm1          \n\t" // clear invalid bit 7 of each byte
            "paddb %%mm1, %%mm0          \n\t" // add (Prev_row/2) to Avg for each byte
            // Add 1st active group (Raw(x-bpp)/2) to Average with LBCarry
            "movq %%mm3, %%mm1           \n\t" // now use mm1 for getting LBCarrys
            "pand %%mm2, %%mm1           \n\t" // get LBCarrys for each byte where both
                               // lsb's were == 1 (Only valid for active group)
            "psrlq $1, %%mm2             \n\t" // divide raw bytes by 2
            "pand  %%mm4, %%mm2          \n\t" // clear invalid bit 7 of each byte
            "paddb %%mm1, %%mm2          \n\t" // add LBCarrys to (Raw(x-bpp)/2) for each byte
            "pand %%mm6, %%mm2           \n\t" // Leave only Active Group 1 bytes to add to Avg
            "paddb %%mm2, %%mm0          \n\t" // add (Raw/2) + LBCarrys to Avg for each Active
                               //  byte
            // Add 2nd active group (Raw(x-bpp)/2) to Average with LBCarry
            "psllq $ShiftBpp, %%mm6      \n\t" // shift the mm6 mask to cover bytes 3-5
            "movq %%mm0, %%mm2           \n\t" // mov updated Raws to mm2
            "psllq $ShiftBpp, %%mm2      \n\t" // shift data to position correctly
            "movq %%mm3, %%mm1           \n\t" // now use mm1 for getting LBCarrys
            "pand %%mm2, %%mm1           \n\t" // get LBCarrys for each byte where both
                               // lsb's were == 1 (Only valid for active group)
            "psrlq $1, %%mm2             \n\t" // divide raw bytes by 2
            "pand  %%mm4, %%mm2          \n\t" // clear invalid bit 7 of each byte
            "paddb %%mm1, %%mm2          \n\t" // add LBCarrys to (Raw(x-bpp)/2) for each byte
            "pand %%mm6, %%mm2           \n\t" // Leave only Active Group 2 bytes to add to Avg
            "paddb %%mm2, %%mm0          \n\t" // add (Raw/2) + LBCarrys to Avg for each Active
                               //  byte

            // Add 3rd active group (Raw(x-bpp)/2) to Average with LBCarry
            "psllq $ShiftBpp, %%mm6      \n\t" // shift the mm6 mask to cover the last two
                                 // bytes
            "movq %%mm0, %%mm2           \n\t" // mov updated Raws to mm2
            "psllq $ShiftBpp, %%mm2      \n\t" // shift data to position correctly
                              // Data only needs to be shifted once here to
                              // get the correct x-bpp offset.
            "movq %%mm3, %%mm1           \n\t" // now use mm1 for getting LBCarrys
            "pand %%mm2, %%mm1           \n\t" // get LBCarrys for each byte where both
                              // lsb's were == 1 (Only valid for active group)
            "psrlq $1, %%mm2             \n\t" // divide raw bytes by 2
            "pand  %%mm4, %%mm2          \n\t" // clear invalid bit 7 of each byte
            "paddb %%mm1, %%mm2          \n\t" // add LBCarrys to (Raw(x-bpp)/2) for each byte
            "pand %%mm6, %%mm2           \n\t" // Leave only Active Group 2 bytes to add to Avg
            "addl $8, %%ebx              \n\t"
            "paddb %%mm2, %%mm0          \n\t" // add (Raw/2) + LBCarrys to Avg for each Active
                                               // byte
            // Now ready to write back to memory
            "movq %%mm0, -8(%%edi,%%ebx,) \n\t"
            // Move updated Raw(x) to use as Raw(x-bpp) for next loop
            "cmpl _MMXLength, %%ebx      \n\t"
            "movq %%mm0, %%mm2           \n\t" // mov updated Raw(x) to mm2
            "jb avg_3lp                  \n\t"

            : // output regs/vars go here, e.g.:  "=m" (memory_var)

            : "S" (prev_row),  // esi          // input regs
              "D" (row)        // edi

            : "%ebx", "%edi", "%esi"           // clobber list
//            GRR: INCLUDE "memory" as clobbered? (_dif, _MMXLength)   PROBABLY
//          , "%mm0", "%mm1", "%mm2", "%mm3",
//            "%mm4", "%mm5", "%mm6", "%mm7"
         );
      }
      break;  // end 3 bpp

      case 6:
      case 4:
      //case 7:   // who wrote this?  PNG doesn't support 5 or 7 bytes/pixel
      //case 5:
      {
         ActiveMask.use  = 0xffffffffffffffff;  // use shift below to clear
                                                // appropriate inactive bytes
         ShiftBpp.use = bpp << 3;
         ShiftRem.use = 64 - ShiftBpp.use;
         __asm__ (
            "movq $HBClearMask, %%mm4    \n\t"

            // Re-init address pointers and offset
            "movl _dif, %%ebx            \n\t" // ebx ==> x = offset to alignment boundary

            // Load ActiveMask and clear all bytes except for 1st active group
            "movq $ActiveMask, %%mm7     \n\t"
            "movl row, %%edi             \n\t" // edi ==> Avg(x)
            "psrlq $ShiftRem, %%mm7      \n\t"
            "movl prev_row, %%esi        \n\t" // esi ==> Prior(x)
            "movq %%mm7, %%mm6           \n\t"
            "movq $LBCarryMask, %%mm5    \n\t"
            "psllq $ShiftBpp, %%mm6      \n\t" // Create mask for 2nd active group

            // PRIME the pump (load the first Raw(x-bpp) data set
            "movq -8(%%edi,%%ebx,), %%mm2 \n\t" // Load previous aligned 8 bytes
                                          // (we correct position in loop below)
         "avg_4lp:                       \n\t"
            "movq (%%edi,%%ebx,), %%mm0  \n\t"
            "psrlq $ShiftRem, %%mm2      \n\t" // shift data to position correctly
            "movq (%%esi,%%ebx,), %%mm1  \n\t"
            // Add (Prev_row/2) to Average
            "movq %%mm5, %%mm3           \n\t"
            "pand %%mm1, %%mm3           \n\t" // get lsb for each prev_row byte
            "psrlq $1, %%mm1             \n\t" // divide prev_row bytes by 2
            "pand  %%mm4, %%mm1          \n\t" // clear invalid bit 7 of each byte
            "paddb %%mm1, %%mm0          \n\t" // add (Prev_row/2) to Avg for each byte
            // Add 1st active group (Raw(x-bpp)/2) to Average with LBCarry
            "movq %%mm3, %%mm1           \n\t" // now use mm1 for getting LBCarrys
            "pand %%mm2, %%mm1           \n\t" // get LBCarrys for each byte where both
                              // lsb's were == 1 (Only valid for active group)
            "psrlq $1, %%mm2             \n\t" // divide raw bytes by 2
            "pand  %%mm4, %%mm2          \n\t" // clear invalid bit 7 of each byte
            "paddb %%mm1, %%mm2          \n\t" // add LBCarrys to (Raw(x-bpp)/2) for each byte
            "pand %%mm7, %%mm2           \n\t" // Leave only Active Group 1 bytes to add to Avg
            "paddb %%mm2, %%mm0          \n\t" // add (Raw/2) + LBCarrys to Avg for each Active
                              // byte
            // Add 2nd active group (Raw(x-bpp)/2) to Average with LBCarry
            "movq %%mm0, %%mm2           \n\t" // mov updated Raws to mm2
            "psllq $ShiftBpp, %%mm2      \n\t" // shift data to position correctly
            "addl $8, %%ebx              \n\t"
            "movq %%mm3, %%mm1           \n\t" // now use mm1 for getting LBCarrys
            "pand %%mm2, %%mm1           \n\t" // get LBCarrys for each byte where both
                              // lsb's were == 1 (Only valid for active group)
            "psrlq $1, %%mm2             \n\t" // divide raw bytes by 2
            "pand  %%mm4, %%mm2          \n\t" // clear invalid bit 7 of each byte
            "paddb %%mm1, %%mm2          \n\t" // add LBCarrys to (Raw(x-bpp)/2) for each byte
            "pand %%mm6, %%mm2           \n\t" // Leave only Active Group 2 bytes to add to Avg
            "paddb %%mm2, %%mm0          \n\t" // add (Raw/2) + LBCarrys to Avg for each Active
                              // byte
            "cmpl _MMXLength, %%ebx      \n\t"
            // Now ready to write back to memory
            "movq %%mm0, -8(%%edi,%%ebx,) \n\t"
            // Prep Raw(x-bpp) for next loop
            "movq %%mm0, %%mm2           \n\t" // mov updated Raws to mm2
            "jb avg_4lp                  \n\t"

            : // FIXASM: output regs/vars go here, e.g.:  "=m" (memory_var)

            : // FIXASM: input regs, e.g.:  "c" (count), "S" (src), "D" (dest)

            : "%ebx", "%edi", "%esi", "%mm0", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5", "%mm6", "%mm7" // CHECKASM: clobber list
         );
      }
      break;  // end 4,6 bpp

      case 2:
      {
         ActiveMask.use  = 0x000000000000ffff;
         ShiftBpp.use = 24;   // == 3 * 8
         ShiftRem.use = 40;   // == 64 - 24
         __asm__ (
            // Load ActiveMask
            "movq $ActiveMask, %%mm7     \n\t"
            // Re-init address pointers and offset
            "movl _dif, %%ebx            \n\t" // ebx ==> x = offset to alignment boundary
            "movq $LBCarryMask, %%mm5    \n\t"
            "movl row, %%edi             \n\t" // edi ==> Avg(x)
            "movq $HBClearMask, %%mm4    \n\t"
            "movl prev_row, %%esi        \n\t" // esi ==> Prior(x)
            // PRIME the pump (load the first Raw(x-bpp) data set
            "movq -8(%%edi,%%ebx,), %%mm2 \n\t" // Load previous aligned 8 bytes
                              // (we correct position in loop below)
         "avg_2lp:                       \n\t"
            "movq (%%edi,%%ebx,), %%mm0  \n\t"
            "psllq $ShiftRem, %%mm2      \n\t" // shift data to position correctly
            "movq (%%esi,%%ebx,), %%mm1  \n\t"
            // Add (Prev_row/2) to Average
            "movq %%mm5, %%mm3           \n\t"
            "pand %%mm1, %%mm3           \n\t" // get lsb for each prev_row byte
            "psrlq $1, %%mm1             \n\t" // divide prev_row bytes by 2
            "pand  %%mm4, %%mm1          \n\t" // clear invalid bit 7 of each byte
            "movq %%mm7, %%mm6           \n\t"
            "paddb %%mm1, %%mm0          \n\t" // add (Prev_row/2) to Avg for each byte
            // Add 1st active group (Raw(x-bpp)/2) to Average with LBCarry
            "movq %%mm3, %%mm1           \n\t" // now use mm1 for getting LBCarrys
            "pand %%mm2, %%mm1           \n\t" // get LBCarrys for each byte where both
                              // lsb's were == 1 (Only valid for active group)
            "psrlq $1, %%mm2             \n\t" // divide raw bytes by 2
            "pand  %%mm4, %%mm2          \n\t" // clear invalid bit 7 of each byte
            "paddb %%mm1, %%mm2          \n\t" // add LBCarrys to (Raw(x-bpp)/2) for each byte
            "pand %%mm6, %%mm2           \n\t" // Leave only Active Group 1 bytes to add to Avg
            "paddb %%mm2, %%mm0          \n\t" // add (Raw/2) + LBCarrys to Avg for each Active byte
            // Add 2nd active group (Raw(x-bpp)/2) to Average with LBCarry
            "psllq $ShiftBpp, %%mm6      \n\t" // shift the mm6 mask to cover bytes 2 & 3
            "movq %%mm0, %%mm2           \n\t" // mov updated Raws to mm2
            "psllq $ShiftBpp, %%mm2      \n\t" // shift data to position correctly
            "movq %%mm3, %%mm1           \n\t" // now use mm1 for getting LBCarrys
            "pand %%mm2, %%mm1           \n\t" // get LBCarrys for each byte where both
                                // lsb's were == 1 (Only valid for active group)
            "psrlq $1, %%mm2             \n\t" // divide raw bytes by 2
            "pand  %%mm4, %%mm2          \n\t" // clear invalid bit 7 of each byte
            "paddb %%mm1, %%mm2          \n\t" // add LBCarrys to (Raw(x-bpp)/2) for each byte
            "pand %%mm6, %%mm2           \n\t" // Leave only Active Group 2 bytes to add to Avg
            "paddb %%mm2, %%mm0          \n\t" // add (Raw/2) + LBCarrys to Avg for each Active byte

            // Add rdd active group (Raw(x-bpp)/2) to Average with LBCarry
            "psllq $ShiftBpp, %%mm6      \n\t" // shift the mm6 mask to cover bytes 4 & 5
            "movq %%mm0, %%mm2           \n\t" // mov updated Raws to mm2
            "psllq $ShiftBpp, %%mm2      \n\t" // shift data to position correctly
                                // Data only needs to be shifted once here to
                                // get the correct x-bpp offset.
            "movq %%mm3, %%mm1           \n\t" // now use mm1 for getting LBCarrys
            "pand %%mm2, %%mm1           \n\t" // get LBCarrys for each byte where both
                                // lsb's were == 1 (Only valid for active group)
            "psrlq $1, %%mm2             \n\t" // divide raw bytes by 2
            "pand  %%mm4, %%mm2          \n\t" // clear invalid bit 7 of each byte
            "paddb %%mm1, %%mm2          \n\t" // add LBCarrys to (Raw(x-bpp)/2) for each byte
            "pand %%mm6, %%mm2           \n\t" // Leave only Active Group 2 bytes to add to Avg
            "paddb %%mm2, %%mm0          \n\t" // add (Raw/2) + LBCarrys to Avg for each Active byte

            // Add 4th active group (Raw(x-bpp)/2) to Average with LBCarry
            "psllq $ShiftBpp, %%mm6      \n\t" // shift the mm6 mask to cover bytes 6 & 7
            "movq %%mm0, %%mm2           \n\t" // mov updated Raws to mm2
            "psllq $ShiftBpp, %%mm2      \n\t" // shift data to position correctly
                                 // Data only needs to be shifted once here to
                                 // get the correct x-bpp offset.
            "addl $8, %%ebx              \n\t"
            "movq %%mm3, %%mm1           \n\t" // now use mm1 for getting LBCarrys
            "pand %%mm2, %%mm1           \n\t" // get LBCarrys for each byte where both
                             // lsb's were == 1 (Only valid for active group)
            "psrlq $1, %%mm2             \n\t" // divide raw bytes by 2
            "pand  %%mm4, %%mm2          \n\t" // clear invalid bit 7 of each byte
            "paddb %%mm1, %%mm2          \n\t" // add LBCarrys to (Raw(x-bpp)/2) for each byte
            "pand %%mm6, %%mm2           \n\t" // Leave only Active Group 2 bytes to add to Avg
            "paddb %%mm2, %%mm0          \n\t" // add (Raw/2) + LBCarrys to Avg for each Active byte

            "cmpl _MMXLength, %%ebx      \n\t"
            // Now ready to write back to memory
            "movq %%mm0, -8(%%edi,%%ebx,) \n\t"
            // Prep Raw(x-bpp) for next loop
            "movq %%mm0, %%mm2           \n\t" // mov updated Raws to mm2
            "jb avg_2lp                  \n\t"

            : // FIXASM: output regs/vars go here, e.g.:  "=m" (memory_var)

            : // FIXASM: input regs, e.g.:  "c" (count), "S" (src), "D" (dest)

            : "%ebx", "%edi", "%esi", "%mm0", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5", "%mm6", "%mm7" // CHECKASM: clobber list
         );
      }
      break;  // end 2 bpp

      case 1:
      {
         __asm__ (
            // Re-init address pointers and offset
            "movl _dif, %%ebx            \n\t" // ebx ==> x = offset to alignment boundary
            "movl row, %%edi             \n\t" // edi ==> Avg(x)
            "cmpl _FullLength, %%ebx     \n\t" // Test if offset at end of array
            "jnb avg_1end                \n\t"
            // Do Paeth decode for remaining bytes
            "movl prev_row, %%esi        \n\t" // esi ==> Prior(x)
            "movl %%edi, %%edx           \n\t"
            "xorl %%ecx, %%ecx           \n\t" // zero ecx before using cl & cx in loop below
            "subl bpp, %%edx             \n\t" // edx ==> Raw(x-bpp)
         "avg_1lp:                       \n\t"
            // Raw(x) = Avg(x) + ((Raw(x-bpp) + Prior(x))/2)
            "xorl %%eax, %%eax           \n\t"
            "movb (%%esi,%%ebx,), %%cl   \n\t" // load cl with Prior(x)
            "movb (%%edx,%%ebx,), %%al   \n\t" // load al with Raw(x-bpp)
            "addw %%cx, %%ax             \n\t"
            "incl %%ebx                  \n\t"
            "shrw %%ax                   \n\t" // divide by 2
            "addb -1(%%edi,%%ebx,), %%al \n\t" // Add Avg(x); -1 to offset inc ebx
            "cmpl _FullLength, %%ebx     \n\t" // Check if at end of array
            "movb %%al, -1(%%edi,%%ebx,) \n\t" // Write back Raw(x);
                         // mov does not affect flags; -1 to offset inc ebx
            "jb avg_1lp                  \n\t"
         "avg_1end:                      \n\t"

            : // FIXASM: output regs/vars go here, e.g.:  "=m" (memory_var)

            : // FIXASM: input regs, e.g.:  "c" (count), "S" (src), "D" (dest)

            : "%eax", "%ebx", "%ecx", "%edx", "%edi", "%esi" // CHECKASM: clobber list
         );
      }
      return;  // end 1 bpp

      case 8:
      {
         __asm__ (
            // Re-init address pointers and offset
            "movl _dif, %%ebx            \n\t" // ebx ==> x = offset to alignment boundary
            "movq $LBCarryMask, %%mm5    \n\t"
            "movl row, %%edi             \n\t" // edi ==> Avg(x)
            "movq $HBClearMask, %%mm4    \n\t"
            "movl prev_row, %%esi        \n\t" // esi ==> Prior(x)
            // PRIME the pump (load the first Raw(x-bpp) data set
            "movq -8(%%edi,%%ebx,), %%mm2 \n\t" // Load previous aligned 8 bytes
                                // (NO NEED to correct position in loop below)
         "avg_8lp:                       \n\t"
            "movq (%%edi,%%ebx,), %%mm0  \n\t"
            "movq %%mm5, %%mm3           \n\t"
            "movq (%%esi,%%ebx,), %%mm1  \n\t"
            "addl $8, %%ebx              \n\t"
            "pand %%mm1, %%mm3           \n\t" // get lsb for each prev_row byte
            "psrlq $1, %%mm1             \n\t" // divide prev_row bytes by 2
            "pand %%mm2, %%mm3           \n\t" // get LBCarrys for each byte where both
                                // lsb's were == 1
            "psrlq $1, %%mm2             \n\t" // divide raw bytes by 2
            "pand  %%mm4, %%mm1          \n\t" // clear invalid bit 7 of each byte
            "paddb %%mm3, %%mm0          \n\t" // add LBCarrys to Avg for each byte
            "pand  %%mm4, %%mm2          \n\t" // clear invalid bit 7 of each byte
            "paddb %%mm1, %%mm0          \n\t" // add (Prev_row/2) to Avg for each byte
            "paddb %%mm2, %%mm0          \n\t" // add (Raw/2) to Avg for each byte
            "cmpl _MMXLength, %%ebx      \n\t"
            "movq %%mm0, -8(%%edi,%%ebx,) \n\t"
            "movq %%mm0, %%mm2           \n\t" // reuse as Raw(x-bpp)
            "jb avg_8lp                  \n\t"

            : // FIXASM: output regs/vars go here, e.g.:  "=m" (memory_var)

            : // FIXASM: input regs, e.g.:  "c" (count), "S" (src), "D" (dest)

            : "%ebx", "%edi", "%esi", "%mm0", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5" // CHECKASM: clobber list
         );
      }
      break;  // end 8 bpp

      default:                  // bpp greater than 8 (!= 1,2,3,4,6,8)
      {

      GRR:  PRINT ERROR HERE:  SHOULD NEVER BE REACHED (unless smaller than 1?)

        __asm__ (
            "movq $LBCarryMask, %%mm5    \n\t"
            // Re-init address pointers and offset
            "movl _dif, %%ebx            \n\t" // ebx ==> x = offset to alignment boundary
            "movl row, %%edi             \n\t" // edi ==> Avg(x)
            "movq $HBClearMask, %%mm4    \n\t"
            "movl %%edi, %%edx           \n\t"
            "movl prev_row, %%esi        \n\t" // esi ==> Prior(x)
            "subl bpp, %%edx             \n\t" // edx ==> Raw(x-bpp)
         "avg_Alp:                       \n\t"
            "movq (%%edi,%%ebx,), %%mm0  \n\t"
            "movq %%mm5, %%mm3           \n\t"
            "movq (%%esi,%%ebx,), %%mm1  \n\t"
            "pand %%mm1, %%mm3           \n\t" // get lsb for each prev_row byte
            "movq (%%edx,%%ebx,), %%mm2  \n\t"
            "psrlq $1, %%mm1             \n\t" // divide prev_row bytes by 2
            "pand %%mm2, %%mm3           \n\t" // get LBCarrys for each byte where both
                                // lsb's were == 1
            "psrlq $1, %%mm2             \n\t" // divide raw bytes by 2
            "pand  %%mm4, %%mm1          \n\t" // clear invalid bit 7 of each byte
            "paddb %%mm3, %%mm0          \n\t" // add LBCarrys to Avg for each byte
            "pand  %%mm4, %%mm2          \n\t" // clear invalid bit 7 of each byte
            "paddb %%mm1, %%mm0          \n\t" // add (Prev_row/2) to Avg for each byte
            "addl $8, %%ebx              \n\t"
            "paddb %%mm2, %%mm0          \n\t" // add (Raw/2) to Avg for each byte
            "cmpl _MMXLength, %%ebx      \n\t"
            "movq %%mm0, -8(%%edi,%%ebx,) \n\t"
            "jb avg_Alp                  \n\t"

            : // FIXASM: output regs/vars go here, e.g.:  "=m" (memory_var)

            : // FIXASM: input regs, e.g.:  "c" (count), "S" (src), "D" (dest)

            : "%ebx", "%edx", "%edi", "%esi", "%mm0", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5" // CHECKASM: clobber list
         );
      }
      break;
   }                         // end switch ( bpp )

   __asm__ (
      // MMX acceleration complete now do clean-up
      // Check if any remaining bytes left to decode
      "movl _MMXLength, %%ebx      \n\t" // ebx ==> x = offset bytes remaining after MMX
      "movl row, %%edi             \n\t" // edi ==> Avg(x)
      "cmpl _FullLength, %%ebx     \n\t" // Test if offset at end of array
      "jnb avg_end                 \n\t"
      // Do Paeth decode for remaining bytes
      "movl prev_row, %%esi        \n\t" // esi ==> Prior(x)
      "movl %%edi, %%edx           \n\t"
      "xorl %%ecx, %%ecx           \n\t" // zero ecx before using cl & cx in loop below
      "subl bpp, %%edx             \n\t" // edx ==> Raw(x-bpp)
   "avg_lp2:                       \n\t"
      // Raw(x) = Avg(x) + ((Raw(x-bpp) + Prior(x))/2)
      "xorl %%eax, %%eax           \n\t"
      "movb (%%esi,%%ebx,), %%cl   \n\t" // load cl with Prior(x)
      "movb (%%edx,%%ebx,), %%al   \n\t" // load al with Raw(x-bpp)
      "addw %%cx, %%ax             \n\t"
      "incl %%ebx                  \n\t"
      "shrw %%ax                   \n\t" // divide by 2
      "addb -1(%%edi,%%ebx,), %%al \n\t" // Add Avg(x); -1 to offset inc ebx
      "cmpl _FullLength, %%ebx     \n\t" // Check if at end of array
      "movb %%al, -1(%%edi,%%ebx,) \n\t" // Write back Raw(x);
                       // mov does not affect flags; -1 to offset inc ebx
      "jb avg_lp2                  \n\t"
   "avg_end:                       \n\t"
      "emms                        \n\t" // End MMX instructions; prep for possible FP instrs.

      : // FIXASM: output regs/vars go here, e.g.:  "=m" (memory_var)

      : // FIXASM: input regs, e.g.:  "c" (count), "S" (src), "D" (dest)

      : "%eax", "%ebx", "%ecx", "%edx", "%edi", "%esi" // CHECKASM: clobber list
   );
#endif /* GRR_GCC_MMX_CONVERTED */
}

// Optimized code for PNG Paeth filter decoder
void /* PRIVATE */
png_read_filter_row_mmx_paeth(png_row_infop row_info, png_bytep row,
                              png_bytep prev_row)
{
#ifdef GRR_GCC_MMX_CONVERTED
   int bpp;
   int patemp, pbtemp, pctemp;

   bpp = (row_info->pixel_depth + 7) >> 3; // Get # bytes per pixel
   _FullLength  = row_info->rowbytes; // # of bytes to filter
   __asm__ (
      "xorl %%ebx, %%ebx           \n\t" // ebx ==> x offset
      "movl row, %%edi             \n\t"
      "xorl %%edx, %%edx           \n\t" // edx ==> x-bpp offset
      "movl prev_row, %%esi        \n\t"
      "xorl %%eax, %%eax           \n\t"

      // Compute the Raw value for the first bpp bytes
      // Note: the formula works out to be always
      //   Paeth(x) = Raw(x) + Prior(x)      where x < bpp
   "paeth_rlp:                     \n\t"
      "movb (%%edi,%%ebx,), %%al   \n\t"
      "addb (%%esi,%%ebx,), %%al   \n\t"
      "incl %%ebx                  \n\t"
      "cmpl bpp, %%ebx             \n\t"
      "movb %%al, -1(%%edi,%%ebx,) \n\t"
      "jb paeth_rlp                \n\t"
      // get # of bytes to alignment
      "movl %%edi, _dif            \n\t" // take start of row
      "addl %%ebx, _dif            \n\t" // add bpp
      "xorl %%ecx, %%ecx           \n\t"
      "addl $0xf, _dif             \n\t" // add 7 + 8 to incr past alignment boundary
      "andl $0xfffffff8, _dif      \n\t" // mask to alignment boundary
      "subl %%edi, _dif            \n\t" // subtract from start ==> value ebx at alignment
      "jz paeth_go                 \n\t"
      // fix alignment
   "paeth_lp1:                     \n\t"
      "xorl %%eax, %%eax           \n\t"
      // pav = p - a = (a + b - c) - a = b - c
      "movb (%%esi,%%ebx,), %%al   \n\t" // load Prior(x) into al
      "movb (%%esi,%%edx,), %%cl   \n\t" // load Prior(x-bpp) into cl
      "subl %%ecx, %%eax           \n\t" // subtract Prior(x-bpp)
      "movl %%eax, patemp          \n\t" // Save pav for later use
      "xorl %%eax, %%eax           \n\t"
      // pbv = p - b = (a + b - c) - b = a - c
      "movb (%%edi,%%edx,), %%al   \n\t" // load Raw(x-bpp) into al
      "subl %%ecx, %%eax           \n\t" // subtract Prior(x-bpp)
      "movl %%eax, %%ecx           \n\t"
      // pcv = p - c = (a + b - c) -c = (a - c) + (b - c) = pav + pbv
      "addl patemp, %%eax          \n\t" // pcv = pav + pbv
      // pc = abs(pcv)
      "testl $0x80000000, %%eax    \n\t"
      "jz paeth_pca                \n\t"
      "negl %%eax                  \n\t" // reverse sign of neg values
   "paeth_pca:                     \n\t"
      "movl %%eax, pctemp          \n\t" // save pc for later use
      // pb = abs(pbv)
      "testl $0x80000000, %%ecx    \n\t"
      "jz paeth_pba                \n\t"
      "negl %%ecx                  \n\t" // reverse sign of neg values
   "paeth_pba:                     \n\t"
      "movl %%ecx, pbtemp          \n\t" // save pb for later use
      // pa = abs(pav)
      "movl patemp, %%eax          \n\t"
      "testl $0x80000000, %%eax    \n\t"
      "jz paeth_paa                \n\t"
      "negl %%eax                  \n\t" // reverse sign of neg values
   "paeth_paa:                     \n\t"
      "movl %%eax, patemp          \n\t" // save pa for later use
      // test if pa <= pb
      "cmpl %%ecx, %%eax           \n\t"
      "jna paeth_abb               \n\t"
      // pa > pb; now test if pb <= pc
      "cmpl pctemp, %%ecx          \n\t"
      "jna paeth_bbc               \n\t"
      // pb > pc; Raw(x) = Paeth(x) + Prior(x-bpp)
      "movb (%%esi,%%edx,), %%cl   \n\t" // load Prior(x-bpp) into cl
      "jmp paeth_paeth             \n\t"
   "paeth_bbc:                     \n\t"
      // pb <= pc; Raw(x) = Paeth(x) + Prior(x)
      "movb (%%esi,%%ebx,), %%cl   \n\t" // load Prior(x) into cl
      "jmp paeth_paeth             \n\t"
   "paeth_abb:                     \n\t"
      // pa <= pb; now test if pa <= pc
      "cmpl pctemp, %%eax          \n\t"
      "jna paeth_abc               \n\t"
      // pa > pc; Raw(x) = Paeth(x) + Prior(x-bpp)
      "movb (%%esi,%%edx,), %%cl   \n\t" // load Prior(x-bpp) into cl
      "jmp paeth_paeth             \n\t"
   "paeth_abc:                     \n\t"
      // pa <= pc; Raw(x) = Paeth(x) + Raw(x-bpp)
      "movb (%%edi,%%edx,), %%cl   \n\t" // load Raw(x-bpp) into cl
   "paeth_paeth:                   \n\t"
      "incl %%ebx                  \n\t"
      "incl %%edx                  \n\t"
      // Raw(x) = (Paeth(x) + Paeth_Predictor( a, b, c )) mod 256
      "addb %%cl, -1(%%edi,%%ebx,) \n\t"
      "cmpl _dif, %%ebx            \n\t"
      "jb paeth_lp1                \n\t"
   "paeth_go:                      \n\t"
      "movl _FullLength, %%ecx     \n\t"
      "movl %%ecx, %%eax           \n\t"
      "subl %%ebx, %%eax           \n\t" // subtract alignment fix
      "andl $0x00000007, %%eax     \n\t" // calc bytes over mult of 8
      "subl %%eax, %%ecx           \n\t" // drop over bytes from original length
      "movl %%ecx, _MMXLength      \n\t"

      : // FIXASM: output regs/vars go here, e.g.:  "=m" (memory_var)

      : // FIXASM: input regs, e.g.:  "c" (count), "S" (src), "D" (dest)

      : "%eax", "%ebx", "%ecx", "%edx", "%edi", "%esi" // CHECKASM: clobber list
   );

   // Now do the math for the rest of the row
   switch ( bpp )
   {
      case 3:
      {
         ActiveMask.use = 0x0000000000ffffff;
         ActiveMaskEnd.use = 0xffff000000000000;
         ShiftBpp.use = 24;    // == bpp(3) * 8
         ShiftRem.use = 40;    // == 64 - 24
         __asm__ (
            "movl _dif, %%ebx            \n\t"
            "movl row, %%edi             \n\t"
            "movl prev_row, %%esi        \n\t"
            "pxor %%mm0, %%mm0           \n\t"
            // PRIME the pump (load the first Raw(x-bpp) data set
            "movq -8(%%edi,%%ebx,), %%mm1 \n\t"
         "paeth_3lp:                     \n\t"
            "psrlq $ShiftRem, %%mm1      \n\t" // shift last 3 bytes to 1st 3 bytes
            "movq (%%esi,%%ebx,), %%mm2  \n\t" // load b=Prior(x)
            "punpcklbw %%mm0, %%mm1      \n\t" // Unpack High bytes of a
            "movq -8(%%esi,%%ebx,), %%mm3 \n\t" // Prep c=Prior(x-bpp) bytes
            "punpcklbw %%mm0, %%mm2      \n\t" // Unpack High bytes of b
            "psrlq $ShiftRem, %%mm3      \n\t" // shift last 3 bytes to 1st 3 bytes
            // pav = p - a = (a + b - c) - a = b - c
            "movq %%mm2, %%mm4           \n\t"
            "punpcklbw %%mm0, %%mm3      \n\t" // Unpack High bytes of c
            // pbv = p - b = (a + b - c) - b = a - c
            "movq %%mm1, %%mm5           \n\t"
            "psubw %%mm3, %%mm4          \n\t"
            "pxor %%mm7, %%mm7           \n\t"
            // pcv = p - c = (a + b - c) -c = (a - c) + (b - c) = pav + pbv
            "movq %%mm4, %%mm6           \n\t"
            "psubw %%mm3, %%mm5          \n\t"

            // pa = abs(p-a) = abs(pav)
            // pb = abs(p-b) = abs(pbv)
            // pc = abs(p-c) = abs(pcv)
            "pcmpgtw %%mm4, %%mm0        \n\t" // Create mask pav bytes < 0
            "paddw %%mm5, %%mm6          \n\t"
            "pand %%mm4, %%mm0           \n\t" // Only pav bytes < 0 in mm7
            "pcmpgtw %%mm5, %%mm7        \n\t" // Create mask pbv bytes < 0
            "psubw %%mm0, %%mm4          \n\t"
            "pand %%mm5, %%mm7           \n\t" // Only pbv bytes < 0 in mm0
            "psubw %%mm0, %%mm4          \n\t"
            "psubw %%mm7, %%mm5          \n\t"
            "pxor %%mm0, %%mm0           \n\t"
            "pcmpgtw %%mm6, %%mm0        \n\t" // Create mask pcv bytes < 0
            "pand %%mm6, %%mm0           \n\t" // Only pav bytes < 0 in mm7
            "psubw %%mm7, %%mm5          \n\t"
            "psubw %%mm0, %%mm6          \n\t"
            //  test pa <= pb
            "movq %%mm4, %%mm7           \n\t"
            "psubw %%mm0, %%mm6          \n\t"
            "pcmpgtw %%mm5, %%mm7        \n\t" // pa > pb?
            "movq %%mm7, %%mm0           \n\t"
            // use mm7 mask to merge pa & pb
            "pand %%mm7, %%mm5           \n\t"
            // use mm0 mask copy to merge a & b
            "pand %%mm0, %%mm2           \n\t"
            "pandn %%mm4, %%mm7          \n\t"
            "pandn %%mm1, %%mm0          \n\t"
            "paddw %%mm5, %%mm7          \n\t"
            "paddw %%mm2, %%mm0          \n\t"
            //  test  ((pa <= pb)? pa:pb) <= pc
            "pcmpgtw %%mm6, %%mm7        \n\t" // pab > pc?
            "pxor %%mm1, %%mm1           \n\t"
            "pand %%mm7, %%mm3           \n\t"
            "pandn %%mm0, %%mm7          \n\t"
            "paddw %%mm3, %%mm7          \n\t"
            "pxor %%mm0, %%mm0           \n\t"
            "packuswb %%mm1, %%mm7       \n\t"
            "movq (%%esi,%%ebx,), %%mm3  \n\t" // load c=Prior(x-bpp)
            "pand $ActiveMask, %%mm7     \n\t"
            "movq %%mm3, %%mm2           \n\t" // load b=Prior(x) step 1
            "paddb (%%edi,%%ebx,), %%mm7 \n\t" // add Paeth predictor with Raw(x)
            "punpcklbw %%mm0, %%mm3      \n\t" // Unpack High bytes of c
            "movq %%mm7, (%%edi,%%ebx,)  \n\t" // write back updated value
            "movq %%mm7, %%mm1           \n\t" // Now mm1 will be used as Raw(x-bpp)
            // Now do Paeth for 2nd set of bytes (3-5)
            "psrlq $ShiftBpp, %%mm2      \n\t" // load b=Prior(x) step 2
            "punpcklbw %%mm0, %%mm1      \n\t" // Unpack High bytes of a
            "pxor %%mm7, %%mm7           \n\t"
            "punpcklbw %%mm0, %%mm2      \n\t" // Unpack High bytes of b
            // pbv = p - b = (a + b - c) - b = a - c
            "movq %%mm1, %%mm5           \n\t"
            // pav = p - a = (a + b - c) - a = b - c
            "movq %%mm2, %%mm4           \n\t"
            "psubw %%mm3, %%mm5          \n\t"
            "psubw %%mm3, %%mm4          \n\t"
            // pcv = p - c = (a + b - c) -c = (a - c) + (b - c) =
            //       pav + pbv = pbv + pav
            "movq %%mm5, %%mm6           \n\t"
            "paddw %%mm4, %%mm6          \n\t"

            // pa = abs(p-a) = abs(pav)
            // pb = abs(p-b) = abs(pbv)
            // pc = abs(p-c) = abs(pcv)
            "pcmpgtw %%mm5, %%mm0        \n\t" // Create mask pbv bytes < 0
            "pcmpgtw %%mm4, %%mm7        \n\t" // Create mask pav bytes < 0
            "pand %%mm5, %%mm0           \n\t" // Only pbv bytes < 0 in mm0
            "pand %%mm4, %%mm7           \n\t" // Only pav bytes < 0 in mm7
            "psubw %%mm0, %%mm5          \n\t"
            "psubw %%mm7, %%mm4          \n\t"
            "psubw %%mm0, %%mm5          \n\t"
            "psubw %%mm7, %%mm4          \n\t"
            "pxor %%mm0, %%mm0           \n\t"
            "pcmpgtw %%mm6, %%mm0        \n\t" // Create mask pcv bytes < 0
            "pand %%mm6, %%mm0           \n\t" // Only pav bytes < 0 in mm7
            "psubw %%mm0, %%mm6          \n\t"
            //  test pa <= pb
            "movq %%mm4, %%mm7           \n\t"
            "psubw %%mm0, %%mm6          \n\t"
            "pcmpgtw %%mm5, %%mm7        \n\t" // pa > pb?
            "movq %%mm7, %%mm0           \n\t"
            // use mm7 mask to merge pa & pb
            "pand %%mm7, %%mm5           \n\t"
            // use mm0 mask copy to merge a & b
            "pand %%mm0, %%mm2           \n\t"
            "pandn %%mm4, %%mm7          \n\t"
            "pandn %%mm1, %%mm0          \n\t"
            "paddw %%mm5, %%mm7          \n\t"
            "paddw %%mm2, %%mm0          \n\t"
            //  test  ((pa <= pb)? pa:pb) <= pc
            "pcmpgtw %%mm6, %%mm7        \n\t" // pab > pc?
            "movq (%%esi,%%ebx,), %%mm2  \n\t" // load b=Prior(x)
            "pand %%mm7, %%mm3           \n\t"
            "pandn %%mm0, %%mm7          \n\t"
            "pxor %%mm1, %%mm1           \n\t"
            "paddw %%mm3, %%mm7          \n\t"
            "pxor %%mm0, %%mm0           \n\t"
            "packuswb %%mm1, %%mm7       \n\t"
            "movq %%mm2, %%mm3           \n\t" // load c=Prior(x-bpp) step 1
            "pand $ActiveMask, %%mm7     \n\t"
            "punpckhbw %%mm0, %%mm2      \n\t" // Unpack High bytes of b
            "psllq $ShiftBpp, %%mm7      \n\t" // Shift bytes to 2nd group of 3 bytes
             // pav = p - a = (a + b - c) - a = b - c
            "movq %%mm2, %%mm4           \n\t"
            "paddb (%%edi,%%ebx,), %%mm7 \n\t" // add Paeth predictor with Raw(x)
            "psllq $ShiftBpp, %%mm3      \n\t" // load c=Prior(x-bpp) step 2
            "movq %%mm7, (%%edi,%%ebx,)  \n\t" // write back updated value
            "movq %%mm7, %%mm1           \n\t"
            "punpckhbw %%mm0, %%mm3      \n\t" // Unpack High bytes of c
            "psllq $ShiftBpp, %%mm1      \n\t" // Shift bytes
                                    // Now mm1 will be used as Raw(x-bpp)
            // Now do Paeth for 3rd, and final, set of bytes (6-7)
            "pxor %%mm7, %%mm7           \n\t"
            "punpckhbw %%mm0, %%mm1      \n\t" // Unpack High bytes of a
            "psubw %%mm3, %%mm4          \n\t"
            // pbv = p - b = (a + b - c) - b = a - c
            "movq %%mm1, %%mm5           \n\t"
            // pcv = p - c = (a + b - c) -c = (a - c) + (b - c) = pav + pbv
            "movq %%mm4, %%mm6           \n\t"
            "psubw %%mm3, %%mm5          \n\t"
            "pxor %%mm0, %%mm0           \n\t"
            "paddw %%mm5, %%mm6          \n\t"

            // pa = abs(p-a) = abs(pav)
            // pb = abs(p-b) = abs(pbv)
            // pc = abs(p-c) = abs(pcv)
            "pcmpgtw %%mm4, %%mm0        \n\t" // Create mask pav bytes < 0
            "pcmpgtw %%mm5, %%mm7        \n\t" // Create mask pbv bytes < 0
            "pand %%mm4, %%mm0           \n\t" // Only pav bytes < 0 in mm7
            "pand %%mm5, %%mm7           \n\t" // Only pbv bytes < 0 in mm0
            "psubw %%mm0, %%mm4          \n\t"
            "psubw %%mm7, %%mm5          \n\t"
            "psubw %%mm0, %%mm4          \n\t"
            "psubw %%mm7, %%mm5          \n\t"
            "pxor %%mm0, %%mm0           \n\t"
            "pcmpgtw %%mm6, %%mm0        \n\t" // Create mask pcv bytes < 0
            "pand %%mm6, %%mm0           \n\t" // Only pav bytes < 0 in mm7
            "psubw %%mm0, %%mm6          \n\t"
            //  test pa <= pb
            "movq %%mm4, %%mm7           \n\t"
            "psubw %%mm0, %%mm6          \n\t"
            "pcmpgtw %%mm5, %%mm7        \n\t" // pa > pb?
            "movq %%mm7, %%mm0           \n\t"
            // use mm0 mask copy to merge a & b
            "pand %%mm0, %%mm2           \n\t"
            // use mm7 mask to merge pa & pb
            "pand %%mm7, %%mm5           \n\t"
            "pandn %%mm1, %%mm0          \n\t"
            "pandn %%mm4, %%mm7          \n\t"
            "paddw %%mm2, %%mm0          \n\t"
            "paddw %%mm5, %%mm7          \n\t"
            //  test  ((pa <= pb)? pa:pb) <= pc
            "pcmpgtw %%mm6, %%mm7        \n\t" // pab > pc?
            "pand %%mm7, %%mm3           \n\t"
            "pandn %%mm0, %%mm7          \n\t"
            "paddw %%mm3, %%mm7          \n\t"
            "pxor %%mm1, %%mm1           \n\t"
            "packuswb %%mm7, %%mm1       \n\t"
            // Step ebx to next set of 8 bytes and repeat loop til done
            "addl $8, %%ebx              \n\t"
            "pand $ActiveMaskEnd, %%mm1  \n\t"
            "paddb -8(%%edi,%%ebx,), %%mm1 \n\t" // add Paeth predictor with Raw(x)

            "cmpl _MMXLength, %%ebx      \n\t"
            "pxor %%mm0, %%mm0           \n\t" // pxor does not affect flags
            "movq %%mm1, -8(%%edi,%%ebx,) \n\t" // write back updated value
                                 // mm1 will be used as Raw(x-bpp) next loop
                           // mm3 ready to be used as Prior(x-bpp) next loop
            "jb paeth_3lp                \n\t"

            : // FIXASM: output regs/vars go here, e.g.:  "=m" (memory_var)

            : // FIXASM: input regs, e.g.:  "c" (count), "S" (src), "D" (dest)

            : "%ebx", "%edi", "%esi", "%mm0", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5", "%mm6", "%mm7" // CHECKASM: clobber list
         );
      }
      break;

      case 6:
      //case 7:   // GRR BOGUS
      //case 5:   // GRR BOGUS
      {
         ActiveMask.use  = 0x00000000ffffffff;
         ActiveMask2.use = 0xffffffff00000000;
         ShiftBpp.use = bpp << 3;    // == bpp * 8
         ShiftRem.use = 64 - ShiftBpp.use;
         __asm__ (
            "movl _dif, %%ebx            \n\t"
            "movl row, %%edi             \n\t"
            "movl prev_row, %%esi        \n\t"
            // PRIME the pump (load the first Raw(x-bpp) data set
            "movq -8(%%edi,%%ebx,), %%mm1 \n\t"
            "pxor %%mm0, %%mm0           \n\t"
         "paeth_6lp:                     \n\t"
            // Must shift to position Raw(x-bpp) data
            "psrlq $ShiftRem, %%mm1      \n\t"
            // Do first set of 4 bytes
            "movq -8(%%esi,%%ebx,), %%mm3 \n\t" // read c=Prior(x-bpp) bytes
            "punpcklbw %%mm0, %%mm1      \n\t" // Unpack Low bytes of a
            "movq (%%esi,%%ebx,), %%mm2  \n\t" // load b=Prior(x)
            "punpcklbw %%mm0, %%mm2      \n\t" // Unpack Low bytes of b
            // Must shift to position Prior(x-bpp) data
            "psrlq $ShiftRem, %%mm3      \n\t"
            // pav = p - a = (a + b - c) - a = b - c
            "movq %%mm2, %%mm4           \n\t"
            "punpcklbw %%mm0, %%mm3      \n\t" // Unpack Low bytes of c
            // pbv = p - b = (a + b - c) - b = a - c
            "movq %%mm1, %%mm5           \n\t"
            "psubw %%mm3, %%mm4          \n\t"
            "pxor %%mm7, %%mm7           \n\t"
            // pcv = p - c = (a + b - c) -c = (a - c) + (b - c) = pav + pbv
            "movq %%mm4, %%mm6           \n\t"
            "psubw %%mm3, %%mm5          \n\t"
            // pa = abs(p-a) = abs(pav)
            // pb = abs(p-b) = abs(pbv)
            // pc = abs(p-c) = abs(pcv)
            "pcmpgtw %%mm4, %%mm0        \n\t" // Create mask pav bytes < 0
            "paddw %%mm5, %%mm6          \n\t"
            "pand %%mm4, %%mm0           \n\t" // Only pav bytes < 0 in mm7
            "pcmpgtw %%mm5, %%mm7        \n\t" // Create mask pbv bytes < 0
            "psubw %%mm0, %%mm4          \n\t"
            "pand %%mm5, %%mm7           \n\t" // Only pbv bytes < 0 in mm0
            "psubw %%mm0, %%mm4          \n\t"
            "psubw %%mm7, %%mm5          \n\t"
            "pxor %%mm0, %%mm0           \n\t"
            "pcmpgtw %%mm6, %%mm0        \n\t" // Create mask pcv bytes < 0
            "pand %%mm6, %%mm0           \n\t" // Only pav bytes < 0 in mm7
            "psubw %%mm7, %%mm5          \n\t"
            "psubw %%mm0, %%mm6          \n\t"
            //  test pa <= pb
            "movq %%mm4, %%mm7           \n\t"
            "psubw %%mm0, %%mm6          \n\t"
            "pcmpgtw %%mm5, %%mm7        \n\t" // pa > pb?
            "movq %%mm7, %%mm0           \n\t"
            // use mm7 mask to merge pa & pb
            "pand %%mm7, %%mm5           \n\t"
            // use mm0 mask copy to merge a & b
            "pand %%mm0, %%mm2           \n\t"
            "pandn %%mm4, %%mm7          \n\t"
            "pandn %%mm1, %%mm0          \n\t"
            "paddw %%mm5, %%mm7          \n\t"
            "paddw %%mm2, %%mm0          \n\t"
            //  test  ((pa <= pb)? pa:pb) <= pc
            "pcmpgtw %%mm6, %%mm7        \n\t" // pab > pc?
            "pxor %%mm1, %%mm1           \n\t"
            "pand %%mm7, %%mm3           \n\t"
            "pandn %%mm0, %%mm7          \n\t"
            "paddw %%mm3, %%mm7          \n\t"
            "pxor %%mm0, %%mm0           \n\t"
            "packuswb %%mm1, %%mm7       \n\t"
            "movq -8(%%esi,%%ebx,), %%mm3 \n\t" // load c=Prior(x-bpp)
            "pand $ActiveMask, %%mm7     \n\t"
            "psrlq $ShiftRem, %%mm3      \n\t"
            "movq (%%esi,%%ebx,), %%mm2  \n\t" // load b=Prior(x) step 1
            "paddb (%%edi,%%ebx,), %%mm7 \n\t" // add Paeth predictor with Raw(x)
            "movq %%mm2, %%mm6           \n\t"
            "movq %%mm7, (%%edi,%%ebx,)  \n\t" // write back updated value
            "movq -8(%%edi,%%ebx,), %%mm1 \n\t"
            "psllq $ShiftBpp, %%mm6      \n\t"
            "movq %%mm7, %%mm5           \n\t"
            "psrlq $ShiftRem, %%mm1      \n\t"
            "por %%mm6, %%mm3            \n\t"
            "psllq $ShiftBpp, %%mm5      \n\t"
            "punpckhbw %%mm0, %%mm3      \n\t" // Unpack High bytes of c
            "por %%mm5, %%mm1            \n\t"
            // Do second set of 4 bytes
            "punpckhbw %%mm0, %%mm2      \n\t" // Unpack High bytes of b
            "punpckhbw %%mm0, %%mm1      \n\t" // Unpack High bytes of a
            // pav = p - a = (a + b - c) - a = b - c
            "movq %%mm2, %%mm4           \n\t"
            // pbv = p - b = (a + b - c) - b = a - c
            "movq %%mm1, %%mm5           \n\t"
            "psubw %%mm3, %%mm4          \n\t"
            "pxor %%mm7, %%mm7           \n\t"
            // pcv = p - c = (a + b - c) -c = (a - c) + (b - c) = pav + pbv
            "movq %%mm4, %%mm6           \n\t"
            "psubw %%mm3, %%mm5          \n\t"
            // pa = abs(p-a) = abs(pav)
            // pb = abs(p-b) = abs(pbv)
            // pc = abs(p-c) = abs(pcv)
            "pcmpgtw %%mm4, %%mm0        \n\t" // Create mask pav bytes < 0
            "paddw %%mm5, %%mm6          \n\t"
            "pand %%mm4, %%mm0           \n\t" // Only pav bytes < 0 in mm7
            "pcmpgtw %%mm5, %%mm7        \n\t" // Create mask pbv bytes < 0
            "psubw %%mm0, %%mm4          \n\t"
            "pand %%mm5, %%mm7           \n\t" // Only pbv bytes < 0 in mm0
            "psubw %%mm0, %%mm4          \n\t"
            "psubw %%mm7, %%mm5          \n\t"
            "pxor %%mm0, %%mm0           \n\t"
            "pcmpgtw %%mm6, %%mm0        \n\t" // Create mask pcv bytes < 0
            "pand %%mm6, %%mm0           \n\t" // Only pav bytes < 0 in mm7
            "psubw %%mm7, %%mm5          \n\t"
            "psubw %%mm0, %%mm6          \n\t"
            //  test pa <= pb
            "movq %%mm4, %%mm7           \n\t"
            "psubw %%mm0, %%mm6          \n\t"
            "pcmpgtw %%mm5, %%mm7        \n\t" // pa > pb?
            "movq %%mm7, %%mm0           \n\t"
            // use mm7 mask to merge pa & pb
            "pand %%mm7, %%mm5           \n\t"
            // use mm0 mask copy to merge a & b
            "pand %%mm0, %%mm2           \n\t"
            "pandn %%mm4, %%mm7          \n\t"
            "pandn %%mm1, %%mm0          \n\t"
            "paddw %%mm5, %%mm7          \n\t"
            "paddw %%mm2, %%mm0          \n\t"
            //  test  ((pa <= pb)? pa:pb) <= pc
            "pcmpgtw %%mm6, %%mm7        \n\t" // pab > pc?
            "pxor %%mm1, %%mm1           \n\t"
            "pand %%mm7, %%mm3           \n\t"
            "pandn %%mm0, %%mm7          \n\t"
            "pxor %%mm1, %%mm1           \n\t"
            "paddw %%mm3, %%mm7          \n\t"
            "pxor %%mm0, %%mm0           \n\t"
            // Step ex to next set of 8 bytes and repeat loop til done
            "addl $8, %%ebx              \n\t"
            "packuswb %%mm7, %%mm1       \n\t"
            "paddb -8(%%edi,%%ebx,), %%mm1 \n\t" // add Paeth predictor with Raw(x)
            "cmpl _MMXLength, %%ebx      \n\t"
            "movq %%mm1, -8(%%edi,%%ebx,) \n\t" // write back updated value
                                // mm1 will be used as Raw(x-bpp) next loop
            "jb paeth_6lp                \n\t"

            : // FIXASM: output regs/vars go here, e.g.:  "=m" (memory_var)

            : // FIXASM: input regs, e.g.:  "c" (count), "S" (src), "D" (dest)

            : "%ebx", "%edi", "%esi", "%mm0", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5", "%mm6", "%mm7" // CHECKASM: clobber list
         );
      }
      break;

      case 4:
      {
         ActiveMask.use  = 0x00000000ffffffff;
         __asm__ (
            "movl _dif, %%ebx            \n\t"
            "movl row, %%edi             \n\t"
            "movl prev_row, %%esi        \n\t"
            "pxor %%mm0, %%mm0           \n\t"
            // PRIME the pump (load the first Raw(x-bpp) data set
            "movq -8(%%edi,%%ebx,), %%mm1 \n\t" // Only time should need to read
                                     //  a=Raw(x-bpp) bytes
         "paeth_4lp:                     \n\t"
            // Do first set of 4 bytes
            "movq -8(%%esi,%%ebx,), %%mm3 \n\t" // read c=Prior(x-bpp) bytes
            "punpckhbw %%mm0, %%mm1      \n\t" // Unpack Low bytes of a
            "movq (%%esi,%%ebx,), %%mm2  \n\t" // load b=Prior(x)
            "punpcklbw %%mm0, %%mm2      \n\t" // Unpack High bytes of b
            // pav = p - a = (a + b - c) - a = b - c
            "movq %%mm2, %%mm4           \n\t"
            "punpckhbw %%mm0, %%mm3      \n\t" // Unpack High bytes of c
            // pbv = p - b = (a + b - c) - b = a - c
            "movq %%mm1, %%mm5           \n\t"
            "psubw %%mm3, %%mm4          \n\t"
            "pxor %%mm7, %%mm7           \n\t"
            // pcv = p - c = (a + b - c) -c = (a - c) + (b - c) = pav + pbv
            "movq %%mm4, %%mm6           \n\t"
            "psubw %%mm3, %%mm5          \n\t"
            // pa = abs(p-a) = abs(pav)
            // pb = abs(p-b) = abs(pbv)
            // pc = abs(p-c) = abs(pcv)
            "pcmpgtw %%mm4, %%mm0        \n\t" // Create mask pav bytes < 0
            "paddw %%mm5, %%mm6          \n\t"
            "pand %%mm4, %%mm0           \n\t" // Only pav bytes < 0 in mm7
            "pcmpgtw %%mm5, %%mm7        \n\t" // Create mask pbv bytes < 0
            "psubw %%mm0, %%mm4          \n\t"
            "pand %%mm5, %%mm7           \n\t" // Only pbv bytes < 0 in mm0
            "psubw %%mm0, %%mm4          \n\t"
            "psubw %%mm7, %%mm5          \n\t"
            "pxor %%mm0, %%mm0           \n\t"
            "pcmpgtw %%mm6, %%mm0        \n\t" // Create mask pcv bytes < 0
            "pand %%mm6, %%mm0           \n\t" // Only pav bytes < 0 in mm7
            "psubw %%mm7, %%mm5          \n\t"
            "psubw %%mm0, %%mm6          \n\t"
            //  test pa <= pb
            "movq %%mm4, %%mm7           \n\t"
            "psubw %%mm0, %%mm6          \n\t"
            "pcmpgtw %%mm5, %%mm7        \n\t" // pa > pb?
            "movq %%mm7, %%mm0           \n\t"
            // use mm7 mask to merge pa & pb
            "pand %%mm7, %%mm5           \n\t"
            // use mm0 mask copy to merge a & b
            "pand %%mm0, %%mm2           \n\t"
            "pandn %%mm4, %%mm7          \n\t"
            "pandn %%mm1, %%mm0          \n\t"
            "paddw %%mm5, %%mm7          \n\t"
            "paddw %%mm2, %%mm0          \n\t"
            //  test  ((pa <= pb)? pa:pb) <= pc
            "pcmpgtw %%mm6, %%mm7        \n\t" // pab > pc?
            "pxor %%mm1, %%mm1           \n\t"
            "pand %%mm7, %%mm3           \n\t"
            "pandn %%mm0, %%mm7          \n\t"
            "paddw %%mm3, %%mm7          \n\t"
            "pxor %%mm0, %%mm0           \n\t"
            "packuswb %%mm1, %%mm7       \n\t"
            "movq (%%esi,%%ebx,), %%mm3  \n\t" // load c=Prior(x-bpp)
            "pand $ActiveMask, %%mm7     \n\t"
            "movq %%mm3, %%mm2           \n\t" // load b=Prior(x) step 1
            "paddb (%%edi,%%ebx,), %%mm7 \n\t" // add Paeth predictor with Raw(x)
            "punpcklbw %%mm0, %%mm3      \n\t" // Unpack High bytes of c
            "movq %%mm7, (%%edi,%%ebx,)  \n\t" // write back updated value
            "movq %%mm7, %%mm1           \n\t" // Now mm1 will be used as Raw(x-bpp)
            // Do second set of 4 bytes
            "punpckhbw %%mm0, %%mm2      \n\t" // Unpack Low bytes of b
            "punpcklbw %%mm0, %%mm1      \n\t" // Unpack Low bytes of a
            // pav = p - a = (a + b - c) - a = b - c
            "movq %%mm2, %%mm4           \n\t"
            // pbv = p - b = (a + b - c) - b = a - c
            "movq %%mm1, %%mm5           \n\t"
            "psubw %%mm3, %%mm4          \n\t"
            "pxor %%mm7, %%mm7           \n\t"
            // pcv = p - c = (a + b - c) -c = (a - c) + (b - c) = pav + pbv
            "movq %%mm4, %%mm6           \n\t"
            "psubw %%mm3, %%mm5          \n\t"
            // pa = abs(p-a) = abs(pav)
            // pb = abs(p-b) = abs(pbv)
            // pc = abs(p-c) = abs(pcv)
            "pcmpgtw %%mm4, %%mm0        \n\t" // Create mask pav bytes < 0
            "paddw %%mm5, %%mm6          \n\t"
            "pand %%mm4, %%mm0           \n\t" // Only pav bytes < 0 in mm7
            "pcmpgtw %%mm5, %%mm7        \n\t" // Create mask pbv bytes < 0
            "psubw %%mm0, %%mm4          \n\t"
            "pand %%mm5, %%mm7           \n\t" // Only pbv bytes < 0 in mm0
            "psubw %%mm0, %%mm4          \n\t"
            "psubw %%mm7, %%mm5          \n\t"
            "pxor %%mm0, %%mm0           \n\t"
            "pcmpgtw %%mm6, %%mm0        \n\t" // Create mask pcv bytes < 0
            "pand %%mm6, %%mm0           \n\t" // Only pav bytes < 0 in mm7
            "psubw %%mm7, %%mm5          \n\t"
            "psubw %%mm0, %%mm6          \n\t"
            //  test pa <= pb
            "movq %%mm4, %%mm7           \n\t"
            "psubw %%mm0, %%mm6          \n\t"
            "pcmpgtw %%mm5, %%mm7        \n\t" // pa > pb?
            "movq %%mm7, %%mm0           \n\t"
            // use mm7 mask to merge pa & pb
            "pand %%mm7, %%mm5           \n\t"
            // use mm0 mask copy to merge a & b
            "pand %%mm0, %%mm2           \n\t"
            "pandn %%mm4, %%mm7          \n\t"
            "pandn %%mm1, %%mm0          \n\t"
            "paddw %%mm5, %%mm7          \n\t"
            "paddw %%mm2, %%mm0          \n\t"
            //  test  ((pa <= pb)? pa:pb) <= pc
            "pcmpgtw %%mm6, %%mm7        \n\t" // pab > pc?
            "pxor %%mm1, %%mm1           \n\t"
            "pand %%mm7, %%mm3           \n\t"
            "pandn %%mm0, %%mm7          \n\t"
            "pxor %%mm1, %%mm1           \n\t"
            "paddw %%mm3, %%mm7          \n\t"
            "pxor %%mm0, %%mm0           \n\t"
            // Step ex to next set of 8 bytes and repeat loop til done
            "addl $8, %%ebx              \n\t"
            "packuswb %%mm7, %%mm1       \n\t"
            "paddb -8(%%edi,%%ebx,), %%mm1 \n\t" // add Paeth predictor with Raw(x)
            "cmpl _MMXLength, %%ebx      \n\t"
            "movq %%mm1, -8(%%edi,%%ebx,) \n\t" // write back updated value
                                // mm1 will be used as Raw(x-bpp) next loop
            "jb paeth_4lp                \n\t"

            : // FIXASM: output regs/vars go here, e.g.:  "=m" (memory_var)

            : // FIXASM: input regs, e.g.:  "c" (count), "S" (src), "D" (dest)

            : "%ebx", "%edi", "%esi", "%mm0", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5", "%mm6", "%mm7" // CHECKASM: clobber list
         );
      }
      break;
      case 8:                          // bpp == 8
      {
         ActiveMask.use  = 0x00000000ffffffff;
         __asm__ (
            "movl _dif, %%ebx            \n\t"
            "movl row, %%edi             \n\t"
            "movl prev_row, %%esi        \n\t"
            "pxor %%mm0, %%mm0           \n\t"
            // PRIME the pump (load the first Raw(x-bpp) data set
            "movq -8(%%edi,%%ebx,), %%mm1 \n\t" // Only time should need to read
                                       //  a=Raw(x-bpp) bytes
         "paeth_8lp:                     \n\t"
            // Do first set of 4 bytes
            "movq -8(%%esi,%%ebx,), %%mm3 \n\t" // read c=Prior(x-bpp) bytes
            "punpcklbw %%mm0, %%mm1      \n\t" // Unpack Low bytes of a
            "movq (%%esi,%%ebx,), %%mm2  \n\t" // load b=Prior(x)
            "punpcklbw %%mm0, %%mm2      \n\t" // Unpack Low bytes of b
            // pav = p - a = (a + b - c) - a = b - c
            "movq %%mm2, %%mm4           \n\t"
            "punpcklbw %%mm0, %%mm3      \n\t" // Unpack Low bytes of c
            // pbv = p - b = (a + b - c) - b = a - c
            "movq %%mm1, %%mm5           \n\t"
            "psubw %%mm3, %%mm4          \n\t"
            "pxor %%mm7, %%mm7           \n\t"
            // pcv = p - c = (a + b - c) -c = (a - c) + (b - c) = pav + pbv
            "movq %%mm4, %%mm6           \n\t"
            "psubw %%mm3, %%mm5          \n\t"
            // pa = abs(p-a) = abs(pav)
            // pb = abs(p-b) = abs(pbv)
            // pc = abs(p-c) = abs(pcv)
            "pcmpgtw %%mm4, %%mm0        \n\t" // Create mask pav bytes < 0
            "paddw %%mm5, %%mm6          \n\t"
            "pand %%mm4, %%mm0           \n\t" // Only pav bytes < 0 in mm7
            "pcmpgtw %%mm5, %%mm7        \n\t" // Create mask pbv bytes < 0
            "psubw %%mm0, %%mm4          \n\t"
            "pand %%mm5, %%mm7           \n\t" // Only pbv bytes < 0 in mm0
            "psubw %%mm0, %%mm4          \n\t"
            "psubw %%mm7, %%mm5          \n\t"
            "pxor %%mm0, %%mm0           \n\t"
            "pcmpgtw %%mm6, %%mm0        \n\t" // Create mask pcv bytes < 0
            "pand %%mm6, %%mm0           \n\t" // Only pav bytes < 0 in mm7
            "psubw %%mm7, %%mm5          \n\t"
            "psubw %%mm0, %%mm6          \n\t"
            //  test pa <= pb
            "movq %%mm4, %%mm7           \n\t"
            "psubw %%mm0, %%mm6          \n\t"
            "pcmpgtw %%mm5, %%mm7        \n\t" // pa > pb?
            "movq %%mm7, %%mm0           \n\t"
            // use mm7 mask to merge pa & pb
            "pand %%mm7, %%mm5           \n\t"
            // use mm0 mask copy to merge a & b
            "pand %%mm0, %%mm2           \n\t"
            "pandn %%mm4, %%mm7          \n\t"
            "pandn %%mm1, %%mm0          \n\t"
            "paddw %%mm5, %%mm7          \n\t"
            "paddw %%mm2, %%mm0          \n\t"
            //  test  ((pa <= pb)? pa:pb) <= pc
            "pcmpgtw %%mm6, %%mm7        \n\t" // pab > pc?
            "pxor %%mm1, %%mm1           \n\t"
            "pand %%mm7, %%mm3           \n\t"
            "pandn %%mm0, %%mm7          \n\t"
            "paddw %%mm3, %%mm7          \n\t"
            "pxor %%mm0, %%mm0           \n\t"
            "packuswb %%mm1, %%mm7       \n\t"
            "movq -8(%%esi,%%ebx,), %%mm3 \n\t" // read c=Prior(x-bpp) bytes
            "pand $ActiveMask, %%mm7     \n\t"
            "movq (%%esi,%%ebx,), %%mm2  \n\t" // load b=Prior(x)
            "paddb (%%edi,%%ebx,), %%mm7 \n\t" // add Paeth predictor with Raw(x)
            "punpckhbw %%mm0, %%mm3      \n\t" // Unpack High bytes of c
            "movq %%mm7, (%%edi,%%ebx,)  \n\t" // write back updated value
            "movq -8(%%edi,%%ebx,), %%mm1 \n\t" // read a=Raw(x-bpp) bytes

            // Do second set of 4 bytes
            "punpckhbw %%mm0, %%mm2      \n\t" // Unpack High bytes of b
            "punpckhbw %%mm0, %%mm1      \n\t" // Unpack High bytes of a
            // pav = p - a = (a + b - c) - a = b - c
            "movq %%mm2, %%mm4           \n\t"
            // pbv = p - b = (a + b - c) - b = a - c
            "movq %%mm1, %%mm5           \n\t"
            "psubw %%mm3, %%mm4          \n\t"
            "pxor %%mm7, %%mm7           \n\t"
            // pcv = p - c = (a + b - c) -c = (a - c) + (b - c) = pav + pbv
            "movq %%mm4, %%mm6           \n\t"
            "psubw %%mm3, %%mm5          \n\t"
            // pa = abs(p-a) = abs(pav)
            // pb = abs(p-b) = abs(pbv)
            // pc = abs(p-c) = abs(pcv)
            "pcmpgtw %%mm4, %%mm0        \n\t" // Create mask pav bytes < 0
            "paddw %%mm5, %%mm6          \n\t"
            "pand %%mm4, %%mm0           \n\t" // Only pav bytes < 0 in mm7
            "pcmpgtw %%mm5, %%mm7        \n\t" // Create mask pbv bytes < 0
            "psubw %%mm0, %%mm4          \n\t"
            "pand %%mm5, %%mm7           \n\t" // Only pbv bytes < 0 in mm0
            "psubw %%mm0, %%mm4          \n\t"
            "psubw %%mm7, %%mm5          \n\t"
            "pxor %%mm0, %%mm0           \n\t"
            "pcmpgtw %%mm6, %%mm0        \n\t" // Create mask pcv bytes < 0
            "pand %%mm6, %%mm0           \n\t" // Only pav bytes < 0 in mm7
            "psubw %%mm7, %%mm5          \n\t"
            "psubw %%mm0, %%mm6          \n\t"
            //  test pa <= pb
            "movq %%mm4, %%mm7           \n\t"
            "psubw %%mm0, %%mm6          \n\t"
            "pcmpgtw %%mm5, %%mm7        \n\t" // pa > pb?
            "movq %%mm7, %%mm0           \n\t"
            // use mm7 mask to merge pa & pb
            "pand %%mm7, %%mm5           \n\t"
            // use mm0 mask copy to merge a & b
            "pand %%mm0, %%mm2           \n\t"
            "pandn %%mm4, %%mm7          \n\t"
            "pandn %%mm1, %%mm0          \n\t"
            "paddw %%mm5, %%mm7          \n\t"
            "paddw %%mm2, %%mm0          \n\t"
            //  test  ((pa <= pb)? pa:pb) <= pc
            "pcmpgtw %%mm6, %%mm7        \n\t" // pab > pc?
            "pxor %%mm1, %%mm1           \n\t"
            "pand %%mm7, %%mm3           \n\t"
            "pandn %%mm0, %%mm7          \n\t"
            "pxor %%mm1, %%mm1           \n\t"
            "paddw %%mm3, %%mm7          \n\t"
            "pxor %%mm0, %%mm0           \n\t"
            // Step ex to next set of 8 bytes and repeat loop til done
            "addl $8, %%ebx              \n\t"
            "packuswb %%mm7, %%mm1       \n\t"
            "paddb -8(%%edi,%%ebx,), %%mm1 \n\t" // add Paeth predictor with Raw(x)
            "cmpl _MMXLength, %%ebx      \n\t"
            "movq %%mm1, -8(%%edi,%%ebx,) \n\t" // write back updated value
                            // mm1 will be used as Raw(x-bpp) next loop
            "jb paeth_8lp                \n\t"

            : // FIXASM: output regs/vars go here, e.g.:  "=m" (memory_var)

            : // FIXASM: input regs, e.g.:  "c" (count), "S" (src), "D" (dest)

            : "%ebx", "%edi", "%esi", "%mm0", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5", "%mm6", "%mm7" // CHECKASM: clobber list
         );
      }
      break;

      case 1:                // bpp = 1
      case 2:                // bpp = 2
      default:               // bpp > 8
      {
         __asm__ (
            "movl _dif, %%ebx            \n\t"
            "cmpl _FullLength, %%ebx     \n\t"
            "jnb paeth_dend              \n\t"
            "movl row, %%edi             \n\t"
            "movl prev_row, %%esi        \n\t"
            // Do Paeth decode for remaining bytes
            "movl %%ebx, %%edx           \n\t"
            "xorl %%ecx, %%ecx           \n\t" // zero ecx before using cl & cx in loop below
            "subl bpp, %%edx             \n\t" // Set edx = ebx - bpp
         "paeth_dlp:                     \n\t"
            "xorl %%eax, %%eax           \n\t"
            // pav = p - a = (a + b - c) - a = b - c
            "movb (%%esi,%%ebx,), %%al   \n\t" // load Prior(x) into al
            "movb (%%esi,%%edx,), %%cl   \n\t" // load Prior(x-bpp) into cl
            "subl %%ecx, %%eax           \n\t" // subtract Prior(x-bpp)
            "movl %%eax, patemp          \n\t" // Save pav for later use
            "xorl %%eax, %%eax           \n\t"
            // pbv = p - b = (a + b - c) - b = a - c
            "movb (%%edi,%%edx,), %%al   \n\t" // load Raw(x-bpp) into al
            "subl %%ecx, %%eax           \n\t" // subtract Prior(x-bpp)
            "movl %%eax, %%ecx           \n\t"
            // pcv = p - c = (a + b - c) -c = (a - c) + (b - c) = pav + pbv
            "addl patemp, %%eax          \n\t" // pcv = pav + pbv
            // pc = abs(pcv)
            "testl $0x80000000, %%eax    \n\t"
            "jz paeth_dpca               \n\t"
            "negl %%eax                  \n\t" // reverse sign of neg values
         "paeth_dpca:                    \n\t"
            "movl %%eax, pctemp          \n\t" // save pc for later use
            // pb = abs(pbv)
            "testl $0x80000000, %%ecx    \n\t"
            "jz paeth_dpba               \n\t"
            "negl %%ecx                  \n\t" // reverse sign of neg values
         "paeth_dpba:                    \n\t"
            "movl %%ecx, pbtemp          \n\t" // save pb for later use
            // pa = abs(pav)
            "movl patemp, %%eax          \n\t"
            "testl $0x80000000, %%eax    \n\t"
            "jz paeth_dpaa               \n\t"
            "negl %%eax                  \n\t" // reverse sign of neg values
         "paeth_dpaa:                    \n\t"
            "movl %%eax, patemp          \n\t" // save pa for later use
            // test if pa <= pb
            "cmpl %%ecx, %%eax           \n\t"
            "jna paeth_dabb              \n\t"
            // pa > pb; now test if pb <= pc
            "cmpl pctemp, %%ecx          \n\t"
            "jna paeth_dbbc              \n\t"
            // pb > pc; Raw(x) = Paeth(x) + Prior(x-bpp)
            "movb (%%esi,%%edx,), %%cl   \n\t" // load Prior(x-bpp) into cl
            "jmp paeth_dpaeth            \n\t"
         "paeth_dbbc:                    \n\t"
            // pb <= pc; Raw(x) = Paeth(x) + Prior(x)
            "movb (%%esi,%%ebx,), %%cl   \n\t" // load Prior(x) into cl
            "jmp paeth_dpaeth            \n\t"
         "paeth_dabb:                    \n\t"
            // pa <= pb; now test if pa <= pc
            "cmpl pctemp, %%eax          \n\t"
            "jna paeth_dabc              \n\t"
            // pa > pc; Raw(x) = Paeth(x) + Prior(x-bpp)
            "movb (%%esi,%%edx,), %%cl   \n\t" // load Prior(x-bpp) into cl
            "jmp paeth_dpaeth            \n\t"
         "paeth_dabc:                    \n\t"
            // pa <= pc; Raw(x) = Paeth(x) + Raw(x-bpp)
            "movb (%%edi,%%edx,), %%cl   \n\t" // load Raw(x-bpp) into cl
         "paeth_dpaeth:                  \n\t"
            "incl %%ebx                  \n\t"
            "incl %%edx                  \n\t"
            // Raw(x) = (Paeth(x) + Paeth_Predictor( a, b, c )) mod 256
            "addb %%cl, -1(%%edi,%%ebx,) \n\t"
            "cmpl _FullLength, %%ebx     \n\t"
            "jb paeth_dlp                \n\t"
         "paeth_dend:                    \n\t"

            : // FIXASM: output regs/vars go here, e.g.:  "=m" (memory_var)

            : // FIXASM: input regs, e.g.:  "c" (count), "S" (src), "D" (dest)

            : "%eax", "%ebx", "%ecx", "%edx", "%edi", "%esi" // CHECKASM: clobber list
         );
      }
      return;                   // No need to go further with this one
   }                         // end switch ( bpp )
   __asm__ (
      // MMX acceleration complete now do clean-up
      // Check if any remaining bytes left to decode
      "movl _MMXLength, %%ebx      \n\t"
      "cmpl _FullLength, %%ebx     \n\t"
      "jnb paeth_end               \n\t"
      "movl row, %%edi             \n\t"
      "movl prev_row, %%esi        \n\t"
      // Do Paeth decode for remaining bytes
      "movl %%ebx, %%edx           \n\t"
      "xorl %%ecx, %%ecx           \n\t" // zero ecx before using cl & cx in loop below
      "subl bpp, %%edx             \n\t" // Set edx = ebx - bpp
   "paeth_lp2:                     \n\t"
      "xorl %%eax, %%eax           \n\t"
      // pav = p - a = (a + b - c) - a = b - c
      "movb (%%esi,%%ebx,), %%al   \n\t" // load Prior(x) into al
      "movb (%%esi,%%edx,), %%cl   \n\t" // load Prior(x-bpp) into cl
      "subl %%ecx, %%eax           \n\t" // subtract Prior(x-bpp)
      "movl %%eax, patemp          \n\t" // Save pav for later use
      "xorl %%eax, %%eax           \n\t"
      // pbv = p - b = (a + b - c) - b = a - c
      "movb (%%edi,%%edx,), %%al   \n\t" // load Raw(x-bpp) into al
      "subl %%ecx, %%eax           \n\t" // subtract Prior(x-bpp)
      "movl %%eax, %%ecx           \n\t"
      // pcv = p - c = (a + b - c) -c = (a - c) + (b - c) = pav + pbv
      "addl patemp, %%eax          \n\t" // pcv = pav + pbv
      // pc = abs(pcv)
      "testl $0x80000000, %%eax    \n\t"
      "jz paeth_pca2               \n\t"
      "negl %%eax                  \n\t" // reverse sign of neg values
   "paeth_pca2:                    \n\t"
      "movl %%eax, pctemp          \n\t" // save pc for later use
      // pb = abs(pbv)
      "testl $0x80000000, %%ecx    \n\t"
      "jz paeth_pba2               \n\t"
      "negl %%ecx                  \n\t" // reverse sign of neg values
   "paeth_pba2:                    \n\t"
      "movl %%ecx, pbtemp          \n\t" // save pb for later use
      // pa = abs(pav)
      "movl patemp, %%eax          \n\t"
      "testl $0x80000000, %%eax    \n\t"
      "jz paeth_paa2               \n\t"
      "negl %%eax                  \n\t" // reverse sign of neg values
   "paeth_paa2:                    \n\t"
      "movl %%eax, patemp          \n\t" // save pa for later use
      // test if pa <= pb
      "cmpl %%ecx, %%eax           \n\t"
      "jna paeth_abb2              \n\t"
      // pa > pb; now test if pb <= pc
      "cmpl pctemp, %%ecx          \n\t"
      "jna paeth_bbc2              \n\t"
      // pb > pc; Raw(x) = Paeth(x) + Prior(x-bpp)
      "movb (%%esi,%%edx,), %%cl   \n\t" // load Prior(x-bpp) into cl
      "jmp paeth_paeth2            \n\t"
   "paeth_bbc2:                    \n\t"
      // pb <= pc; Raw(x) = Paeth(x) + Prior(x)
      "movb (%%esi,%%ebx,), %%cl   \n\t" // load Prior(x) into cl
      "jmp paeth_paeth2            \n\t"
   "paeth_abb2:                    \n\t"
      // pa <= pb; now test if pa <= pc
      "cmpl pctemp, %%eax          \n\t"
      "jna paeth_abc2              \n\t"
      // pa > pc; Raw(x) = Paeth(x) + Prior(x-bpp)
      "movb (%%esi,%%edx,), %%cl   \n\t" // load Prior(x-bpp) into cl
      "jmp paeth_paeth2            \n\t"
   "paeth_abc2:                    \n\t"
      // pa <= pc; Raw(x) = Paeth(x) + Raw(x-bpp)
      "movb (%%edi,%%edx,), %%cl   \n\t" // load Raw(x-bpp) into cl
   "paeth_paeth2:                  \n\t"
      "incl %%ebx                  \n\t"
      "incl %%edx                  \n\t"
      // Raw(x) = (Paeth(x) + Paeth_Predictor( a, b, c )) mod 256
      "addb %%cl, -1(%%edi,%%ebx,) \n\t"
      "cmpl _FullLength, %%ebx     \n\t"
      "jb paeth_lp2                \n\t"
   "paeth_end:                     \n\t"
      "emms                        \n\t" // End MMX instructions; prep for possible FP instrs.

      : // FIXASM: output regs/vars go here, e.g.:  "=m" (memory_var)

      : // FIXASM: input regs, e.g.:  "c" (count), "S" (src), "D" (dest)

      : "%eax", "%ebx", "%ecx", "%edx", "%edi", "%esi" // CHECKASM: clobber list
   );
#endif /* GRR_GCC_MMX_CONVERTED */
}

// Optimized code for PNG Sub filter decoder
void /* PRIVATE */
png_read_filter_row_mmx_sub(png_row_infop row_info, png_bytep row)
{
#ifdef GRR_GCC_MMX_CONVERTED
   int bpp;

   bpp = (row_info->pixel_depth + 7) >> 3; // Get # bytes per pixel
   _FullLength  = row_info->rowbytes - bpp; // # of bytes to filter
   __asm__ (
      "movl row, %%edi             \n\t"
      "movl %%edi, %%esi           \n\t" // lp = row
      "addl bpp, %%edi             \n\t" // rp = row + bpp
      "xorl %%eax, %%eax           \n\t"
      // get # of bytes to alignment
      "movl %%edi, _dif            \n\t" // take start of row
      "addl $0xf, _dif             \n\t" // add 7 + 8 to incr past
                                         // alignment boundary
      "xorl %%ebx, %%ebx           \n\t"
      "andl $0xfffffff8, _dif      \n\t" // mask to alignment boundary
      "subl %%edi, _dif            \n\t" // subtract from start ==> value
                                         //  ebx at alignment
      "jz sub_go                   \n\t"
      // fix alignment
   "sub_lp1:                       \n\t"
      "movb (%%esi,%%ebx,), %%al   \n\t"
      "addb %%al, (%%edi,%%ebx,)   \n\t"
      "incl %%ebx                  \n\t"
      "cmpl _dif, %%ebx            \n\t"
      "jb sub_lp1                  \n\t"
   "sub_go:                        \n\t"
      "movl _FullLength, %%ecx     \n\t"
      "movl %%ecx, %%edx           \n\t"
      "subl %%ebx, %%edx           \n\t" // subtract alignment fix
      "andl $0x00000007, %%edx     \n\t" // calc bytes over mult of 8
      "subl %%edx, %%ecx           \n\t" // drop over bytes from length
      "movl %%ecx, _MMXLength      \n\t"

      : // FIXASM: output regs/vars go here, e.g.:  "=m" (memory_var)

      : // FIXASM: input regs, e.g.:  "c" (count), "S" (src), "D" (dest)

      : "%eax", "%ebx", "%ecx", "%edx", "%edi", "%esi" // CHECKASM: clobber list
   );

   // Now do the math for the rest of the row
   switch ( bpp )
   {
        case 3:
        {
         ActiveMask.use  = 0x0000ffffff000000;
         ShiftBpp.use = 24;       // == 3 * 8
         ShiftRem.use  = 40;      // == 64 - 24
         __asm__ (
            "movl row, %%edi             \n\t"
            "movq $ActiveMask, %%mm7     \n\t" // Load ActiveMask for 2nd active byte group
            "movl %%edi, %%esi           \n\t" // lp = row
            "addl bpp, %%edi             \n\t" // rp = row + bpp
            "movq %%mm7, %%mm6           \n\t"
            "movl _dif, %%ebx            \n\t"
            "psllq $ShiftBpp, %%mm6      \n\t" // Move mask in mm6 to cover 3rd active
                                  // byte group
            // PRIME the pump (load the first Raw(x-bpp) data set
            "movq -8(%%edi,%%ebx,), %%mm1 \n\t"
         "sub_3lp:                       \n\t"
            "psrlq $ShiftRem, %%mm1      \n\t" // Shift data for adding 1st bpp bytes
                          // no need for mask; shift clears inactive bytes
            // Add 1st active group
            "movq (%%edi,%%ebx,), %%mm0  \n\t"
            "paddb %%mm1, %%mm0          \n\t"
            // Add 2nd active group
            "movq %%mm0, %%mm1           \n\t" // mov updated Raws to mm1
            "psllq $ShiftBpp, %%mm1      \n\t" // shift data to position correctly
            "pand %%mm7, %%mm1           \n\t" // mask to use only 2nd active group
            "paddb %%mm1, %%mm0          \n\t"
            // Add 3rd active group
            "movq %%mm0, %%mm1           \n\t" // mov updated Raws to mm1
            "psllq $ShiftBpp, %%mm1      \n\t" // shift data to position correctly
            "pand %%mm6, %%mm1           \n\t" // mask to use only 3rd active group
            "addl $8, %%ebx              \n\t"
            "paddb %%mm1, %%mm0          \n\t"
            "cmpl _MMXLength, %%ebx      \n\t"
            "movq %%mm0, -8(%%edi,%%ebx,) \n\t" // Write updated Raws back to array
            // Prep for doing 1st add at top of loop
            "movq %%mm0, %%mm1           \n\t"
            "jb sub_3lp                  \n\t"

            : // FIXASM: output regs/vars go here, e.g.:  "=m" (memory_var)

            : // FIXASM: input regs, e.g.:  "c" (count), "S" (src), "D" (dest)

            : "%ebx", "%edi", "%esi", "%mm0", "%mm1", "%mm6", "%mm7" // CHECKASM: clobber list
         );
      }
      break;

      case 1:
      {
         // Placed here just in case this is a duplicate of the
         // non-MMX code for the SUB filter in png_read_filter_row above
         //
         //         png_bytep rp;
         //         png_bytep lp;
         //         png_uint_32 i;
         //         bpp = (row_info->pixel_depth + 7) >> 3;
         //         for (i = (png_uint_32)bpp, rp = row + bpp, lp = row;
         //            i < row_info->rowbytes; i++, rp++, lp++)
         //      {
         //            *rp = (png_byte)(((int)(*rp) + (int)(*lp)) & 0xff);
         //      }
         __asm__ (
            "movl _dif, %%ebx            \n\t"
            "movl row, %%edi             \n\t"
            "cmpl _FullLength, %%ebx     \n\t"
            "jnb sub_1end                \n\t"
            "movl %%edi, %%esi           \n\t" // lp = row
            "xorl %%eax, %%eax           \n\t"
            "addl bpp, %%edi             \n\t" // rp = row + bpp
         "sub_1lp:                       \n\t"
            "movb (%%esi,%%ebx,), %%al   \n\t"
            "addb %%al, (%%edi,%%ebx,)   \n\t"
            "incl %%ebx                  \n\t"
            "cmpl _FullLength, %%ebx     \n\t"
            "jb sub_1lp                  \n\t"
         "sub_1end:                      \n\t"

            : // FIXASM: output regs/vars go here, e.g.:  "=m" (memory_var)

            : // FIXASM: input regs, e.g.:  "c" (count), "S" (src), "D" (dest)

            : "%eax", "%ebx", "%edi", "%esi" // CHECKASM: clobber list
         );
      }
      return;

      case 6:
      case 7:
      case 4:
      case 5:
      {
         ShiftBpp.use = bpp << 3;
         ShiftRem.use = 64 - ShiftBpp.use;
         __asm__ (
            "movl row, %%edi             \n\t"
            "movl _dif, %%ebx            \n\t"
            "movl %%edi, %%esi           \n\t" // lp = row
            "addl bpp, %%edi             \n\t" // rp = row + bpp
            // PRIME the pump (load the first Raw(x-bpp) data set
            "movq -8(%%edi,%%ebx,), %%mm1 \n\t"
         "sub_4lp:                       \n\t"
            "psrlq $ShiftRem, %%mm1      \n\t" // Shift data for adding 1st bpp bytes
                          // no need for mask; shift clears inactive bytes
            "movq (%%edi,%%ebx,), %%mm0  \n\t"
            "paddb %%mm1, %%mm0          \n\t"
            // Add 2nd active group
            "movq %%mm0, %%mm1           \n\t" // mov updated Raws to mm1
            "psllq $ShiftBpp, %%mm1      \n\t" // shift data to position correctly
                                   // there is no need for any mask
                                   // since shift clears inactive bits/bytes
            "addl $8, %%ebx              \n\t"
            "paddb %%mm1, %%mm0          \n\t"
            "cmpl _MMXLength, %%ebx      \n\t"
            "movq %%mm0, -8(%%edi,%%ebx,) \n\t"
            "movq %%mm0, %%mm1           \n\t" // Prep for doing 1st add at top of loop
            "jb sub_4lp                  \n\t"

            : // FIXASM: output regs/vars go here, e.g.:  "=m" (memory_var)

            : // FIXASM: input regs, e.g.:  "c" (count), "S" (src), "D" (dest)

            : "%ebx", "%edi", "%esi", "%mm0", "%mm1" // CHECKASM: clobber list
         );
      }
      break;

      case 2:
      {
         ActiveMask.use  = 0x00000000ffff0000;
         ShiftBpp.use = 16;       // == 2 * 8
         ShiftRem.use = 48;       // == 64 - 16
         __asm__ (
            "movq $ActiveMask, %%mm7     \n\t" // Load ActiveMask for 2nd active byte group
            "movl _dif, %%ebx            \n\t"
            "movq %%mm7, %%mm6           \n\t"
            "movl row, %%edi             \n\t"
            "psllq $ShiftBpp, %%mm6      \n\t" // Move mask in mm6 to cover 3rd active
                                    //  byte group
            "movl %%edi, %%esi           \n\t" // lp = row
            "movq %%mm6, %%mm5           \n\t"
            "addl bpp, %%edi             \n\t" // rp = row + bpp
            "psllq $ShiftBpp, %%mm5      \n\t" // Move mask in mm5 to cover 4th active
                                    //  byte group
            // PRIME the pump (load the first Raw(x-bpp) data set
            "movq -8(%%edi,%%ebx,), %%mm1 \n\t"
         "sub_2lp:                       \n\t"
            // Add 1st active group
            "psrlq $ShiftRem, %%mm1      \n\t" // Shift data for adding 1st bpp bytes
                                    // no need for mask; shift clears inactive
                                    //  bytes
            "movq (%%edi,%%ebx,), %%mm0  \n\t"
            "paddb %%mm1, %%mm0          \n\t"
            // Add 2nd active group
            "movq %%mm0, %%mm1           \n\t" // mov updated Raws to mm1
            "psllq $ShiftBpp, %%mm1      \n\t" // shift data to position correctly
            "pand %%mm7, %%mm1           \n\t" // mask to use only 2nd active group
            "paddb %%mm1, %%mm0          \n\t"
            // Add 3rd active group
            "movq %%mm0, %%mm1           \n\t" // mov updated Raws to mm1
            "psllq $ShiftBpp, %%mm1      \n\t" // shift data to position correctly
            "pand %%mm6, %%mm1           \n\t" // mask to use only 3rd active group
            "paddb %%mm1, %%mm0          \n\t"
            // Add 4th active group
            "movq %%mm0, %%mm1           \n\t" // mov updated Raws to mm1
            "psllq $ShiftBpp, %%mm1      \n\t" // shift data to position correctly
            "pand %%mm5, %%mm1           \n\t" // mask to use only 4th active group
            "addl $8, %%ebx              \n\t"
            "paddb %%mm1, %%mm0          \n\t"
            "cmpl _MMXLength, %%ebx      \n\t"
            "movq %%mm0, -8(%%edi,%%ebx,) \n\t" // Write updated Raws back to array
            "movq %%mm0, %%mm1           \n\t" // Prep for doing 1st add at top of loop
            "jb sub_2lp                  \n\t"

            : // FIXASM: output regs/vars go here, e.g.:  "=m" (memory_var)

            : // FIXASM: input regs, e.g.:  "c" (count), "S" (src), "D" (dest)

            : "%ebx", "%edi", "%esi", "%mm0", "%mm1", "%mm5", "%mm6", "%mm7" // CHECKASM: clobber list
         );
      }
      break;
      case 8:
      {
         __asm__ (
            "movl row, %%edi             \n\t"
            "movl _dif, %%ebx            \n\t"
            "movl %%edi, %%esi           \n\t" // lp = row
            "addl bpp, %%edi             \n\t" // rp = row + bpp
            "movl _MMXLength, %%ecx      \n\t"
            "movq -8(%%edi,%%ebx,), %%mm7 \n\t" // PRIME the pump (load the first
                                    // Raw(x-bpp) data set
            "andl $0x0000003f, %%ecx     \n\t" // calc bytes over mult of 64
         "sub_8lp:                       \n\t"
            "movq (%%edi,%%ebx,), %%mm0  \n\t" // Load Sub(x) for 1st 8 bytes
            "paddb %%mm7, %%mm0          \n\t"
            "movq 8(%%edi,%%ebx,), %%mm1 \n\t" // Load Sub(x) for 2nd 8 bytes
            "movq %%mm0, (%%edi,%%ebx,)  \n\t" // Write Raw(x) for 1st 8 bytes
                                   // Now mm0 will be used as Raw(x-bpp) for
                                   // the 2nd group of 8 bytes.  This will be
                                   // repeated for each group of 8 bytes with
                                   // the 8th group being used as the Raw(x-bpp)
                                   // for the 1st group of the next loop.
            "paddb %%mm0, %%mm1          \n\t"
            "movq 16(%%edi,%%ebx,), %%mm2 \n\t" // Load Sub(x) for 3rd 8 bytes
            "movq %%mm1, 8(%%edi,%%ebx,) \n\t" // Write Raw(x) for 2nd 8 bytes
            "paddb %%mm1, %%mm2          \n\t"
            "movq 24(%%edi,%%ebx,), %%mm3 \n\t" // Load Sub(x) for 4th 8 bytes
            "movq %%mm2, 16(%%edi,%%ebx,) \n\t" // Write Raw(x) for 3rd 8 bytes
            "paddb %%mm2, %%mm3          \n\t"
            "movq 32(%%edi,%%ebx,), %%mm4 \n\t" // Load Sub(x) for 5th 8 bytes
            "movq %%mm3, 24(%%edi,%%ebx,) \n\t" // Write Raw(x) for 4th 8 bytes
            "paddb %%mm3, %%mm4          \n\t"
            "movq 40(%%edi,%%ebx,), %%mm5 \n\t" // Load Sub(x) for 6th 8 bytes
            "movq %%mm4, 32(%%edi,%%ebx,) \n\t" // Write Raw(x) for 5th 8 bytes
            "paddb %%mm4, %%mm5          \n\t"
            "movq 48(%%edi,%%ebx,), %%mm6 \n\t" // Load Sub(x) for 7th 8 bytes
            "movq %%mm5, 40(%%edi,%%ebx,) \n\t" // Write Raw(x) for 6th 8 bytes
            "paddb %%mm5, %%mm6          \n\t"
            "movq 56(%%edi,%%ebx,), %%mm7 \n\t" // Load Sub(x) for 8th 8 bytes
            "movq %%mm6, 48(%%edi,%%ebx,) \n\t" // Write Raw(x) for 7th 8 bytes
            "addl $64, %%ebx             \n\t"
            "paddb %%mm6, %%mm7          \n\t"
            "cmpl %%ecx, %%ebx           \n\t"
            "movq %%mm7, -8(%%edi,%%ebx,) \n\t" // Write Raw(x) for 8th 8 bytes
            "jb sub_8lp                  \n\t"
            "cmpl _MMXLength, %%ebx      \n\t"
            "jnb sub_8lt8                \n\t"
         "sub_8lpA:                      \n\t"
            "movq (%%edi,%%ebx,), %%mm0  \n\t"
            "addl $8, %%ebx              \n\t"
            "paddb %%mm7, %%mm0          \n\t"
            "cmpl _MMXLength, %%ebx      \n\t"
            "movq %%mm0, -8(%%edi,%%ebx,) \n\t" // use -8 to offset early add to ebx
            "movq %%mm0, %%mm7           \n\t" // Move calculated Raw(x) data to mm1 to
                                    // be the new Raw(x-bpp) for the next loop
            "jb sub_8lpA                 \n\t"
         "sub_8lt8:                      \n\t"

            : // FIXASM: output regs/vars go here, e.g.:  "=m" (memory_var)

            : // FIXASM: input regs, e.g.:  "c" (count), "S" (src), "D" (dest)

            : "%ebx", "%ecx", "%edi", "%esi", "%mm0", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5", "%mm6", "%mm7" // CHECKASM: clobber list
         );
      }
      break;

      default:                // bpp greater than 8 bytes
      {
         __asm__ (
            "movl _dif, %%ebx            \n\t"
            "movl row, %%edi             \n\t"
            "movl %%edi, %%esi           \n\t" // lp = row
            "addl bpp, %%edi             \n\t" // rp = row + bpp
         "sub_Alp:                       \n\t"
            "movq (%%edi,%%ebx,), %%mm0  \n\t"
            "movq (%%esi,%%ebx,), %%mm1  \n\t"
            "addl $8, %%ebx              \n\t"
            "paddb %%mm1, %%mm0          \n\t"
            "cmpl _MMXLength, %%ebx      \n\t"
            "movq %%mm0, -8(%%edi,%%ebx,) \n\t" // mov does not affect flags; -8 to offset
                                   //  add ebx
            "jb sub_Alp                  \n\t"

            : // FIXASM: output regs/vars go here, e.g.:  "=m" (memory_var)

            : // FIXASM: input regs, e.g.:  "c" (count), "S" (src), "D" (dest)

            : "%ebx", "%edi", "%esi", "%mm0", "%mm1" // CHECKASM: clobber list
         );
      }
      break;

   } // end switch ( bpp )

   __asm__ (
      "movl _MMXLength, %%ebx      \n\t"
      "movl row, %%edi             \n\t"
      "cmpl _FullLength, %%ebx     \n\t"
      "jnb sub_end                 \n\t"
      "movl %%edi, %%esi           \n\t" // lp = row
      "xorl %%eax, %%eax           \n\t"
      "addl bpp, %%edi             \n\t" // rp = row + bpp
   "sub_lp2:                       \n\t"
      "movb (%%esi,%%ebx,), %%al   \n\t"
      "addb %%al, (%%edi,%%ebx,)   \n\t"
      "incl %%ebx                  \n\t"
      "cmpl _FullLength, %%ebx     \n\t"
      "jb sub_lp2                  \n\t"
   "sub_end:                       \n\t"
      "emms                        \n\t" // end MMX instructions

      : // FIXASM: output regs/vars go here, e.g.:  "=m" (memory_var)

      : // FIXASM: input regs, e.g.:  "c" (count), "S" (src), "D" (dest)

      : "%eax", "%ebx", "%edi", "%esi" // CHECKASM: clobber list
   );
#endif /* GRR_GCC_MMX_CONVERTED */
}

// Optimized code for PNG Up filter decoder
void /* PRIVATE */
png_read_filter_row_mmx_up(png_row_infop row_info, png_bytep row,
                           png_bytep prev_row)
{
#ifdef GRR_GCC_MMX_CONVERTED
   png_uint_32 len;

   len = row_info->rowbytes;       // # of bytes to filter
   __asm__ (
      "movl row, %%edi             \n\t"
      // get # of bytes to alignment
      "movl %%edi, %%ecx           \n\t"
      "xorl %%ebx, %%ebx           \n\t"
      "addl $0x7, %%ecx            \n\t"
      "xorl %%eax, %%eax           \n\t"
      "andl $0xfffffff8, %%ecx     \n\t"
      "movl prev_row, %%esi        \n\t"
      "subl %%edi, %%ecx           \n\t"
      "jz up_go                    \n\t"
      // fix alignment
   "up_lp1:                        \n\t"
      "movb (%%edi,%%ebx,), %%al   \n\t"
      "addb (%%esi,%%ebx,), %%al   \n\t"
      "incl %%ebx                  \n\t"
      "cmpl %%ecx, %%ebx           \n\t"
      "movb %%al, -1(%%edi,%%ebx,) \n\t" // mov does not affect flags; -1 to offset inc ebx
      "jb up_lp1                   \n\t"
   "up_go:                         \n\t"
      "movl len, %%ecx             \n\t"
      "movl %%ecx, %%edx           \n\t"
      "subl %%ebx, %%edx           \n\t" // subtract alignment fix
      "andl $0x0000003f, %%edx     \n\t" // calc bytes over mult of 64
      "subl %%edx, %%ecx           \n\t" // drop over bytes from length
      // Unrolled loop - use all MMX registers and interleave to reduce
      // number of branch instructions (loops) and reduce partial stalls
   "up_loop:                       \n\t"
      "movq (%%esi,%%ebx,), %%mm1  \n\t"
      "movq (%%edi,%%ebx,), %%mm0  \n\t"
      "movq 8(%%esi,%%ebx,), %%mm3 \n\t"
      "paddb %%mm1, %%mm0          \n\t"
      "movq 8(%%edi,%%ebx,), %%mm2 \n\t"
      "movq %%mm0, (%%edi,%%ebx,)  \n\t"
      "paddb %%mm3, %%mm2          \n\t"
      "movq 16(%%esi,%%ebx,), %%mm5 \n\t"
      "movq %%mm2, 8(%%edi,%%ebx,) \n\t"
      "movq 16(%%edi,%%ebx,), %%mm4 \n\t"
      "movq 24(%%esi,%%ebx,), %%mm7 \n\t"
      "paddb %%mm5, %%mm4          \n\t"
      "movq 24(%%edi,%%ebx,), %%mm6 \n\t"
      "movq %%mm4, 16(%%edi,%%ebx,) \n\t"
      "paddb %%mm7, %%mm6          \n\t"
      "movq 32(%%esi,%%ebx,), %%mm1 \n\t"
      "movq %%mm6, 24(%%edi,%%ebx,) \n\t"
      "movq 32(%%edi,%%ebx,), %%mm0 \n\t"
      "movq 40(%%esi,%%ebx,), %%mm3 \n\t"
      "paddb %%mm1, %%mm0          \n\t"
      "movq 40(%%edi,%%ebx,), %%mm2 \n\t"
      "movq %%mm0, 32(%%edi,%%ebx,) \n\t"
      "paddb %%mm3, %%mm2          \n\t"
      "movq 48(%%esi,%%ebx,), %%mm5 \n\t"
      "movq %%mm2, 40(%%edi,%%ebx,) \n\t"
      "movq 48(%%edi,%%ebx,), %%mm4 \n\t"
      "movq 56(%%esi,%%ebx,), %%mm7 \n\t"
      "paddb %%mm5, %%mm4          \n\t"
      "movq 56(%%edi,%%ebx,), %%mm6 \n\t"
      "movq %%mm4, 48(%%edi,%%ebx,) \n\t"
      "addl $64, %%ebx             \n\t"
      "paddb %%mm7, %%mm6          \n\t"
      "cmpl %%ecx, %%ebx           \n\t"
      "movq %%mm6, -8(%%edi,%%ebx,) \n\t" // (+56)movq does not affect flags;
                                     // -8 to offset add ebx
      "jb up_loop                  \n\t"

      "cmpl $0, %%edx              \n\t" // Test for bytes over mult of 64
      "jz up_end                   \n\t"


      // 2 lines added by lcreeve@netins.net
      // (mail 11 Jul 98 in png-implement list)
      "cmpl $8, %%edx              \n\t" //test for less than 8 bytes
      "jb up_lt8                   \n\t"


      "addl %%edx, %%ecx           \n\t"
      "andl $0x00000007, %%edx     \n\t" // calc bytes over mult of 8
      "subl %%edx, %%ecx           \n\t" // drop over bytes from length
      "jz up_lt8                   \n\t"
      // Loop using MMX registers mm0 & mm1 to update 8 bytes simultaneously
   "up_lpA:                        \n\t"
      "movq (%%esi,%%ebx,), %%mm1  \n\t"
      "movq (%%edi,%%ebx,), %%mm0  \n\t"
      "addl $8, %%ebx              \n\t"
      "paddb %%mm1, %%mm0          \n\t"
      "cmpl %%ecx, %%ebx           \n\t"
      "movq %%mm0, -8(%%edi,%%ebx,) \n\t" // movq does not affect flags; -8 to offset add ebx
      "jb up_lpA                   \n\t"
      "cmpl $0, %%edx              \n\t" // Test for bytes over mult of 8
      "jz up_end                   \n\t"
   "up_lt8:                        \n\t"
      "xorl %%eax, %%eax           \n\t"
      "addl %%edx, %%ecx           \n\t" // move over byte count into counter
      // Loop using x86 registers to update remaining bytes
   "up_lp2:                        \n\t"
      "movb (%%edi,%%ebx,), %%al   \n\t"
      "addb (%%esi,%%ebx,), %%al   \n\t"
      "incl %%ebx                  \n\t"
      "cmpl %%ecx, %%ebx           \n\t"
      "movb %%al, -1(%%edi,%%ebx,) \n\t" // mov does not affect flags; -1 to offset inc ebx
      "jb up_lp2                   \n\t"
   "up_end:                        \n\t"
      // Conversion of filtered row completed
      "emms                        \n\t" // End MMX instructions; prep for possible FP instrs.

      : // FIXASM: output regs/vars go here, e.g.:  "=m" (memory_var)

      : // FIXASM: input regs, e.g.:  "c" (count), "S" (src), "D" (dest)

      : "%eax", "%ebx", "%ecx", "%edx", "%edi", "%esi", "%mm0", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5", "%mm6", "%mm7" // CHECKASM: clobber list
   );
#endif /* GRR_GCC_MMX_CONVERTED */
}


#if defined(PNG_HAVE_ASSEMBLER_READ_FILTER_ROW)

// Optimized png_read_filter_row routines

void /* PRIVATE */
png_read_filter_row(png_structp png_ptr, png_row_infop row_info, png_bytep
   row, png_bytep prev_row, int filter)
{
#ifdef PNG_DEBUG
   char filnm[6];
#endif
   #define UseMMX 1

   if (mmx_supported == 2)
       mmx_supported = mmxsupport();

#ifdef GRR_GCC_MMX_CONVERTED
   if (!mmx_supported)
#endif
   {
       png_read_filter_row_c(png_ptr, row_info, row, prev_row, filter);
       return ;
   }

#ifdef PNG_DEBUG
   png_debug(1, "in png_read_filter_row\n");
#if (UseMMX == 1)
   png_debug1(0,"%s, ", "MMX");
#else
   png_debug1(0,"%s, ", "x86");
#endif
   switch (filter)
   {
      case 0: sprintf(filnm, "None ");
         break;
      case 1: sprintf(filnm, "Sub  ");
         break;
      case 2: sprintf(filnm, "Up   ");
         break;
      case 3: sprintf(filnm, "Avg  ");
         break;
      case 4: sprintf(filnm, "Paeth");
         break;
      default: sprintf(filnm, "Unknw");
         break;
   }
   png_debug2(0,"row=%5d, %s, ", png_ptr->row_number, filnm);
   png_debug2(0, "pd=%2d, b=%d, ", (int)row_info->pixel_depth,
      (int)((row_info->pixel_depth + 7) >> 3));
   png_debug1(0,"len=%8d, ", row_info->rowbytes);
#endif

   switch (filter)
   {
      case PNG_FILTER_VALUE_NONE:
         break;

      case PNG_FILTER_VALUE_SUB:
#if (UseMMX == 1)
         if ((row_info->pixel_depth > 8) && (row_info->rowbytes >= 128))
         {
            png_read_filter_row_mmx_sub(row_info, row);
         }
         else
#endif
         {
            png_uint_32 i;
            png_uint_32 istop = row_info->rowbytes;
            png_uint_32 bpp = (row_info->pixel_depth + 7) >> 3;
            png_bytep rp = row + bpp;
            png_bytep lp = row;

            for (i = bpp; i < istop; i++)
            {
               *rp = (png_byte)(((int)(*rp) + (int)(*lp++)) & 0xff);
               rp++;
            }
         }  //end !UseMMX
         break;

      case PNG_FILTER_VALUE_UP:
#if (UseMMX == 1)
         if ((row_info->pixel_depth > 8) && (row_info->rowbytes >= 128))
         {
            png_read_filter_row_mmx_up(row_info, row, prev_row);
         }
         else
#endif
         {
            png_bytep rp;
            png_bytep pp;
            png_uint_32 i;
            for (i = 0, rp = row, pp = prev_row;
               i < row_info->rowbytes; i++, rp++, pp++)
            {
                  *rp = (png_byte)(((int)(*rp) + (int)(*pp)) & 0xff);
            }
         }  //end !UseMMX
         break;

      case PNG_FILTER_VALUE_AVG:
#if (UseMMX == 1)
         if ((row_info->pixel_depth > 8) && (row_info->rowbytes >= 128))
         {
            png_read_filter_row_mmx_avg(row_info, row, prev_row);
         }
         else
#endif
         {
            png_uint_32 i;
            png_bytep rp = row;
            png_bytep pp = prev_row;
            png_bytep lp = row;
            png_uint_32 bpp = (row_info->pixel_depth + 7) >> 3;
            png_uint_32 istop = row_info->rowbytes - bpp;

            for (i = 0; i < bpp; i++)
            {
               *rp = (png_byte)(((int)(*rp) +
                  ((int)(*pp++) >> 1)) & 0xff);
               rp++;
            }

            for (i = 0; i < istop; i++)
            {
               *rp = (png_byte)(((int)(*rp) +
                  ((int)(*pp++ + *lp++) >> 1)) & 0xff);
               rp++;
            }
         }  //end !UseMMX
         break;

      case PNG_FILTER_VALUE_PAETH:
#if (UseMMX == 1)
         if ((row_info->pixel_depth > 8) && (row_info->rowbytes >= 128))
         {
            png_read_filter_row_mmx_paeth(row_info, row, prev_row);
         }
         else
#endif
         {
            png_uint_32 i;
            png_bytep rp = row;
            png_bytep pp = prev_row;
            png_bytep lp = row;
            png_bytep cp = prev_row;
            png_uint_32 bpp = (row_info->pixel_depth + 7) >> 3;
            png_uint_32 istop=row_info->rowbytes - bpp;

            for (i = 0; i < bpp; i++)
            {
               *rp = (png_byte)(((int)(*rp) + (int)(*pp++)) & 0xff);
               rp++;
            }

            for (i = 0; i < istop; i++)   // use leftover rp,pp
            {
               int a, b, c, pa, pb, pc, p;

               a = *lp++;
               b = *pp++;
               c = *cp++;

               p = b - c;
               pc = a - c;

#ifdef PNG_USE_ABS
               pa = abs(p);
               pb = abs(pc);
               pc = abs(p + pc);
#else
               pa = p < 0 ? -p : p;
               pb = pc < 0 ? -pc : pc;
               pc = (p + pc) < 0 ? -(p + pc) : p + pc;
#endif

               /*
                  if (pa <= pb && pa <= pc)
                     p = a;
                  else if (pb <= pc)
                     p = b;
                  else
                     p = c;
                */

               p = (pa <= pb && pa <=pc) ? a : (pb <= pc) ? b : c;

               *rp = (png_byte)(((int)(*rp) + p) & 0xff);
               rp++;
            }
         }  //end !UseMMX
         break;

      default:
         png_warning(png_ptr, "Ignoring bad adaptive filter type");
         *row=0;
         break;
   }
}

#endif /* PNG_HAVE_ASSEMBLER_READ_FILTER_ROW */


// GRR NOTES:  (1) the following code assumes 386 or better (pushfl/popfl)
//             (2) all instructions compile with gcc 2.7.2.3 and later
//             (3) the function is moved down here to prevent gcc from
//                  inlining it in multiple places and then barfing be-
//                  cause the ".NOT_SUPPORTED" label is multiply defined
//             [is there a way to signal that a *single* function should
//              not be inlined?  is there a way to modify the label for
//              each inlined instance, e.g., by appending _1, _2, etc.?
//              maybe if don't use leading "." in label name? (not tested)]

#ifdef ORIG_THAT_USED_TO_CLOBBER_EBX

int mmxsupport(void)
{
    int mmx_supported_local = 0;

    __asm__ (
//      ".byte  0x66          \n\t"  // convert 16-bit pushf to 32-bit pushfd
//      "pushf                \n\t"  // save Eflag to stack
        "pushfl               \n\t"  // save Eflag to stack
        "popl %%eax           \n\t"  // get Eflag from stack into eax
        "movl %%eax, %%ecx    \n\t"  // make another copy of Eflag in ecx
        "xorl $0x200000, %%eax \n\t" // toggle ID bit in Eflag (i.e., bit 21)
        "pushl %%eax          \n\t"  // save modified Eflag back to stack
//      ".byte  0x66          \n\t"  // convert 16-bit popf to 32-bit popfd
//      "popf                 \n\t"  // restore modified value to Eflag reg
        "popfl                \n\t"  // restore modified value to Eflag reg
        "pushfl               \n\t"  // save Eflag to stack
        "popl %%eax           \n\t"  // get Eflag from stack
        "xorl %%ecx, %%eax    \n\t"  // compare new Eflag with original Eflag
        "jz .NOT_SUPPORTED    \n\t"  // if same, CPUID instr. is not supported

        "xorl %%eax, %%eax    \n\t"  // set eax to zero
//      ".byte  0x0f, 0xa2    \n\t"  // CPUID instruction (two-byte opcode)
        "cpuid                \n\t"  // get the CPU identification info
        "cmpl $1, %%eax       \n\t"  // make sure eax return non-zero value
        "jl .NOT_SUPPORTED    \n\t"  // if eax is zero, MMX is not supported

        "xorl %%eax, %%eax    \n\t"  // set eax to zero and...
        "incl %%eax           \n\t"  // ...increment eax to 1.  This pair is
                                     // faster than the instruction "mov eax, 1"
        "cpuid                \n\t"  // get the CPU identification info again
        "andl $0x800000, %%edx \n\t" // mask out all bits but MMX bit (23)
        "cmpl $0, %%edx       \n\t"  // 0 = MMX not supported
        "jz .NOT_SUPPORTED    \n\t"  // non-zero = yes, MMX IS supported

        "movl $1, %0          \n\t"  // set return value to 1 and fall through

    ".NOT_SUPPORTED:          \n\t"  // target label for jump instructions
        "movl %0, %%eax       \n\t"  // move return value to eax
                                     // DONE

        : "=m" (mmx_supported_local) // %0 (output list:  memory only)

        :                            // any variables used on input (none)

        : "%eax", "%ebx",            // clobber list
          "%ecx", "%edx"
//      , "memory"   // if write to a variable gcc thought was in a reg
//      , "cc"       // "condition codes" (flag bits)
    );

    //mmx_supported_local=0; // test code for force don't support MMX
    //printf("MMX : %u (1=MMX supported)\n",mmx_supported_local);

    return mmx_supported_local;
}

#else /* !ORIG_THAT_USED_TO_CLOBBER_EBX */

int mmxsupport(void)
{
    __asm__ (
        "pushl %%ebx          \n\t"  // ebx gets clobbered by CPUID instruction
        "pushl %%ecx          \n\t"  // so does ecx...
        "pushl %%edx          \n\t"  // ...and edx (but ecx & edx safe on Linux)
//      ".byte  0x66          \n\t"  // convert 16-bit pushf to 32-bit pushfd
//      "pushf                \n\t"  // save Eflag to stack
        "pushfl               \n\t"  // save Eflag to stack
        "popl %%eax           \n\t"  // get Eflag from stack into eax
        "movl %%eax, %%ecx    \n\t"  // make another copy of Eflag in ecx
        "xorl $0x200000, %%eax \n\t" // toggle ID bit in Eflag (i.e., bit 21)
        "pushl %%eax          \n\t"  // save modified Eflag back to stack
//      ".byte  0x66          \n\t"  // convert 16-bit popf to 32-bit popfd
//      "popf                 \n\t"  // restore modified value to Eflag reg
        "popfl                \n\t"  // restore modified value to Eflag reg
        "pushfl               \n\t"  // save Eflag to stack
        "popl %%eax           \n\t"  // get Eflag from stack
        "xorl %%ecx, %%eax    \n\t"  // compare new Eflag with original Eflag
        "jz .NOT_SUPPORTED    \n\t"  // if same, CPUID instr. is not supported

        "xorl %%eax, %%eax    \n\t"  // set eax to zero
//      ".byte  0x0f, 0xa2    \n\t"  // CPUID instruction (two-byte opcode)
        "cpuid                \n\t"  // get the CPU identification info
        "cmpl $1, %%eax       \n\t"  // make sure eax return non-zero value
        "jl .NOT_SUPPORTED    \n\t"  // if eax is zero, MMX is not supported

        "xorl %%eax, %%eax    \n\t"  // set eax to zero and...
        "incl %%eax           \n\t"  // ...increment eax to 1.  This pair is
                                     // faster than the instruction "mov eax, 1"
        "cpuid                \n\t"  // get the CPU identification info again
        "andl $0x800000, %%edx \n\t" // mask out all bits but MMX bit (23)
        "cmpl $0, %%edx       \n\t"  // 0 = MMX not supported
        "jz .NOT_SUPPORTED    \n\t"  // non-zero = yes, MMX IS supported

        "movl $1, %%eax       \n\t"  // set return value to 1
        "popl %%edx           \n\t"  // restore edx
        "popl %%ecx           \n\t"  // restore ecx
        "popl %%ebx           \n\t"  // restore ebx ("row" in png_do_interlace)
        "ret                  \n\t"  // DONE:  have MMX support

    ".NOT_SUPPORTED:          \n\t"  // target label for jump instructions
        "movl $0, %%eax       \n\t"  // set return value to 0
        "popl %%edx           \n\t"  // restore edx
        "popl %%ecx           \n\t"  // restore ecx
        "popl %%ebx           \n\t"  // restore ebx ("row" in png_do_interlace)
//      "ret                  \n\t"  // DONE:  no MMX support
                                     // (fall through to standard C "ret")

        : // "=m" (mmx_supported_local) // %0 (output list:  memory only)

        :                            // any variables used on input (none)

        : "%eax"                     // clobber list
//      , "%ebx", "%ecx", "%edx"     // GRR:  we handle these manually
//      , "memory"   // if write to a variable gcc thought was in a reg
//      , "cc"       // "condition codes" (flag bits)
    );

    //mmx_supported_local=0; // test code for force don't support MMX
    //printf("MMX : %u (1=MMX supported)\n",mmx_supported_local);

    //return mmx_supported_local;
}

#endif /* ?ORIG_THAT_USED_TO_CLOBBER_EBX */

#endif /* PNG_ASSEMBLER_CODE_SUPPORTED && PNG_USE_PNGGCCRD */


