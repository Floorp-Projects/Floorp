/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __G_LIB_H__
#define __G_LIB_H__

/* system specific config file glibconfig.h provides definitions for
 * the extrema of many of the standard types. These are:
 *
 *  G_MINSHORT, G_MAXSHORT
 *  G_MININT, G_MAXINT
 *  G_MINLONG, G_MAXLONG
 *  G_MINFLOAT, G_MAXFLOAT
 *  G_MINDOUBLE, G_MAXDOUBLE
 *
 * It also provides the following typedefs:
 *
 *  gint8, guint8
 *  gint16, guint16
 *  gint32, guint32
 *  gint64, guint64
 *
 * It defines the G_BYTE_ORDER symbol to one of G_*_ENDIAN (see later in
 * this file). 
 *
 * And it provides a way to store and retrieve a `gint' in/from a `gpointer'.
 * This is useful to pass an integer instead of a pointer to a callback.
 *
 *  GINT_TO_POINTER(i), GUINT_TO_POINTER(i)
 *  GPOINTER_TO_INT(p), GPOINTER_TO_UINT(p)
 *
 * Finally, it provide the following wrappers to STDC functions:
 *
 *  g_ATEXIT
 *    To register hooks which are executed on exit().
 *    Usually a wrapper for STDC atexit.
 *
 *  void *g_memmove(void *dest, const void *src, guint count);
 *    A wrapper for STDC memmove, or an implementation, if memmove doesn't
 *    exist.  The prototype looks like the above, give or take a const,
 *    or size_t.
 */
#include <glibconfig.h>

/* include varargs functions for assertment macros
 */
#include <stdarg.h>

/* optionally feature DMALLOC memory allocation debugger
 */
#ifdef USE_DMALLOC
#include "dmalloc.h"
#endif


#ifdef XP_MAC

#define G_DIR_SEPARATOR ':'
#define G_DIR_SEPARATOR_S ":"
#define G_SEARCHPATH_SEPARATOR ';'				// XXX ??
#define G_SEARCHPATH_SEPARATOR_S ";"			// XXX ??

#elif defined(NATIVE_WIN32)

/* On native Win32, directory separator is the backslash, and search path
 * separator is the semicolon.
 */
#define G_DIR_SEPARATOR '\\'
#define G_DIR_SEPARATOR_S "\\"
#define G_SEARCHPATH_SEPARATOR ';'
#define G_SEARCHPATH_SEPARATOR_S ";"

#else  /* !NATIVE_WIN32 */

/* Unix */

#define G_DIR_SEPARATOR '/'
#define G_DIR_SEPARATOR_S "/"
#define G_SEARCHPATH_SEPARATOR ':'
#define G_SEARCHPATH_SEPARATOR_S ":"

#endif /* !NATIVE_WIN32 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* Provide definitions for some commonly used macros.
 *  Some of them are only provided if they haven't already
 *  been defined. It is assumed that if they are already
 *  defined then the current definition is correct.
 */
#ifndef	NULL
#define	NULL	((void*) 0)
#endif

#ifndef	FALSE
#define	FALSE	(0)
#endif

#ifndef	TRUE
#define	TRUE	(!FALSE)
#endif

#undef	MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

#undef	MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

#undef	ABS
#define ABS(a)	   (((a) < 0) ? -(a) : (a))

#undef	CLAMP
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))


/* Define G_VA_COPY() to do the right thing for copying va_list variables.
 * glibconfig.h may have already defined G_VA_COPY as va_copy or __va_copy.
 */
#if !defined (G_VA_COPY)
#  if defined (__GNUC__) && defined (__PPC__) && (defined (_CALL_SYSV) || defined (_WIN32))
#  define G_VA_COPY(ap1, ap2)	  (*(ap1) = *(ap2))
#  elif defined (G_VA_COPY_AS_ARRAY)
#  define G_VA_COPY(ap1, ap2)	  g_memmove ((ap1), (ap2), sizeof (va_list))
#  else /* va_list is a pointer */
#  define G_VA_COPY(ap1, ap2)	  ((ap1) = (ap2))
#  endif /* va_list is a pointer */
#endif /* !G_VA_COPY */


/* Provide convenience macros for handling structure
 * fields through their offsets.
 */
#define G_STRUCT_OFFSET(struct_type, member)	\
    ((gulong) ((gchar*) &((struct_type*) 0)->member))
#define G_STRUCT_MEMBER_P(struct_p, struct_offset)   \
    ((gpointer) ((gchar*) (struct_p) + (gulong) (struct_offset)))
#define G_STRUCT_MEMBER(member_type, struct_p, struct_offset)   \
    (*(member_type*) G_STRUCT_MEMBER_P ((struct_p), (struct_offset)))


/* inlining hassle. for compilers that don't allow the `inline' keyword,
 * mostly because of strict ANSI C compliance or dumbness, we try to fall
 * back to either `__inline__' or `__inline'.
 * we define G_CAN_INLINE, if the compiler seems to be actually
 * *capable* to do function inlining, in which case inline function bodys
 * do make sense. we also define G_INLINE_FUNC to properly export the
 * function prototypes if no inlining can be performed.
 * we special case most of the stuff, so inline functions can have a normal
 * implementation by defining G_INLINE_FUNC to extern and G_CAN_INLINE to 1.
 */
#ifndef G_INLINE_FUNC
#  define G_CAN_INLINE 1
#endif
#ifdef G_HAVE_INLINE
#  if defined (__GNUC__) && defined (__STRICT_ANSI__)
#    undef inline
#    define inline __inline__
#  endif
#else /* !G_HAVE_INLINE */
#  undef inline
#  if defined (G_HAVE___INLINE__)
#    define inline __inline__
#  else /* !inline && !__inline__ */
#    if defined (G_HAVE___INLINE)
#      define inline __inline
#    else /* !inline && !__inline__ && !__inline */
#      define inline /* don't inline, then */
#      ifndef G_INLINE_FUNC
#	 undef G_CAN_INLINE
#      endif
#    endif
#  endif
#endif
#ifndef G_INLINE_FUNC
#  ifdef __GNUC__
#    ifdef __OPTIMIZE__
#      define G_INLINE_FUNC extern inline
#    else
#      undef G_CAN_INLINE
#      define G_INLINE_FUNC extern
#    endif
#  else /* !__GNUC__ */
#    ifdef G_CAN_INLINE
#      define G_INLINE_FUNC static inline
#    else
#      define G_INLINE_FUNC extern
#    endif
#  endif /* !__GNUC__ */
#endif /* !G_INLINE_FUNC */


/* Provide simple macro statement wrappers (adapted from Perl):
 *  G_STMT_START { statements; } G_STMT_END;
 *  can be used as a single statement, as in
 *  if (x) G_STMT_START { ... } G_STMT_END; else ...
 *
 *  For gcc we will wrap the statements within `({' and `})' braces.
 *  For SunOS they will be wrapped within `if (1)' and `else (void) 0',
 *  and otherwise within `do' and `while (0)'.
 */
#if !(defined (G_STMT_START) && defined (G_STMT_END))
#  if defined (__GNUC__) && !defined (__STRICT_ANSI__) && !defined (__cplusplus)
#    define G_STMT_START	(void)(
#    define G_STMT_END		)
#  else
#    if (defined (sun) || defined (__sun__))
#      define G_STMT_START	if (1)
#      define G_STMT_END	else (void)0
#    else
#      define G_STMT_START	do
#      define G_STMT_END	while (0)
#    endif
#  endif
#endif


/* Provide macros to feature the GCC function attribute.
 */
#if	__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
#define G_GNUC_PRINTF( format_idx, arg_idx )	\
  __attribute__((format (printf, format_idx, arg_idx)))
#define G_GNUC_SCANF( format_idx, arg_idx )	\
  __attribute__((format (scanf, format_idx, arg_idx)))
#define G_GNUC_FORMAT( arg_idx )		\
  __attribute__((format_arg (arg_idx)))
#define G_GNUC_NORETURN				\
  __attribute__((noreturn))
#define G_GNUC_CONST				\
  __attribute__((const))
#define G_GNUC_UNUSED				\
  __attribute__((unused))
#else	/* !__GNUC__ */
#define G_GNUC_PRINTF( format_idx, arg_idx )
#define G_GNUC_SCANF( format_idx, arg_idx )
#define G_GNUC_FORMAT( arg_idx )
#define G_GNUC_NORETURN
#define G_GNUC_CONST
#define	G_GNUC_UNUSED
#endif	/* !__GNUC__ */


/* Wrap the gcc __PRETTY_FUNCTION__ and __FUNCTION__ variables with
 * macros, so we can refer to them as strings unconditionally.
 */
#ifdef	__GNUC__
#define	G_GNUC_FUNCTION		(__FUNCTION__)
#define	G_GNUC_PRETTY_FUNCTION	(__PRETTY_FUNCTION__)
#else	/* !__GNUC__ */
#define	G_GNUC_FUNCTION		("")
#define	G_GNUC_PRETTY_FUNCTION	("")
#endif	/* !__GNUC__ */

/* we try to provide a usefull equivalent for ATEXIT if it is
 * not defined, but use is actually abandoned. people should
 * use g_atexit() instead.
 */
#ifndef ATEXIT
# define ATEXIT(proc)	g_ATEXIT(proc)
#else
# define G_NATIVE_ATEXIT
#endif /* ATEXIT */

/* Hacker macro to place breakpoints for elected machines.
 * Actual use is strongly deprecated of course ;)
 */
#if defined (__i386__) && defined (__GNUC__) && __GNUC__ >= 2
#define	G_BREAKPOINT()		G_STMT_START{ __asm__ __volatile__ ("int $03"); }G_STMT_END
#elif defined (__alpha__) && defined (__GNUC__) && __GNUC__ >= 2
#define	G_BREAKPOINT()		G_STMT_START{ __asm__ __volatile__ ("bpt"); }G_STMT_END
#else	/* !__i386__ && !__alpha__ */
#define	G_BREAKPOINT()
#endif	/* __i386__ */


/* Provide macros for easily allocating memory. The macros
 *  will cast the allocated memory to the specified type
 *  in order to avoid compiler warnings. (Makes the code neater).
 */

#ifdef __DMALLOC_H__
#  define g_new(type, count)		(ALLOC (type, count))
#  define g_new0(type, count)		(CALLOC (type, count))
#  define g_renew(type, mem, count)	(REALLOC (mem, type, count))
#else /* __DMALLOC_H__ */
#  define g_new(type, count)	  \
      ((type *) g_malloc ((unsigned) sizeof (type) * (count)))
#  define g_new0(type, count)	  \
      ((type *) g_malloc0 ((unsigned) sizeof (type) * (count)))
#  define g_renew(type, mem, count)	  \
      ((type *) g_realloc (mem, (unsigned) sizeof (type) * (count)))
#endif /* __DMALLOC_H__ */

#define g_mem_chunk_create(type, pre_alloc, alloc_type)	( \
  g_mem_chunk_new (#type " mem chunks (" #pre_alloc ")", \
		   sizeof (type), \
		   sizeof (type) * (pre_alloc), \
		   (alloc_type)) \
)
#define g_chunk_new(type, chunk)	( \
  (type *) g_mem_chunk_alloc (chunk) \
)
#define g_chunk_new0(type, chunk)	( \
  (type *) g_mem_chunk_alloc0 (chunk) \
)
#define g_chunk_free(mem, mem_chunk)	G_STMT_START { \
  g_mem_chunk_free ((mem_chunk), (mem)); \
} G_STMT_END


#define g_string(x) #x


/* Provide macros for error handling. The "assert" macros will
 *  exit on failure. The "return" macros will exit the current
 *  function. Two different definitions are given for the macros
 *  if G_DISABLE_ASSERT is not defined, in order to support gcc's
 *  __PRETTY_FUNCTION__ capability.
 */

#ifdef G_DISABLE_ASSERT

#define g_assert(expr)
#define g_assert_not_reached()

#else /* !G_DISABLE_ASSERT */

#ifdef __GNUC__

#define g_assert(expr)			G_STMT_START{		\
     if (!(expr))						\
       g_log (G_LOG_DOMAIN,					\
	      G_LOG_LEVEL_ERROR,				\
	      "file %s: line %d (%s): assertion failed: (%s)",	\
	      __FILE__,						\
	      __LINE__,						\
	      __PRETTY_FUNCTION__,				\
	      #expr);			}G_STMT_END

#define g_assert_not_reached()		G_STMT_START{		\
     g_log (G_LOG_DOMAIN,					\
	    G_LOG_LEVEL_ERROR,					\
	    "file %s: line %d (%s): should not be reached",	\
	    __FILE__,						\
	    __LINE__,						\
	    __PRETTY_FUNCTION__);	}G_STMT_END

#else /* !__GNUC__ */

#define g_assert(expr)			G_STMT_START{		\
     if (!(expr))						\
       g_log (G_LOG_DOMAIN,					\
	      G_LOG_LEVEL_ERROR,				\
	      "file %s: line %d: assertion failed: (%s)",	\
	      __FILE__,						\
	      __LINE__,						\
	      #expr);			}G_STMT_END

#define g_assert_not_reached()		G_STMT_START{	\
     g_log (G_LOG_DOMAIN,				\
	    G_LOG_LEVEL_ERROR,				\
	    "file %s: line %d: should not be reached",	\
	    __FILE__,					\
	    __LINE__);		}G_STMT_END

#endif /* __GNUC__ */

#endif /* !G_DISABLE_ASSERT */


#ifdef G_DISABLE_CHECKS

#define g_return_if_fail(expr)
#define g_return_val_if_fail(expr,val)

#else /* !G_DISABLE_CHECKS */

#ifdef __GNUC__

#define g_return_if_fail(expr)		G_STMT_START{			\
     if (!(expr))							\
       {								\
	 g_log (G_LOG_DOMAIN,						\
		G_LOG_LEVEL_CRITICAL,					\
		"file %s: line %d (%s): assertion `%s' failed.",	\
		__FILE__,						\
		__LINE__,						\
		__PRETTY_FUNCTION__,					\
		#expr);							\
	 return;							\
       };				}G_STMT_END

#define g_return_val_if_fail(expr,val)	G_STMT_START{			\
     if (!(expr))							\
       {								\
	 g_log (G_LOG_DOMAIN,						\
		G_LOG_LEVEL_CRITICAL,					\
		"file %s: line %d (%s): assertion `%s' failed.",	\
		__FILE__,						\
		__LINE__,						\
		__PRETTY_FUNCTION__,					\
		#expr);							\
	 return val;							\
       };				}G_STMT_END

#else /* !__GNUC__ */

#define g_return_if_fail(expr)		G_STMT_START{		\
     if (!(expr))						\
       {							\
	 g_log (G_LOG_DOMAIN,					\
		G_LOG_LEVEL_CRITICAL,				\
		"file %s: line %d: assertion `%s' failed.",	\
		__FILE__,					\
		__LINE__,					\
		#expr);						\
	 return;						\
       };				}G_STMT_END

#define g_return_val_if_fail(expr, val)	G_STMT_START{		\
     if (!(expr))						\
       {							\
	 g_log (G_LOG_DOMAIN,					\
		G_LOG_LEVEL_CRITICAL,				\
		"file %s: line %d: assertion `%s' failed.",	\
		__FILE__,					\
		__LINE__,					\
		#expr);						\
	 return val;						\
       };				}G_STMT_END

#endif /* !__GNUC__ */

#endif /* !G_DISABLE_CHECKS */


/* Provide type definitions for commonly used types.
 *  These are useful because a "gint8" can be adjusted
 *  to be 1 byte (8 bits) on all platforms. Similarly and
 *  more importantly, "gint32" can be adjusted to be
 *  4 bytes (32 bits) on all platforms.
 */

typedef char   gchar;
typedef short  gshort;
typedef long   glong;
typedef int    gint;
typedef gint   gboolean;

typedef unsigned char	guchar;
typedef unsigned short	gushort;
typedef unsigned long	gulong;
typedef unsigned int	guint;

typedef float	gfloat;
typedef double	gdouble;

/* HAVE_LONG_DOUBLE doesn't work correctly on all platforms.
 * Since gldouble isn't used anywhere, just disable it for now */

#if 0
#ifdef HAVE_LONG_DOUBLE
typedef long double gldouble;
#else /* HAVE_LONG_DOUBLE */
typedef double gldouble;
#endif /* HAVE_LONG_DOUBLE */
#endif /* 0 */

typedef void* gpointer;
typedef const void *gconstpointer;


typedef gint32	gssize;
typedef guint32 gsize;
typedef guint32 GQuark;
typedef gint32	GTime;


/* Portable endian checks and conversions
 *
 * glibconfig.h defines G_BYTE_ORDER which expands to one of
 * the below macros.
 */
#define G_LITTLE_ENDIAN 1234
#define G_BIG_ENDIAN    4321
#define G_PDP_ENDIAN    3412		/* unused, need specific PDP check */	


/* Basic bit swapping functions
 */
#define GUINT16_SWAP_LE_BE_CONSTANT(val)	((guint16) ( \
    (((guint16) (val) & (guint16) 0x00ffU) << 8) | \
    (((guint16) (val) & (guint16) 0xff00U) >> 8)))
#define GUINT32_SWAP_LE_BE_CONSTANT(val)	((guint32) ( \
    (((guint32) (val) & (guint32) 0x000000ffU) << 24) | \
    (((guint32) (val) & (guint32) 0x0000ff00U) <<  8) | \
    (((guint32) (val) & (guint32) 0x00ff0000U) >>  8) | \
    (((guint32) (val) & (guint32) 0xff000000U) >> 24)))

/* Intel specific stuff for speed
 */
#if defined (__i386__) && defined (__GNUC__) && __GNUC__ >= 2
#  define GUINT16_SWAP_LE_BE_X86(val) \
     (__extension__					\
      ({ register guint16 __v;				\
	 if (__builtin_constant_p (val))		\
	   __v = GUINT16_SWAP_LE_BE_CONSTANT (val);	\
	 else						\
	   __asm__ __const__ ("rorw $8, %w0"		\
			      : "=r" (__v)		\
			      : "0" ((guint16) (val)));	\
	__v; }))
#  define GUINT16_SWAP_LE_BE(val) (GUINT16_SWAP_LE_BE_X86 (val))
#  if !defined(__i486__) && !defined(__i586__) \
      && !defined(__pentium__) && !defined(__i686__) && !defined(__pentiumpro__)
#     define GUINT32_SWAP_LE_BE_X86(val) \
        (__extension__						\
         ({ register guint32 __v;				\
	    if (__builtin_constant_p (val))			\
	      __v = GUINT32_SWAP_LE_BE_CONSTANT (val);		\
	  else							\
	    __asm__ __const__ ("rorw $8, %w0\n\t"		\
			       "rorl $16, %0\n\t"		\
			       "rorw $8, %w0"			\
			       : "=r" (__v)			\
			       : "0" ((guint32) (val)));	\
	__v; }))
#  else /* 486 and higher has bswap */
#     define GUINT32_SWAP_LE_BE_X86(val) \
        (__extension__						\
         ({ register guint32 __v;				\
	    if (__builtin_constant_p (val))			\
	      __v = GUINT32_SWAP_LE_BE_CONSTANT (val);		\
	  else							\
	    __asm__ __const__ ("bswap %0"			\
			       : "=r" (__v)			\
			       : "0" ((guint32) (val)));	\
	__v; }))
#  endif /* processor specific 32-bit stuff */
#  define GUINT32_SWAP_LE_BE(val) (GUINT32_SWAP_LE_BE_X86 (val))
#else /* !__i386__ */
#  define GUINT16_SWAP_LE_BE(val) (GUINT16_SWAP_LE_BE_CONSTANT (val))
#  define GUINT32_SWAP_LE_BE(val) (GUINT32_SWAP_LE_BE_CONSTANT (val))
#endif /* __i386__ */

#ifdef G_HAVE_GINT64
#  define GUINT64_SWAP_LE_BE_CONSTANT(val)	((guint64) ( \
      (((guint64) (val) &						\
	(guint64) G_GINT64_CONSTANT(0x00000000000000ffU)) << 56) |	\
      (((guint64) (val) &						\
	(guint64) G_GINT64_CONSTANT(0x000000000000ff00U)) << 40) |	\
      (((guint64) (val) &						\
	(guint64) G_GINT64_CONSTANT(0x0000000000ff0000U)) << 24) |	\
      (((guint64) (val) &						\
	(guint64) G_GINT64_CONSTANT(0x00000000ff000000U)) <<  8) |	\
      (((guint64) (val) &						\
	(guint64) G_GINT64_CONSTANT(0x000000ff00000000U)) >>  8) |	\
      (((guint64) (val) &						\
	(guint64) G_GINT64_CONSTANT(0x0000ff0000000000U)) >> 24) |	\
      (((guint64) (val) &						\
	(guint64) G_GINT64_CONSTANT(0x00ff000000000000U)) >> 40) |	\
      (((guint64) (val) &						\
	(guint64) G_GINT64_CONSTANT(0xff00000000000000U)) >> 56)))
#  if defined (__i386__) && defined (__GNUC__) && __GNUC__ >= 2
#    define GUINT64_SWAP_LE_BE_X86(val) \
	(__extension__						\
	 ({ union { guint64 __ll;				\
		    guint32 __l[2]; } __r;			\
	    if (__builtin_constant_p (val))			\
	      __r.__ll = GUINT64_SWAP_LE_BE_CONSTANT (val);	\
	    else						\
	      {							\
	 	union { guint64 __ll;				\
			guint32 __l[2]; } __w;			\
		__w.__ll = ((guint64) val);			\
		__r.__l[0] = GUINT32_SWAP_LE_BE (__w.__l[1]);	\
		__r.__l[1] = GUINT32_SWAP_LE_BE (__w.__l[0]);	\
	      }							\
	  __r.__ll; }))
#    define GUINT64_SWAP_LE_BE(val) (GUINT64_SWAP_LE_BE_X86 (val))
#  else /* !__i386__ */
#    define GUINT64_SWAP_LE_BE(val) (GUINT64_SWAP_LE_BE_CONSTANT(val))
#  endif
#endif

#define GUINT16_SWAP_LE_PDP(val)	((guint16) (val))
#define GUINT16_SWAP_BE_PDP(val)	(GUINT16_SWAP_LE_BE (val))
#define GUINT32_SWAP_LE_PDP(val)	((guint32) ( \
    (((guint32) (val) & (guint32) 0x0000ffffU) << 16) | \
    (((guint32) (val) & (guint32) 0xffff0000U) >> 16)))
#define GUINT32_SWAP_BE_PDP(val)	((guint32) ( \
    (((guint32) (val) & (guint32) 0x00ff00ffU) << 8) | \
    (((guint32) (val) & (guint32) 0xff00ff00U) >> 8)))

/* The G*_TO_?E() macros are defined in glibconfig.h.
 * The transformation is symmetric, so the FROM just maps to the TO.
 */
#define GINT16_FROM_LE(val)	(GINT16_TO_LE (val))
#define GUINT16_FROM_LE(val)	(GUINT16_TO_LE (val))
#define GINT16_FROM_BE(val)	(GINT16_TO_BE (val))
#define GUINT16_FROM_BE(val)	(GUINT16_TO_BE (val))
#define GINT32_FROM_LE(val)	(GINT32_TO_LE (val))
#define GUINT32_FROM_LE(val)	(GUINT32_TO_LE (val))
#define GINT32_FROM_BE(val)	(GINT32_TO_BE (val))
#define GUINT32_FROM_BE(val)	(GUINT32_TO_BE (val))

#ifdef G_HAVE_GINT64
#define GINT64_FROM_LE(val)	(GINT64_TO_LE (val))
#define GUINT64_FROM_LE(val)	(GUINT64_TO_LE (val))
#define GINT64_FROM_BE(val)	(GINT64_TO_BE (val))
#define GUINT64_FROM_BE(val)	(GUINT64_TO_BE (val))
#endif

#define GLONG_FROM_LE(val)	(GLONG_TO_LE (val))
#define GULONG_FROM_LE(val)	(GULONG_TO_LE (val))
#define GLONG_FROM_BE(val)	(GLONG_TO_BE (val))
#define GULONG_FROM_BE(val)	(GULONG_TO_BE (val))

#define GINT_FROM_LE(val)	(GINT_TO_LE (val))
#define GUINT_FROM_LE(val)	(GUINT_TO_LE (val))
#define GINT_FROM_BE(val)	(GINT_TO_BE (val))
#define GUINT_FROM_BE(val)	(GUINT_TO_BE (val))


/* Portable versions of host-network order stuff
 */
#define g_ntohl(val) (GUINT32_FROM_BE (val))
#define g_ntohs(val) (GUINT16_FROM_BE (val))
#define g_htonl(val) (GUINT32_TO_BE (val))
#define g_htons(val) (GUINT16_TO_BE (val))


/* Glib version.
 * we prefix variable declarations so they can
 * properly get exported in windows dlls.
 */
#ifdef NATIVE_WIN32
#  ifdef GLIB_COMPILATION
#    define GUTILS_C_VAR __declspec(dllexport)
#  else /* !GLIB_COMPILATION */
#    define GUTILS_C_VAR extern __declspec(dllimport)
#  endif /* !GLIB_COMPILATION */
#else /* !NATIVE_WIN32 */
#  define GUTILS_C_VAR extern
#endif /* !NATIVE_WIN32 */

GUTILS_C_VAR const guint glib_major_version;
GUTILS_C_VAR const guint glib_minor_version;
GUTILS_C_VAR const guint glib_micro_version;
GUTILS_C_VAR const guint glib_interface_age;
GUTILS_C_VAR const guint glib_binary_age;

#define GLIB_CHECK_VERSION(major,minor,micro)    \
    (GLIB_MAJOR_VERSION > (major) || \
     (GLIB_MAJOR_VERSION == (major) && GLIB_MINOR_VERSION > (minor)) || \
     (GLIB_MAJOR_VERSION == (major) && GLIB_MINOR_VERSION == (minor) && \
      GLIB_MICRO_VERSION >= (micro)))

/* Forward declarations of glib types.
 */
typedef struct _GAllocator	GAllocator;
typedef struct _GArray		GArray;
typedef struct _GByteArray	GByteArray;
typedef struct _GCache		GCache;
typedef struct _GCompletion	GCompletion;
typedef	struct _GData		GData;
typedef struct _GDebugKey	GDebugKey;
typedef struct _GHashTable	GHashTable;
typedef struct _GHook		GHook;
typedef struct _GHookList	GHookList;
typedef struct _GList		GList;
typedef struct _GMemChunk	GMemChunk;
typedef struct _GNode		GNode;
typedef struct _GPtrArray	GPtrArray;
typedef struct _GRelation	GRelation;
typedef struct _GScanner	GScanner;
typedef struct _GScannerConfig	GScannerConfig;
typedef struct _GSList		GSList;
typedef struct _GString		GString;
typedef struct _GStringChunk	GStringChunk;
typedef struct _GTimer		GTimer;
typedef struct _GTree		GTree;
typedef struct _GTuples		GTuples;
typedef union  _GTokenValue	GTokenValue;
typedef struct _GIOChannel	GIOChannel;

