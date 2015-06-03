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
 * bio.h -- Sphinx-3 binary file I/O functions.
 * 
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1996 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 * 
 * HISTORY
 * $Log: bio.h,v $
 * Revision 1.8  2005/06/21 20:40:46  arthchan2003
 * 1, Fixed doxygen documentation, 2, Add the $ keyword.
 *
 * Revision 1.5  2005/06/13 04:02:57  archan
 * Fixed most doxygen-style documentation under libs3decoder.
 *
 * Revision 1.4  2005/05/10 21:21:52  archan
 * Three functionalities added but not tested. Code on 1) addition/deletion of LM in mode 4. 2) reading text-based LM 3) Converting txt-based LM to dmp-based LM.
 *
 * Revision 1.3  2005/03/30 01:22:46  archan
 * Fixed mistakes in last updates. Add
 *
 * 
 * 28-Apr-1999	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Created.
 */


#ifndef _S3_BIO_H_
#define _S3_BIO_H_

#include <stdio.h>
#include <stdarg.h>

/* Win32/WinCE DLL gunk */
#include <sphinxbase/sphinxbase_export.h>
#include <sphinxbase/prim_type.h>
#include <sphinxbase/byteorder.h>

/** \file bio.h
 * \brief Cross platform binary IO to process files in sphinx3 format. 
 * 
 *
 */

#ifdef __cplusplus
extern "C" {
#endif
#if 0
/* Fool Emacs. */
}
#endif

#define BYTE_ORDER_MAGIC	(0x11223344)

/** "reversed senses" SWAP, ARCHAN: This is still incorporated in
    Sphinx 3 because lm3g2dmp used it.  Don't think that I am very
    happy with it. */

#if (__BIG_ENDIAN__)
#define REVERSE_SENSE_SWAP_INT16(x)  x = ( (((x)<<8)&0x0000ff00) | (((x)>>8)&0x00ff) )
#define REVERSE_SENSE_SWAP_INT32(x)  x = ( (((x)<<24)&0xff000000) | (((x)<<8)&0x00ff0000) | \
                         (((x)>>8)&0x0000ff00) | (((x)>>24)&0x000000ff) )
#else
#define REVERSE_SENSE_SWAP_INT16(x)
#define REVERSE_SENSE_SWAP_INT32(x)

#endif



/**
 * Read binary file format header: has the following format
 * <pre>
 *     s3
 *     <argument-name> <argument-value>
 *     <argument-name> <argument-value>
 *     ...
 *     endhdr
 *     4-byte byte-order word used to find file byte ordering relative to host machine.
 * </pre>
 * Lines beginning with # are ignored.
 * Memory for name and val allocated by this function; use bio_hdrarg_free to free them.
 * @return 0 if successful, -1 otherwise.
 */
SPHINXBASE_EXPORT
int32 bio_readhdr (FILE *fp,		/**< In: File to read */
		   char ***name,	/**< Out: array of argument name strings read */
		   char ***val,		/**< Out: corresponding value strings read */
		   int32 *swap	/**< Out: file needs byteswapping iff (*swap) */
		   );
/**
 * Write a simple binary file header, containing only the version string.  Also write
 * the byte order magic word.
 * @return 0 if successful, -1 otherwise.
 */
SPHINXBASE_EXPORT
int32 bio_writehdr_version (FILE *fp,  /**< Output: File to write */
			    char *version /**< Input: A string of version */
	);


/**
 * Write a simple binary file header with only byte order magic word.
 * @return 0 if successful, -1 otherwise.
 */
SPHINXBASE_EXPORT
int32 bio_writehdr(FILE *fp, ...);

/**
 * Free name and value strings previously allocated and returned by bio_readhdr.
 */
SPHINXBASE_EXPORT
void bio_hdrarg_free (char **name,	/**< In: Array previously returned by bio_readhdr */
		      char **val	/**< In: Array previously returned by bio_readhdr */
		      );

/**
 * Like fread but perform byteswapping and accumulate checksum (the 2 extra arguments).
 *
 * @return unlike fread, returns -1 if required number of elements (n_el) not read; also,
 * no byteswapping or checksum accumulation is performed in that case.
 */
SPHINXBASE_EXPORT
int32 bio_fread (void *buf,		/**< In: buffer to write */
		 int32 el_sz,		/**< In: element size */
		 int32 n_el,		/**< In: number of elements */
		 FILE *fp,              /**< In: An input file pointer */
		 int32 swap,		/**< In: Byteswap iff (swap != 0) */
		 uint32 *chksum	/**< In/Out: Accumulated checksum */
		 );

/**
 * Like fwrite but perform byteswapping and accumulate checksum (the 2 extra arguments).
 *
 * @return the number of elemens written (like fwrite).
 */
