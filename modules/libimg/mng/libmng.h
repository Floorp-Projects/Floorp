/* ************************************************************************** */
/* *                                                                        * */
/* * COPYRIGHT NOTICE:                                                      * */
/* *                                                                        * */
/* * Copyright (c) 2000,2001 Gerard Juyn (gerard@libmng.com)                * */
/* * [You may insert additional notices after this sentence if you modify   * */
/* *  this source]                                                          * */
/* *                                                                        * */
/* * For the purposes of this copyright and license, "Contributing Authors" * */
/* * is defined as the following set of individuals:                        * */
/* *                                                                        * */
/* *    Gerard Juyn (gerard@libmng.com)                                     * */
/* *                                                                        * */
/* * The MNG Library is supplied "AS IS".  The Contributing Authors         * */
/* * disclaim all warranties, expressed or implied, including, without      * */
/* * limitation, the warranties of merchantability and of fitness for any   * */
/* * purpose.  The Contributing Authors assume no liability for direct,     * */
/* * indirect, incidental, special, exemplary, or consequential damages,    * */
/* * which may result from the use of the MNG Library, even if advised of   * */
/* * the possibility of such damage.                                        * */
/* *                                                                        * */
/* * Permission is hereby granted to use, copy, modify, and distribute this * */
/* * source code, or portions hereof, for any purpose, without fee, subject * */
/* * to the following restrictions:                                         * */
/* *                                                                        * */
/* * 1. The origin of this source code must not be misrepresented;          * */
/* *    you must not claim that you wrote the original software.            * */
/* *                                                                        * */
/* * 2. Altered versions must be plainly marked as such and must not be     * */
/* *    misrepresented as being the original source.                        * */
/* *                                                                        * */
/* * 3. This Copyright notice may not be removed or altered from any source * */
/* *    or altered source distribution.                                     * */
/* *                                                                        * */
/* * The Contributing Authors specifically permit, without fee, and         * */
/* * encourage the use of this source code as a component to supporting     * */
/* * the MNG and JNG file format in commercial products.  If you use this   * */
/* * source code in a product, acknowledgment would be highly appreciated.  * */
/* *                                                                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * Parts of this software have been adapted from the libpng package.      * */
/* * Although this library supports all features from the PNG specification * */
/* * (as MNG descends from it) it does not require the libpng package.      * */
/* * It does require the zlib library and optionally the IJG jpeg library,  * */
/* * and/or the "little-cms" library by Marti Maria (depending on the       * */
/* * inclusion of support for JNG and Full-Color-Management respectively.   * */
/* *                                                                        * */
/* * This library's function is primarily to read and display MNG           * */
/* * animations. It is not meant as a full-featured image-editing           * */
/* * component! It does however offer creation and editing functionality    * */
/* * at the chunk level.                                                    * */
/* * (future modifications may include some more support for creation       * */
/* *  and or editing)                                                       * */
/* *                                                                        * */
/* ************************************************************************** */

/* ************************************************************************** */
/* *                                                                        * */
/* * Version numbering                                                      * */
/* *                                                                        * */
/* * X.Y.Z : X = release (0 = initial build)                                * */
/* *         Y = major version (uneven = test; even = production)           * */
/* *         Z = minor version (bugfixes; 2 is older than 10)               * */
/* *                                                                        * */
/* * production versions only appear when a test-version is extensively     * */
/* * tested and found stable or for intermediate bug-fixes (recognized by   * */
/* * a change in the Z number)                                              * */
/* *                                                                        * */
/* * x.1.x      = test version                                              * */
/* * x.2.x      = production version                                        * */
/* * x.3.x      = test version                                              * */
/* * x.4.x      = production version                                        * */
/* *  etc.                                                                  * */
/* *                                                                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * Identifier naming conventions throughout this library                  * */
/* *                                                                        * */
/* * iXxxx      = an integer                                                * */
/* * dXxxx      = a float                                                   * */
/* * pXxxx      = a pointer                                                 * */
/* * bXxxx      = a boolean                                                 * */
/* * eXxxx      = an enumeration                                            * */
/* * hXxxx      = a handle                                                  * */
/* * zXxxx      = a zero-terminated string (pchar)                          * */
/* * fXxxx      = a pointer to a function (callback)                        * */
/* * aXxxx      = an array                                                  * */
/* * sXxxx      = a structure                                               * */
/* *                                                                        * */
/* * Macros & defines are in all uppercase.                                 * */
/* * Functions & typedefs in all lowercase.                                 * */
/* * Exported stuff is prefixed with MNG_ or mng_ respectively.             * */
/* *                                                                        * */
/* * (I may have missed a couple; don't hesitate to let me know!)           * */
/* *                                                                        * */
/* ************************************************************************** */

/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : libmng.h                  copyright (c) 2000 G.Juyn        * */
/* * version   : 1.0.3                                                      * */
/* *                                                                        * */
/* * purpose   : main application interface                                 * */
/* *                                                                        * */
/* * author    : G.Juyn                                                     * */
/* * web       : http://www.3-t.com                                         * */
/* * email     : mailto:info@3-t.com                                        * */
/* *                                                                        * */
/* * comment   : The main application interface. An application should not  * */
/* *             need access to any of the other modules!                   * */
/* *                                                                        * */
/* * changes   : 0.5.1 - 05/06/2000 - G.Juyn                                * */
/* *             - changed chunk iteration function                         * */
/* *             0.5.1 - 05/08/2000 - G.Juyn                                * */
/* *             - added chunk access functions                             * */
/* *             - added version control constants & functions              * */
/* *             - changed strict-ANSI stuff                                * */
/* *             0.5.1 - 05/11/2000 - G.Juyn                                * */
/* *             - added set_outputprofile2 & set_srgbprofile2              * */
/* *             - added empty-chunk put-routines                           * */
/* *             0.5.1 - 05/12/2000 - G.Juyn                                * */
/* *             - added version_dll & VERSION_DLL (for consistency)        * */
/* *             - added version control explanatory text & samples         * */
/* *             0.5.1 - 05/15/2000 - G.Juyn                                * */
/* *             - added getimgdata & putimgdata functions                  * */
/* *                                                                        * */
/* *             0.5.2 - 05/16/2000 - G.Juyn                                * */
/* *             - changed the version parameters (obviously)               * */
/* *             0.5.2 - 05/18/2000 - G.Juyn                                * */
/* *             - complimented constants for chunk-property values         * */
/* *             0.5.2 - 05/23/2000 - G.Juyn                                * */
/* *             - fixed MNG_UINT_pHYg value                                * */
/* *             0.5.2 - 05/24/2000 - G.Juyn                                * */
/* *             - added support for get/set default zlib/IJG parms         * */
/* *             0.5.2 - 06/02/2000 - G.Juyn                                * */
/* *             - added MNG_BIGENDIAN_SUPPORT (contributed by Tim Rowley)  * */
/* *             - separated configuration-options into "mng_conf.h"        * */
/* *             - added RGB8_A8 canvasstyle                                * */
/* *             - added getalphaline callback for RGB8_A8 canvasstyle      * */
/* *             0.5.2 - 06/06/2000 - G.Juyn                                * */
/* *             - moved errorcodes from "mng_error.h"                      * */
/* *             - added mng_read_resume function to support                * */
/* *               read-suspension                                          * */
/* *                                                                        * */
/* *             0.5.3 - 06/16/2000 - G.Juyn                                * */
/* *             - changed the version parameters (obviously)               * */
/* *             0.5.3 - 06/21/2000 - G.Juyn                                * */
/* *             - added get/set for speedtype to facilitate testing        * */
/* *             - added get for imagelevel during processtext callback     * */
/* *             0.5.3 - 06/24/2000 - G.Juyn                                * */
/* *             - fixed inclusion of IJG read/write code                   * */
/* *             0.5.3 - 06/26/2000 - G.Juyn                                * */
/* *             - changed userdata variable to mng_ptr                     * */
/* *                                                                        * */
/* *             0.9.0 - 06/30/2000 - G.Juyn                                * */
/* *             - changed refresh parameters to 'x,y,width,height'         * */
/* *                                                                        * */
/* *             0.9.1 - 07/06/2000 - G.Juyn                                * */
/* *             - added MNG_NEEDTIMERWAIT errorcode                        * */
/* *             - changed comments to indicate modified behavior for       * */
/* *               timer & suspension breaks                                * */
/* *             0.9.1 - 07/08/2000 - G.Juyn                                * */
/* *             - added get routines for internal display variables        * */
/* *             - added get/set routines for suspensionmode variable       * */
/* *             0.9.1 - 07/15/2000 - G.Juyn                                * */
/* *             - added callbacks for SAVE/SEEK processing                 * */
/* *             - added get/set routines for sectionbreak variable         * */
/* *             - added NEEDSECTIONWAIT errorcode                          * */
/* *             0.9.1 - 07/19/2000 - G.Juyn                                * */
/* *             - added function to set frame-/layer-count & playtime      * */
/* *             - added errorcode for updatemngheader if not a MNG         * */
/* *                                                                        * */
/* *             0.9.2 - 07/31/2000 - G.Juyn                                * */
/* *             - fixed problem with trace-functions improperly wrapped    * */
/* *             - added status_xxxx functions                              * */
/* *             0.9.2 - 08/05/2000 - G.Juyn                                * */
/* *             - changed file-prefixes                                    * */
/* *             - added function to set simplicity field                   * */
/* *                                                                        * */
/* *             0.9.3 - 08/09/2000 - G.Juyn                                * */
/* *             - added check for simplicity-bits in MHDR                  * */
/* *             0.9.3 - 08/12/2000 - G.Juyn                                * */
/* *             - added workaround for faulty PhotoShop iCCP chunk         * */
/* *             0.9.3 - 08/26/2000 - G.Juyn                                * */
/* *             - added MAGN chunk                                         * */
/* *             0.9.3 - 09/07/2000 - G.Juyn                                * */
/* *             - added support for new filter_types                       * */
/* *             0.9.3 - 10/10/2000 - G.Juyn                                * */
/* *             - added support for alpha-depth prediction                 * */
/* *             0.9.3 - 10/11/2000 - G.Juyn                                * */
/* *             - fixed processing of unknown critical chunks              * */
/* *             - removed test-MaGN                                        * */
/* *             - added PNG/MNG spec version indicators                    * */
/* *             - added support for nEED                                   * */
/* *             0.9.3 - 10/16/2000 - G.Juyn                                * */
/* *             - added functions to retrieve PNG/JNG specific header-info * */
/* *             - added JDAA chunk                                         * */
/* *             0.9.3 - 10/17/2000 - G.Juyn                                * */
/* *             - added callback to process non-critical unknown chunks    * */
/* *             0.9.3 - 10/20/2000 - G.Juyn                                * */
/* *             - added errocode for delayed delta-processing              * */
/* *             - added get/set for bKGD preference setting                * */
/* *             0.9.3 - 10/21/2000 - G.Juyn                                * */
/* *             - added get function for interlace/progressive display     * */
/* *                                                                        * */
/* *             0.9.4 - 01/18/2001 - G.Juyn                                * */
/* *             - added errorcode for MAGN methods                         * */
/* *             - removed test filter-methods 1 & 65                       * */
/* *                                                                        * */
/* *             1.0.0 - 02/05/2001 - G.Juyn                                * */
/* *             - version numbers (obviously)                              * */
/* *                                                                        * */
/* *             1.0.1 - 02/08/2001 - G.Juyn                                * */
/* *             - added MEND processing callback                           * */
/* *             1.0.1 - 04/21/2001 - G.Juyn (code by G.Kelly)              * */
/* *             - added BGRA8 canvas with premultiplied alpha              * */
/* *             1.0.1 - 05/02/2001 - G.Juyn                                * */
/* *             - added "default" sRGB generation (Thanks Marti!)          * */
/* *                                                                        * */
/* *             1.0.2 - 06/23/2001 - G.Juyn                                * */
/* *             - added optimization option for MNG-video playback         * */
/* *             - added processterm callback                               * */
/* *             1.0.2 - 06/25/2001 - G.Juyn                                * */
/* *             - added late binding errorcode (not used internally)       * */
/* *             - added option to turn off progressive refresh             * */
/* *                                                                        * */
/* *             1.0.3 - 08/06/2001 - G.Juyn                                * */
/* *             - added get function for last processed BACK chunk         * */
/* *                                                                        * */
/* ************************************************************************** */

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

#ifndef _libmng_h_
#define _libmng_h_

/* ************************************************************************** */

#include "libmng_conf.h"               /* user-specific configuration options */

/* ************************************************************************** */

#define MNG_CHECK_BAD_ICCP             /* let's catch that sucker !!! */

#ifdef MNG_SUPPORT_READ                /* dependencies based on user-configuration */
#define MNG_INCLUDE_READ_PROCS
#endif

#ifdef MNG_SUPPORT_WRITE
#define MNG_INCLUDE_WRITE_PROCS
#endif

#ifdef MNG_SUPPORT_DISPLAY
#define MNG_INCLUDE_FILTERS
#define MNG_INCLUDE_INTERLACE
#define MNG_INCLUDE_OBJECTS
#define MNG_INCLUDE_DISPLAY_PROCS
#define MNG_INCLUDE_TIMING_PROCS
#define MNG_INCLUDE_ZLIB
#endif

#ifdef MNG_STORE_CHUNKS
#define MNG_INCLUDE_ZLIB
#endif

#ifdef MNG_SUPPORT_IJG6B
#define MNG_INCLUDE_JNG
#define MNG_INCLUDE_IJG6B
#define MNG_USE_SETJMP
#endif

#ifdef MNG_INCLUDE_JNG
#if defined(MNG_SUPPORT_DISPLAY) || defined(MNG_ACCESS_CHUNKS)
#define MNG_INCLUDE_JNG_READ
#endif
#if defined(MNG_SUPPORT_WRITE) || defined(MNG_ACCESS_CHUNKS)
#define MNG_INCLUDE_JNG_WRITE
#endif
#endif

#ifdef MNG_FULL_CMS
#define MNG_INCLUDE_LCMS
#endif

#ifdef MNG_AUTO_DITHER
#define MNG_INCLUDE_DITHERING
#endif

#ifdef MNG_SUPPORT_TRACE
#define MNG_INCLUDE_TRACE_PROCS
#ifdef MNG_TRACE_TELLTALE
#define MNG_INCLUDE_TRACE_STRINGS
#endif
#endif

#ifdef MNG_ERROR_TELLTALE
#define MNG_INCLUDE_ERROR_STRINGS
#endif

/* ************************************************************************** */

#include "libmng_types.h"              /* platform-specific definitions
                                          and other assorted stuff */

/* ************************************************************************** */