typedef enum
{
  G_TRAVERSE_LEAFS	= 1 << 0,
  G_TRAVERSE_NON_LEAFS	= 1 << 1,
  G_TRAVERSE_ALL	= G_TRAVERSE_LEAFS | G_TRAVERSE_NON_LEAFS,
  G_TRAVERSE_MASK	= 0x03
} GTraverseFlags;

/* XP_MAC I hacked this enum to compile. */
typedef enum
{
  G_IN_ORDER,
  G_PRE_ORDER,
  G_POST_ORDER,
  G_LEVEL_ORDER
} GTraverseType;

/* Log level shift offset for user defined
 * log levels (0-7 are used by GLib).
 */
#define	G_LOG_LEVEL_USER_SHIFT	(8)

/* Glib log levels and flags.
 */
typedef enum
{
  /* log flags */
  G_LOG_FLAG_RECURSION		= 1 << 0,
  G_LOG_FLAG_FATAL		= 1 << 1,
  
  /* GLib log levels */
  G_LOG_LEVEL_ERROR		= 1 << 2,	/* always fatal */
  G_LOG_LEVEL_CRITICAL		= 1 << 3,
  G_LOG_LEVEL_WARNING		= 1 << 4,
  G_LOG_LEVEL_MESSAGE		= 1 << 5,
  G_LOG_LEVEL_INFO		= 1 << 6,
  G_LOG_LEVEL_DEBUG		= 1 << 7,
  
  G_LOG_LEVEL_MASK		= ~(G_LOG_FLAG_RECURSION | G_LOG_FLAG_FATAL)
} GLogLevelFlags;

/* GLib log levels that are considered fatal by default */
#define	G_LOG_FATAL_MASK	(G_LOG_FLAG_RECURSION | G_LOG_LEVEL_ERROR)


typedef gpointer	(*GCacheNewFunc)	(gpointer	key);
typedef gpointer	(*GCacheDupFunc)	(gpointer	value);
typedef void		(*GCacheDestroyFunc)	(gpointer	value);
typedef gint		(*GCompareFunc)		(gconstpointer	a,
						 gconstpointer	b);
typedef gchar*		(*GCompletionFunc)	(gpointer);
typedef void		(*GDestroyNotify)	(gpointer	data);
typedef void		(*GDataForeachFunc)	(GQuark		key_id,
						 gpointer	data,
						 gpointer	user_data);
typedef void		(*GFunc)		(gpointer	data,
						 gpointer	user_data);
typedef guint		(*GHashFunc)		(gconstpointer	key);
typedef void		(*GFreeFunc)		(gpointer	data);
typedef void		(*GHFunc)		(gpointer	key,
						 gpointer	value,
						 gpointer	user_data);
typedef gboolean	(*GHRFunc)		(gpointer	key,
						 gpointer	value,
						 gpointer	user_data);
typedef gint		(*GHookCompareFunc)	(GHook		*new_hook,
						 GHook		*sibling);
typedef gboolean	(*GHookFindFunc)	(GHook		*hook,
						 gpointer	 data);
typedef void		(*GHookMarshaller)	(GHook		*hook,
						 gpointer	 data);
typedef gboolean	(*GHookCheckMarshaller)	(GHook		*hook,
						 gpointer	 data);
typedef void		(*GHookFunc)		(gpointer	 data);
typedef gboolean	(*GHookCheckFunc)	(gpointer	 data);
typedef void		(*GHookFreeFunc)	(GHookList      *hook_list,
						 GHook          *hook);
typedef void		(*GLogFunc)		(const gchar   *log_domain,
						 GLogLevelFlags	log_level,
						 const gchar   *message,
						 gpointer	user_data);
typedef gboolean	(*GNodeTraverseFunc)	(GNode	       *node,
						 gpointer	data);
typedef void		(*GNodeForeachFunc)	(GNode	       *node,
						 gpointer	data);
typedef gint		(*GSearchFunc)		(gpointer	key,
						 gpointer	data);
typedef void		(*GScannerMsgFunc)	(GScanner      *scanner,
						 gchar	       *message,
						 gint		error);
typedef gint		(*GTraverseFunc)	(gpointer	key,
						 gpointer	value,
						 gpointer	data);
typedef	void		(*GVoidFunc)		(void);


struct _GList
{
  gpointer data;
  GList *next;
  GList *prev;
};

struct _GSList
{
  gpointer data;
  GSList *next;
};

struct _GString
{
  gchar *str;
  gint len;
};

struct _GArray
{
  gchar *data;
  guint len;
};

struct _GByteArray
{
  guint8 *data;
  guint	  len;
};

struct _GPtrArray
{
  gpointer *pdata;
  guint	    len;
};

struct _GTuples
{
  guint len;
};

struct _GDebugKey
{
  gchar *key;
  guint	 value;
};


/* Doubly linked lists
 */
void   g_list_push_allocator    (GAllocator     *allocator);
void   g_list_pop_allocator     (void);
GList* g_list_alloc		(void);
void   g_list_free		(GList		*list);
void   g_list_free_1		(GList		*list);
GList* g_list_append		(GList		*list,
				 gpointer	 data);
GList* g_list_prepend		(GList		*list,
				 gpointer	 data);
GList* g_list_insert		(GList		*list,
				 gpointer	 data,
				 gint		 position);
GList* g_list_insert_sorted	(GList		*list,
				 gpointer	 data,
				 GCompareFunc	 func);
GList* g_list_concat		(GList		*list1,
				 GList		*list2);
GList* g_list_remove		(GList		*list,
				 gpointer	 data);
GList* g_list_remove_link	(GList		*list,
				 GList		*llink);
GList* g_list_reverse		(GList		*list);
GList* g_list_copy		(GList		*list);
GList* g_list_nth		(GList		*list,
				 guint		 n);
GList* g_list_find		(GList		*list,
				 gpointer	 data);
GList* g_list_find_custom	(GList		*list,
				 gpointer	 data,
				 GCompareFunc	 func);
gint   g_list_position		(GList		*list,
				 GList		*llink);
gint   g_list_index		(GList		*list,
				 gpointer	 data);
GList* g_list_last		(GList		*list);
GList* g_list_first		(GList		*list);
guint  g_list_length		(GList		*list);
void   g_list_foreach		(GList		*list,
				 GFunc		 func,
				 gpointer	 user_data);
GList* g_list_sort              (GList          *list,
		                 GCompareFunc    compare_func);
gpointer g_list_nth_data	(GList		*list,
				 guint		 n);
#define g_list_previous(list)	((list) ? (((GList *)(list))->prev) : NULL)
#define g_list_next(list)	((list) ? (((GList *)(list))->next) : NULL)


/* Singly linked lists
 */
void    g_slist_push_allocator  (GAllocator     *allocator);
void    g_slist_pop_allocator   (void);
GSList* g_slist_alloc		(void);
void	g_slist_free		(GSList		*list);
void	g_slist_free_1		(GSList		*list);
GSList* g_slist_append		(GSList		*list,
				 gpointer	 data);
GSList* g_slist_prepend		(GSList		*list,
				 gpointer	 data);
GSList* g_slist_insert		(GSList		*list,
				 gpointer	 data,
				 gint		 position);
GSList* g_slist_insert_sorted	(GSList		*list,
				 gpointer	 data,
				 GCompareFunc	 func);
GSList* g_slist_concat		(GSList		*list1,
				 GSList		*list2);
GSList* g_slist_remove		(GSList		*list,
				 gpointer	 data);
GSList* g_slist_remove_link	(GSList		*list,
				 GSList		*llink);
GSList* g_slist_reverse		(GSList		*list);
GSList*	g_slist_copy		(GSList		*list);
GSList* g_slist_nth		(GSList		*list,
				 guint		 n);
GSList* g_slist_find		(GSList		*list,
				 gpointer	 data);
GSList* g_slist_find_custom	(GSList		*list,
				 gpointer	 data,
				 GCompareFunc	 func);
gint	g_slist_position	(GSList		*list,
				 GSList		*llink);
gint	g_slist_index		(GSList		*list,
				 gpointer	 data);
GSList* g_slist_last		(GSList		*list);
guint	g_slist_length		(GSList		*list);
void	g_slist_foreach		(GSList		*list,
				 GFunc		 func,
				 gpointer	 user_data);
GSList*  g_slist_sort           (GSList          *list,
		                 GCompareFunc    compare_func);
gpointer g_slist_nth_data	(GSList		*list,
				 guint		 n);
#define g_slist_next(slist)	((slist) ? (((GSList *)(slist))->next) : NULL)


/* Hash tables
 */
GHashTable* g_hash_table_new		(GHashFunc	 hash_func,
					 GCompareFunc	 key_compare_func);
void	    g_hash_table_destroy	(GHashTable	*hash_table);
void	    g_hash_table_insert		(GHashTable	*hash_table,
					 gpointer	 key,
					 gpointer	 value);
void	    g_hash_table_remove		(GHashTable	*hash_table,
					 gconstpointer	 key);
gpointer    g_hash_table_lookup		(GHashTable	*hash_table,
					 gconstpointer	 key);
gboolean    g_hash_table_lookup_extended(GHashTable	*hash_table,
					 gconstpointer	 lookup_key,
					 gpointer	*orig_key,
					 gpointer	*value);
void	    g_hash_table_freeze		(GHashTable	*hash_table);
void	    g_hash_table_thaw		(GHashTable	*hash_table);
void	    g_hash_table_foreach	(GHashTable	*hash_table,
					 GHFunc		 func,
					 gpointer	 user_data);
guint	    g_hash_table_foreach_remove	(GHashTable	*hash_table,
					 GHRFunc	 func,
					 gpointer	 user_data);
guint	    g_hash_table_size		(GHashTable	*hash_table);


/* Caches
 */
GCache*	 g_cache_new	       (GCacheNewFunc	   value_new_func,
				GCacheDestroyFunc  value_destroy_func,
				GCacheDupFunc	   key_dup_func,
				GCacheDestroyFunc  key_destroy_func,
				GHashFunc	   hash_key_func,
				GHashFunc	   hash_value_func,
				GCompareFunc	   key_compare_func);
void	 g_cache_destroy       (GCache		  *cache);
gpointer g_cache_insert	       (GCache		  *cache,
				gpointer	   key);
void	 g_cache_remove	       (GCache		  *cache,
				gpointer	   value);
void	 g_cache_key_foreach   (GCache		  *cache,
				GHFunc		   func,
				gpointer	   user_data);
void	 g_cache_value_foreach (GCache		  *cache,
				GHFunc		   func,
				gpointer	   user_data);


/* Balanced binary trees
 */
GTree*	 g_tree_new	 (GCompareFunc	 key_compare_func);
void	 g_tree_destroy	 (GTree		*tree);
void	 g_tree_insert	 (GTree		*tree,
			  gpointer	 key,
			  gpointer	 value);
void	 g_tree_remove	 (GTree		*tree,
			  gpointer	 key);
gpointer g_tree_lookup	 (GTree		*tree,
			  gpointer	 key);
void	 g_tree_traverse (GTree		*tree,
			  GTraverseFunc	 traverse_func,
			  GTraverseType	 traverse_type,
			  gpointer	 data);
gpointer g_tree_search	 (GTree		*tree,
			  GSearchFunc	 search_func,
			  gpointer	 data);
gint	 g_tree_height	 (GTree		*tree);
gint	 g_tree_nnodes	 (GTree		*tree);



/* N-way tree implementation
 */
struct _GNode
{
  gpointer data;
  GNode	  *next;
  GNode	  *prev;
  GNode	  *parent;
  GNode	  *children;
};

#define	 G_NODE_IS_ROOT(node)	(((GNode*) (node))->parent == NULL && \
				 ((GNode*) (node))->prev == NULL && \
				 ((GNode*) (node))->next == NULL)
#define	 G_NODE_IS_LEAF(node)	(((GNode*) (node))->children == NULL)

void     g_node_push_allocator  (GAllocator       *allocator);
void     g_node_pop_allocator   (void);
GNode*	 g_node_new		(gpointer	   data);
void	 g_node_destroy		(GNode		  *root);
void	 g_node_unlink		(GNode		  *node);
GNode*	 g_node_insert		(GNode		  *parent,
				 gint		   position,
				 GNode		  *node);
GNode*	 g_node_insert_before	(GNode		  *parent,
				 GNode		  *sibling,
				 GNode		  *node);
GNode*	 g_node_prepend		(GNode		  *parent,
				 GNode		  *node);
guint	 g_node_n_nodes		(GNode		  *root,
				 GTraverseFlags	   flags);
GNode*	 g_node_get_root	(GNode		  *node);
gboolean g_node_is_ancestor	(GNode		  *node,
				 GNode		  *descendant);
guint	 g_node_depth		(GNode		  *node);
GNode*	 g_node_find		(GNode		  *root,
				 GTraverseType	   order,
				 GTraverseFlags	   flags,
				 gpointer	   data);

