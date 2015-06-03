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
 * blkarray_list.h -- array-based list structure, for memory and access
 * 	efficiency.
 * 
 * HISTORY
 * 
 * $Log: blkarray_list.h,v $
 * Revision 1.1.1.1  2006/05/23 18:45:02  dhuggins
 * re-importation
 *
 * Revision 1.2  2004/12/10 16:48:58  rkm
 * Added continuous density acoustic model handling
 *
 * Revision 1.1  2004/07/16 00:57:12  egouvea
 * Added Ravi's implementation of FSG support.
 *
 * Revision 1.2  2004/05/27 14:22:57  rkm
 * FSG cross-word triphones completed (but for single-phone words)
 *
 * Revision 1.1.1.1  2004/03/01 14:30:31  rkm
 *
 *
 * Revision 1.1  2004/02/26 01:14:48  rkm
 * *** empty log message ***
 *
 * 
 * 18-Feb-2004	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon
 * 		Started.
 */


#ifndef __S2_BLKARRAY_LIST_H__
#define __S2_BLKARRAY_LIST_H__


#include <sphinxbase/prim_type.h>


/*
 * For maintaining a (conceptual) "list" of pointers to arbitrary data.
 * The application is responsible for knowing the true data type.
 * Use an array instead of a true list for efficiency (both memory and
 * speed).  But use a blocked (2-D) array to allow dynamic resizing at a
 * coarse grain.  An entire block is allocated or freed, as appropriate.
 */
typedef struct blkarray_list_s {
  void ***ptr;		/* ptr[][] is the user-supplied ptr */
  int32 maxblks;	/* size of ptr (#rows) */
  int32 blksize;	/* size of ptr[] (#cols, ie, size of each row) */
  int32 n_valid;	/* # entries actually stored in the list */
  int32 cur_row;	/* The current row being that has empty entry */
  int32 cur_row_free;	/* First entry valid within the current row */
} blkarray_list_t;

/* Access macros */
#define blkarray_list_ptr(l,r,c)	((l)->ptr[r][c])
#define blkarray_list_maxblks(l)	((l)->maxblks)
#define blkarray_list_blksize(l)	((l)->blksize)
#define blkarray_list_n_valid(l)	((l)->n_valid)
#define blkarray_list_cur_row(l)	((l)->cur_row)
#define blkarray_list_cur_row_free(l)	((l)->cur_row_free)


/*
 * Initialize and return a new blkarray_list containing an empty list
 * (i.e., 0 length).  Sized for the given values of maxblks and blksize.
 * NOTE: (maxblks * blksize) should not overflow int32, but this is not
 * checked.
 * Return the allocated entry if successful, NULL if any error.
 */
blkarray_list_t *_blkarray_list_init (int32 maxblks, int32 blksize);


/*
 * Like _blkarray_list_init() above, but for some default values of
 * maxblks and blksize.
 */
blkarray_list_t *blkarray_list_init ( void );

/**
 * Completely finalize a blkarray_list.
 */
void blkarray_list_free(blkarray_list_t *bl);


/*
 * Append the given new entry (data) to the end of the list.
 * Return the index of the entry if successful, -1 if any error.
 * The returned indices are guaranteed to be successive integers (i.e.,
 * 0, 1, 2...) for successive append operations, until the list is reset,
 * when they resume from 0.
 */
int32 blkarray_list_append (blkarray_list_t *, void *data);


/*
 * Free all the entries in the list (using ckd_free) and reset the
 * list length to 0.
 */
void blkarray_list_reset (blkarray_list_t *);


/* Gets n-th element of the array list */
void * blkarray_list_get(blkarray_list_t *, int32 n);

#endif
