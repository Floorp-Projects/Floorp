/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : libmng_conf.h             copyright (c) 2000 G.Juyn        * */
/* * version   : 1.0.0                                                      * */
/* *                                                                        * */
/* * purpose   : main configuration file                                    * */
/* *                                                                        * */
/* * author    : G.Juyn                                                     * */
/* * web       : http://www.3-t.com                                         * */
/* * email     : mailto:info@3-t.com                                        * */
/* *                                                                        * */
/* * comment   : The configuration file. Change this to include/exclude     * */
/* *             the options you want or do not want in libmng.             * */
/* *                                                                        * */
/* * changes   : 0.5.2 - 06/02/2000 - G.Juyn                                * */
/* *             - separated configuration-options into this file           * */
/* *             - changed to most likely configuration (?)                 * */
/* *             0.5.2 - 06/03/2000 - G.Juyn                                * */
/* *             - changed options to create a standard so-library          * */
/* *               with everything enabled                                  * */
/* *             0.5.2 - 06/04/2000 - G.Juyn                                * */
/* *             - changed options to create a standard win32-dll           * */
/* *               with everything enabled                                  * */
/* *                                                                        * */
/* *             0.9.2 - 08/05/2000 - G.Juyn                                * */
/* *             - changed file-prefixes                                    * */
/* *                                                                        * */
/* *             0.9.3 - 08/12/2000 - G.Juyn                                * */
/* *             - added workaround for faulty PhotoShop iCCP chunk         * */
/* *             0.9.3 - 09/16/2000 - G.Juyn                                * */
/* *             - removed trace-options from default SO/DLL builds         * */
/* *                                                                        * */
/* ************************************************************************** */

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

#ifndef _libmng_conf_h_
#define _libmng_conf_h_

/* ************************************************************************** */
/* *                                                                        * */
/* *  User-selectable compile-time options                                  * */
/* *                                                                        * */
/* ************************************************************************** */

/* enable exactly one(1) of the MNG-(sub)set selectors */
/* use this to select which (sub)set of the MNG specification you wish
   to support */
/* generally you'll want full support as the library provides it automatically
   for you! if you're really strung on memory-requirements you can opt
   to enable less support (but it's just NOT a good idea!) */
/* NOTE that this isn't actually implemented yet */

#if !defined(MNG_SUPPORT_FULL) && !defined(MNG_SUPPORT_LC) && !defined(MNG_SUPPORT_VLC)
#define MNG_SUPPORT_FULL
/* #define MNG_SUPPORT_LC */
/* #define MNG_SUPPORT_VLC */
#endif

/* ************************************************************************** */

/* enable JPEG support if required */
/* use this to enable the JNG support routines */
/* this requires an external jpeg package;
   currently only IJG's jpgsrc6b is supported! */
/* NOTE that the IJG code can be either 8- or 12-bit (eg. not both);
   so choose the one you've defined in jconfig.h; if you don't know what
   the heck I'm talking about, just leave it at 8-bit support (thank you!) */

#ifdef MNG_SUPPORT_FULL                /* full support includes JNG */
#define MNG_SUPPORT_IJG6B
#endif

#ifndef MNG_SUPPORT_IJG6B
#if defined(MNG_BUILD_SO) || defined(MNG_USE_SO) || defined(MNG_BUILD_DLL) || defined(MNG_USE_DLL)
#define MNG_SUPPORT_IJG6B
#endif
#endif

#if defined(MNG_SUPPORT_IJG6B) && !defined(MNG_SUPPORT_JPEG8) && !defined(MNG_SUPPORT_JPEG12)
#define MNG_SUPPORT_JPEG8
/* #define MNG_SUPPORT_JPEG12 */
#endif

/* ************************************************************************** */

/* enable required high-level functions */
/* use this to select the high-level functions you require */
/* if you only need to display a MNG, disable write support! */
/* if you only need to examine a MNG, disable write & display support! */
/* if you only need to copy a MNG, disable display support! */
/* if you only need to create a MNG, disable read & display support! */
/* NOTE that turning all options off will be very unuseful! */