SPHINXBASE_EXPORT
int32 bio_fwrite(const void *buf,	/**< In: buffer to write */
		 int32 el_sz,		/**< In: element size */
		 int32 n_el,		/**< In: number of elements */
		 FILE *fp,              /**< In: An input file pointer */
		 int32 swap,		/**< In: Byteswap iff (swap != 0) */
		 uint32 *chksum	/**< In/Out: Accumulated checksum */
		 );

/**
 * Read a 1-d array (fashioned after fread):
 *
 *  - 4-byte array size (returned in n_el)
 *  - memory allocated for the array and read (returned in buf)
 * 
 * Byteswapping and checksum accumulation performed as necessary.
 * Fails fatally if expected data not read.
 * @return number of array elements allocated and read; -1 if error.
 */
SPHINXBASE_EXPORT
int32 bio_fread_1d (void **buf,		/**< Out: contains array data; allocated by this
					   function; can be freed using ckd_free */
		    size_t el_sz,	/**< In: Array element size */
		    uint32 *n_el,	/**< Out: Number of array elements allocated/read */
		    FILE *fp,		/**< In: File to read */
		    int32 sw,		/**< In: Byteswap iff (swap != 0) */
		    uint32 *ck	/**< In/Out: Accumulated checksum */
		    );

/**
 * Read a 2-d matrix:
 *
 * - 4-byte # rows, # columns (returned in d1, d2, d3)
 * - memory allocated for the array and read (returned in buf)
 *
 * Byteswapping and checksum accumulation performed as necessary.
 * Fails fatally if expected data not read.
 * @return number of array elements allocated and read; -1 if error.
 */
SPHINXBASE_EXPORT
int32 bio_fread_2d(void ***arr,
                   size_t e_sz,
                   uint32 *d1,
                   uint32 *d2,
                   FILE *fp,
                   uint32 swap,
                   uint32 *chksum);

/**
 * Read a 3-d array (set of matrices)
 *
 * - 4-byte # matrices, # rows, # columns (returned in d1, d2, d3)
 * - memory allocated for the array and read (returned in buf)
 *
 * Byteswapping and checksum accumulation performed as necessary.
 * Fails fatally if expected data not read.
 * @return number of array elements allocated and read; -1 if error.
 */
SPHINXBASE_EXPORT
int32 bio_fread_3d(void ****arr,
                   size_t e_sz,
                   uint32 *d1,
                   uint32 *d2,
                   uint32 *d3,
                   FILE *fp,
                   uint32 swap,
                   uint32 *chksum);

/**
 * Read and verify checksum at the end of binary file.  Fails fatally if there is
 * a mismatch.
 */
SPHINXBASE_EXPORT
void bio_verify_chksum (FILE *fp,	/**< In: File to read */
			int32 byteswap,	/**< In: Byteswap iff (swap != 0) */
			uint32 chksum	/**< In: Value to compare with checksum in file */
			);



/**
 * Write a 1-d array.
 * Checksum accumulation performed as necessary.
 *
 * @return number of array elements successfully written or -1 if error.
 */
SPHINXBASE_EXPORT
int bio_fwrite_1d(void *arr,       /**< In: Data to write */
                  size_t e_sz,     /**< In: Size of the elements in bytes */
                  uint32 d1,       /**< In: First dimension */
                  FILE *fp,        /**< In: File to write to */
                  uint32 *chksum   /**< In/Out: Checksum accumulator */
                  );

/**
 * Write a 3-d array (set of matrices).
 * Checksum accumulation performed as necessary.
 *
 * @return number of array elements successfully written or -1 if error.
 */
SPHINXBASE_EXPORT
int bio_fwrite_3d(void ***arr,    /**< In: Data to write */
                  size_t e_sz,    /**< In: Size of the elements in bytes */
                  uint32 d1,      /**< In: First dimension */
                  uint32 d2,      /**< In: Second dimension */
                  uint32 d3,      /**< In: Third dimension */
                  FILE *fp,       /**< In: File to write to */
                  uint32 *chksum  /**< In/Out: Checksum accumulator */
                  );

/**
 * Read raw data from the wav file.
 *
 * @return pointer to the data.
 */
SPHINXBASE_EXPORT
int16* bio_read_wavfile(char const *directory, /**< In: the folder where the file is located */
			char const *filename,  /**< In: the name of the file */
			char const *extension, /**< In: file extension */
			int32 header,          /**< In: the size of the header to skip usually 44 bytes */
			int32 endian,          /**< In: endian of the data */
			size_t *nsamps         /**< Out: number of samples read */
			);

#ifdef __cplusplus
}
#endif

#endif