#ifdef __cplusplus
extern "C" {
#endif

/* ************************************************************************** */
/* *                                                                        * */
/* *  Versioning control                                                    * */
/* *                                                                        * */
/* *  version_so and version_dll will NOT reflect version_major;            * */
/* *  these will only change for binary incompatible changes (which will    * */
/* *  hopefully never occur)                                                * */
/* *  note: they will be set to 1 on the first public release !!!           * */
/* *                                                                        * */
/* *  first public release:                                                 * */
/* *  #define MNG_VERSION_TEXT    "1.0.0"                                   * */
/* *  #define MNG_VERSION_SO      1       eg. libmng.so.1                   * */
/* *  #define MNG_VERSION_DLL     1       eg. libmng.dll                    * */
/* *  #define MNG_VERSION_MAJOR   1                                         * */
/* *  #define MNG_VERSION_MINOR   0                                         * */
/* *  #define MNG_VERSION_RELEASE 0                                         * */
/* *                                                                        * */
/* *  bug fix & cosmetics :                                                 * */
/* *  #define MNG_VERSION_TEXT    "1.0.1"                                   * */
/* *  #define MNG_VERSION_SO      1       eg. libmng.so.1                   * */
/* *  #define MNG_VERSION_DLL     1       eg. libmng.dll                    * */
/* *  #define MNG_VERSION_MAJOR   1                                         * */
/* *  #define MNG_VERSION_MINOR   0                                         * */
/* *  #define MNG_VERSION_RELEASE 1                                         * */
/* *                                                                        * */
/* *  feature change :                                                      * */
/* *  #define MNG_VERSION_TEXT    "1.2.0"                                   * */
/* *  #define MNG_VERSION_SO      1       eg. libmng.so.1                   * */
/* *  #define MNG_VERSION_DLL     1       eg. libmng.dll                    * */
/* *  #define MNG_VERSION_MAJOR   1                                         * */
/* *  #define MNG_VERSION_MINOR   2                                         * */
/* *  #define MNG_VERSION_RELEASE 0                                         * */
/* *                                                                        * */
/* *  major rewrite (still binary compatible) :                             * */
/* *  #define MNG_VERSION_TEXT    "2.0.0"                                   * */
/* *  #define MNG_VERSION_SO      1       eg. libmng.so.1                   * */
/* *  #define MNG_VERSION_DLL     1       eg. libmng.dll                    * */
/* *  #define MNG_VERSION_MAJOR   2                                         * */
/* *  #define MNG_VERSION_MINOR   0                                         * */
/* *  #define MNG_VERSION_RELEASE 0                                         * */
/* *                                                                        * */
/* *  binary incompatible change:                                           * */
/* *  #define MNG_VERSION_TEXT    "13.0.0"                                  * */
/* *  #define MNG_VERSION_SO      2       eg. libmng.so.2                   * */
/* *  #define MNG_VERSION_DLL     2       eg. libmng2.dll                   * */
/* *  #define MNG_VERSION_MAJOR   13                                        * */
/* *  #define MNG_VERSION_MINOR   0                                         * */
/* *  #define MNG_VERSION_RELEASE 0                                         * */
/* *                                                                        * */
/* *  note that version_so & version_dll will always remain equal so it     * */
/* *  doesn't matter which one is called to do version-checking; they are   * */
/* *  just provided for their target platform                               * */
/* *                                                                        * */
/* ************************************************************************** */

#define MNG_VERSION_TEXT    "1.0.3"
#define MNG_VERSION_SO      1          /* eg. libmng.so.1  */
#define MNG_VERSION_DLL     1          /* but: libmng.dll (!) */
#define MNG_VERSION_MAJOR   1
#define MNG_VERSION_MINOR   0
#define MNG_VERSION_RELEASE 3

MNG_EXT mng_pchar MNG_DECL mng_version_text    (void);
MNG_EXT mng_uint8 MNG_DECL mng_version_so      (void);
MNG_EXT mng_uint8 MNG_DECL mng_version_dll     (void);
MNG_EXT mng_uint8 MNG_DECL mng_version_major   (void);
MNG_EXT mng_uint8 MNG_DECL mng_version_minor   (void);
MNG_EXT mng_uint8 MNG_DECL mng_version_release (void);

/* ************************************************************************** */
/* *                                                                        * */
/* *  MNG/PNG specification level conformance                               * */
/* *                                                                        * */
/* ************************************************************************** */

#define MNG_PNG_VERSION     "1.2"
#define MNG_PNG_VERSION_MAJ 1
#define MNG_PNG_VERSION_MIN 2

#define MNG_MNG_VERSION     "1.0"
#define MNG_MNG_VERSION_MAJ 1
#define MNG_MNG_VERSION_MIN 0
#define MNG_MNG_DRAFT       99         /* deprecated;
                                          only used for nEED "MNG DRAFT nn" */

/* ************************************************************************** */
/* *                                                                        * */
/* *  High-level application functions                                      * */
/* *                                                                        * */
/* ************************************************************************** */

/* library initialization function */
/* must be the first called before anything can be done at all */
/* initializes internal datastructure(s) */
MNG_EXT mng_handle  MNG_DECL mng_initialize      (mng_ptr       pUserdata,
                                                  mng_memalloc  fMemalloc,
                                                  mng_memfree   fMemfree,
                                                  mng_traceproc fTraceproc);

/* library reset function */
/* can be used to re-initialize the library, so another image can be
   processed. there's absolutely no harm in calling it, even when it's not
   really necessary */
MNG_EXT mng_retcode MNG_DECL mng_reset           (mng_handle    hHandle);

/* library cleanup function */
/* must be the last called to clean up internal datastructure(s) */
MNG_EXT mng_retcode MNG_DECL mng_cleanup         (mng_handle*   hHandle);

/* high-level read functions */
/* use mng_read if you simply want to read a Network Graphic */
/* mng_read_resume is used in I/O-read-suspension scenarios, where the
   "readdata" callback may return FALSE & length=0 indicating it's buffer is
   depleted or too short to supply the required bytes, and the buffer needs
   to be refilled; libmng will return the errorcode MNG_NEEDMOREDATA telling
   the app to refill it's read-buffer after which it must call mng_read_resume
   (or mng_display_resume if it also displaying the image simultaneously) */
#ifdef MNG_SUPPORT_READ
MNG_EXT mng_retcode MNG_DECL mng_read            (mng_handle    hHandle);
MNG_EXT mng_retcode MNG_DECL mng_read_resume     (mng_handle    hHandle);
#endif

/* high-level write & create functions */
/* use this if you want to write a previously read Network Graphic or
   if you want to create a new graphic and write it */
/* to write a previously read graphic you must have defined MNG_STORE_CHUNKS */
/* to create a new graphic you'll also need access to the chunks
   (eg. #define MNG_ACCESS_CHUNKS !) */
#ifdef MNG_SUPPORT_WRITE
MNG_EXT mng_retcode MNG_DECL mng_write           (mng_handle    hHandle);
MNG_EXT mng_retcode MNG_DECL mng_create          (mng_handle    hHandle);
#endif

/* high-level display functions */
/* use these to display a previously read or created graphic or
   to read & display a graphic simultaneously */
/* mng_display_resume should be called after a timer-interval
   expires that was set through the settimer-callback, after a
   read suspension-break, or, to resume an animation after a call
   to mng_display_freeze/mng_display_reset */
/* mng_display_freeze thru mng_display_gotime can be used to influence
   the display of an image, BUT ONLY if it has been completely read! */
#ifdef MNG_SUPPORT_DISPLAY
#ifdef MNG_SUPPORT_READ
MNG_EXT mng_retcode MNG_DECL mng_readdisplay     (mng_handle    hHandle);
#endif
MNG_EXT mng_retcode MNG_DECL mng_display         (mng_handle    hHandle);
MNG_EXT mng_retcode MNG_DECL mng_display_resume  (mng_handle    hHandle);
MNG_EXT mng_retcode MNG_DECL mng_display_freeze  (mng_handle    hHandle);
MNG_EXT mng_retcode MNG_DECL mng_display_reset   (mng_handle    hHandle);
MNG_EXT mng_retcode MNG_DECL mng_display_goframe (mng_handle    hHandle,
                                                  mng_uint32    iFramenr);
MNG_EXT mng_retcode MNG_DECL mng_display_golayer (mng_handle    hHandle,
                                                  mng_uint32    iLayernr);
MNG_EXT mng_retcode MNG_DECL mng_display_gotime  (mng_handle    hHandle,
                                                  mng_uint32    iPlaytime);
#endif /* MNG_SUPPORT_DISPLAY */

/* error reporting function */
/* use this if you need more detailed info on the last error */
/* iExtra1 & iExtra2 may contain errorcodes from zlib, jpeg, etc... */
/* zErrortext will only be filled if you #define MNG_ERROR_TELLTALE */
MNG_EXT mng_retcode MNG_DECL mng_getlasterror    (mng_handle    hHandle,
                                                  mng_int8*     iSeverity,
                                                  mng_chunkid*  iChunkname,
                                                  mng_uint32*   iChunkseq,
                                                  mng_int32*    iExtra1,
                                                  mng_int32*    iExtra2,
                                                  mng_pchar*    zErrortext);

/* ************************************************************************** */
/* *                                                                        * */
/* *  Callback set functions                                                * */
/* *                                                                        * */
/* ************************************************************************** */

/* memory callbacks */
/* called to allocate and release internal datastructures */
#ifndef MNG_INTERNAL_MEMMNGMT
MNG_EXT mng_retcode MNG_DECL mng_setcb_memalloc      (mng_handle        hHandle,
                                                      mng_memalloc      fProc);
MNG_EXT mng_retcode MNG_DECL mng_setcb_memfree       (mng_handle        hHandle,
                                                      mng_memfree       fProc);
#endif /* MNG_INTERNAL_MEMMNGMT */

/* open- & close-stream callbacks */
/* called to open & close streams for input or output */
#if defined(MNG_SUPPORT_READ) || defined(MNG_SUPPORT_WRITE)
MNG_EXT mng_retcode MNG_DECL mng_setcb_openstream    (mng_handle        hHandle,
                                                      mng_openstream    fProc);
MNG_EXT mng_retcode MNG_DECL mng_setcb_closestream   (mng_handle        hHandle,
                                                      mng_closestream   fProc);
#endif

/* read callback */
/* called to get data from the inputstream */
#ifdef MNG_SUPPORT_READ
MNG_EXT mng_retcode MNG_DECL mng_setcb_readdata      (mng_handle        hHandle,
                                                      mng_readdata      fProc);
#endif

/* write callback */
/* called to put data into the outputstream */
#ifdef MNG_SUPPORT_WRITE
MNG_EXT mng_retcode MNG_DECL mng_setcb_writedata     (mng_handle        hHandle,
                                                      mng_writedata     fProc);
#endif

/* error callback */
/* called when an error occurs */
/* the application can determine if the error is recoverable,
   and may inform the library by setting specific returncodes */
MNG_EXT mng_retcode MNG_DECL mng_setcb_errorproc     (mng_handle        hHandle,
                                                      mng_errorproc     fProc);

/* trace callback */
/* called to show the currently executing function */
#ifdef MNG_SUPPORT_TRACE
MNG_EXT mng_retcode MNG_DECL mng_setcb_traceproc     (mng_handle        hHandle,
                                                      mng_traceproc     fProc);
#endif

/* callbacks for read processing */
/* processheader is called when all header information has been gathered
   from the inputstream */
/* processtext is called for every tEXt, zTXt and iTXt chunk in the
   inputstream (iType=0 for tEXt, 1 for zTXt and 2 for iTXt);
   you can call get_imagelevel to check at what nesting-level the chunk is
   encountered (eg. tEXt inside an embedded image inside a MNG -> level == 2;
                in most other case -> level == 1) */
/* processsave & processseek are called for SAVE/SEEK chunks */
/* processneed is called for the nEED chunk; you should specify a callback
   for this as the default behavior will be to abort processing */
/* processunknown is called after reading each non-critical unknown chunk */
#ifdef MNG_SUPPORT_READ
MNG_EXT mng_retcode MNG_DECL mng_setcb_processheader (mng_handle        hHandle,
                                                      mng_processheader fProc);
MNG_EXT mng_retcode MNG_DECL mng_setcb_processtext   (mng_handle        hHandle,
                                                      mng_processtext   fProc);
MNG_EXT mng_retcode MNG_DECL mng_setcb_processsave   (mng_handle        hHandle,
                                                      mng_processsave   fProc);
MNG_EXT mng_retcode MNG_DECL mng_setcb_processseek   (mng_handle        hHandle,
                                                      mng_processseek   fProc);
MNG_EXT mng_retcode MNG_DECL mng_setcb_processneed   (mng_handle        hHandle,
                                                      mng_processneed   fProc);
MNG_EXT mng_retcode MNG_DECL mng_setcb_processmend   (mng_handle        hHandle,
                                                      mng_processmend   fProc);
MNG_EXT mng_retcode MNG_DECL mng_setcb_processunknown(mng_handle        hHandle,
                                                      mng_processunknown fProc);
MNG_EXT mng_retcode MNG_DECL mng_setcb_processterm   (mng_handle        hHandle,
                                                      mng_processterm   fProc);
#endif

/* callbacks for display processing */
/* getcanvasline is called to get an access-pointer to a line on the
   drawing-canvas */
/* getbkgdline is called to get an access-pointer to a line from the
   background-canvas */
/* refresh is called to inform the GUI to redraw the current canvas onto
   it's output device (eg. in Win32 this would mean sending an
   invalidate message for the specified region */
/* NOTE that the update-region is specified as x,y,width,height; eg. the
   invalidate message for Windows requires left,top,right,bottom parameters
   where the bottom-right is exclusive of the region!!
   to get these correctly is as simple as:
   left   = x;
   top    = y;
   right  = x + width;
   bottom = y + height;
   if your implementation requires inclusive points, simply subtract 1 from
   both the right & bottom values calculated above.
   */
#ifdef MNG_SUPPORT_DISPLAY
MNG_EXT mng_retcode MNG_DECL mng_setcb_getcanvasline (mng_handle        hHandle,
                                                      mng_getcanvasline fProc);
MNG_EXT mng_retcode MNG_DECL mng_setcb_getbkgdline   (mng_handle        hHandle,
                                                      mng_getbkgdline   fProc);
MNG_EXT mng_retcode MNG_DECL mng_setcb_getalphaline  (mng_handle        hHandle,
                                                      mng_getalphaline  fProc);
MNG_EXT mng_retcode MNG_DECL mng_setcb_refresh       (mng_handle        hHandle,
                                                      mng_refresh       fProc);

/* timing callbacks */
/* gettickcount is called to get the system tickcount (milliseconds);
   this is used to determine the remaining interval between frames */
/* settimer is called to inform the application that it should set a timer;
   when the timer is triggered the app must call mng_display_resume */
MNG_EXT mng_retcode MNG_DECL mng_setcb_gettickcount  (mng_handle        hHandle,
                                                      mng_gettickcount  fProc);
MNG_EXT mng_retcode MNG_DECL mng_setcb_settimer      (mng_handle        hHandle,
                                                      mng_settimer      fProc);

/* color management callbacks */
/* called to transmit color management information to the application */
/* these are only used when you #define MNG_APP_CMS */
#ifdef MNG_APP_CMS
MNG_EXT mng_retcode MNG_DECL mng_setcb_processgamma  (mng_handle        hHandle,
                                                      mng_processgamma  fProc);
MNG_EXT mng_retcode MNG_DECL mng_setcb_processchroma (mng_handle        hHandle,
                                                      mng_processchroma fProc);
MNG_EXT mng_retcode MNG_DECL mng_setcb_processsrgb   (mng_handle        hHandle,
                                                      mng_processsrgb   fProc);
MNG_EXT mng_retcode MNG_DECL mng_setcb_processiccp   (mng_handle        hHandle,
                                                      mng_processiccp   fProc);
MNG_EXT mng_retcode MNG_DECL mng_setcb_processarow   (mng_handle        hHandle,
                                                      mng_processarow   fProc);
#endif /* MNG_APP_CMS */
#endif /* MNG_SUPPORT_DISPLAY */

/* ************************************************************************** */
/* *                                                                        * */
/* *  Callback get functions                                                * */
/* *                                                                        * */
/* ************************************************************************** */

/* see _setcb_ */
#ifndef MNG_INTERNAL_MEMMNGMT
MNG_EXT mng_memalloc      MNG_DECL mng_getcb_memalloc      (mng_handle hHandle);
MNG_EXT mng_memfree       MNG_DECL mng_getcb_memfree       (mng_handle hHandle);
#endif

/* see _setcb_ */
#if defined(MNG_SUPPORT_READ) || defined(MNG_WRITE_SUPPORT)
MNG_EXT mng_openstream    MNG_DECL mng_getcb_openstream    (mng_handle hHandle);
MNG_EXT mng_closestream   MNG_DECL mng_getcb_closestream   (mng_handle hHandle);
#endif

/* see _setcb_ */
#ifdef MNG_SUPPORT_READ
MNG_EXT mng_readdata      MNG_DECL mng_getcb_readdata      (mng_handle hHandle);
#endif

/* see _setcb_ */
#ifdef MNG_SUPPORT_WRITE
MNG_EXT mng_writedata     MNG_DECL mng_getcb_writedata     (mng_handle hHandle);
#endif

/* see _setcb_ */
MNG_EXT mng_errorproc     MNG_DECL mng_getcb_errorproc     (mng_handle hHandle);

/* see _setcb_ */
#ifdef MNG_SUPPORT_TRACE
MNG_EXT mng_traceproc     MNG_DECL mng_getcb_traceproc     (mng_handle hHandle);
#endif

/* see _setcb_ */
#ifdef MNG_SUPPORT_READ
MNG_EXT mng_processheader MNG_DECL mng_getcb_processheader (mng_handle hHandle);
MNG_EXT mng_processtext   MNG_DECL mng_getcb_processtext   (mng_handle hHandle);
MNG_EXT mng_processsave   MNG_DECL mng_getcb_processsave   (mng_handle hHandle);
MNG_EXT mng_processseek   MNG_DECL mng_getcb_processseek   (mng_handle hHandle);
MNG_EXT mng_processneed   MNG_DECL mng_getcb_processneed   (mng_handle hHandle);
MNG_EXT mng_processunknown MNG_DECL mng_getcb_processunknown (mng_handle hHandle);
MNG_EXT mng_processterm   MNG_DECL mng_getcb_processterm   (mng_handle hHandle);
#endif

/* see _setcb_ */
#ifdef MNG_SUPPORT_DISPLAY
MNG_EXT mng_getcanvasline MNG_DECL mng_getcb_getcanvasline (mng_handle hHandle);
MNG_EXT mng_getbkgdline   MNG_DECL mng_getcb_getbkgdline   (mng_handle hHandle);
MNG_EXT mng_getalphaline  MNG_DECL mng_getcb_getalphaline  (mng_handle hHandle);
MNG_EXT mng_refresh       MNG_DECL mng_getcb_refresh       (mng_handle hHandle);

/* see _setcb_ */
MNG_EXT mng_gettickcount  MNG_DECL mng_getcb_gettickcount  (mng_handle hHandle);
MNG_EXT mng_settimer      MNG_DECL mng_getcb_settimer      (mng_handle hHandle);

/* see _setcb_ */
#ifdef MNG_APP_CMS
MNG_EXT mng_processgamma  MNG_DECL mng_getcb_processgamma  (mng_handle hHandle);
MNG_EXT mng_processchroma MNG_DECL mng_getcb_processchroma (mng_handle hHandle);
MNG_EXT mng_processsrgb   MNG_DECL mng_getcb_processsrgb   (mng_handle hHandle);
MNG_EXT mng_processiccp   MNG_DECL mng_getcb_processiccp   (mng_handle hHandle);
MNG_EXT mng_processarow   MNG_DECL mng_getcb_processarow   (mng_handle hHandle);
#endif /* MNG_APP_CMS */
#endif /* MNG_SUPPORT_DISPLAY */

/* ************************************************************************** */
/* *                                                                        * */
/* *  Property set functions                                                * */
/* *                                                                        * */
/* ************************************************************************** */

/* Application data pointer */
/* provided for application use; not used by the library */
MNG_EXT mng_retcode MNG_DECL mng_set_userdata        (mng_handle        hHandle,
                                                      mng_ptr           pUserdata);

/* The style of the drawing- & background-canvas */
/* only used for displaying images */
/* both are initially set to 24-bit RGB (eg. 8-bit per channel) */
MNG_EXT mng_retcode MNG_DECL mng_set_canvasstyle     (mng_handle        hHandle,
                                                      mng_uint32        iStyle);
MNG_EXT mng_retcode MNG_DECL mng_set_bkgdstyle       (mng_handle        hHandle,
                                                      mng_uint32        iStyle);

/* The default background color */
/* only used if the getbkgdline callback is not defined */
/* for initially painting the canvas and restoring (part of) the background */
MNG_EXT mng_retcode MNG_DECL mng_set_bgcolor         (mng_handle        hHandle,
                                                      mng_uint16        iRed,
                                                      mng_uint16        iGreen,
                                                      mng_uint16        iBlue);

/* Indicates preferred use of the bKGD chunk for PNG images */
MNG_EXT mng_retcode MNG_DECL mng_set_usebkgd         (mng_handle        hHandle,
                                                      mng_bool          bUseBKGD);

/* Indicates storage of read chunks */
/* only useful if you #define mng_store_chunks */
/* can be used to dynamically change storage management */
MNG_EXT mng_retcode MNG_DECL mng_set_storechunks     (mng_handle        hHandle,
                                                      mng_bool          bStorechunks);

/* Indicates breaks requested when processing SAVE/SEEK */
/* set this to let the app handle section breaks; the library will return
   MNG_NEEDSECTIONWAIT return-codes for each SEEK chunk */
MNG_EXT mng_retcode MNG_DECL mng_set_sectionbreaks   (mng_handle        hHandle,
                                                      mng_bool          bSectionbreaks);

/* Indicates storage of playback info (ON by default!) */
/* can be used to turn off caching of playback info; this is useful to
   specifically optimize MNG-video playback; note that if caching is turned off
   LOOP chunks will be flagged as errors! TERM chunks will be ignored and only
   passed to the processterm() callback if it is defined by the app; also, this
   feature can only be used with mng_readdisplay(); mng_read(),
   mng_display_reset() and mng_display_goxxxx() will return an error;
   once this option is turned off it can't be turned on for the same stream!!! */
MNG_EXT mng_retcode MNG_DECL mng_set_cacheplayback   (mng_handle        hHandle,
                                                      mng_bool          bCacheplayback);

/* Indicates automatic progressive refreshes for large images (ON by default!) */
/* turn this off if you do not want intermittent painting while a large image
   is being read. useful if the input-stream comes from a fast medium, such
   as a local harddisk */
MNG_EXT mng_retcode MNG_DECL mng_set_doprogressive   (mng_handle        hHandle,
                                                      mng_bool          bDoProgressive);

/* Color-management necessaries */
/*
    *************************************************************************
                  !!!!!!!! THIS BIT IS IMPORTANT !!!!!!!!!
    *************************************************************************

    If you have defined MNG_FULL_CMS (and are using lcms), you will have to
    think hard about the following routines.

    lcms requires 2 profiles to work off the differences in the input-image
    and the output-device. The ICC profile for the input-image will be
    embedded within it to reflect its color-characteristics, but the output
    profile depends on the output-device, which is something only *YOU* know
    about. sRGB (standard RGB) is common for x86 compatible environments
    (eg. Windows, Linux and some others)

    If you are compiling for a sRGB compliant system you probably won't have
    to do anything special. (unless you want to ofcourse)

    If you are compiling for a non-sRGB compliant system
    (eg. SGI, Mac, Next, others...)
    you *MUST* define a proper ICC profile for the generic output-device
    associated with that platform.

    In either event, you may also want to offer an option to your users to
    set the profile manually, or, if you know how, set it from a
    system-defined default.

    TO RECAP: for sRGB systems (Windows, Linux) no action required!
              for non-sRGB systems (SGI, Mac, Next) ACTION REQUIRED!

    Please visit http://www.srgb.com, http://www.color.org and
    http://www.littlecms.com for more info.

    *************************************************************************
                  !!!!!!!! THIS BIT IS IMPORTANT !!!!!!!!!
    *************************************************************************
*/
/* mng_set_srgb tells libmng if it's running on a sRGB compliant system or not
   the default is already set to MNG_TRUE */
/* mng_set_outputprofile, mng_set_outputprofile2, mng_set_outputsrgb
   are used to set the default profile describing the output-device
   by default it is already initialized with an sRGB profile */
/* mng_set_srgbprofile, mng_set_srgbprofile2, mng_set_srgbimplicit
   are used to set the default profile describing a standard sRGB device
   this is used when the input-image is tagged only as being sRGB, but the
   output-device is defined as not being sRGB compliant
   by default it is already initialized with a standard sRGB profile */
#if defined(MNG_SUPPORT_DISPLAY)
MNG_EXT mng_retcode MNG_DECL mng_set_srgb            (mng_handle        hHandle,
                                                      mng_bool          bIssRGB);
MNG_EXT mng_retcode MNG_DECL mng_set_outputprofile   (mng_handle        hHandle,
                                                      mng_pchar         zFilename);
MNG_EXT mng_retcode MNG_DECL mng_set_outputprofile2  (mng_handle        hHandle,
                                                      mng_uint32        iProfilesize,
                                                      mng_ptr           pProfile);
MNG_EXT mng_retcode MNG_DECL mng_set_outputsrgb      (mng_handle        hHandle);
MNG_EXT mng_retcode MNG_DECL mng_set_srgbprofile     (mng_handle        hHandle,
                                                      mng_pchar         zFilename);
MNG_EXT mng_retcode MNG_DECL mng_set_srgbprofile2    (mng_handle        hHandle,
                                                      mng_uint32        iProfilesize,
                                                      mng_ptr           pProfile);
MNG_EXT mng_retcode MNG_DECL mng_set_srgbimplicit    (mng_handle        hHandle);
#endif

/* Gamma settings */
/* only used if you #define MNG_FULL_CMS or #define MNG_GAMMA_ONLY */
/* ... blabla (explain gamma processing a little; eg. formula & stuff) ... */
MNG_EXT mng_retcode MNG_DECL mng_set_viewgamma       (mng_handle        hHandle,
                                                      mng_float         dGamma);
MNG_EXT mng_retcode MNG_DECL mng_set_displaygamma    (mng_handle        hHandle,
                                                      mng_float         dGamma);
MNG_EXT mng_retcode MNG_DECL mng_set_dfltimggamma    (mng_handle        hHandle,
                                                      mng_float         dGamma);
MNG_EXT mng_retcode MNG_DECL mng_set_viewgammaint    (mng_handle        hHandle,
                                                      mng_uint32        iGamma);
MNG_EXT mng_retcode MNG_DECL mng_set_displaygammaint (mng_handle        hHandle,
                                                      mng_uint32        iGamma);
MNG_EXT mng_retcode MNG_DECL mng_set_dfltimggammaint (mng_handle        hHandle,
                                                      mng_uint32        iGamma);

/* Ultimate clipping size */
/* used to limit extreme graphics from overloading the system */
/* if a graphic exceeds these limits a warning is issued, which can
   be ignored by the app (using the errorproc callback). in that case
   the library will use these settings to clip the input graphic, and
   the app's canvas must account for this */
MNG_EXT mng_retcode MNG_DECL mng_set_maxcanvaswidth  (mng_handle        hHandle,
                                                      mng_uint32        iMaxwidth);
MNG_EXT mng_retcode MNG_DECL mng_set_maxcanvasheight (mng_handle        hHandle,
                                                      mng_uint32        iMaxheight);
MNG_EXT mng_retcode MNG_DECL mng_set_maxcanvassize   (mng_handle        hHandle,
                                                      mng_uint32        iMaxwidth,
                                                      mng_uint32        iMaxheight);

/* ZLIB default compression parameters */
/* these are used when writing out chunks */
/* they are also used when compressing PNG image-data or JNG alpha-data;
   in this case you can set them just before calling mng_putimgdata_ihdr */
/* set to your liking; usually the defaults will suffice though! */
/* check the documentation for ZLIB for details on these parameters */
#ifdef MNG_INCLUDE_ZLIB
MNG_EXT mng_retcode MNG_DECL mng_set_zlib_level      (mng_handle        hHandle,
                                                      mng_int32         iZlevel);
MNG_EXT mng_retcode MNG_DECL mng_set_zlib_method     (mng_handle        hHandle,
                                                      mng_int32         iZmethod);
MNG_EXT mng_retcode MNG_DECL mng_set_zlib_windowbits (mng_handle        hHandle,
                                                      mng_int32         iZwindowbits);
MNG_EXT mng_retcode MNG_DECL mng_set_zlib_memlevel   (mng_handle        hHandle,
                                                      mng_int32         iZmemlevel);
MNG_EXT mng_retcode MNG_DECL mng_set_zlib_strategy   (mng_handle        hHandle,
                                                      mng_int32         iZstrategy);

MNG_EXT mng_retcode MNG_DECL mng_set_zlib_maxidat    (mng_handle        hHandle,
                                                      mng_uint32        iMaxIDAT);
#endif /* MNG_INCLUDE_ZLIB */

/* JNG default compression parameters (based on IJG code) */
/* these are used when compressing JNG image-data; so you can set them
   just before calling mng_putimgdata_jhdr */
/* set to your liking; usually the defaults will suffice though! */
/* check the documentation for IJGSRC6B for details on these parameters */
#ifdef MNG_INCLUDE_JNG
#ifdef MNG_INCLUDE_IJG6B
MNG_EXT mng_retcode MNG_DECL mng_set_jpeg_dctmethod  (mng_handle        hHandle,
                                                      mngjpeg_dctmethod eJPEGdctmethod);
#endif
MNG_EXT mng_retcode MNG_DECL mng_set_jpeg_quality    (mng_handle        hHandle,
                                                      mng_int32         iJPEGquality);
MNG_EXT mng_retcode MNG_DECL mng_set_jpeg_smoothing  (mng_handle        hHandle,
                                                      mng_int32         iJPEGsmoothing);
MNG_EXT mng_retcode MNG_DECL mng_set_jpeg_progressive(mng_handle        hHandle,
                                                      mng_bool          bJPEGprogressive);
MNG_EXT mng_retcode MNG_DECL mng_set_jpeg_optimized  (mng_handle        hHandle,
                                                      mng_bool          bJPEGoptimized);

MNG_EXT mng_retcode MNG_DECL mng_set_jpeg_maxjdat    (mng_handle        hHandle,
                                                      mng_uint32        iMaxJDAT);
#endif /* MNG_INCLUDE_JNG */

/* Suspension-mode setting */
/* use this to activate the internal suspension-buffer to improve
   read-suspension processing */
/* TODO: write-suspension ??? */   
#if defined(MNG_SUPPORT_READ)
MNG_EXT mng_retcode MNG_DECL mng_set_suspensionmode  (mng_handle        hHandle,
                                                      mng_bool          bSuspensionmode);
#endif

/* Speed setting */
/* use this to influence the display-speed of animations */
#if defined(MNG_SUPPORT_DISPLAY)
MNG_EXT mng_retcode MNG_DECL mng_set_speed           (mng_handle        hHandle,
                                                      mng_speedtype     iSpeed);
#endif

/* ************************************************************************** */
/* *                                                                        * */
/* *  Property get functions                                                * */
/* *                                                                        * */
/* ************************************************************************** */

/* see _set_ */
MNG_EXT mng_ptr     MNG_DECL mng_get_userdata        (mng_handle        hHandle);

/* Network Graphic header details */
/* these get filled once the graphics header is processed,
   so they are available in the processheader callback; before that
   they are zeroed out and imagetype is set to it_unknown */
/* this might be a good point for the app to initialize the drawing-canvas! */
/* note that some fields are only set for the first(!) header-chunk:
   MNG/MHDR (imagetype = mng_it_mng) - ticks thru simplicity
   PNG/IHDR (imagetype = mng_it_png) - bitdepth thru interlace
   JNG/JHDR (imagetype = mng_it_jng) - bitdepth thru compression &
                                       interlace thru alphainterlace */
MNG_EXT mng_imgtype MNG_DECL mng_get_sigtype         (mng_handle        hHandle);
MNG_EXT mng_imgtype MNG_DECL mng_get_imagetype       (mng_handle        hHandle);
MNG_EXT mng_uint32  MNG_DECL mng_get_imagewidth      (mng_handle        hHandle);
MNG_EXT mng_uint32  MNG_DECL mng_get_imageheight     (mng_handle        hHandle);

MNG_EXT mng_uint32  MNG_DECL mng_get_ticks           (mng_handle        hHandle);
MNG_EXT mng_uint32  MNG_DECL mng_get_framecount      (mng_handle        hHandle);
MNG_EXT mng_uint32  MNG_DECL mng_get_layercount      (mng_handle        hHandle);
MNG_EXT mng_uint32  MNG_DECL mng_get_playtime        (mng_handle        hHandle);
MNG_EXT mng_uint32  MNG_DECL mng_get_simplicity      (mng_handle        hHandle);

MNG_EXT mng_uint8   MNG_DECL mng_get_bitdepth        (mng_handle        hHandle);
MNG_EXT mng_uint8   MNG_DECL mng_get_colortype       (mng_handle        hHandle);
MNG_EXT mng_uint8   MNG_DECL mng_get_compression     (mng_handle        hHandle);
MNG_EXT mng_uint8   MNG_DECL mng_get_filter          (mng_handle        hHandle);
MNG_EXT mng_uint8   MNG_DECL mng_get_interlace       (mng_handle        hHandle);
MNG_EXT mng_uint8   MNG_DECL mng_get_alphabitdepth   (mng_handle        hHandle);
MNG_EXT mng_uint8   MNG_DECL mng_get_alphacompression(mng_handle        hHandle);
MNG_EXT mng_uint8   MNG_DECL mng_get_alphafilter     (mng_handle        hHandle);
MNG_EXT mng_uint8   MNG_DECL mng_get_alphainterlace  (mng_handle        hHandle);

/* indicates the predicted alpha-depth required to properly display the image */
/* gets set once the graphics header is processed and is available in the
   processheader callback for any type of input-image (PNG, JNG or MNG) */
/* possible values are 0,1,2,4,8,16
   0  = no transparency required
   1  = on/off transparency required (alpha-values are 0 or 2^bit_depth-1)
   2+ = semi-transparency required (values will be scaled to the bitdepth of the
                                    canvasstyle supplied by the application) */
MNG_EXT mng_uint8   MNG_DECL mng_get_alphadepth      (mng_handle        hHandle);

/* defines whether a refresh() callback is called for an interlace pass (PNG)
   or progressive scan (JNG) */
/* returns the interlace pass number for PNG or a fabricated pass number for JNG;
   returns 0 in all other cases */
/* only useful if the image_type = mng_it_png or mng_it_jng and if the image
   is actually interlaced (PNG) or progressive (JNG) */
MNG_EXT mng_uint8   MNG_DECL mng_get_refreshpass     (mng_handle        hHandle);

/* see _set_ */
MNG_EXT mng_uint32  MNG_DECL mng_get_canvasstyle     (mng_handle        hHandle);
MNG_EXT mng_uint32  MNG_DECL mng_get_bkgdstyle       (mng_handle        hHandle);

/* see _set_ */
MNG_EXT mng_retcode MNG_DECL mng_get_bgcolor         (mng_handle        hHandle,
                                                      mng_uint16*       iRed,
                                                      mng_uint16*       iGreen,
                                                      mng_uint16*       iBlue);

/* see _set_ */
MNG_EXT mng_bool    MNG_DECL mng_get_usebkgd         (mng_handle        hHandle);

/* see _set_ */
MNG_EXT mng_bool    MNG_DECL mng_get_storechunks     (mng_handle        hHandle);

/* see _set_ */
MNG_EXT mng_bool    MNG_DECL mng_get_sectionbreaks   (mng_handle        hHandle);

/* see _set_ */
MNG_EXT mng_bool    MNG_DECL mng_get_cacheplayback   (mng_handle        hHandle);

/* see _set_ */
MNG_EXT mng_bool    MNG_DECL mng_get_doprogressive   (mng_handle        hHandle);

/* see _set_ */
#if defined(MNG_SUPPORT_DISPLAY) && defined(MNG_FULL_CMS)
MNG_EXT mng_bool    MNG_DECL mng_get_srgb            (mng_handle        hHandle);
#endif

/* see _set_ */
MNG_EXT mng_float   MNG_DECL mng_get_viewgamma       (mng_handle        hHandle);
MNG_EXT mng_float   MNG_DECL mng_get_displaygamma    (mng_handle        hHandle);
MNG_EXT mng_float   MNG_DECL mng_get_dfltimggamma    (mng_handle        hHandle);
MNG_EXT mng_uint32  MNG_DECL mng_get_viewgammaint    (mng_handle        hHandle);
MNG_EXT mng_uint32  MNG_DECL mng_get_displaygammaint (mng_handle        hHandle);
MNG_EXT mng_uint32  MNG_DECL mng_get_dfltimggammaint (mng_handle        hHandle);

/* see _set_ */
MNG_EXT mng_uint32  MNG_DECL mng_get_maxcanvaswidth  (mng_handle        hHandle);
MNG_EXT mng_uint32  MNG_DECL mng_get_maxcanvasheight (mng_handle        hHandle);

/* see _set_ */
#ifdef MNG_INCLUDE_ZLIB
MNG_EXT mng_int32   MNG_DECL mng_get_zlib_level      (mng_handle        hHandle);
MNG_EXT mng_int32   MNG_DECL mng_get_zlib_method     (mng_handle        hHandle);
MNG_EXT mng_int32   MNG_DECL mng_get_zlib_windowbits (mng_handle        hHandle);
MNG_EXT mng_int32   MNG_DECL mng_get_zlib_memlevel   (mng_handle        hHandle);
MNG_EXT mng_int32   MNG_DECL mng_get_zlib_strategy   (mng_handle        hHandle);

MNG_EXT mng_uint32  MNG_DECL mng_get_zlib_maxidat    (mng_handle        hHandle);
#endif /* MNG_INCLUDE_ZLIB */

/* see _set_ */
#ifdef MNG_INCLUDE_JNG
#ifdef MNG_INCLUDE_IJG6B
MNG_EXT mngjpeg_dctmethod
                    MNG_DECL mng_get_jpeg_dctmethod  (mng_handle        hHandle);
#endif
MNG_EXT mng_int32   MNG_DECL mng_get_jpeg_quality    (mng_handle        hHandle);
MNG_EXT mng_int32   MNG_DECL mng_get_jpeg_smoothing  (mng_handle        hHandle);
MNG_EXT mng_bool    MNG_DECL mng_get_jpeg_progressive(mng_handle        hHandle);
MNG_EXT mng_bool    MNG_DECL mng_get_jpeg_optimized  (mng_handle        hHandle);

MNG_EXT mng_uint32  MNG_DECL mng_get_jpeg_maxjdat    (mng_handle        hHandle);
#endif /* MNG_INCLUDE_JNG */

/* see _set_  */
#if defined(MNG_SUPPORT_READ)
MNG_EXT mng_bool    MNG_DECL mng_get_suspensionmode  (mng_handle        hHandle);
#endif

/* see _set_  */
#if defined(MNG_SUPPORT_DISPLAY)
MNG_EXT mng_speedtype
                    MNG_DECL mng_get_speed           (mng_handle        hHandle);
#endif

/* Image-level */
/* this can be used inside the processtext callback to determine the level of
   text of the image being processed; the value 1 is returned for top-level
   texts, and the value 2 for a text inside an embedded image inside a MNG */
MNG_EXT mng_uint32  MNG_DECL mng_get_imagelevel      (mng_handle        hHandle);

/* BACK info */
/* can be used to retrieve the color & mandatory values for the last processed
   BACK chunk of a MNG (will fail for other image-types);
   if no BACK chunk was processed yet, it will return all zeroes */
MNG_EXT mng_retcode MNG_DECL mng_get_lastbackchunk   (mng_handle        hHandle,
                                                      mng_uint16*       iRed,
                                                      mng_uint16*       iGreen,
                                                      mng_uint16*       iBlue,
                                                      mng_uint8*        iMandatory);

/* Display status variables */
/* these get filled & updated during display processing */
/* starttime is the tickcount at the start of displaying the animation */
/* runtime is the actual number of millisecs since the start of the animation */
/* currentframe, currentlayer & currentplaytime indicate the current
   frame/layer/playtime(msecs) in the animation (these keep increasing;
   even after the animation loops back to the TERM chunk) */
#if defined(MNG_SUPPORT_DISPLAY)
MNG_EXT mng_uint32  MNG_DECL mng_get_starttime       (mng_handle        hHandle);
MNG_EXT mng_uint32  MNG_DECL mng_get_runtime         (mng_handle        hHandle);
MNG_EXT mng_uint32  MNG_DECL mng_get_currentframe    (mng_handle        hHandle);
MNG_EXT mng_uint32  MNG_DECL mng_get_currentlayer    (mng_handle        hHandle);
MNG_EXT mng_uint32  MNG_DECL mng_get_currentplaytime (mng_handle        hHandle);
#endif

/* Status variables */
/* these indicate the internal state of the library */
/* most indicate exactly what you would expect:
   status_error returns MNG_TRUE if the last function call returned an errorcode
   status_reading returns MNG_TRUE if the library is (still) reading an image
   status_suspendbreak returns MNG_TRUE if the library has suspended for "I/O"
   status_creating returns MNG_TRUE if the library is in the middle of creating an image
   status_writing returns MNG_TRUE if the library is in the middle of writing an image
   status_displaying returns MNG_TRUE if the library is displaying an image
   status_running returns MNG_TRUE if display processing is active (eg. not frozen or reset)
   status_timerbreak returns MNG_TRUE if the library has suspended for a "timer-break" */
/* eg. mng_readdisplay() will turn the reading, displaying and running status on;
   when EOF is reached the reading status will be turned off */   
MNG_EXT mng_bool    MNG_DECL mng_status_error        (mng_handle        hHandle);
#ifdef MNG_SUPPORT_READ
MNG_EXT mng_bool    MNG_DECL mng_status_reading      (mng_handle        hHandle);
MNG_EXT mng_bool    MNG_DECL mng_status_suspendbreak (mng_handle        hHandle);
#endif
#ifdef MNG_SUPPORT_WRITE
MNG_EXT mng_bool    MNG_DECL mng_status_creating     (mng_handle        hHandle);
MNG_EXT mng_bool    MNG_DECL mng_status_writing      (mng_handle        hHandle);
#endif
#ifdef MNG_SUPPORT_DISPLAY
MNG_EXT mng_bool    MNG_DECL mng_status_displaying   (mng_handle        hHandle);
MNG_EXT mng_bool    MNG_DECL mng_status_running      (mng_handle        hHandle);
MNG_EXT mng_bool    MNG_DECL mng_status_timerbreak   (mng_handle        hHandle);
#endif

/* ************************************************************************** */
/* *                                                                        * */
/* *  Chunk access functions                                                * */
/* *                                                                        * */
/* ************************************************************************** */

#ifdef MNG_ACCESS_CHUNKS

/* ************************************************************************** */

/* use this to iterate the stored chunks */
/* requires MNG_ACCESS_CHUNKS & MNG_STORE_CHUNKS */
/* starts from the supplied chunk-index-nr; the first chunk has index 0!! */
MNG_EXT mng_retcode MNG_DECL mng_iterate_chunks      (mng_handle       hHandle,
                                                      mng_uint32       iChunkseq,
                                                      mng_iteratechunk fProc);

/* ************************************************************************** */

/* use these to get chunk data from within the callback in iterate_chunks */
MNG_EXT mng_retcode MNG_DECL mng_getchunk_ihdr       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint32       *iWidth,
                                                      mng_uint32       *iHeight,
                                                      mng_uint8        *iBitdepth,
                                                      mng_uint8        *iColortype,
                                                      mng_uint8        *iCompression,
                                                      mng_uint8        *iFilter,
                                                      mng_uint8        *iInterlace);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_plte       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint32       *iCount,
                                                      mng_palette8     *aPalette);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_idat       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint32       *iRawlen,
                                                      mng_ptr          *pRawdata);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_trns       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_bool         *bEmpty,
                                                      mng_bool         *bGlobal,
                                                      mng_uint8        *iType,
                                                      mng_uint32       *iCount,
                                                      mng_uint8arr     *aAlphas,
                                                      mng_uint16       *iGray,
                                                      mng_uint16       *iRed,
                                                      mng_uint16       *iGreen,
                                                      mng_uint16       *iBlue,
                                                      mng_uint32       *iRawlen,
                                                      mng_uint8arr     *aRawdata);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_gama       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_bool         *bEmpty,
                                                      mng_uint32       *iGamma);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_chrm       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_bool         *bEmpty,
                                                      mng_uint32       *iWhitepointx,
                                                      mng_uint32       *iWhitepointy,
                                                      mng_uint32       *iRedx,
                                                      mng_uint32       *iRedy,
                                                      mng_uint32       *iGreenx,
                                                      mng_uint32       *iGreeny,
                                                      mng_uint32       *iBluex,
                                                      mng_uint32       *iBluey);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_srgb       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_bool         *bEmpty,
                                                      mng_uint8        *iRenderingintent);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_iccp       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_bool         *bEmpty,
                                                      mng_uint32       *iNamesize,
                                                      mng_pchar        *zName,
                                                      mng_uint8        *iCompression,
                                                      mng_uint32       *iProfilesize,
                                                      mng_ptr          *pProfile);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_text       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint32       *iKeywordsize,
                                                      mng_pchar        *zKeyword,
                                                      mng_uint32       *iTextsize,
                                                      mng_pchar        *zText);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_ztxt       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint32       *iKeywordsize,
                                                      mng_pchar        *zKeyword,
                                                      mng_uint8        *iCompression,
                                                      mng_uint32       *iTextsize,
                                                      mng_pchar        *zText);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_itxt       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint32       *iKeywordsize,
                                                      mng_pchar        *zKeyword,
                                                      mng_uint8        *iCompressionflag,
                                                      mng_uint8        *iCompressionmethod,
                                                      mng_uint32       *iLanguagesize,
                                                      mng_pchar        *zLanguage,
                                                      mng_uint32       *iTranslationsize,
                                                      mng_pchar        *zTranslation,
                                                      mng_uint32       *iTextsize,
                                                      mng_pchar        *zText);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_bkgd       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_bool         *bEmpty,
                                                      mng_uint8        *iType,
                                                      mng_uint8        *iIndex,
                                                      mng_uint16       *iGray,
                                                      mng_uint16       *iRed,
                                                      mng_uint16       *iGreen,
                                                      mng_uint16       *iBlue);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_phys       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_bool         *bEmpty,
                                                      mng_uint32       *iSizex,
                                                      mng_uint32       *iSizey,
                                                      mng_uint8        *iUnit);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_sbit       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_bool         *bEmpty,
                                                      mng_uint8        *iType,
                                                      mng_uint8arr4    *aBits);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_splt       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_bool         *bEmpty,
                                                      mng_uint32       *iNamesize,
                                                      mng_pchar        *zName,
                                                      mng_uint8        *iSampledepth,
                                                      mng_uint32       *iEntrycount,
                                                      mng_ptr          *pEntries);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_hist       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint32       *iEntrycount,
                                                      mng_uint16arr    *aEntries);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_time       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint16       *iYear,
                                                      mng_uint8        *iMonth,
                                                      mng_uint8        *iDay,
                                                      mng_uint8        *iHour,
                                                      mng_uint8        *iMinute,
                                                      mng_uint8        *iSecond);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_mhdr       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint32       *iWidth,
                                                      mng_uint32       *iHeight,
                                                      mng_uint32       *iTicks,
                                                      mng_uint32       *iLayercount,
                                                      mng_uint32       *iFramecount,
                                                      mng_uint32       *iPlaytime,
                                                      mng_uint32       *iSimplicity);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_loop       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint8        *iLevel,
                                                      mng_uint32       *iRepeat,
                                                      mng_uint8        *iTermination,
                                                      mng_uint32       *iItermin,
                                                      mng_uint32       *iItermax,
                                                      mng_uint32       *iCount,
                                                      mng_uint32p      *pSignals);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_endl       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint8        *iLevel);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_defi       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint16       *iObjectid,
                                                      mng_uint8        *iDonotshow,
                                                      mng_uint8        *iConcrete,
                                                      mng_bool         *bHasloca,
                                                      mng_int32        *iXlocation,
                                                      mng_int32        *iYlocation,
                                                      mng_bool         *bHasclip,
                                                      mng_int32        *iLeftcb,
                                                      mng_int32        *iRightcb,
                                                      mng_int32        *iTopcb,
                                                      mng_int32        *iBottomcb);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_basi       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint32       *iWidth,
                                                      mng_uint32       *iHeight,
                                                      mng_uint8        *iBitdepth,
                                                      mng_uint8        *iColortype,
                                                      mng_uint8        *iCompression,
                                                      mng_uint8        *iFilter,
                                                      mng_uint8        *iInterlace,
                                                      mng_uint16       *iRed,
                                                      mng_uint16       *iGreen,
                                                      mng_uint16       *iBlue,
                                                      mng_uint16       *iAlpha,
                                                      mng_uint8        *iViewable);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_clon       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint16       *iSourceid,
                                                      mng_uint16       *iCloneid,
                                                      mng_uint8        *iClonetype,
                                                      mng_uint8        *iDonotshow,
                                                      mng_uint8        *iConcrete,
                                                      mng_bool         *bHasloca,
                                                      mng_uint8        *iLocationtype,
                                                      mng_int32        *iLocationx,
                                                      mng_int32        *iLocationy);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_past       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint16       *iDestid,
                                                      mng_uint8        *iTargettype,
                                                      mng_int32        *iTargetx,
                                                      mng_int32        *iTargety,
                                                      mng_uint32       *iCount);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_past_src   (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint32       iEntry,
                                                      mng_uint16       *iSourceid,
                                                      mng_uint8        *iComposition,
                                                      mng_uint8        *iOrientation,
                                                      mng_uint8        *iOffsettype,
                                                      mng_int32        *iOffsetx,
                                                      mng_int32        *iOffsety,
                                                      mng_uint8        *iBoundarytype,
                                                      mng_int32        *iBoundaryl,
                                                      mng_int32        *iBoundaryr,
                                                      mng_int32        *iBoundaryt,
                                                      mng_int32        *iBoundaryb);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_disc       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint32       *iCount,
                                                      mng_uint16p      *pObjectids);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_back       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint16       *iRed,
                                                      mng_uint16       *iGreen,
                                                      mng_uint16       *iBlue,
                                                      mng_uint8        *iMandatory,
                                                      mng_uint16       *iImageid,
                                                      mng_uint8        *iTile);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_fram       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_bool         *bEmpty,
                                                      mng_uint8        *iMode,
                                                      mng_uint32       *iNamesize,
                                                      mng_pchar        *zName,
                                                      mng_uint8        *iChangedelay,
                                                      mng_uint8        *iChangetimeout,
                                                      mng_uint8        *iChangeclipping,
                                                      mng_uint8        *iChangesyncid,
                                                      mng_uint32       *iDelay,
                                                      mng_uint32       *iTimeout,
                                                      mng_uint8        *iBoundarytype,
                                                      mng_int32        *iBoundaryl,
                                                      mng_int32        *iBoundaryr,
                                                      mng_int32        *iBoundaryt,
                                                      mng_int32        *iBoundaryb,
                                                      mng_uint32       *iCount,
                                                      mng_uint32p      *pSyncids);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_move       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint16       *iFirstid,
                                                      mng_uint16       *iLastid,
                                                      mng_uint8        *iMovetype,
                                                      mng_int32        *iMovex,
                                                      mng_int32        *iMovey);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_clip       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint16       *iFirstid,
                                                      mng_uint16       *iLastid,
                                                      mng_uint8        *iCliptype,
                                                      mng_int32        *iClipl,
                                                      mng_int32        *iClipr,
                                                      mng_int32        *iClipt,
                                                      mng_int32        *iClipb);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_show       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_bool         *bEmpty,
                                                      mng_uint16       *iFirstid,
                                                      mng_uint16       *iLastid,
                                                      mng_uint8        *iMode);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_term       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint8        *iTermaction,
                                                      mng_uint8        *iIteraction,
                                                      mng_uint32       *iDelay,
                                                      mng_uint32       *iItermax);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_save       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_bool         *bEmpty,
                                                      mng_uint8        *iOffsettype,
                                                      mng_uint32       *iCount);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_save_entry (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint32       iEntry,
                                                      mng_uint8        *iEntrytype,
                                                      mng_uint32arr2   *iOffset,
                                                      mng_uint32arr2   *iStarttime,
                                                      mng_uint32       *iLayernr,
                                                      mng_uint32       *iFramenr,
                                                      mng_uint32       *iNamesize,
                                                      mng_pchar        *zName);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_seek       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint32       *iNamesize,
                                                      mng_pchar        *zName);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_expi       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint16       *iSnapshotid,
                                                      mng_uint32       *iNamesize,
                                                      mng_pchar        *zName);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_fpri       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint8        *iDeltatype,
                                                      mng_uint8        *iPriority);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_need       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint32       *iKeywordssize,
                                                      mng_pchar        *zKeywords);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_phyg       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_bool         *bEmpty,
                                                      mng_uint32       *iSizex,
                                                      mng_uint32       *iSizey,
                                                      mng_uint8        *iUnit);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_jhdr       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint32       *iWidth,
                                                      mng_uint32       *iHeight,
                                                      mng_uint8        *iColortype,
                                                      mng_uint8        *iImagesampledepth,
                                                      mng_uint8        *iImagecompression,
                                                      mng_uint8        *iImageinterlace,
                                                      mng_uint8        *iAlphasampledepth,
                                                      mng_uint8        *iAlphacompression,
                                                      mng_uint8        *iAlphafilter,
                                                      mng_uint8        *iAlphainterlace);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_jdat       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint32       *iRawlen,
                                                      mng_ptr          *pRawdata);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_jdaa       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint32       *iRawlen,
                                                      mng_ptr          *pRawdata);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_dhdr       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint16       *iObjectid,
                                                      mng_uint8        *iImagetype,
                                                      mng_uint8        *iDeltatype,
                                                      mng_uint32       *iBlockwidth,
                                                      mng_uint32       *iBlockheight,
                                                      mng_uint32       *iBlockx,
                                                      mng_uint32       *iBlocky);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_prom       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint8        *iColortype,
                                                      mng_uint8        *iSampledepth,
                                                      mng_uint8        *iFilltype);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_pplt       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint32       *iCount);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_pplt_entry (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint32       iEntry,
                                                      mng_uint16       *iRed,
                                                      mng_uint16       *iGreen,
                                                      mng_uint16       *iBlue,
                                                      mng_uint16       *iAlpha,
                                                      mng_bool         *bUsed);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_drop       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint32       *iCount,
                                                      mng_chunkidp     *pChunknames);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_dbyk       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_chunkid      *iChunkname,
                                                      mng_uint8        *iPolarity,
                                                      mng_uint32       *iKeywordssize,
                                                      mng_pchar        *zKeywords);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_ordr       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint32       *iCount);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_ordr_entry (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint32       iEntry,
                                                      mng_chunkid      *iChunkname,
                                                      mng_uint8        *iOrdertype);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_magn       (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_uint16       *iFirstid,
                                                      mng_uint16       *iLastid,
                                                      mng_uint16       *iMethodX,
                                                      mng_uint16       *iMX,
                                                      mng_uint16       *iMY,
                                                      mng_uint16       *iML,
                                                      mng_uint16       *iMR,
                                                      mng_uint16       *iMT,
                                                      mng_uint16       *iMB,
                                                      mng_uint16       *iMethodY);

MNG_EXT mng_retcode MNG_DECL mng_getchunk_unknown    (mng_handle       hHandle,
                                                      mng_handle       hChunk,
                                                      mng_chunkid      *iChunkname,
                                                      mng_uint32       *iRawlen,
                                                      mng_ptr          *pRawdata);

/* ************************************************************************** */

#ifdef MNG_INCLUDE_WRITE_PROCS

/* use these to create new chunks at the end of the chunk-list */
/* requires at least MNG_ACCESS_CHUNKS (MNG_SUPPORT_WRITE may be nice too) */
MNG_EXT mng_retcode MNG_DECL mng_putchunk_ihdr       (mng_handle       hHandle,
                                                      mng_uint32       iWidth,
                                                      mng_uint32       iHeight,
                                                      mng_uint8        iBitdepth,
                                                      mng_uint8        iColortype,
                                                      mng_uint8        iCompression,
                                                      mng_uint8        iFilter,
                                                      mng_uint8        iInterlace);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_plte       (mng_handle       hHandle,
                                                      mng_uint32       iCount,
                                                      mng_palette8     aPalette);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_idat       (mng_handle       hHandle,
                                                      mng_uint32       iRawlen,
                                                      mng_ptr          pRawdata);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_iend       (mng_handle       hHandle);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_trns       (mng_handle       hHandle,
                                                      mng_bool         bEmpty,
                                                      mng_bool         bGlobal,
                                                      mng_uint8        iType,
                                                      mng_uint32       iCount,
                                                      mng_uint8arr     aAlphas,
                                                      mng_uint16       iGray,
                                                      mng_uint16       iRed,
                                                      mng_uint16       iGreen,
                                                      mng_uint16       iBlue,
                                                      mng_uint32       iRawlen,
                                                      mng_uint8arr     aRawdata);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_gama       (mng_handle       hHandle,
                                                      mng_bool         bEmpty,
                                                      mng_uint32       iGamma);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_chrm       (mng_handle       hHandle,
                                                      mng_bool         bEmpty,
                                                      mng_uint32       iWhitepointx,
                                                      mng_uint32       iWhitepointy,
                                                      mng_uint32       iRedx,
                                                      mng_uint32       iRedy,
                                                      mng_uint32       iGreenx,
                                                      mng_uint32       iGreeny,
                                                      mng_uint32       iBluex,
                                                      mng_uint32       iBluey);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_srgb       (mng_handle       hHandle,
                                                      mng_bool         bEmpty,
                                                      mng_uint8        iRenderingintent);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_iccp       (mng_handle       hHandle,
                                                      mng_bool         bEmpty,
                                                      mng_uint32       iNamesize,
                                                      mng_pchar        zName,
                                                      mng_uint8        iCompression,
                                                      mng_uint32       iProfilesize,
                                                      mng_ptr          pProfile);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_text       (mng_handle       hHandle,
                                                      mng_uint32       iKeywordsize,
                                                      mng_pchar        zKeyword,
                                                      mng_uint32       iTextsize,
                                                      mng_pchar        zText);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_ztxt       (mng_handle       hHandle,
                                                      mng_uint32       iKeywordsize,
                                                      mng_pchar        zKeyword,
                                                      mng_uint8        iCompression,
                                                      mng_uint32       iTextsize,
                                                      mng_pchar        zText);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_itxt       (mng_handle       hHandle,
                                                      mng_uint32       iKeywordsize,
                                                      mng_pchar        zKeyword,
                                                      mng_uint8        iCompressionflag,
                                                      mng_uint8        iCompressionmethod,
                                                      mng_uint32       iLanguagesize,
                                                      mng_pchar        zLanguage,
                                                      mng_uint32       iTranslationsize,
                                                      mng_pchar        zTranslation,
                                                      mng_uint32       iTextsize,
                                                      mng_pchar        zText);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_bkgd       (mng_handle       hHandle,
                                                      mng_bool         bEmpty,
                                                      mng_uint8        iType,
                                                      mng_uint8        iIndex,
                                                      mng_uint16       iGray,
                                                      mng_uint16       iRed,
                                                      mng_uint16       iGreen,
                                                      mng_uint16       iBlue);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_phys       (mng_handle       hHandle,
                                                      mng_bool         bEmpty,
                                                      mng_uint32       iSizex,
                                                      mng_uint32       iSizey,
                                                      mng_uint8        iUnit);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_sbit       (mng_handle       hHandle,
                                                      mng_bool         bEmpty,
                                                      mng_uint8        iType,
                                                      mng_uint8arr4    aBits);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_splt       (mng_handle       hHandle,
                                                      mng_bool         bEmpty,
                                                      mng_uint32       iNamesize,
                                                      mng_pchar        zName,
                                                      mng_uint8        iSampledepth,
                                                      mng_uint32       iEntrycount,
                                                      mng_ptr          pEntries);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_hist       (mng_handle       hHandle,
                                                      mng_uint32       iEntrycount,
                                                      mng_uint16arr    aEntries);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_time       (mng_handle       hHandle,
                                                      mng_uint16       iYear,
                                                      mng_uint8        iMonth,
                                                      mng_uint8        iDay,
                                                      mng_uint8        iHour,
                                                      mng_uint8        iMinute,
                                                      mng_uint8        iSecond);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_mhdr       (mng_handle       hHandle,
                                                      mng_uint32       iWidth,
                                                      mng_uint32       iHeight,
                                                      mng_uint32       iTicks,
                                                      mng_uint32       iLayercount,
                                                      mng_uint32       iFramecount,
                                                      mng_uint32       iPlaytime,
                                                      mng_uint32       iSimplicity);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_mend       (mng_handle       hHandle);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_loop       (mng_handle       hHandle,
                                                      mng_uint8        iLevel,
                                                      mng_uint32       iRepeat,
                                                      mng_uint8        iTermination,
                                                      mng_uint32       iItermin,
                                                      mng_uint32       iItermax,
                                                      mng_uint32       iCount,
                                                      mng_uint32p      pSignals);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_endl       (mng_handle       hHandle,
                                                      mng_uint8        iLevel);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_defi       (mng_handle       hHandle,
                                                      mng_uint16       iObjectid,
                                                      mng_uint8        iDonotshow,
                                                      mng_uint8        iConcrete,
                                                      mng_bool         bHasloca,
                                                      mng_int32        iXlocation,
                                                      mng_int32        iYlocation,
                                                      mng_bool         bHasclip,
                                                      mng_int32        iLeftcb,
                                                      mng_int32        iRightcb,
                                                      mng_int32        iTopcb,
                                                      mng_int32        iBottomcb);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_basi       (mng_handle       hHandle,
                                                      mng_uint32       iWidth,
                                                      mng_uint32       iHeight,
                                                      mng_uint8        iBitdepth,
                                                      mng_uint8        iColortype,
                                                      mng_uint8        iCompression,
                                                      mng_uint8        iFilter,
                                                      mng_uint8        iInterlace,
                                                      mng_uint16       iRed,
                                                      mng_uint16       iGreen,
                                                      mng_uint16       iBlue,
                                                      mng_uint16       iAlpha,
                                                      mng_uint8        iViewable);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_clon       (mng_handle       hHandle,
                                                      mng_uint16       iSourceid,
                                                      mng_uint16       iCloneid,
                                                      mng_uint8        iClonetype,
                                                      mng_uint8        iDonotshow,
                                                      mng_uint8        iConcrete,
                                                      mng_bool         bHasloca,
                                                      mng_uint8        iLocationtype,
                                                      mng_int32        iLocationx,
                                                      mng_int32        iLocationy);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_past       (mng_handle       hHandle,
                                                      mng_uint16       iDestid,
                                                      mng_uint8        iTargettype,
                                                      mng_int32        iTargetx,
                                                      mng_int32        iTargety,
                                                      mng_uint32       iCount);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_past_src   (mng_handle       hHandle,
                                                      mng_uint32       iEntry,
                                                      mng_uint16       iSourceid,
                                                      mng_uint8        iComposition,
                                                      mng_uint8        iOrientation,
                                                      mng_uint8        iOffsettype,
                                                      mng_int32        iOffsetx,
                                                      mng_int32        iOffsety,
                                                      mng_uint8        iBoundarytype,
                                                      mng_int32        iBoundaryl,
                                                      mng_int32        iBoundaryr,
                                                      mng_int32        iBoundaryt,
                                                      mng_int32        iBoundaryb);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_disc       (mng_handle       hHandle,
                                                      mng_uint32       iCount,
                                                      mng_uint16p      pObjectids);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_back       (mng_handle       hHandle,
                                                      mng_uint16       iRed,
                                                      mng_uint16       iGreen,
                                                      mng_uint16       iBlue,
                                                      mng_uint8        iMandatory,
                                                      mng_uint16       iImageid,
                                                      mng_uint8        iTile);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_fram       (mng_handle       hHandle,
                                                      mng_bool         bEmpty,
                                                      mng_uint8        iMode,
                                                      mng_uint32       iNamesize,
                                                      mng_pchar        zName,
                                                      mng_uint8        iChangedelay,
                                                      mng_uint8        iChangetimeout,
                                                      mng_uint8        iChangeclipping,
                                                      mng_uint8        iChangesyncid,
                                                      mng_uint32       iDelay,
                                                      mng_uint32       iTimeout,
                                                      mng_uint8        iBoundarytype,
                                                      mng_int32        iBoundaryl,
                                                      mng_int32        iBoundaryr,
                                                      mng_int32        iBoundaryt,
                                                      mng_int32        iBoundaryb,
                                                      mng_uint32       iCount,
                                                      mng_uint32p      pSyncids);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_move       (mng_handle       hHandle,
                                                      mng_uint16       iFirstid,
                                                      mng_uint16       iLastid,
                                                      mng_uint8        iMovetype,
                                                      mng_int32        iMovex,
                                                      mng_int32        iMovey);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_clip       (mng_handle       hHandle,
                                                      mng_uint16       iFirstid,
                                                      mng_uint16       iLastid,
                                                      mng_uint8        iCliptype,
                                                      mng_int32        iClipl,
                                                      mng_int32        iClipr,
                                                      mng_int32        iClipt,
                                                      mng_int32        iClipb);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_show       (mng_handle       hHandle,
                                                      mng_bool         bEmpty,
                                                      mng_uint16       iFirstid,
                                                      mng_uint16       iLastid,
                                                      mng_uint8        iMode);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_term       (mng_handle       hHandle,
                                                      mng_uint8        iTermaction,
                                                      mng_uint8        iIteraction,
                                                      mng_uint32       iDelay,
                                                      mng_uint32       iItermax);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_save       (mng_handle       hHandle,
                                                      mng_bool         bEmpty,
                                                      mng_uint8        iOffsettype,
                                                      mng_uint32       iCount);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_save_entry (mng_handle       hHandle,
                                                      mng_uint32       iEntry,
                                                      mng_uint8        iEntrytype,
                                                      mng_uint32arr2   iOffset,
                                                      mng_uint32arr2   iStarttime,
                                                      mng_uint32       iLayernr,
                                                      mng_uint32       iFramenr,
                                                      mng_uint32       iNamesize,
                                                      mng_pchar        zName);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_seek       (mng_handle       hHandle,
                                                      mng_uint32       iNamesize,
                                                      mng_pchar        zName);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_expi       (mng_handle       hHandle,
                                                      mng_uint16       iSnapshotid,
                                                      mng_uint32       iNamesize,
                                                      mng_pchar        zName);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_fpri       (mng_handle       hHandle,
                                                      mng_uint8        iDeltatype,
                                                      mng_uint8        iPriority);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_need       (mng_handle       hHandle,
                                                      mng_uint32       iKeywordssize,
                                                      mng_pchar        zKeywords);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_phyg       (mng_handle       hHandle,
                                                      mng_bool         bEmpty,
                                                      mng_uint32       iSizex,
                                                      mng_uint32       iSizey,
                                                      mng_uint8        iUnit);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_jhdr       (mng_handle       hHandle,
                                                      mng_uint32       iWidth,
                                                      mng_uint32       iHeight,
                                                      mng_uint8        iColortype,
                                                      mng_uint8        iImagesampledepth,
                                                      mng_uint8        iImagecompression,
                                                      mng_uint8        iImageinterlace,
                                                      mng_uint8        iAlphasampledepth,
                                                      mng_uint8        iAlphacompression,
                                                      mng_uint8        iAlphafilter,
                                                      mng_uint8        iAlphainterlace);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_jdat       (mng_handle       hHandle,
                                                      mng_uint32       iRawlen,
                                                      mng_ptr          pRawdata);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_jdaa       (mng_handle       hHandle,
                                                      mng_uint32       iRawlen,
                                                      mng_ptr          pRawdata);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_jsep       (mng_handle       hHandle);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_dhdr       (mng_handle       hHandle,
                                                      mng_uint16       iObjectid,
                                                      mng_uint8        iImagetype,
                                                      mng_uint8        iDeltatype,
                                                      mng_uint32       iBlockwidth,
                                                      mng_uint32       iBlockheight,
                                                      mng_uint32       iBlockx,
                                                      mng_uint32       iBlocky);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_prom       (mng_handle       hHandle,
                                                      mng_uint8        iColortype,
                                                      mng_uint8        iSampledepth,
                                                      mng_uint8        iFilltype);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_ipng       (mng_handle       hHandle);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_pplt       (mng_handle       hHandle,
                                                      mng_uint32       iCount);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_pplt_entry (mng_handle       hHandle,
                                                      mng_uint32       iEntry,
                                                      mng_uint16       iRed,
                                                      mng_uint16       iGreen,
                                                      mng_uint16       iBlue,
                                                      mng_uint16       iAlpha,
                                                      mng_bool         bUsed);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_jpng       (mng_handle       hHandle);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_drop       (mng_handle       hHandle,
                                                      mng_uint32       iCount,
                                                      mng_chunkidp     pChunknames);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_dbyk       (mng_handle       hHandle,
                                                      mng_chunkid      iChunkname,
                                                      mng_uint8        iPolarity,
                                                      mng_uint32       iKeywordssize,
                                                      mng_pchar        zKeywords);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_ordr       (mng_handle       hHandle,
                                                      mng_uint32       iCount);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_ordr_entry (mng_handle       hHandle,
                                                      mng_uint32       iEntry,
                                                      mng_chunkid      iChunkname,
                                                      mng_uint8        iOrdertype);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_magn       (mng_handle       hHandle,
                                                      mng_uint16       iFirstid,
                                                      mng_uint16       iLastid,
                                                      mng_uint16       iMethodX,
                                                      mng_uint16       iMX,
                                                      mng_uint16       iMY,
                                                      mng_uint16       iML,
                                                      mng_uint16       iMR,
                                                      mng_uint16       iMT,
                                                      mng_uint16       iMB,
                                                      mng_uint16       iMethodY);