#if !defined(MNG_SUPPORT_READ) && !defined(MNG_SUPPORT_WRITE) && !defined(MNG_SUPPORT_DISPLAY)
#define MNG_SUPPORT_READ
#if defined(MNG_BUILD_SO) || defined(MNG_USE_SO) || defined(MNG_BUILD_DLL) || defined(MNG_USE_DLL)
#define MNG_SUPPORT_WRITE
#endif
#define MNG_SUPPORT_DISPLAY
#endif

/* ************************************************************************** */

/* enable chunk access functions */
/* use this to select whether you need access to the individual chunks */
/* useful if you want to examine a read MNG (you'll also need MNG_STORE_CHUNKS !)*/
/* required if you need to create & write a new MNG! */

#ifndef MNG_ACCESS_CHUNKS
#if defined(MNG_BUILD_SO) || defined(MNG_USE_SO) || defined(MNG_BUILD_DLL) || defined(MNG_USE_DLL)
#define MNG_ACCESS_CHUNKS
#endif
#endif

/* ************************************************************************** */

/* enable exactly one of the color-management-functionality selectors */
/* use this to select the level of automatic color support */
/* MNG_FULL_CMS requires the lcms (little cms) external package ! */
/* if you want your own app (or the OS) to handle color-management
   select MNG_APP_CMS */

#if !defined(MNG_FULL_CMS) && !defined(MNG_GAMMA_ONLY) && !defined(MNG_NO_CMS) && !defined(MNG_APP_CMS)
#if defined(MNG_BUILD_DLL) || defined(MNG_USE_DLL)
#define MNG_FULL_CMS
#else
#define MNG_GAMMA_ONLY
#endif
/* #define MNG_NO_CMS */
/* #define MNG_APP_CMS */
#endif

/* ************************************************************************** */

/* enable automatic dithering */
/* use this if you need dithering support to convert high-resolution
   images to a low-resolution output-device */
/* NOTE that this is not supported yet */

/* #define MNG_AUTO_DITHER */

/* ************************************************************************** */

/* enable whether chunks should be stored for reference later */
/* use this if you need to examine the chunks of a MNG you have read,
   or (re-)write a MNG you have read */
/* turn this off if you want to reduce memory-consumption */

#ifndef MNG_STORE_CHUNKS
#if defined(MNG_BUILD_SO) || defined(MNG_USE_SO) || defined(MNG_BUILD_DLL) || defined(MNG_USE_DLL)
#define MNG_STORE_CHUNKS
#endif
#endif

/* ************************************************************************** */

/* enable internal memory management (if your compiler supports it) */
/* use this if your compiler supports the 'standard' memory functions
   (calloc & free), and you want the library to use these functions and not
   bother your app with memory-callbacks */

/* #define MNG_INTERNAL_MEMMNGMT */

/* ************************************************************************** */

/* enable internal tracing-functionality (manual debugging purposes) */
/* use this if you have trouble location bugs or problems */
/* NOTE that you'll need to specify the trace callback function! */

/* #define MNG_SUPPORT_TRACE */

/* ************************************************************************** */

/* enable extended error- and trace-telltaling */
/* use this if you need explanatory messages with errors and/or tracing */

#if !defined(MNG_ERROR_TELLTALE) && !defined(MNG_TRACE_TELLTALE)
#if defined(MNG_BUILD_SO) || defined(MNG_USE_SO) || defined(MNG_BUILD_DLL) || defined(MNG_USE_DLL)
#define MNG_ERROR_TELLTALE
#define MNG_TRACE_TELLTALE
#endif
#endif

/* ************************************************************************** */

/* enable big-endian support  */
/* enable this if you're on an architecture that supports big-endian reads
   and writes that aren't word-aligned */
/* according to reliable sources this only works for PowerPC (bigendian mode)
   and 680x0 */

/* #define MNG_BIGENDIAN_SUPPORTED */

/* ************************************************************************** */
/* *                                                                        * */
/* *  End of user-selectable compile-time options                           * */
/* *                                                                        * */
/* ************************************************************************** */

#endif /* _libmng_conf_h_ */

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */

