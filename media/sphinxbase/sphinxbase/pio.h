/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 1999-2004 Carnegie Mellon University.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * This work was supported in part by funding from the Defense Advanced 
 * Research Projects Agency and the National Science Foundation of the 
 * United States of America, and the CMU Sphinx Speech Consortium.
 *
 * THIS SOFTWARE IS PROVIDED BY CARNEGIE MELLON UNIVERSITY ``AS IS'' AND 
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
 * NOR ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ====================================================================
 *
 */
/*
 * pio.h -- Packaged I/O routines.
 *
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1999 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 * 
 * HISTORY
 * $Log: pio.h,v $
 * Revision 1.3  2005/06/22 08:00:09  arthchan2003
 * Completed all doxygen documentation on file description for libs3decoder/libutil/libs3audio and programs.
 *
 * Revision 1.2  2005/06/22 03:09:52  arthchan2003
 * 1, Fixed doxygen documentation, 2, Added  keyword.
 *
 * Revision 1.2  2005/06/16 00:14:08  archan
 * Added const keyword to file argument for file_open
 *
 * Revision 1.1  2005/06/15 06:11:03  archan
 * sphinx3 to s3.generic: change io.[ch] to pio.[ch]
 *
 * Revision 1.5  2005/06/15 04:21:46  archan
 * 1, Fixed doxygen-documentation, 2, Add  keyword such that changes will be logged into a file.
 *
 * Revision 1.4  2005/04/20 03:49:32  archan
 * Add const to string argument of myfopen.
 *
 * Revision 1.3  2005/03/30 01:22:48  archan
 * Fixed mistakes in last updates. Add
 *
 * 
 * 08-Dec-1999	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Added stat_mtime().
 * 
 * 11-Mar-1999	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Added _myfopen() and myfopen macro.
 * 
 * 05-Sep-97	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Started.
 */


#ifndef _LIBUTIL_IO_H_
#define _LIBUTIL_IO_H_

#include <stdio.h>
#if !defined(_WIN32_WCE) && !(defined(__ADSPBLACKFIN__) && !defined(__linux__))
#include <sys/stat.h>
#endif

/* Win32/WinCE DLL gunk */
#include <sphinxbase/sphinxbase_export.h>
#include <sphinxbase/prim_type.h>

/** \file pio.h
 * \brief file IO related operations.  
 *
 * Custom fopen with error checking is implemented. fopen_comp can
 * open a file with .z, .Z, .gz or .GZ extension
 *  
 * WARNING: Usage of stat_retry will results in 100s of waiting time
 * if the file doesn't exist.  
*/

#ifdef __cplusplus
extern "C" {
#endif
#if 0
/* Fool Emacs. */
}
#endif

/**
 * Like fopen, but use popen and zcat if it is determined that "file" is compressed
 * (i.e., has a .z, .Z, .gz, or .GZ extension).
 */
SPHINXBASE_EXPORT
FILE *fopen_comp (const char *file,		/**< In: File to be opened */
		  const char *mode,		/**< In: "r" or "w", as with normal fopen */
		  int32 *ispipe	/**< Out: On return *ispipe is TRUE iff file
				   was opened via a pipe */
	);

/**
 * Close a file opened using fopen_comp.
 */
SPHINXBASE_EXPORT
void fclose_comp (FILE *fp,		/**< In: File pointer to be closed */
		  int32 ispipe	/**< In: ispipe argument that was returned by the
				   corresponding fopen_comp() call */
	);

/**
 * Open a file for reading, but if file not present try to open compressed version (if
 * file is uncompressed, and vice versa).
 */
SPHINXBASE_EXPORT
FILE *fopen_compchk (const char *file,	/**< In: File to be opened */
		     int32 *ispipe	/**< Out: On return *ispipe is TRUE iff file
					   was opened via a pipe */
	);

/**
 * Wrapper around fopen to check for failure and E_FATAL if failed.
 */
SPHINXBASE_EXPORT
FILE *_myfopen(const char *file, const char *mode,
	       const char *pgm, int32 line);	/* In: __FILE__, __LINE__ from where called */