MNG_EXT mng_retcode MNG_DECL mng_putchunk_unknown    (mng_handle       hHandle,
                                                      mng_chunkid      iChunkname,
                                                      mng_uint32       iRawlen,
                                                      mng_ptr          pRawdata);

#endif /* MNG_INCLUDE_WRITE_PROCS */

/* ************************************************************************** */

/* use these functions to access the actual image-data in stored chunks,
   as opposed to the IDAT/JDAT data */
/* to get accurate pixel-data the canvasstyle should seriously reflect the
   bitdepth/colortype combination of the preceding IHDR/JHDR/BASI/DHDR;
   all input can be converted to rgb(a)8 (rgb(a)16 for 16-bit images), but
   there are only limited conversions back (see below for putimgdata)  */

/* call this function if you want to extract the nth image from the list;
   the first image is designated seqnr 0! */
/* this function finds the IHDR/JHDR/BASI/DHDR with the appropriate seqnr,
   starting from the beginning of the chunk-list; this may tend to get a little
   slow for animations with a large number of chunks for images near the end */
/* supplying a seqnr past the last image in the animation will return with
   an errorcode */   
MNG_EXT mng_retcode MNG_DECL mng_getimgdata_seq      (mng_handle        hHandle,
                                                      mng_uint32        iSeqnr,
                                                      mng_uint32        iCanvasstyle,
                                                      mng_getcanvasline fGetcanvasline);