/* convenience macros */
#define g_node_append(parent, node)				\
     g_node_insert_before ((parent), NULL, (node))
#define	g_node_insert_data(parent, position, data)		\
     g_node_insert ((parent), (position), g_node_new (data))
#define	g_node_insert_data_before(parent, sibling, data)	\
     g_node_insert_before ((parent), (sibling), g_node_new (data))
#define	g_node_prepend_data(parent, data)			\
     g_node_prepend ((parent), g_node_new (data))
#define	g_node_append_data(parent, data)			\
     g_node_insert_before ((parent), NULL, g_node_new (data))

/* traversal function, assumes that `node' is root
 * (only traverses `node' and its subtree).
 * this function is just a high level interface to
 * low level traversal functions, optimized for speed.
 */
void	 g_node_traverse	(GNode		  *root,
				 GTraverseType	   order,
				 GTraverseFlags	   flags,
				 gint		   max_depth,
				 GNodeTraverseFunc func,
				 gpointer	   data);

/* return the maximum tree height starting with `node', this is an expensive
 * operation, since we need to visit all nodes. this could be shortened by
 * adding `guint height' to struct _GNode, but then again, this is not very
 * often needed, and would make g_node_insert() more time consuming.
 */
guint	 g_node_max_height	 (GNode *root);

void	 g_node_children_foreach (GNode		  *node,
				  GTraverseFlags   flags,
				  GNodeForeachFunc func,
				  gpointer	   data);
void	 g_node_reverse_children (GNode		  *node);
guint	 g_node_n_children	 (GNode		  *node);
GNode*	 g_node_nth_child	 (GNode		  *node,
				  guint		   n);
GNode*	 g_node_last_child	 (GNode		  *node);
GNode*	 g_node_find_child	 (GNode		  *node,
				  GTraverseFlags   flags,
				  gpointer	   data);
gint	 g_node_child_position	 (GNode		  *node,
				  GNode		  *child);
gint	 g_node_child_index	 (GNode		  *node,
				  gpointer	   data);

GNode*	 g_node_first_sibling	 (GNode		  *node);
GNode*	 g_node_last_sibling	 (GNode		  *node);

#define	 g_node_prev_sibling(node)	((node) ? \
					 ((GNode*) (node))->prev : NULL)
#define	 g_node_next_sibling(node)	((node) ? \
					 ((GNode*) (node))->next : NULL)
#define	 g_node_first_child(node)	((node) ? \
					 ((GNode*) (node))->children : NULL)


/* Callback maintenance functions
 */
#define G_HOOK_FLAG_USER_SHIFT	(4)
typedef enum
{
  G_HOOK_FLAG_ACTIVE	= 1 << 0,
  G_HOOK_FLAG_IN_CALL	= 1 << 1,
  G_HOOK_FLAG_MASK	= 0x0f
} GHookFlagMask;

#define	G_HOOK_DEFERRED_DESTROY	((GHookFreeFunc) 0x01)

struct _GHookList
{
  guint		 seq_id;
  guint		 hook_size;
  guint		 is_setup : 1;
  GHook		*hooks;
  GMemChunk	*hook_memchunk;
  GHookFreeFunc	 hook_free; /* virtual function */
  GHookFreeFunc	 hook_destroy; /* virtual function */
};

struct _GHook
{
  gpointer	 data;
  GHook		*next;
  GHook		*prev;
  guint		 ref_count;
  guint		 hook_id;
  guint		 flags;
  gpointer	 func;
  GDestroyNotify destroy;
};

#define	G_HOOK_ACTIVE(hook)		((((GHook*) hook)->flags & \
					  G_HOOK_FLAG_ACTIVE) != 0)
#define	G_HOOK_IN_CALL(hook)		((((GHook*) hook)->flags & \
					  G_HOOK_FLAG_IN_CALL) != 0)
#define G_HOOK_IS_VALID(hook)		(((GHook*) hook)->hook_id != 0 && \
					 G_HOOK_ACTIVE (hook))
#define G_HOOK_IS_UNLINKED(hook)	(((GHook*) hook)->next == NULL && \
					 ((GHook*) hook)->prev == NULL && \
					 ((GHook*) hook)->hook_id == 0 && \
					 ((GHook*) hook)->ref_count == 0)

void	 g_hook_list_init		(GHookList		*hook_list,
					 guint			 hook_size);
void	 g_hook_list_clear		(GHookList		*hook_list);
GHook*	 g_hook_alloc			(GHookList		*hook_list);
void	 g_hook_free			(GHookList		*hook_list,
					 GHook			*hook);
void	 g_hook_ref			(GHookList		*hook_list,
					 GHook			*hook);
void	 g_hook_unref			(GHookList		*hook_list,
					 GHook			*hook);
gboolean g_hook_destroy			(GHookList		*hook_list,
					 guint			 hook_id);
void	 g_hook_destroy_link		(GHookList		*hook_list,
					 GHook			*hook);
void	 g_hook_prepend			(GHookList		*hook_list,
					 GHook			*hook);
void	 g_hook_insert_before		(GHookList		*hook_list,
					 GHook			*sibling,
					 GHook			*hook);
void	 g_hook_insert_sorted		(GHookList		*hook_list,
					 GHook			*hook,
					 GHookCompareFunc	 func);
GHook*	 g_hook_get			(GHookList		*hook_list,
					 guint			 hook_id);
GHook*	 g_hook_find			(GHookList		*hook_list,
					 gboolean		 need_valids,
					 GHookFindFunc		 func,
					 gpointer		 data);
GHook*	 g_hook_find_data		(GHookList		*hook_list,
					 gboolean		 need_valids,
					 gpointer		 data);
GHook*	 g_hook_find_func		(GHookList		*hook_list,
					 gboolean		 need_valids,
					 gpointer		 func);
GHook*	 g_hook_find_func_data		(GHookList		*hook_list,
					 gboolean		 need_valids,
					 gpointer		 func,
					 gpointer		 data);
/* return the first valid hook, and increment its reference count */
GHook*	 g_hook_first_valid		(GHookList		*hook_list,
					 gboolean		 may_be_in_call);
/* return the next valid hook with incremented reference count, and
 * decrement the reference count of the original hook
 */
GHook*	 g_hook_next_valid		(GHookList		*hook_list,
					 GHook			*hook,
					 gboolean		 may_be_in_call);

/* GHookCompareFunc implementation to insert hooks sorted by their id */
gint	 g_hook_compare_ids		(GHook			*new_hook,
					 GHook			*sibling);

/* convenience macros */
#define	 g_hook_append( hook_list, hook )  \
     g_hook_insert_before ((hook_list), NULL, (hook))

/* invoke all valid hooks with the (*GHookFunc) signature.
 */
void	 g_hook_list_invoke		(GHookList		*hook_list,
					 gboolean		 may_recurse);
/* invoke all valid hooks with the (*GHookCheckFunc) signature,
 * and destroy the hook if FALSE is returned.
 */
void	 g_hook_list_invoke_check	(GHookList		*hook_list,
					 gboolean		 may_recurse);
/* invoke a marshaller on all valid hooks.
 */
void	 g_hook_list_marshal		(GHookList		*hook_list,
					 gboolean		 may_recurse,
					 GHookMarshaller	 marshaller,
					 gpointer		 data);
void	 g_hook_list_marshal_check	(GHookList		*hook_list,
					 gboolean		 may_recurse,
					 GHookCheckMarshaller	 marshaller,
					 gpointer		 data);


/* Fatal error handlers.
 * g_on_error_query() will prompt the user to either
 * [E]xit, [H]alt, [P]roceed or show [S]tack trace.
 * g_on_error_stack_trace() invokes gdb, which attaches to the current
 * process and shows a stack trace.
 * These function may cause different actions on non-unix platforms.
 * The prg_name arg is required by gdb to find the executable, if it is
 * passed as NULL, g_on_error_query() will try g_get_prgname().
 */
void g_on_error_query (const gchar *prg_name);
void g_on_error_stack_trace (const gchar *prg_name);


/* Logging mechanism
 */
extern	        const gchar		*g_log_domain_glib;
guint		g_log_set_handler	(const gchar	*log_domain,
					 GLogLevelFlags	 log_levels,
					 GLogFunc	 log_func,
					 gpointer	 user_data);
void		g_log_remove_handler	(const gchar	*log_domain,
					 guint		 handler_id);
void		g_log_default_handler	(const gchar	*log_domain,
					 GLogLevelFlags	 log_level,
					 const gchar	*message,
					 gpointer	 unused_data);
void		g_log			(const gchar	*log_domain,
					 GLogLevelFlags	 log_level,
					 const gchar	*format,
					 ...) G_GNUC_PRINTF (3, 4);
void		g_logv			(const gchar	*log_domain,
					 GLogLevelFlags	 log_level,
					 const gchar	*format,
					 va_list	 args);
GLogLevelFlags	g_log_set_fatal_mask	(const gchar	*log_domain,
					 GLogLevelFlags	 fatal_mask);
GLogLevelFlags	g_log_set_always_fatal	(GLogLevelFlags	 fatal_mask);
#ifndef	G_LOG_DOMAIN
#define	G_LOG_DOMAIN	((gchar*) 0)
#endif	/* G_LOG_DOMAIN */
#ifdef	__GNUC__
#define	g_error(format, args...)	g_log (G_LOG_DOMAIN, \
					       G_LOG_LEVEL_ERROR, \
					       format, ##args)
#define	g_message(format, args...)	g_log (G_LOG_DOMAIN, \
					       G_LOG_LEVEL_MESSAGE, \
					       format, ##args)
#define	g_warning(format, args...)	g_log (G_LOG_DOMAIN, \
					       G_LOG_LEVEL_WARNING, \
					       format, ##args)
#else	/* !__GNUC__ */
static void
g_error (const gchar *format,
	 ...)
{
  va_list args;
  va_start (args, format);
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, format, args);
  va_end (args);
}
static void
g_message (const gchar *format,
	   ...)
{
  va_list args;
  va_start (args, format);
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, format, args);
  va_end (args);
}
static void
g_warning (const gchar *format,
	   ...)
{
  va_list args;
  va_start (args, format);
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, format, args);
  va_end (args);
}
#endif	/* !__GNUC__ */

typedef void	(*GPrintFunc)		(const gchar	*string);
void		g_print			(const gchar	*format,
					 ...) G_GNUC_PRINTF (1, 2);
GPrintFunc	g_set_print_handler	(GPrintFunc	 func);
void		g_printerr		(const gchar	*format,
					 ...) G_GNUC_PRINTF (1, 2);
GPrintFunc	g_set_printerr_handler	(GPrintFunc	 func);

/* deprecated compatibility functions, use g_log_set_handler() instead */
typedef void		(*GErrorFunc)		(const gchar *str);
typedef void		(*GWarningFunc)		(const gchar *str);
GErrorFunc   g_set_error_handler   (GErrorFunc	 func);
GWarningFunc g_set_warning_handler (GWarningFunc func);
GPrintFunc   g_set_message_handler (GPrintFunc func);


/* Memory allocation and debugging
 */
#ifdef USE_DMALLOC

#define g_malloc(size)	     ((gpointer) MALLOC (size))
#define g_malloc0(size)	     ((gpointer) CALLOC (char, size))
#define g_realloc(mem,size)  ((gpointer) REALLOC (mem, char, size))
#define g_free(mem)	     FREE (mem)

#else /* !USE_DMALLOC */

gpointer g_malloc      (gulong	  size);
gpointer g_malloc0     (gulong	  size);
gpointer g_realloc     (gpointer  mem,
			gulong	  size);
void	 g_free	       (gpointer  mem);

#endif /* !USE_DMALLOC */

void	 g_mem_profile (void);
void	 g_mem_check   (gpointer  mem);

/* Generic allocators
 */
GAllocator* g_allocator_new   (const gchar  *name,
			       guint         n_preallocs);
void        g_allocator_free  (GAllocator   *allocator);

#define	G_ALLOCATOR_LIST	(1)
#define	G_ALLOCATOR_SLIST	(2)
#define	G_ALLOCATOR_NODE	(3)


/* "g_mem_chunk_new" creates a new memory chunk.
 * Memory chunks are used to allocate pieces of memory which are
 *  always the same size. Lists are a good example of such a data type.
 * The memory chunk allocates and frees blocks of memory as needed.
 *  Just be sure to call "g_mem_chunk_free" and not "g_free" on data
 *  allocated in a mem chunk. ("g_free" will most likely cause a seg
 *  fault...somewhere).
 *
 * Oh yeah, GMemChunk is an opaque data type. (You don't really
 *  want to know what's going on inside do you?)
 */

/* ALLOC_ONLY MemChunk's can only allocate memory. The free operation
 *  is interpreted as a no op. ALLOC_ONLY MemChunk's save 4 bytes per
 *  atom. (They are also useful for lists which use MemChunk to allocate
 *  memory but are also part of the MemChunk implementation).
 * ALLOC_AND_FREE MemChunk's can allocate and free memory.
 */

#define G_ALLOC_ONLY	  1
#define G_ALLOC_AND_FREE  2

GMemChunk* g_mem_chunk_new     (gchar	  *name,
				gint	   atom_size,
				gulong	   area_size,
				gint	   type);
void	   g_mem_chunk_destroy (GMemChunk *mem_chunk);
gpointer   g_mem_chunk_alloc   (GMemChunk *mem_chunk);
gpointer   g_mem_chunk_alloc0  (GMemChunk *mem_chunk);
void	   g_mem_chunk_free    (GMemChunk *mem_chunk,
				gpointer   mem);