#define myfopen(file,mode)	_myfopen((file),(mode),__FILE__,__LINE__)


/**
 * NFS file reads seem to fail now and then.  Use the following functions in place of
 * the regular fread.  It retries failed freads several times and quits only if all of
 * them fail.  Be aware, however, that even normal failures such as attempting to read
 * beyond EOF will trigger such retries, wasting about a minute in retries.
 * Arguments identical to regular fread.
 */
SPHINXBASE_EXPORT
int32 fread_retry(void *pointer, int32 size, int32 num_items, FILE *stream);

/**
 * Read a line of arbitrary length from a file and return it as a
 * newly allocated string.
 *
 * @deprecated Use line iterators instead.
 *
 * @param stream The file handle to read from.
 * @param out_len Output: if not NULL, length of the string read.
 * @return allocated string containing the line, or NULL on error or EOF.
 */
SPHINXBASE_EXPORT
char *fread_line(FILE *stream, size_t *out_len);

/**
 * Line iterator for files.
 */
typedef struct lineiter_t {
    char *buf;
    FILE *fh;
    int32 bsiz;
    int32 len;
    int32 clean;
    int32 lineno;
} lineiter_t;

/**
 * Start reading lines from a file.
 */
SPHINXBASE_EXPORT
lineiter_t *lineiter_start(FILE *fh);

/**
 * Start reading lines from a file, skip comments and trim lines.
 */
SPHINXBASE_EXPORT
lineiter_t *lineiter_start_clean(FILE *fh);

/**
 * Move to the next line in the file.
 */
SPHINXBASE_EXPORT
lineiter_t *lineiter_next(lineiter_t *li);

/**
 * Stop reading lines from a file.
 */
SPHINXBASE_EXPORT
void lineiter_free(lineiter_t *li);

/**
 * Returns current line number.
 */
SPHINXBASE_EXPORT
int lineiter_lineno(lineiter_t *li);


#ifdef _WIN32_WCE
/* Fake this for WinCE which has no stat() */
#include <windows.h>
struct stat {
    DWORD st_mtime;
    DWORD st_size;
};
#endif /* _WIN32_WCE */

#if defined(__ADSPBLACKFIN__) && !defined(__linux__)
struct stat {
    int32 st_mtime;
    int32 st_size;
};

#endif

/**
 * Bitstream encoder - for writing compressed files.
 */
typedef struct bit_encode_s bit_encode_t;

/**
 * Attach bitstream encoder to a file.
 */
bit_encode_t *bit_encode_attach(FILE *outfh);

/**
 * Retain pointer to a bit encoder.
 */
bit_encode_t *bit_encode_retain(bit_encode_t *be);

/**
 * Release pointer to a bit encoder.
 *
 * Note that this does NOT flush any leftover bits.
 */
int bit_encode_free(bit_encode_t *be);

/**
 * Write bits to encoder.
 */
int bit_encode_write(bit_encode_t *be, unsigned char const *bits, int nbits);

/**
 * Write lowest-order bits of codeword to encoder.
 */
int bit_encode_write_cw(bit_encode_t *be, uint32 codeword, int nbits);

/**
 * Flush any unwritten bits, zero-padding if necessary.
 */
int bit_encode_flush(bit_encode_t *be);

/**
 * There is no bitstream decoder, because a stream abstraction is too
 * slow.  Instead we read blocks of bits and treat them as bitvectors.
 */

/**
 * Like fread_retry, but for stat.  Arguments identical to regular stat.
 * Return value: 0 if successful, -1 if stat failed several attempts.
 */
SPHINXBASE_EXPORT
int32 stat_retry (const char *file, struct stat *statbuf);

/**
 * Return time of last modification for the given file, or -1 if stat fails.
 */

SPHINXBASE_EXPORT
int32 stat_mtime (const char *file);

/**
 * Create a directory and all of its parent directories, as needed.
 *
 * @return 0 on success, <0 on failure.
 */
SPHINXBASE_EXPORT
int build_directory(const char *path);

#ifdef __cplusplus
}
#endif

#endif