/* both the following functions will search forward to find the first IDAT/JDAT,
   and then traverse back to find the start of the image (IHDR,JHDR,DHDR,BASI);
   note that this is very fast compared to decoding the IDAT/JDAT, so there's
   not really a need for optimization; either can be called from the
   iterate_chunks callback when a IHDR/JHDR is encountered; for BASI/DHDR there
   may not be real image-data so it's wisest to keep iterating till the IEND,
   and then call either of these functions if necessary (remember the correct seqnr!) */

/* call this function if you want to extract the image starting at or after the nth
   position in the chunk-list; this number is returned in the iterate_chunks callback */
MNG_EXT mng_retcode MNG_DECL mng_getimgdata_chunkseq (mng_handle        hHandle,
                                                      mng_uint32        iSeqnr,
                                                      mng_uint32        iCanvasstyle,
                                                      mng_getcanvasline fGetcanvasline);

/* call this function if you want to extract the image starting at or after the
   indicated chunk; the handle of a chunk is returned in the iterate_chunks callback */
MNG_EXT mng_retcode MNG_DECL mng_getimgdata_chunk    (mng_handle        hHandle,
                                                      mng_handle        hChunk,
                                                      mng_uint32        iCanvasstyle,
                                                      mng_getcanvasline fGetcanvasline);