void	   g_mem_chunk_clean   (GMemChunk *mem_chunk);
void	   g_mem_chunk_reset   (GMemChunk *mem_chunk);
void	   g_mem_chunk_print   (GMemChunk *mem_chunk);
void	   g_mem_chunk_info    (void);

/* Ah yes...we have a "g_blow_chunks" function.
 * "g_blow_chunks" simply compresses all the chunks. This operation
 *  consists of freeing every memory area that should be freed (but
 *  which we haven't gotten around to doing yet). And, no,
 *  "g_blow_chunks" doesn't follow the naming scheme, but it is a
 *  much better name than "g_mem_chunk_clean_all" or something
 *  similar.
 */
void g_blow_chunks (void);


/* Timer
 */
GTimer* g_timer_new	(void);
void	g_timer_destroy (GTimer	 *timer);
void	g_timer_start	(GTimer	 *timer);
void	g_timer_stop	(GTimer	 *timer);
void	g_timer_reset	(GTimer	 *timer);
gdouble g_timer_elapsed (GTimer	 *timer,
			 gulong	 *microseconds);


/* String utility functions that modify a string argument or
 * return a constant string that must not be freed.
 */
#define	 G_STR_DELIMITERS	"_-|> <."
gchar*	 g_strdelimit		(gchar	     *string,
				 const gchar *delimiters,
				 gchar	      new_delimiter);
gdouble	 g_strtod		(const gchar *nptr,
				 gchar	    **endptr);
gchar*	 g_strerror		(gint	      errnum);
gchar*	 g_strsignal		(gint	      signum);
gint	 g_strcasecmp		(const gchar *s1,
				 const gchar *s2);
gint	 g_strncasecmp		(const gchar *s1,
				 const gchar *s2,
				 guint 	      n);
void	 g_strdown		(gchar	     *string);
void	 g_strup		(gchar	     *string);
void	 g_strreverse		(gchar	     *string);
/* removes leading spaces */
gchar*   g_strchug              (gchar        *string);
/* removes trailing spaces */
gchar*  g_strchomp              (gchar        *string);
/* removes leading & trailing spaces */
#define g_strstrip( string )	g_strchomp (g_strchug (string))

/* String utility functions that return a newly allocated string which
 * ought to be freed from the caller at some point.
 */
gchar*	 g_strdup		(const gchar *str);
gchar*	 g_strdup_printf	(const gchar *format,
				 ...) G_GNUC_PRINTF (1, 2);
gchar*	 g_strdup_vprintf	(const gchar *format,
				 va_list      args);
gchar*	 g_strndup		(const gchar *str,
				 guint	      n);
gchar*	 g_strnfill		(guint	      length,
				 gchar	      fill_char);
gchar*	 g_strconcat		(const gchar *string1,
				 ...); /* NULL terminated */
gchar*   g_strjoin		(const gchar  *separator,
				 ...); /* NULL terminated */
gchar*	 g_strescape		(gchar	      *string);
gpointer g_memdup		(gconstpointer mem,
				 guint	       byte_size);

/* NULL terminated string arrays.
 * g_strsplit() splits up string into max_tokens tokens at delim and
 * returns a newly allocated string array.
 * g_strjoinv() concatenates all of str_array's strings, sliding in an
 * optional separator, the returned string is newly allocated.
 * g_strfreev() frees the array itself and all of its strings.
 */
gchar**	 g_strsplit		(const gchar  *string,
				 const gchar  *delimiter,
				 gint          max_tokens);
gchar*   g_strjoinv		(const gchar  *separator,
				 gchar       **str_array);
void     g_strfreev		(gchar       **str_array);



/* calculate a string size, guarranteed to fit format + args.
 */
guint	g_printf_string_upper_bound (const gchar* format,
				     va_list	  args);


/* Retrive static string info
 */
gchar*	g_get_user_name		(void);
gchar*	g_get_real_name		(void);
gchar*	g_get_home_dir		(void);
gchar*	g_get_tmp_dir		(void);
gchar*	g_get_prgname		(void);
void	g_set_prgname		(const gchar *prgname);


/* Miscellaneous utility functions
 */
guint	g_parse_debug_string	(const gchar *string,
				 GDebugKey   *keys,
				 guint	      nkeys);
gint	g_snprintf		(gchar	     *string,
				 gulong	      n,
				 gchar const *format,
				 ...) G_GNUC_PRINTF (3, 4);
gint	g_vsnprintf		(gchar	     *string,
				 gulong	      n,
				 gchar const *format,
				 va_list      args);
gchar*	g_basename		(const gchar *file_name);
/* Check if a file name is an absolute path */
gboolean g_path_is_absolute	(const gchar *file_name);
/* In case of absolute paths, skip the root part */
gchar*  g_path_skip_root	(gchar       *file_name);

/* strings are newly allocated with g_malloc() */
gchar*	g_dirname		(const gchar *file_name);
gchar*	g_get_current_dir	(void);
gchar*  g_getenv		(const gchar *variable);


/* we use a GLib function as a replacement for ATEXIT, so
 * the programmer is not required to check the return value
 * (if there is any in the implementation) and doesn't encounter
 * missing include files.
 */
void	g_atexit		(GVoidFunc    func);


/* Bit tests
 */
G_INLINE_FUNC gint	g_bit_nth_lsf (guint32 mask,
				       gint    nth_bit);
#ifdef	G_CAN_INLINE
G_INLINE_FUNC gint
g_bit_nth_lsf (guint32 mask,
	       gint    nth_bit)
{
  do
    {
      nth_bit++;
      if (mask & (1 << (guint) nth_bit))
	return nth_bit;
    }
  while (nth_bit < 32);
  return -1;
}
#endif	/* G_CAN_INLINE */

G_INLINE_FUNC gint	g_bit_nth_msf (guint32 mask,
				       gint    nth_bit);
#ifdef G_CAN_INLINE
G_INLINE_FUNC gint
g_bit_nth_msf (guint32 mask,
	       gint    nth_bit)
{
  if (nth_bit < 0)
    nth_bit = 32;
  do
    {
      nth_bit--;
      if (mask & (1 << (guint) nth_bit))
	return nth_bit;
    }
  while (nth_bit > 0);
  return -1;
}
#endif	/* G_CAN_INLINE */

G_INLINE_FUNC guint	g_bit_storage (guint number);
#ifdef G_CAN_INLINE
G_INLINE_FUNC guint
g_bit_storage (guint number)
{
  register guint n_bits = 0;
  
  do
    {
      n_bits++;
      number >>= 1;
    }
  while (number);
  return n_bits;
}
#endif	/* G_CAN_INLINE */

/* String Chunks
 */
GStringChunk* g_string_chunk_new	   (gint size);
void	      g_string_chunk_free	   (GStringChunk *chunk);
gchar*	      g_string_chunk_insert	   (GStringChunk *chunk,
					    const gchar	 *string);
gchar*	      g_string_chunk_insert_const  (GStringChunk *chunk,
					    const gchar	 *string);


/* Strings
 */
GString* g_string_new	    (const gchar *init);
GString* g_string_sized_new (guint	  dfl_size);
void	 g_string_free	    (GString	 *string,
			     gint	  free_segment);
GString* g_string_assign    (GString	 *lval,
			     const gchar *rval);
GString* g_string_truncate  (GString	 *string,
			     gint	  len);
GString* g_string_append    (GString	 *string,
			     const gchar *val);
GString* g_string_append_c  (GString	 *string,
			     gchar	  c);
GString* g_string_prepend   (GString	 *string,
			     const gchar *val);
GString* g_string_prepend_c (GString	 *string,
			     gchar	  c);
GString* g_string_insert    (GString	 *string,
			     gint	  pos,
			     const gchar *val);
GString* g_string_insert_c  (GString	 *string,
			     gint	  pos,
			     gchar	  c);
GString* g_string_erase	    (GString	 *string,
			     gint	  pos,
			     gint	  len);
GString* g_string_down	    (GString	 *string);
GString* g_string_up	    (GString	 *string);
void	 g_string_sprintf   (GString	 *string,
			     const gchar *format,
			     ...) G_GNUC_PRINTF (2, 3);
void	 g_string_sprintfa  (GString	 *string,
			     const gchar *format,
			     ...) G_GNUC_PRINTF (2, 3);


/* Resizable arrays, remove fills any cleared spot and shortens the
 * array, while preserving the order. remove_fast will distort the
 * order by moving the last element to the position of the removed 
 */

#define g_array_append_val(a,v)	  g_array_append_vals (a, &v, 1)
#define g_array_prepend_val(a,v)  g_array_prepend_vals (a, &v, 1)
#define g_array_insert_val(a,i,v) g_array_insert_vals (a, i, &v, 1)
#define g_array_index(a,t,i)      (((t*) (a)->data) [(i)])

GArray* g_array_new	          (gboolean	    zero_terminated,
				   gboolean	    clear,
				   guint	    element_size);
void	g_array_free	          (GArray	   *array,
				   gboolean	    free_segment);
GArray* g_array_append_vals       (GArray	   *array,
				   gconstpointer    data,
				   guint	    len);
GArray* g_array_prepend_vals      (GArray	   *array,
				   gconstpointer    data,
				   guint	    len);
GArray* g_array_insert_vals       (GArray          *array,
				   guint            index,
				   gconstpointer    data,
				   guint            len);
GArray* g_array_set_size          (GArray	   *array,
				   guint	    length);
GArray* g_array_remove_index	  (GArray	   *array,
				   guint	    index);
GArray* g_array_remove_index_fast (GArray	   *array,
				   guint	    index);

/* Resizable pointer array.  This interface is much less complicated
 * than the above.  Add appends appends a pointer.  Remove fills any
 * cleared spot and shortens the array. remove_fast will again distort
 * order.  
 */
#define	    g_ptr_array_index(array,index) (array->pdata)[index]
GPtrArray*  g_ptr_array_new		   (void);
void	    g_ptr_array_free		   (GPtrArray	*array,
					    gboolean	 free_seg);
void	    g_ptr_array_set_size	   (GPtrArray	*array,
					    gint	 length);
gpointer    g_ptr_array_remove_index	   (GPtrArray	*array,
					    guint	 index);
gpointer    g_ptr_array_remove_index_fast  (GPtrArray	*array,
					    guint	 index);
gboolean    g_ptr_array_remove		   (GPtrArray	*array,
					    gpointer	 data);
gboolean    g_ptr_array_remove_fast        (GPtrArray	*array,
					    gpointer	 data);
void	    g_ptr_array_add		   (GPtrArray	*array,
					    gpointer	 data);

/* Byte arrays, an array of guint8.  Implemented as a GArray,
 * but type-safe.
 */

GByteArray* g_byte_array_new	           (void);
void	    g_byte_array_free	           (GByteArray	 *array,
					    gboolean	  free_segment);
GByteArray* g_byte_array_append	           (GByteArray	 *array,
					    const guint8 *data,
					    guint	  len);
GByteArray* g_byte_array_prepend           (GByteArray	 *array,
					    const guint8 *data,
					    guint	  len);
GByteArray* g_byte_array_set_size          (GByteArray	 *array,
					    guint	  length);
GByteArray* g_byte_array_remove_index	   (GByteArray	 *array,
					    guint	  index);
GByteArray* g_byte_array_remove_index_fast (GByteArray	 *array,
					    guint	  index);


/* Hash Functions
 */
gint  g_str_equal (gconstpointer   v,
		   gconstpointer   v2);
guint g_str_hash  (gconstpointer   v);

gint  g_int_equal (gconstpointer   v,
		   gconstpointer   v2);
guint g_int_hash  (gconstpointer   v);

/* This "hash" function will just return the key's adress as an
 * unsigned integer. Useful for hashing on plain adresses or
 * simple integer values.
 * passing NULL into g_hash_table_new() as GHashFunc has the
 * same effect as passing g_direct_hash().
 */
guint g_direct_hash  (gconstpointer v);
gint  g_direct_equal (gconstpointer v,
		      gconstpointer v2);


/* Quarks (string<->id association)
 */
GQuark	  g_quark_try_string		(const gchar	*string);
GQuark	  g_quark_from_static_string	(const gchar	*string);
GQuark	  g_quark_from_string		(const gchar	*string);
gchar*	  g_quark_to_string		(GQuark		 quark);


/* Keyed Data List
 */
void	  g_datalist_init		 (GData		 **datalist);
void	  g_datalist_clear		 (GData		 **datalist);
gpointer  g_datalist_id_get_data	 (GData		 **datalist,
					  GQuark	   key_id);
void	  g_datalist_id_set_data_full	 (GData		 **datalist,
					  GQuark	   key_id,
					  gpointer	   data,
					  GDestroyNotify   destroy_func);
void	  g_datalist_id_remove_no_notify (GData		 **datalist,
					  GQuark	   key_id);
void	  g_datalist_foreach		 (GData		 **datalist,
					  GDataForeachFunc func,
					  gpointer	   user_data);
#define	  g_datalist_id_set_data(dl, q, d)	\
     g_datalist_id_set_data_full ((dl), (q), (d), NULL)
#define	  g_datalist_id_remove_data(dl, q)	\
     g_datalist_id_set_data ((dl), (q), NULL)
#define	  g_datalist_get_data(dl, k)		\
     (g_datalist_id_get_data ((dl), g_quark_try_string (k)))
#define	  g_datalist_set_data_full(dl, k, d, f)	\
     g_datalist_id_set_data_full ((dl), g_quark_from_string (k), (d), (f))
#define	  g_datalist_remove_no_notify(dl, k)	\
     g_datalist_id_remove_no_notify ((dl), g_quark_try_string (k))
#define	  g_datalist_set_data(dl, k, d)		\
     g_datalist_set_data_full ((dl), (k), (d), NULL)
#define	  g_datalist_remove_data(dl, k)		\
     g_datalist_id_set_data ((dl), g_quark_try_string (k), NULL)


/* Location Associated Keyed Data
 */
void	  g_dataset_destroy		(gconstpointer	  dataset_location);
gpointer  g_dataset_id_get_data		(gconstpointer	  dataset_location,
					 GQuark		  key_id);
void	  g_dataset_id_set_data_full	(gconstpointer	  dataset_location,
					 GQuark		  key_id,
					 gpointer	  data,
					 GDestroyNotify	  destroy_func);
void	  g_dataset_id_remove_no_notify	(gconstpointer	  dataset_location,
					 GQuark		  key_id);
void	  g_dataset_foreach		(gconstpointer	  dataset_location,
					 GDataForeachFunc func,
					 gpointer	  user_data);
#define	  g_dataset_id_set_data(l, k, d)	\
     g_dataset_id_set_data_full ((l), (k), (d), NULL)
#define	  g_dataset_id_remove_data(l, k)	\
     g_dataset_id_set_data ((l), (k), NULL)
#define	  g_dataset_get_data(l, k)		\
     (g_dataset_id_get_data ((l), g_quark_try_string (k)))
#define	  g_dataset_set_data_full(l, k, d, f)	\
     g_dataset_id_set_data_full ((l), g_quark_from_string (k), (d), (f))
#define	  g_dataset_remove_no_notify(l, k)	\
     g_dataset_id_remove_no_notify ((l), g_quark_try_string (k))
#define	  g_dataset_set_data(l, k, d)		\
     g_dataset_set_data_full ((l), (k), (d), NULL)
#define	  g_dataset_remove_data(l, k)		\
     g_dataset_id_set_data ((l), g_quark_try_string (k), NULL)


/* GScanner: Flexible lexical scanner for general purpose.
 */

/* Character sets */
#define G_CSET_A_2_Z	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define G_CSET_a_2_z	"abcdefghijklmnopqrstuvwxyz"
#define G_CSET_LATINC	"\300\301\302\303\304\305\306"\
			"\307\310\311\312\313\314\315\316\317\320"\
			"\321\322\323\324\325\326"\
			"\330\331\332\333\334\335\336"
#define G_CSET_LATINS	"\337\340\341\342\343\344\345\346"\
			"\347\350\351\352\353\354\355\356\357\360"\
			"\361\362\363\364\365\366"\
			"\370\371\372\373\374\375\376\377"

/* Error types */
typedef enum
{
  G_ERR_UNKNOWN,
  G_ERR_UNEXP_EOF,
  G_ERR_UNEXP_EOF_IN_STRING,
  G_ERR_UNEXP_EOF_IN_COMMENT,
  G_ERR_NON_DIGIT_IN_CONST,
  G_ERR_DIGIT_RADIX,
  G_ERR_FLOAT_RADIX,
  G_ERR_FLOAT_MALFORMED
} GErrorType;

/* Token types */
typedef enum
{
  G_TOKEN_EOF			=   0,
  
  G_TOKEN_LEFT_PAREN		= '(',
  G_TOKEN_RIGHT_PAREN		= ')',
  G_TOKEN_LEFT_CURLY		= '{',
  G_TOKEN_RIGHT_CURLY		= '}',
  G_TOKEN_LEFT_BRACE		= '[',
  G_TOKEN_RIGHT_BRACE		= ']',
  G_TOKEN_EQUAL_SIGN		= '=',
  G_TOKEN_COMMA			= ',',
  
  G_TOKEN_NONE			= 256,
  
  G_TOKEN_ERROR,
  
  G_TOKEN_CHAR,
  G_TOKEN_BINARY,
  G_TOKEN_OCTAL,
  G_TOKEN_INT,
  G_TOKEN_HEX,
  G_TOKEN_FLOAT,
  G_TOKEN_STRING,
  
  G_TOKEN_SYMBOL,
  G_TOKEN_IDENTIFIER,
  G_TOKEN_IDENTIFIER_NULL,
  
  G_TOKEN_COMMENT_SINGLE,
  G_TOKEN_COMMENT_MULTI,
  G_TOKEN_LAST
} GTokenType;

union	_GTokenValue
{
  gpointer	v_symbol;
  gchar		*v_identifier;
  gulong	v_binary;
  gulong	v_octal;
  gulong	v_int;
  gdouble	v_float;
  gulong	v_hex;
  gchar		*v_string;
  gchar		*v_comment;
  guchar	v_char;
  guint		v_error;
};

struct	_GScannerConfig
{
  /* Character sets
   */
  gchar		*cset_skip_characters;		/* default: " \t\n" */
  gchar		*cset_identifier_first;
  gchar		*cset_identifier_nth;
  gchar		*cpair_comment_single;		/* default: "#\n" */
  
  /* Should symbol lookup work case sensitive?
   */
  guint		case_sensitive : 1;
  
  /* Boolean values to be adjusted "on the fly"
   * to configure scanning behaviour.
   */
  guint		skip_comment_multi : 1;		/* C like comment */
  guint		skip_comment_single : 1;	/* single line comment */
  guint		scan_comment_multi : 1;		/* scan multi line comments? */
  guint		scan_identifier : 1;
  guint		scan_identifier_1char : 1;
  guint		scan_identifier_NULL : 1;
  guint		scan_symbols : 1;
  guint		scan_binary : 1;
  guint		scan_octal : 1;
  guint		scan_float : 1;
  guint		scan_hex : 1;			/* `0x0ff0' */
  guint		scan_hex_dollar : 1;		/* `$0ff0' */
  guint		scan_string_sq : 1;		/* string: 'anything' */
  guint		scan_string_dq : 1;		/* string: "\\-escapes!\n" */
  guint		numbers_2_int : 1;		/* bin, octal, hex => int */
  guint		int_2_float : 1;		/* int => G_TOKEN_FLOAT? */
  guint		identifier_2_string : 1;
  guint		char_2_token : 1;		/* return G_TOKEN_CHAR? */
  guint		symbol_2_token : 1;
  guint		scope_0_fallback : 1;		/* try scope 0 on lookups? */
};

struct	_GScanner
{
  /* unused fields */
  gpointer		user_data;
  guint			max_parse_errors;
  
  /* g_scanner_error() increments this field */
  guint			parse_errors;
  
  /* name of input stream, featured by the default message handler */
  const gchar		*input_name;
  
  /* data pointer for derived structures */
  gpointer		derived_data;
  
  /* link into the scanner configuration */
  GScannerConfig	*config;
  
  /* fields filled in after g_scanner_get_next_token() */
  GTokenType		token;
  GTokenValue		value;
  guint			line;
  guint			position;
  
  /* fields filled in after g_scanner_peek_next_token() */
  GTokenType		next_token;
  GTokenValue		next_value;
  guint			next_line;
  guint			next_position;
  
  /* to be considered private */
  GHashTable		*symbol_table;
  gint			input_fd;
  const gchar		*text;
  const gchar		*text_end;
  gchar			*buffer;
  guint			scope_id;
  
  /* handler function for _warn and _error */
  GScannerMsgFunc	msg_handler;
};

GScanner*	g_scanner_new			(GScannerConfig *config_templ);
void		g_scanner_destroy		(GScanner	*scanner);
void		g_scanner_input_file		(GScanner	*scanner,
						 gint		input_fd);
void		g_scanner_sync_file_offset	(GScanner	*scanner);
void		g_scanner_input_text		(GScanner	*scanner,
						 const	gchar	*text,
						 guint		text_len);
GTokenType	g_scanner_get_next_token	(GScanner	*scanner);
GTokenType	g_scanner_peek_next_token	(GScanner	*scanner);
GTokenType	g_scanner_cur_token		(GScanner	*scanner);
GTokenValue	g_scanner_cur_value		(GScanner	*scanner);
guint		g_scanner_cur_line		(GScanner	*scanner);
guint		g_scanner_cur_position		(GScanner	*scanner);
gboolean	g_scanner_eof			(GScanner	*scanner);
guint		g_scanner_set_scope		(GScanner	*scanner,
						 guint		 scope_id);
void		g_scanner_scope_add_symbol	(GScanner	*scanner,
						 guint		 scope_id,
						 const gchar	*symbol,
						 gpointer	value);
void		g_scanner_scope_remove_symbol	(GScanner	*scanner,
						 guint		 scope_id,
						 const gchar	*symbol);
gpointer	g_scanner_scope_lookup_symbol	(GScanner	*scanner,
						 guint		 scope_id,
						 const gchar	*symbol);
void		g_scanner_scope_foreach_symbol	(GScanner	*scanner,
						 guint		 scope_id,
						 GHFunc		 func,
						 gpointer	 user_data);
gpointer	g_scanner_lookup_symbol		(GScanner	*scanner,
						 const gchar	*symbol);
void		g_scanner_freeze_symbol_table	(GScanner	*scanner);
void		g_scanner_thaw_symbol_table	(GScanner	*scanner);
void		g_scanner_unexp_token		(GScanner	*scanner,
						 GTokenType	expected_token,
						 const gchar	*identifier_spec,
						 const gchar	*symbol_spec,
						 const gchar	*symbol_name,
						 const gchar	*message,
						 gint		 is_error);
void		g_scanner_error			(GScanner	*scanner,
						 const gchar	*format,
						 ...) G_GNUC_PRINTF (2,3);
void		g_scanner_warn			(GScanner	*scanner,
						 const gchar	*format,
						 ...) G_GNUC_PRINTF (2,3);
gint		g_scanner_stat_mode		(const gchar	*filename);
/* keep downward source compatibility */
#define		g_scanner_add_symbol( scanner, symbol, value )	G_STMT_START { \
  g_scanner_scope_add_symbol ((scanner), 0, (symbol), (value)); \
} G_STMT_END
#define		g_scanner_remove_symbol( scanner, symbol )	G_STMT_START { \
  g_scanner_scope_remove_symbol ((scanner), 0, (symbol)); \
} G_STMT_END
#define		g_scanner_foreach_symbol( scanner, func, data )	G_STMT_START { \
  g_scanner_scope_foreach_symbol ((scanner), 0, (func), (data)); \
} G_STMT_END


/* GCompletion
 */

struct _GCompletion
{
  GList* items;
  GCompletionFunc func;
  
  gchar* prefix;
  GList* cache;
};

GCompletion* g_completion_new	       (GCompletionFunc func);
void	     g_completion_add_items    (GCompletion*	cmp,
					GList*		items);
void	     g_completion_remove_items (GCompletion*	cmp,
					GList*		items);
void	     g_completion_clear_items  (GCompletion*	cmp);
GList*	     g_completion_complete     (GCompletion*	cmp,
					gchar*		prefix,
					gchar**		new_prefix);
void	     g_completion_free	       (GCompletion*	cmp);


/* GDate
 *
 * Date calculations (not time for now, to be resolved). These are a
 * mutant combination of Steffen Beyer's DateCalc routines
 * (http://www.perl.com/CPAN/authors/id/STBEY/) and Jon Trowbridge's
 * date routines (written for in-house software).  Written by Havoc
 * Pennington <hp@pobox.com> 
 */

typedef guint16 GDateYear;
typedef guint8  GDateDay;   /* day of the month */
typedef struct _GDate GDate;
/* make struct tm known without having to include time.h */
struct tm;

/* enum used to specify order of appearance in parsed date strings */
typedef enum
{
  G_DATE_DAY   = 0,
  G_DATE_MONTH = 1,
  G_DATE_YEAR  = 2
} GDateDMY;

/* actual week and month values */
typedef enum
{
  G_DATE_BAD_WEEKDAY  = 0,
  G_DATE_MONDAY       = 1,
  G_DATE_TUESDAY      = 2,
  G_DATE_WEDNESDAY    = 3,
  G_DATE_THURSDAY     = 4,
  G_DATE_FRIDAY       = 5,
  G_DATE_SATURDAY     = 6,
  G_DATE_SUNDAY       = 7
} GDateWeekday;
typedef enum
{
  G_DATE_BAD_MONTH = 0,
  G_DATE_JANUARY   = 1,
  G_DATE_FEBRUARY  = 2,
  G_DATE_MARCH     = 3,
  G_DATE_APRIL     = 4,
  G_DATE_MAY       = 5,
  G_DATE_JUNE      = 6,
  G_DATE_JULY      = 7,
  G_DATE_AUGUST    = 8,
  G_DATE_SEPTEMBER = 9,
  G_DATE_OCTOBER   = 10,
  G_DATE_NOVEMBER  = 11,
  G_DATE_DECEMBER  = 12
} GDateMonth;

#define G_DATE_BAD_JULIAN 0U
#define G_DATE_BAD_DAY    0U
#define G_DATE_BAD_YEAR   0U

/* Note: directly manipulating structs is generally a bad idea, but
 * in this case it's an *incredibly* bad idea, because all or part
 * of this struct can be invalid at any given time. Use the functions,
 * or you will get hosed, I promise.
 */
struct _GDate
{ 
  guint julian_days : 32; /* julian days representation - we use a
                           *  bitfield hoping that 64 bit platforms
                           *  will pack this whole struct in one big
                           *  int 
                           */

  guint julian : 1;    /* julian is valid */
  guint dmy    : 1;    /* dmy is valid */

  /* DMY representation */
  guint day    : 6;  
  guint month  : 4; 
  guint year   : 16; 
};

/* g_date_new() returns an invalid date, you then have to _set() stuff 
 * to get a usable object. You can also allocate a GDate statically,
 * then call g_date_clear() to initialize.
 */
GDate*       g_date_new                   (void);
GDate*       g_date_new_dmy               (GDateDay     day, 
                                           GDateMonth   month, 
                                           GDateYear    year);
GDate*       g_date_new_julian            (guint32      julian_day);
void         g_date_free                  (GDate       *date);

/* check g_date_valid() after doing an operation that might fail, like
 * _parse.  Almost all g_date operations are undefined on invalid
 * dates (the exceptions are the mutators, since you need those to
 * return to validity).  
 */
gboolean     g_date_valid                 (GDate       *date);
gboolean     g_date_valid_day             (GDateDay     day);
gboolean     g_date_valid_month           (GDateMonth   month);
gboolean     g_date_valid_year            (GDateYear    year);
gboolean     g_date_valid_weekday         (GDateWeekday weekday);
gboolean     g_date_valid_julian          (guint32      julian_date);
gboolean     g_date_valid_dmy             (GDateDay     day,
                                           GDateMonth   month,
                                           GDateYear    year);

GDateWeekday g_date_weekday               (GDate       *date);
GDateMonth   g_date_month                 (GDate       *date);
GDateYear    g_date_year                  (GDate       *date);
GDateDay     g_date_day                   (GDate       *date);
guint32      g_date_julian                (GDate       *date);
guint        g_date_day_of_year           (GDate       *date);

/* First monday/sunday is the start of week 1; if we haven't reached
 * that day, return 0. These are not ISO weeks of the year; that
 * routine needs to be added.
 * these functions return the number of weeks, starting on the
 * corrsponding day
 */
guint        g_date_monday_week_of_year   (GDate      *date);
guint        g_date_sunday_week_of_year   (GDate      *date);

/* If you create a static date struct you need to clear it to get it
 * in a sane state before use. You can clear a whole array at
 * once with the ndates argument.
 */
void         g_date_clear                 (GDate       *date, 
                                           guint        n_dates);

/* The parse routine is meant for dates typed in by a user, so it
 * permits many formats but tries to catch common typos. If your data
 * needs to be strictly validated, it is not an appropriate function.
 */
void         g_date_set_parse             (GDate       *date,
                                           const gchar *str);
void         g_date_set_time              (GDate       *date, 
                                           GTime        time);
void         g_date_set_month             (GDate       *date, 
                                           GDateMonth   month);
void         g_date_set_day               (GDate       *date, 
                                           GDateDay     day);
void         g_date_set_year              (GDate       *date,
                                           GDateYear    year);
void         g_date_set_dmy               (GDate       *date,
                                           GDateDay     day,
                                           GDateMonth   month,
                                           GDateYear    y);
void         g_date_set_julian            (GDate       *date,
                                           guint32      julian_date);
gboolean     g_date_is_first_of_month     (GDate       *date);
gboolean     g_date_is_last_of_month      (GDate       *date);

/* To go forward by some number of weeks just go forward weeks*7 days */
void         g_date_add_days              (GDate       *date, 
                                           guint        n_days);
void         g_date_subtract_days         (GDate       *date, 
                                           guint        n_days);

/* If you add/sub months while day > 28, the day might change */
void         g_date_add_months            (GDate       *date,
                                           guint        n_months);
void         g_date_subtract_months       (GDate       *date,
                                           guint        n_months);

/* If it's feb 29, changing years can move you to the 28th */
void         g_date_add_years             (GDate       *date,
                                           guint        n_years);
void         g_date_subtract_years        (GDate       *date,
                                           guint        n_years);
gboolean     g_date_is_leap_year          (GDateYear    year);
guint8       g_date_days_in_month         (GDateMonth   month, 
                                           GDateYear    year);
guint8       g_date_monday_weeks_in_year  (GDateYear    year);
guint8       g_date_sunday_weeks_in_year  (GDateYear    year);

/* qsort-friendly (with a cast...) */
gint         g_date_compare               (GDate       *lhs,
                                           GDate       *rhs);
void         g_date_to_struct_tm          (GDate       *date,
                                           struct tm   *tm);

/* Just like strftime() except you can only use date-related formats.
 *   Using a time format is undefined.
 */
gsize        g_date_strftime              (gchar       *s,
                                           gsize        slen,
                                           const gchar *format,
                                           GDate       *date);


/* GRelation
 *
 * Indexed Relations.  Imagine a really simple table in a
 * database.  Relations are not ordered.  This data type is meant for
 * maintaining a N-way mapping.
 *
 * g_relation_new() creates a relation with FIELDS fields
 *
 * g_relation_destroy() frees all resources
 * g_tuples_destroy() frees the result of g_relation_select()
 *
 * g_relation_index() indexes relation FIELD with the provided
 *   equality and hash functions.  this must be done before any
 *   calls to insert are made.
 *
 * g_relation_insert() inserts a new tuple.  you are expected to
 *   provide the right number of fields.
 *
 * g_relation_delete() deletes all relations with KEY in FIELD
 * g_relation_select() returns ...
 * g_relation_count() counts ...
 */

GRelation* g_relation_new     (gint	    fields);
void	   g_relation_destroy (GRelation   *relation);
void	   g_relation_index   (GRelation   *relation,
			       gint	    field,
			       GHashFunc    hash_func,
			       GCompareFunc key_compare_func);
void	   g_relation_insert  (GRelation   *relation,
			       ...);
gint	   g_relation_delete  (GRelation   *relation,
			       gconstpointer  key,
			       gint	    field);
GTuples*   g_relation_select  (GRelation   *relation,
			       gconstpointer  key,
			       gint	    field);
gint	   g_relation_count   (GRelation   *relation,
			       gconstpointer  key,
			       gint	    field);
gboolean   g_relation_exists  (GRelation   *relation,
			       ...);
void	   g_relation_print   (GRelation   *relation);

void	   g_tuples_destroy   (GTuples	   *tuples);
gpointer   g_tuples_index     (GTuples	   *tuples,
			       gint	    index,
			       gint	    field);


/* Prime numbers.
 */

/* This function returns prime numbers spaced by approximately 1.5-2.0
 * and is for use in resizing data structures which prefer
 * prime-valued sizes.	The closest spaced prime function returns the
 * next largest prime, or the highest it knows about which is about
 * MAXINT/4.
 */
guint	   g_spaced_primes_closest (guint num);


/* GIOChannel
 */

typedef struct _GIOFuncs GIOFuncs;
typedef enum
{
  G_IO_ERROR_NONE,
  G_IO_ERROR_AGAIN,
  G_IO_ERROR_INVAL,
  G_IO_ERROR_UNKNOWN
} GIOError;
typedef enum
{
  G_SEEK_CUR,
  G_SEEK_SET,
  G_SEEK_END
} GSeekType;

#ifndef XP_MAC
typedef enum
{
  G_IO_IN	GLIB_SYSDEF_POLLIN,
  G_IO_OUT	GLIB_SYSDEF_POLLOUT,
  G_IO_PRI	GLIB_SYSDEF_POLLPRI,
  G_IO_ERR	GLIB_SYSDEF_POLLERR,
  G_IO_HUP	GLIB_SYSDEF_POLLHUP,
  G_IO_NVAL	GLIB_SYSDEF_POLLNVAL
} GIOCondition;
#else
typedef enum
{
  GLIB_SYSDEF_POLLIN,
  GLIB_SYSDEF_POLLOUT,
  GLIB_SYSDEF_POLLPRI,
  GLIB_SYSDEF_POLLERR,
  GLIB_SYSDEF_POLLHUP,
  GLIB_SYSDEF_POLLNVAL
} GIOCondition;
#endif

struct _GIOChannel
{
  guint channel_flags;
  guint ref_count;
  GIOFuncs *funcs;
};

typedef gboolean (*GIOFunc) (GIOChannel   *source,
			     GIOCondition  condition,
			     gpointer      data);
struct _GIOFuncs
{
  GIOError (*io_read)   (GIOChannel 	*channel, 
		         gchar      	*buf, 
		         guint      	 count,
			 guint      	*bytes_read);
  GIOError (*io_write)  (GIOChannel 	*channel, 
		 	 gchar      	*buf, 
			 guint      	 count,
			 guint      	*bytes_written);
  GIOError (*io_seek)   (GIOChannel   	*channel, 
		 	 gint       	 offset, 
		  	 GSeekType  	 type);
  void (*io_close)      (GIOChannel	*channel);
  guint (*io_add_watch) (GIOChannel     *channel,
			 gint            priority,
			 GIOCondition    condition,
			 GIOFunc         func,
			 gpointer        user_data,
			 GDestroyNotify  notify);
  void (*io_free)       (GIOChannel	*channel);
};

void        g_io_channel_init   (GIOChannel    *channel);
void        g_io_channel_ref    (GIOChannel    *channel);
void        g_io_channel_unref  (GIOChannel    *channel);
GIOError    g_io_channel_read   (GIOChannel    *channel, 
			         gchar         *buf, 
			         guint          count,
			         guint         *bytes_read);
GIOError  g_io_channel_write    (GIOChannel    *channel, 
			         gchar         *buf, 
			         guint          count,
			         guint         *bytes_written);
GIOError  g_io_channel_seek     (GIOChannel    *channel,
			         gint           offset, 
			         GSeekType      type);
void      g_io_channel_close    (GIOChannel    *channel);
guint     g_io_add_watch_full   (GIOChannel    *channel,
			         gint           priority,
			         GIOCondition   condition,
			         GIOFunc        func,
			         gpointer       user_data,
			         GDestroyNotify notify);
guint    g_io_add_watch         (GIOChannel    *channel,
			         GIOCondition   condition,
			         GIOFunc        func,
			         gpointer       user_data);


/* Main loop
 */
typedef struct _GTimeVal	GTimeVal;
typedef struct _GSourceFuncs	GSourceFuncs;
typedef struct _GMainLoop	GMainLoop;	/* Opaque */

struct _GTimeVal
{
  glong tv_sec;
  glong tv_usec;
};
struct _GSourceFuncs
{
  gboolean (*prepare)  (gpointer  source_data, 
			GTimeVal *current_time,
			gint     *timeout,
			gpointer  user_data);
  gboolean (*check)    (gpointer  source_data,
			GTimeVal *current_time,
			gpointer  user_data);
  gboolean (*dispatch) (gpointer  source_data, 
			GTimeVal *current_time,
			gpointer  user_data);
  GDestroyNotify destroy;
};

/* Standard priorities */

#define G_PRIORITY_HIGH            -100
#define G_PRIORITY_DEFAULT          0
#define G_PRIORITY_HIGH_IDLE        100
#define G_PRIORITY_DEFAULT_IDLE     200
#define G_PRIORITY_LOW	            300

typedef gboolean (*GSourceFunc) (gpointer data);

/* Hooks for adding to the main loop */
guint    g_source_add                        (gint           priority, 
					      gboolean       can_recurse,
					      GSourceFuncs  *funcs,
					      gpointer       source_data, 
					      gpointer       user_data,
					      GDestroyNotify notify);
gboolean g_source_remove                     (guint          tag);
gboolean g_source_remove_by_user_data        (gpointer       user_data);
gboolean g_source_remove_by_source_data      (gpointer       source_data);
gboolean g_source_remove_by_funcs_user_data  (GSourceFuncs  *funcs,
					      gpointer       user_data);

void g_get_current_time		    (GTimeVal	   *result);

/* Running the main loop */
GMainLoop*	g_main_new		(gboolean	 is_running);
void		g_main_run		(GMainLoop	*loop);
void		g_main_quit		(GMainLoop	*loop);
void		g_main_destroy		(GMainLoop	*loop);
gboolean	g_main_is_running	(GMainLoop	*loop);

/* Run a single iteration of the mainloop. If block is FALSE,
 * will never block
 */
gboolean	g_main_iteration	(gboolean	may_block);

/* See if any events are pending */
gboolean	g_main_pending		(void);

/* Idles and timeouts */
guint		g_timeout_add_full	(gint           priority,
					 guint          interval, 
					 GSourceFunc    function,
					 gpointer       data,
					 GDestroyNotify notify);
guint		g_timeout_add		(guint          interval,
					 GSourceFunc    function,
					 gpointer       data);
guint		g_idle_add	   	(GSourceFunc	function,
					 gpointer	data);
guint	   	g_idle_add_full		(gint   	priority,
					 GSourceFunc	function,
					 gpointer	data,
					 GDestroyNotify destroy);
gboolean	g_idle_remove_by_data	(gpointer	data);