/* ************************************************************************** */

#ifdef MNG_INCLUDE_WRITE_PROCS

/* use the following functions to add image-data to the list of stored chunks */
/* note that this only adds the IDAT or JDAT chunks and no others; you must call
   one of these functions after you 'put' the initial chunks of the image and
   before you 'put' the closing chunks */
/* the canvasstyle should seriously reflect the bitdepth/colortype combination;
   eg. bitdepth=16 would expect a 16-bit canvasstyle,
   colortype=g or ga would expect a gray or gray+alpha style respectively
   and so on, and so forth ...
   (nb. the number of conversions will be extremely limited for the moment!) */

MNG_EXT mng_retcode MNG_DECL mng_putimgdata_ihdr     (mng_handle        hHandle,
                                                      mng_uint32        iWidth,
                                                      mng_uint32        iHeight,
                                                      mng_uint8         iColortype,
                                                      mng_uint8         iBitdepth,
                                                      mng_uint8         iCompression,
                                                      mng_uint8         iFilter,
                                                      mng_uint8         iInterlace,
                                                      mng_uint32        iCanvasstyle,
                                                      mng_getcanvasline fGetcanvasline);

MNG_EXT mng_retcode MNG_DECL mng_putimgdata_jhdr     (mng_handle        hHandle,
                                                      mng_uint32        iWidth,
                                                      mng_uint32        iHeight,
                                                      mng_uint8         iColortype,
                                                      mng_uint8         iBitdepth,
                                                      mng_uint8         iCompression,
                                                      mng_uint8         iInterlace,
                                                      mng_uint8         iAlphaBitdepth,
                                                      mng_uint8         iAlphaCompression,
                                                      mng_uint8         iAlphaFilter,
                                                      mng_uint8         iAlphaInterlace,
                                                      mng_uint32        iCanvasstyle,
                                                      mng_getcanvasline fGetcanvasline);

/* ************************************************************************** */

/* use the following functions to set the framecount/layercount/playtime or
   simplicity of an animation you are creating; this may be useful if these
   variables are calculated during the creation-process */

MNG_EXT mng_retcode MNG_DECL mng_updatemngheader     (mng_handle        hHandle,
                                                      mng_uint32        iFramecount,
                                                      mng_uint32        iLayercount,
                                                      mng_uint32        iPlaytime);

MNG_EXT mng_retcode MNG_DECL mng_updatemngsimplicity (mng_handle        hHandle,
                                                      mng_uint32        iSimplicity);

/* ************************************************************************** */

#endif /* MNG_INCLUDE_WRITE_PROCS */

#endif /* MNG_ACCESS_CHUNKS */

/* ************************************************************************** */
/* *                                                                        * */
/* * Error-code structure                                                   * */
/* *                                                                        * */
/* * 0b0000 00xx xxxx xxxx - basic errors; severity 9 (environment)         * */
/* * 0b0000 01xx xxxx xxxx - chunk errors; severity 9 (image induced)       * */
/* * 0b0000 10xx xxxx xxxx - severity 5 errors (application induced)        * */
/* * 0b0001 00xx xxxx xxxx - severity 2 warnings (recoverable)              * */
/* * 0b0010 00xx xxxx xxxx - severity 1 warnings (recoverable)              * */
/* *                                                                        * */
/* ************************************************************************** */

#define MNG_NOERROR          (mng_retcode)0    /* er.. indicates all's well   */

#define MNG_OUTOFMEMORY      (mng_retcode)1    /* oops, buy some megabytes!   */
#define MNG_INVALIDHANDLE    (mng_retcode)2    /* call mng_initialize first   */
#define MNG_NOCALLBACK       (mng_retcode)3    /* set the callbacks please    */
#define MNG_UNEXPECTEDEOF    (mng_retcode)4    /* what'd ya do with the data? */
#define MNG_ZLIBERROR        (mng_retcode)5    /* zlib burped                 */
#define MNG_JPEGERROR        (mng_retcode)6    /* jpglib complained           */
#define MNG_LCMSERROR        (mng_retcode)7    /* little cms stressed out     */
#define MNG_NOOUTPUTPROFILE  (mng_retcode)8    /* no output-profile defined   */
#define MNG_NOSRGBPROFILE    (mng_retcode)9    /* no sRGB-profile defined     */
#define MNG_BUFOVERFLOW      (mng_retcode)10   /* zlib output-buffer overflow */
#define MNG_FUNCTIONINVALID  (mng_retcode)11   /* ay, totally inappropriate   */
#define MNG_OUTPUTERROR      (mng_retcode)12   /* disk full ?                 */
#define MNG_JPEGBUFTOOSMALL  (mng_retcode)13   /* can't handle buffer overflow*/
#define MNG_NEEDMOREDATA     (mng_retcode)14   /* I'm hungry, give me more    */
#define MNG_NEEDTIMERWAIT    (mng_retcode)15   /* Sleep a while then wake me  */
#define MNG_NEEDSECTIONWAIT  (mng_retcode)16   /* just processed a SEEK       */
#define MNG_LOOPWITHCACHEOFF (mng_retcode)17   /* LOOP when playback info off */

#define MNG_DLLNOTLOADED     (mng_retcode)99   /* late binding failed         */

#define MNG_APPIOERROR       (mng_retcode)901  /* application I/O error       */
#define MNG_APPTIMERERROR    (mng_retcode)902  /* application timing error    */
#define MNG_APPCMSERROR      (mng_retcode)903  /* application CMS error       */
#define MNG_APPMISCERROR     (mng_retcode)904  /* application other error     */
#define MNG_APPTRACEABORT    (mng_retcode)905  /* application aborts on trace */

#define MNG_INTERNALERROR    (mng_retcode)999  /* internal inconsistancy      */

#define MNG_INVALIDSIG       (mng_retcode)1025 /* invalid graphics file       */
#define MNG_INVALIDCRC       (mng_retcode)1027 /* crc check failed            */
#define MNG_INVALIDLENGTH    (mng_retcode)1028 /* chunklength mystifies me    */
#define MNG_SEQUENCEERROR    (mng_retcode)1029 /* invalid chunk sequence      */
#define MNG_CHUNKNOTALLOWED  (mng_retcode)1030 /* completely out-of-place     */
#define MNG_MULTIPLEERROR    (mng_retcode)1031 /* only one occurence allowed  */
#define MNG_PLTEMISSING      (mng_retcode)1032 /* indexed-color requires PLTE */
#define MNG_IDATMISSING      (mng_retcode)1033 /* IHDR-block requires IDAT    */
#define MNG_CANNOTBEEMPTY    (mng_retcode)1034 /* must contain some data      */
#define MNG_GLOBALLENGTHERR  (mng_retcode)1035 /* global data incorrect       */
#define MNG_INVALIDBITDEPTH  (mng_retcode)1036 /* bitdepth out-of-range       */
#define MNG_INVALIDCOLORTYPE (mng_retcode)1037 /* colortype out-of-range      */
#define MNG_INVALIDCOMPRESS  (mng_retcode)1038 /* compression method invalid  */
#define MNG_INVALIDFILTER    (mng_retcode)1039 /* filter method invalid       */
#define MNG_INVALIDINTERLACE (mng_retcode)1040 /* interlace method invalid    */
#define MNG_NOTENOUGHIDAT    (mng_retcode)1041 /* ran out of compressed data  */
#define MNG_PLTEINDEXERROR   (mng_retcode)1042 /* palette-index out-of-range  */
#define MNG_NULLNOTFOUND     (mng_retcode)1043 /* couldn't find null-separator*/
#define MNG_KEYWORDNULL      (mng_retcode)1044 /* keyword cannot be empty     */
#define MNG_OBJECTUNKNOWN    (mng_retcode)1045 /* the object can't be found   */
#define MNG_OBJECTEXISTS     (mng_retcode)1046 /* the object already exists   */
#define MNG_TOOMUCHIDAT      (mng_retcode)1047 /* got too much compressed data*/
#define MNG_INVSAMPLEDEPTH   (mng_retcode)1048 /* sampledepth out-of-range    */
#define MNG_INVOFFSETSIZE    (mng_retcode)1049 /* invalid offset-size         */
#define MNG_INVENTRYTYPE     (mng_retcode)1050 /* invalid entry-type          */
#define MNG_ENDWITHNULL      (mng_retcode)1051 /* may not end with NULL       */
#define MNG_INVIMAGETYPE     (mng_retcode)1052 /* invalid image_type          */
#define MNG_INVDELTATYPE     (mng_retcode)1053 /* invalid delta_type          */
#define MNG_INVALIDINDEX     (mng_retcode)1054 /* index-value invalid         */
#define MNG_TOOMUCHJDAT      (mng_retcode)1055 /* got too much compressed data*/
#define MNG_JPEGPARMSERR     (mng_retcode)1056 /* JHDR/JPEG parms do not match*/
#define MNG_INVFILLMETHOD    (mng_retcode)1057 /* invalid fill_method         */
#define MNG_OBJNOTCONCRETE   (mng_retcode)1058 /* object must be concrete     */
#define MNG_TARGETNOALPHA    (mng_retcode)1059 /* object has no alpha-channel */
#define MNG_MNGTOOCOMPLEX    (mng_retcode)1060 /* can't handle complexity     */
#define MNG_UNKNOWNCRITICAL  (mng_retcode)1061 /* unknown critical chunk found*/
#define MNG_UNSUPPORTEDNEED  (mng_retcode)1062 /* nEED requirement unsupported*/
#define MNG_INVALIDDELTA     (mng_retcode)1063 /* Delta operation illegal     */
#define MNG_INVALIDMETHOD    (mng_retcode)1064 /* invalid MAGN method         */

#define MNG_INVALIDCNVSTYLE  (mng_retcode)2049 /* can't make anything of this */
#define MNG_WRONGCHUNK       (mng_retcode)2050 /* accessing the wrong chunk   */
#define MNG_INVALIDENTRYIX   (mng_retcode)2051 /* accessing the wrong entry   */
#define MNG_NOHEADER         (mng_retcode)2052 /* must have had header first  */
#define MNG_NOCORRCHUNK      (mng_retcode)2053 /* can't find parent chunk     */
#define MNG_NOMHDR           (mng_retcode)2054 /* no MNG header available     */

#define MNG_IMAGETOOLARGE    (mng_retcode)4097 /* input-image way too big     */
#define MNG_NOTANANIMATION   (mng_retcode)4098 /* file not a MNG              */
#define MNG_FRAMENRTOOHIGH   (mng_retcode)4099 /* frame-nr out-of-range       */
#define MNG_LAYERNRTOOHIGH   (mng_retcode)4100 /* layer-nr out-of-range       */
#define MNG_PLAYTIMETOOHIGH  (mng_retcode)4101 /* playtime out-of-range       */
#define MNG_FNNOTIMPLEMENTED (mng_retcode)4102 /* function not yet available  */

#define MNG_IMAGEFROZEN      (mng_retcode)8193 /* stopped displaying          */

#define MNG_LCMS_NOHANDLE    1                 /* LCMS returned NULL handle   */
#define MNG_LCMS_NOMEM       2                 /* LCMS returned NULL gammatab */
#define MNG_LCMS_NOTRANS     3                 /* LCMS returned NULL transform*/

/* ************************************************************************** */
/* *                                                                        * */
/* *  Canvas styles                                                         * */
/* *                                                                        * */
/* *  Note that the intentions are pretty darn good, but that the focus     * */
/* *  is currently on 8-bit color support                                   * */
/* *                                                                        * */
/* *  The RGB8_A8 style is defined for apps that require a separate         * */
/* *  canvas for the color-planes and the alpha-plane (eg. mozilla)         * */
/* *  This requires for the app to supply the "getalphaline" callback!!!    * */
/* *                                                                        * */
/* ************************************************************************** */

#define MNG_CANVAS_RGB8      0x00000000L
#define MNG_CANVAS_RGBA8     0x00001000L
#define MNG_CANVAS_ARGB8     0x00003000L
#define MNG_CANVAS_RGB8_A8   0x00005000L
#define MNG_CANVAS_BGR8      0x00000001L
#define MNG_CANVAS_BGRA8     0x00001001L
#define MNG_CANVAS_BGRA8PM   0x00009001L
#define MNG_CANVAS_ABGR8     0x00003001L
#define MNG_CANVAS_RGB16     0x00000100L         /* not supported yet */
#define MNG_CANVAS_RGBA16    0x00001100L         /* not supported yet */
#define MNG_CANVAS_ARGB16    0x00003100L         /* not supported yet */
#define MNG_CANVAS_BGR16     0x00000101L         /* not supported yet */
#define MNG_CANVAS_BGRA16    0x00001101L         /* not supported yet */
#define MNG_CANVAS_ABGR16    0x00003101L         /* not supported yet */
#define MNG_CANVAS_GRAY8     0x00000002L         /* not supported yet */
#define MNG_CANVAS_GRAY16    0x00000102L         /* not supported yet */
#define MNG_CANVAS_GRAYA8    0x00001002L         /* not supported yet */
#define MNG_CANVAS_GRAYA16   0x00001102L         /* not supported yet */
#define MNG_CANVAS_AGRAY8    0x00003002L         /* not supported yet */
#define MNG_CANVAS_AGRAY16   0x00003102L         /* not supported yet */
#define MNG_CANVAS_DX15      0x00000003L         /* not supported yet */
#define MNG_CANVAS_DX16      0x00000004L         /* not supported yet */

#define MNG_CANVAS_PIXELTYPE(C)  (C & 0x000000FFL)
#define MNG_CANVAS_BITDEPTH(C)   (C & 0x00000100L)
#define MNG_CANVAS_HASALPHA(C)   (C & 0x00001000L)
#define MNG_CANVAS_ALPHAFIRST(C) (C & 0x00002000L)
#define MNG_CANVAS_ALPHASEPD(C)  (C & 0x00004000L)
#define MNG_CANVAS_ALPHAPM(C)    (C & 0x00008000L)

#define MNG_CANVAS_RGB(C)        (MNG_CANVAS_PIXELTYPE (C) == 0)
#define MNG_CANVAS_BGR(C)        (MNG_CANVAS_PIXELTYPE (C) == 1)
#define MNG_CANVAS_GRAY(C)       (MNG_CANVAS_PIXELTYPE (C) == 2)
#define MNG_CANVAS_DIRECTX15(C)  (MNG_CANVAS_PIXELTYPE (C) == 3)
#define MNG_CANVAS_DIRECTX16(C)  (MNG_CANVAS_PIXELTYPE (C) == 4)
#define MNG_CANVAS_8BIT(C)       (!MNG_CANVAS_BITDEPTH (C))
#define MNG_CANVAS_16BIT(C)      (MNG_CANVAS_BITDEPTH (C))
#define MNG_CANVAS_PIXELFIRST(C) (!MNG_CANVAS_ALPHAFIRST (C))

/* ************************************************************************** */
/* *                                                                        * */
/* *  Chunk names (idea adapted from libpng 1.1.0 - png.h)                  * */
/* *                                                                        * */
/* ************************************************************************** */