/* GPollFD
 *
 * System-specific IO and main loop calls
 *
 * On Win32, the fd in a GPollFD should be Win32 HANDLE (*not* a file
 * descriptor as provided by the C runtime) that can be used by
 * MsgWaitForMultipleObjects. This does *not* include file handles
 * from CreateFile, SOCKETs, nor pipe handles. (But you can use
 * WSAEventSelect to signal events when a SOCKET is readable).
 *
 * On Win32, fd can also be the special value G_WIN32_MSG_HANDLE to
 * indicate polling for messages. These message queue GPollFDs should
 * be added with the g_main_poll_win32_msg_add function.
 *
 * But note that G_WIN32_MSG_HANDLE GPollFDs should not be used by GDK
 * (GTK) programs, as GDK itself wants to read messages and convert them
 * to GDK events.
 *
 * So, unless you really know what you are doing, it's best not to try
 * to use the main loop polling stuff for your own needs on
 * Win32. It's really only written for the GIMP's needs so
 * far.
 */

typedef struct _GPollFD GPollFD;
typedef gint	(*GPollFunc)	(GPollFD *ufds,
				 guint	  nfsd,
				 gint     timeout);
struct _GPollFD
{
  gint		fd;
  gushort 	events;
  gushort 	revents;
};

void        g_main_add_poll          (GPollFD    *fd,
				      gint        priority);
void        g_main_remove_poll       (GPollFD    *fd);
void        g_main_set_poll_func     (GPollFunc   func);

/* On Unix, IO channels created with this function for any file
 * descriptor or socket.
 *
 * On Win32, use this only for plain files opened with the MSVCRT (the
 * Microsoft run-time C library) _open(), including file descriptors
 * 0, 1 and 2 (corresponding to stdin, stdout and stderr).
 * Actually, don't do even that, this code isn't done yet.
 *
 * The term file descriptor as used in the context of Win32 refers to
 * the emulated Unix-like file descriptors MSVCRT provides.
 */
GIOChannel* g_io_channel_unix_new    (int         fd);
gint        g_io_channel_unix_get_fd (GIOChannel *channel);

#ifdef NATIVE_WIN32

GUTILS_C_VAR guint g_pipe_readable_msg;

#define G_WIN32_MSG_HANDLE 19981206

/* This is used to add polling for Windows messages. GDK (GTk+) programs
 * should *not* use this. (In fact, I can't think of any program that
 * would want to use this, but it's here just for completeness's sake.
 */
void        g_main_poll_win32_msg_add(gint        priority,
				      GPollFD    *fd,
				      guint       hwnd);

/* An IO channel for Windows messages for window handle hwnd. */
GIOChannel *g_io_channel_win32_new_messages (guint hwnd);

/* An IO channel for an anonymous pipe as returned from the MSVCRT
 * _pipe(), with no mechanism for the writer to tell the reader when
 * there is data in the pipe.
 *
 * This is not really implemented yet.
 */
GIOChannel *g_io_channel_win32_new_pipe (int fd);

/* An IO channel for a pipe as returned from the MSVCRT _pipe(), with
 * Windows user messages used to signal data in the pipe for the
 * reader.
 *
 * fd is the file descriptor. For the write end, peer is the thread id
 * of the reader, and peer_fd is his file descriptor for the read end
 * of the pipe.
 *
 * This is used by the GIMP, and works.
 */
GIOChannel *g_io_channel_win32_new_pipe_with_wakeups (int   fd,
						      guint peer,
						      int   peer_fd);

void        g_io_channel_win32_pipe_request_wakeups (GIOChannel *channel,
						     guint       peer,
						     int         peer_fd);

void        g_io_channel_win32_pipe_readable (int   fd,
					      guint offset);

/* Get the C runtime file descriptor of a channel. */
gint        g_io_channel_win32_get_fd (GIOChannel *channel);

/* An IO channel for a SOCK_STREAM winsock socket. The parameter is
 * actually a SOCKET.
 */
GIOChannel *g_io_channel_win32_new_stream_socket (int socket);

#endif

/* Windows emulation stubs for common Unix functions
 */
#ifdef NATIVE_WIN32
#  define MAXPATHLEN 1024
#  ifdef _MSC_VER
typedef int pid_t;

/* These POSIXish functions are available in the Microsoft C library
 * prefixed with underscore (which of course technically speaking is
 * the Right Thing, as they are non-ANSI. Not that being non-ANSI
 * prevents Microsoft from practically requiring you to include
 * <windows.h> every now and then...).
 *
 * You still need to include the appropriate headers to get the
 * prototypes, <io.h> or <direct.h>.
 *
 * For some functions, we provide emulators in glib, which are prefixed
 * with gwin_.
 */
#    define getcwd		_getcwd
#    define getpid		_getpid
#    define access		_access
#    define open		_open
#    define read		_read
#    define write		_write
#    define lseek		_lseek
#    define close		_close
#    define pipe(phandles)	_pipe (phandles, 4096, _O_BINARY)
#    define popen		_popen
#    define pclose		_pclose
#    define fdopen		_fdopen
#    define ftruncate(fd, size)	gwin_ftruncate (fd, size)
#    define opendir		gwin_opendir
#    define readdir		gwin_readdir
#    define rewinddir		gwin_rewinddir
#    define closedir		gwin_closedir
#    define NAME_MAX 255
struct DIR
{
  gchar    *dir_name;
  gboolean  just_opened;
  guint     find_file_handle;
  gpointer  find_file_data;
};
typedef struct DIR DIR;
struct dirent
{
  gchar  d_name[NAME_MAX + 1];
};
/* emulation functions */
extern int	gwin_ftruncate	(gint		 f,
				 guint		 size);
DIR*		gwin_opendir	(const gchar	*dirname);
struct dirent*	gwin_readdir  	(DIR		*dir);
void		gwin_rewinddir 	(DIR		*dir);
gint		gwin_closedir  	(DIR		*dir);
#  endif /* _MSC_VER */
#endif	 /* NATIVE_WIN32 */


/* GLib Thread support
 */
typedef struct _GMutex		GMutex;
typedef struct _GCond		GCond;
typedef struct _GPrivate	GPrivate;
typedef struct _GStaticPrivate	GStaticPrivate;
typedef struct _GThreadFunctions GThreadFunctions;
struct _GThreadFunctions
{
  GMutex*  (*mutex_new)       (void);
  void     (*mutex_lock)      (GMutex		*mutex);
  gboolean (*mutex_trylock)   (GMutex		*mutex);
  void     (*mutex_unlock)    (GMutex		*mutex);
  void     (*mutex_free)      (GMutex		*mutex);
  GCond*   (*cond_new)        (void);
  void     (*cond_signal)     (GCond		*cond);
  void     (*cond_broadcast)  (GCond		*cond);
  void     (*cond_wait)       (GCond		*cond,
			       GMutex		*mutex);
  gboolean (*cond_timed_wait) (GCond		*cond,
			       GMutex		*mutex, 
			       GTimeVal 	*end_time);
  void      (*cond_free)      (GCond		*cond);
  GPrivate* (*private_new)    (GDestroyNotify	 destructor);
  gpointer  (*private_get)    (GPrivate		*private_key);
  void      (*private_set)    (GPrivate		*private_key,
			       gpointer		 data);
};

GUTILS_C_VAR GThreadFunctions	g_thread_functions_for_glib_use;
GUTILS_C_VAR gboolean		g_thread_use_default_impl;
GUTILS_C_VAR gboolean		g_threads_got_initialized;

/* initializes the mutex/cond/private implementation for glib, might
 * only be called once, and must not be called directly or indirectly
 * from another glib-function, e.g. as a callback.
 */
void	g_thread_init	(GThreadFunctions	*vtable);

/* internal function for fallback static mutex implementation */
GMutex*	g_static_mutex_get_mutex_impl	(GMutex	**mutex);

/* shorthands for conditional and unconditional function calls */
#define G_THREAD_UF(name, arglist) \
    (*g_thread_functions_for_glib_use . name) arglist
#define G_THREAD_CF(name, fail, arg) \
    (g_thread_supported () ? G_THREAD_UF (name, arg) : (fail))
/* keep in mind, all those mutexes and static mutexes are not 
 * recursive in general, don't rely on that
 */
#define	g_thread_supported()	(g_threads_got_initialized)
#define g_mutex_new()            G_THREAD_UF (mutex_new,      ())
#define g_mutex_lock(mutex)      G_THREAD_CF (mutex_lock,     (void)0, (mutex))
#define g_mutex_trylock(mutex)   G_THREAD_CF (mutex_trylock,  TRUE,    (mutex))
#define g_mutex_unlock(mutex)    G_THREAD_CF (mutex_unlock,   (void)0, (mutex))
#define g_mutex_free(mutex)      G_THREAD_CF (mutex_free,     (void)0, (mutex))
#define g_cond_new()             G_THREAD_UF (cond_new,       ())
#define g_cond_signal(cond)      G_THREAD_CF (cond_signal,    (void)0, (cond))
#define g_cond_broadcast(cond)   G_THREAD_CF (cond_broadcast, (void)0, (cond))
#define g_cond_wait(cond, mutex) G_THREAD_CF (cond_wait,      (void)0, (cond, \
                                                                        mutex))
#define g_cond_free(cond)        G_THREAD_CF (cond_free,      (void)0, (cond))
#define g_cond_timed_wait(cond, mutex, abs_time) G_THREAD_CF (cond_timed_wait, \
                                                              TRUE, \
                                                              (cond, mutex, \
							       abs_time))
#define g_private_new(destructor)	  G_THREAD_UF (private_new, (destructor))
#define g_private_get(private_key)	  G_THREAD_CF (private_get, \
                                                       ((gpointer)private_key), \
                                                       (private_key))
#define g_private_set(private_key, value) G_THREAD_CF (private_set, \
                                                       (void) (private_key = \
                                                        (GPrivate*) (value)), \
                                                       (private_key, value))
/* GStaticMutexes can be statically initialized with the value
 * G_STATIC_MUTEX_INIT, and then they can directly be used, that is
 * much easier, than having to explicitly allocate the mutex before
 * use
 */
#define g_static_mutex_lock(mutex) \
    g_mutex_lock (g_static_mutex_get_mutex (mutex))
#define g_static_mutex_trylock(mutex) \
    g_mutex_trylock (g_static_mutex_get_mutex (mutex))
#define g_static_mutex_unlock(mutex) \
    g_mutex_unlock (g_static_mutex_get_mutex (mutex)) 
struct _GStaticPrivate
{
  guint index;
};
#define G_STATIC_PRIVATE_INIT { 0 }
gpointer g_static_private_get (GStaticPrivate	*private_key);
void     g_static_private_set (GStaticPrivate	*private_key, 
			       gpointer        	 data,
			       GDestroyNotify    notify);

/* these are some convenience macros that expand to nothing if GLib
 * was configured with --disable-threads. for using StaticMutexes,
 * you define them with G_LOCK_DEFINE_STATIC (name) or G_LOCK_DEFINE (name)
 * if you need to export the mutex. With G_LOCK_EXTERN (name) you can
 * declare such an globally defined lock. name is a unique identifier
 * for the protected varibale or code portion. locking, testing and
 * unlocking of such mutexes can be done with G_LOCK(), G_UNLOCK() and
 * G_TRYLOCK() respectively.  
 */
extern void glib_dummy_decl (void);
#define G_LOCK_NAME(name)		(g__ ## name ## _lock)
#ifdef	G_THREADS_ENABLED
#  define G_LOCK_DEFINE_STATIC(name)	static G_LOCK_DEFINE (name)
#  define G_LOCK_DEFINE(name)		\
    GStaticMutex G_LOCK_NAME (name) = G_STATIC_MUTEX_INIT 
#  define G_LOCK_EXTERN(name)		extern GStaticMutex G_LOCK_NAME (name)

#  ifdef G_DEBUG_LOCKS
#    define G_LOCK(name)		G_STMT_START{		  \
        g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,			  \
	       "file %s: line %d (%s): locking: %s ",	          \
	       __FILE__,	__LINE__, G_GNUC_PRETTY_FUNCTION, \
               #name);                                            \
        g_static_mutex_lock (&G_LOCK_NAME (name));                \
     }G_STMT_END
#    define G_UNLOCK(name)		G_STMT_START{		  \
        g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,			  \
	       "file %s: line %d (%s): unlocking: %s ",	          \
	       __FILE__,	__LINE__, G_GNUC_PRETTY_FUNCTION, \
               #name);                                            \
       g_static_mutex_unlock (&G_LOCK_NAME (name));               \
     }G_STMT_END
#    define G_TRYLOCK(name)		G_STMT_START{		  \
        g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,			  \
	       "file %s: line %d (%s): try locking: %s ",         \
	       __FILE__,	__LINE__, G_GNUC_PRETTY_FUNCTION, \
               #name);                                            \
     }G_STMT_END,	g_static_mutex_trylock (&G_LOCK_NAME (name))
#  else	 /* !G_DEBUG_LOCKS */
#    define G_LOCK(name) g_static_mutex_lock	   (&G_LOCK_NAME (name)) 
#    define G_UNLOCK(name) g_static_mutex_unlock   (&G_LOCK_NAME (name))
#    define G_TRYLOCK(name) g_static_mutex_trylock (&G_LOCK_NAME (name))
#  endif /* !G_DEBUG_LOCKS */
#else	/* !G_THREADS_ENABLED */
#  define G_LOCK_DEFINE_STATIC(name)	extern void glib_dummy_decl (void)
#  define G_LOCK_DEFINE(name)		extern void glib_dummy_decl (void)
#  define G_LOCK_EXTERN(name)		extern void glib_dummy_decl (void)
#  define G_LOCK(name)
#  define G_UNLOCK(name)
#  define G_TRYLOCK(name)		(FALSE)
#endif	/* !G_THREADS_ENABLED */

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __G_LIB_H__ */