#define MNG_UINT_HUH  0x40404040L

#define MNG_UINT_BACK 0x4241434bL
#define MNG_UINT_BASI 0x42415349L
#define MNG_UINT_CLIP 0x434c4950L
#define MNG_UINT_CLON 0x434c4f4eL
#define MNG_UINT_DBYK 0x4442594bL
#define MNG_UINT_DEFI 0x44454649L
#define MNG_UINT_DHDR 0x44484452L
#define MNG_UINT_DISC 0x44495343L
#define MNG_UINT_DROP 0x44524f50L
#define MNG_UINT_ENDL 0x454e444cL
#define MNG_UINT_FRAM 0x4652414dL
#define MNG_UINT_IDAT 0x49444154L
#define MNG_UINT_IEND 0x49454e44L
#define MNG_UINT_IHDR 0x49484452L
#define MNG_UINT_IJNG 0x494a4e47L
#define MNG_UINT_IPNG 0x49504e47L
#define MNG_UINT_JDAA 0x4a444141L
#define MNG_UINT_JDAT 0x4a444154L
#define MNG_UINT_JHDR 0x4a484452L
#define MNG_UINT_JSEP 0x4a534550L
#define MNG_UINT_JdAA 0x4a644141L
#define MNG_UINT_LOOP 0x4c4f4f50L
#define MNG_UINT_MAGN 0x4d41474eL
#define MNG_UINT_MEND 0x4d454e44L
#define MNG_UINT_MHDR 0x4d484452L
#define MNG_UINT_MOVE 0x4d4f5645L
#define MNG_UINT_ORDR 0x4f524452L
#define MNG_UINT_PAST 0x50415354L
#define MNG_UINT_PLTE 0x504c5445L
#define MNG_UINT_PPLT 0x50504c54L
#define MNG_UINT_PROM 0x50524f4dL
#define MNG_UINT_SAVE 0x53415645L
#define MNG_UINT_SEEK 0x5345454bL
#define MNG_UINT_SHOW 0x53484f57L
#define MNG_UINT_TERM 0x5445524dL
#define MNG_UINT_bKGD 0x624b4744L
#define MNG_UINT_cHRM 0x6348524dL
#define MNG_UINT_eXPI 0x65585049L
#define MNG_UINT_fPRI 0x66505249L
#define MNG_UINT_gAMA 0x67414d41L
#define MNG_UINT_hIST 0x68495354L
#define MNG_UINT_iCCP 0x69434350L
#define MNG_UINT_iTXt 0x69545874L
#define MNG_UINT_nEED 0x6e454544L
#define MNG_UINT_oFFs 0x6f464673L
#define MNG_UINT_pCAL 0x7043414cL
#define MNG_UINT_pHYg 0x70444167L
#define MNG_UINT_pHYs 0x70485973L
#define MNG_UINT_sBIT 0x73424954L
#define MNG_UINT_sCAL 0x7343414cL
#define MNG_UINT_sPLT 0x73504c54L
#define MNG_UINT_sRGB 0x73524742L
#define MNG_UINT_tEXt 0x74455874L
#define MNG_UINT_tIME 0x74494d45L
#define MNG_UINT_tRNS 0x74524e53L
#define MNG_UINT_zTXt 0x7a545874L

/* ************************************************************************** */
/* *                                                                        * */
/* *  Chunk property values                                                 * */
/* *                                                                        * */
/* ************************************************************************** */

#define MNG_BITDEPTH_1                   1       /* IHDR, BASI, JHDR, PROM */
#define MNG_BITDEPTH_2                   2
#define MNG_BITDEPTH_4                   4
#define MNG_BITDEPTH_8                   8       /* sPLT */
#define MNG_BITDEPTH_16                 16

#define MNG_COLORTYPE_GRAY               0       /* IHDR, BASI, PROM */
#define MNG_COLORTYPE_RGB                2
#define MNG_COLORTYPE_INDEXED            3
#define MNG_COLORTYPE_GRAYA              4
#define MNG_COLORTYPE_RGBA               6

#define MNG_COMPRESSION_DEFLATE          0       /* IHDR, zTXt, iTXt, iCCP,
                                                    BASI, JHDR */

#define MNG_FILTER_ADAPTIVE              0       /* IHDR, BASI, JHDR */
/* #define MNG_FILTER_NO_ADAPTIVE           1 */
#define MNG_FILTER_NO_DIFFERING          0
#define MNG_FILTER_DIFFERING             0x40
/* #define MNG_FILTER_MASK                  (MNG_FILTER_NO_ADAPTIVE | MNG_FILTER_DIFFERING) */

#define MNG_INTERLACE_NONE               0       /* IHDR, BASI, JHDR */
#define MNG_INTERLACE_ADAM7              1

#define MNG_FILTER_NONE                  0       /* IDAT */
#define MNG_FILTER_SUB                   1
#define MNG_FILTER_UP                    2
#define MNG_FILTER_AVERAGE               3
#define MNG_FILTER_PAETH                 4

#define MNG_INTENT_PERCEPTUAL            0       /* sRGB */
#define MNG_INTENT_RELATIVECOLORIMETRIC  1
#define MNG_INTENT_SATURATION            2
#define MNG_INTENT_ABSOLUTECOLORIMETRIC  3
                                                 /* tEXt, zTXt, iTXt */
#define MNG_TEXT_TITLE                   "Title"
#define MNG_TEXT_AUTHOR                  "Author"
#define MNG_TEXT_DESCRIPTION             "Description"
#define MNG_TEXT_COPYRIGHT               "Copyright"
#define MNG_TEXT_CREATIONTIME            "Creation Time"
#define MNG_TEXT_SOFTWARE                "Software"
#define MNG_TEXT_DISCLAIMER              "Disclaimer"
#define MNG_TEXT_WARNING                 "Warning"
#define MNG_TEXT_SOURCE                  "Source"
#define MNG_TEXT_COMMENT                 "Comment"

#define MNG_FLAG_UNCOMPRESSED            0       /* iTXt */
#define MNG_FLAG_COMPRESSED              1

#define MNG_UNIT_UNKNOWN                 0       /* pHYs, pHYg */
#define MNG_UNIT_METER                   1
                                                 /* MHDR */
#define MNG_SIMPLICITY_VALID             0x00000001
#define MNG_SIMPLICITY_SIMPLEFEATURES    0x00000002
#define MNG_SIMPLICITY_COMPLEXFEATURES   0x00000004
#define MNG_SIMPLICITY_TRANSPARENCY      0x00000008
#define MNG_SIMPLICITY_JNG               0x00000010
#define MNG_SIMPLICITY_DELTAPNG          0x00000020

#define MNG_TERMINATION_DECODER_NC       0       /* LOOP */
#define MNG_TERMINATION_USER_NC          1
#define MNG_TERMINATION_EXTERNAL_NC      2
#define MNG_TERMINATION_DETERMINISTIC_NC 3
#define MNG_TERMINATION_DECODER_C        4
#define MNG_TERMINATION_USER_C           5
#define MNG_TERMINATION_EXTERNAL_C       6
#define MNG_TERMINATION_DETERMINISTIC_C  7

#define MNG_DONOTSHOW_VISIBLE            0       /* DEFI */
#define MNG_DONOTSHOW_NOTVISIBLE         1

#define MNG_ABSTRACT                     0       /* DEFI */
#define MNG_CONCRETE                     1

#define MNG_NOTVIEWABLE                  0       /* BASI */
#define MNG_VIEWABLE                     1

#define MNG_FULL_CLONE                   0       /* CLON */
#define MNG_PARTIAL_CLONE                1
#define MNG_RENUMBER                     2

#define MNG_CONCRETE_ASPARENT            0       /* CLON */
#define MNG_CONCRETE_MAKEABSTRACT        1

#define MNG_LOCATION_ABSOLUTE            0       /* CLON, MOVE */
#define MNG_LOCATION_RELATIVE            1

#define MNG_TARGET_ABSOLUTE              0       /* PAST */
#define MNG_TARGET_RELATIVE_SAMEPAST     1
#define MNG_TARGET_RELATIVE_PREVPAST     2

#define MNG_COMPOSITE_OVER               0       /* PAST */
#define MNG_COMPOSITE_REPLACE            1
#define MNG_COMPOSITE_UNDER              2

#define MNG_ORIENTATION_SAME             0       /* PAST */
#define MNG_ORIENTATION_180DEG           2
#define MNG_ORIENTATION_FLIPHORZ         4
#define MNG_ORIENTATION_FLIPVERT         6
#define MNG_ORIENTATION_TILED            8

#define MNG_OFFSET_ABSOLUTE              0       /* PAST */
#define MNG_OFFSET_RELATIVE              1

#define MNG_BOUNDARY_ABSOLUTE            0       /* PAST, FRAM */
#define MNG_BOUNDARY_RELATIVE            1

#define MNG_BACKGROUNDCOLOR_MANDATORY    0x01    /* BACK */
#define MNG_BACKGROUNDIMAGE_MANDATORY    0x02    /* BACK */

#define MNG_BACKGROUNDIMAGE_NOTILE       0       /* BACK */
#define MNG_BACKGROUNDIMAGE_TILE         1

#define MNG_FRAMINGMODE_NOCHANGE         0       /* FRAM */
#define MNG_FRAMINGMODE_1                1
#define MNG_FRAMINGMODE_2                2
#define MNG_FRAMINGMODE_3                3
#define MNG_FRAMINGMODE_4                4

#define MNG_CHANGEDELAY_NO               0       /* FRAM */
#define MNG_CHANGEDELAY_NEXTSUBFRAME     1
#define MNG_CHANGEDELAY_DEFAULT          2

#define MNG_CHANGETIMOUT_NO              0       /* FRAM */
#define MNG_CHANGETIMOUT_DETERMINISTIC_1 1
#define MNG_CHANGETIMOUT_DETERMINISTIC_2 2
#define MNG_CHANGETIMOUT_DECODER_1       3
#define MNG_CHANGETIMOUT_DECODER_2       4
#define MNG_CHANGETIMOUT_USER_1          5
#define MNG_CHANGETIMOUT_USER_2          6
#define MNG_CHANGETIMOUT_EXTERNAL_1      7
#define MNG_CHANGETIMOUT_EXTERNAL_2      8

#define MNG_CHANGECLIPPING_NO            0       /* FRAM */
#define MNG_CHANGECLIPPING_NEXTSUBFRAME  1
#define MNG_CHANGECLIPPING_DEFAULT       2

#define MNG_CHANGESYNCID_NO              0       /* FRAM */
#define MNG_CHANGESYNCID_NEXTSUBFRAME    1
#define MNG_CHANGESYNCID_DEFAULT         2

#define MNG_CLIPPING_ABSOLUTE            0       /* CLIP */
#define MNG_CLIPPING_RELATIVE            1

#define MNG_SHOWMODE_0                   0       /* SHOW */
#define MNG_SHOWMODE_1                   1
#define MNG_SHOWMODE_2                   2
#define MNG_SHOWMODE_3                   3
#define MNG_SHOWMODE_4                   4
#define MNG_SHOWMODE_5                   5
#define MNG_SHOWMODE_6                   6
#define MNG_SHOWMODE_7                   7

#define MNG_TERMACTION_LASTFRAME         0       /* TERM */
#define MNG_TERMACTION_CLEAR             1
#define MNG_TERMACTION_FIRSTFRAME        2
#define MNG_TERMACTION_REPEAT            3

#define MNG_ITERACTION_LASTFRAME         0       /* TERM */
#define MNG_ITERACTION_CLEAR             1
#define MNG_ITERACTION_FIRSTFRAME        2

#define MNG_SAVEOFFSET_4BYTE             4       /* SAVE */
#define MNG_SAVEOFFSET_8BYTE             8

#define MNG_SAVEENTRY_SEGMENTFULL        0       /* SAVE */
#define MNG_SAVEENTRY_SEGMENT            1
#define MNG_SAVEENTRY_SUBFRAME           2
#define MNG_SAVEENTRY_EXPORTEDIMAGE      3

#define MNG_PRIORITY_ABSOLUTE            0       /* fPRI */
#define MNG_PRIORITY_RELATIVE            1

#ifdef MNG_INCLUDE_JNG
#define MNG_COLORTYPE_JPEGGRAY           8       /* JHDR */
#define MNG_COLORTYPE_JPEGCOLOR         10
#define MNG_COLORTYPE_JPEGGRAYA         12
#define MNG_COLORTYPE_JPEGCOLORA        14

#define MNG_BITDEPTH_JPEG8               8       /* JHDR */
#define MNG_BITDEPTH_JPEG12             12
#define MNG_BITDEPTH_JPEG8AND12         20

#define MNG_COMPRESSION_BASELINEJPEG     8       /* JHDR */

#define MNG_INTERLACE_SEQUENTIAL         0       /* JHDR */
#define MNG_INTERLACE_PROGRESSIVE        8
#endif /* MNG_INCLUDE_JNG */

#define MNG_IMAGETYPE_UNKNOWN            0       /* DHDR */
#define MNG_IMAGETYPE_PNG                1
#define MNG_IMAGETYPE_JNG                2

#define MNG_DELTATYPE_REPLACE            0       /* DHDR */
#define MNG_DELTATYPE_BLOCKPIXELADD      1
#define MNG_DELTATYPE_BLOCKALPHAADD      2
#define MNG_DELTATYPE_BLOCKCOLORADD      3
#define MNG_DELTATYPE_BLOCKPIXELREPLACE  4
#define MNG_DELTATYPE_BLOCKALPHAREPLACE  5
#define MNG_DELTATYPE_BLOCKCOLORREPLACE  6
#define MNG_DELTATYPE_NOCHANGE           7

#define MNG_FILLMETHOD_LEFTBITREPLICATE  0       /* PROM */
#define MNG_FILLMETHOD_ZEROFILL          1

#define MNG_DELTATYPE_REPLACERGB         0       /* PPLT */
#define MNG_DELTATYPE_DELTARGB           1
#define MNG_DELTATYPE_REPLACEALPHA       2
#define MNG_DELTATYPE_DELTAALPHA         3
#define MNG_DELTATYPE_REPLACERGBA        4
#define MNG_DELTATYPE_DELTARGBA          5

#define MNG_POLARITY_ONLY                0       /* DBYK */
#define MNG_POLARITY_ALLBUT              1

/* ************************************************************************** */
/* *                                                                        * */
/* *  Processtext callback types                                            * */
/* *                                                                        * */
/* ************************************************************************** */

#define MNG_TYPE_TEXT 0
#define MNG_TYPE_ZTXT 1
#define MNG_TYPE_ITXT 2

/* ************************************************************************** */

#ifdef __cplusplus
}
#endif

#endif /* _libmng_h_ */

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */

