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
 * glist.h -- Module for maintaining a generic, linear linked-list structure.
 *
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1999 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 * 
 * HISTORY
 * $Log: glist.h,v $
 * Revision 1.9  2005/06/22 03:02:51  arthchan2003
 * 1, Fixed doxygen documentation, 2, add  keyword.
 *
 * Revision 1.4  2005/05/03 04:09:11  archan
 * Implemented the heart of word copy search. For every ci-phone, every word end, a tree will be allocated to preserve its pathscore.  This is different from 3.5 or below, only the best score for a particular ci-phone, regardless of the word-ends will be preserved at every frame.  The graph propagation will not collect unused word tree at this point. srch_WST_propagate_wd_lv2 is also as the most stupid in the century.  But well, after all, everything needs a start.  I will then really get the results from the search and see how it looks.
 *
 * Revision 1.3  2005/03/30 01:22:48  archan
 * Fixed mistakes in last updates. Add
 *
 * 
 * 09-Mar-1999	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Added glist_chkdup_*().
 * 
 * 13-Feb-1999	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Created from earlier version.
 */


/**
 * \file glist.h
 * \brief Generic linked-lists maintenance.
 *
 * Only insert at the head of the list.  A convenient little
 * linked-list package, but a double-edged sword: the user must keep
 * track of the data type within the linked list elements.  When it
 * was first written, there was no selective deletions except to
 * destroy the entire list.  This is modified in later version. 
 * 
 * 
 * (C++ would be good for this, but that's a double-edged sword as well.)
 */


#ifndef _LIBUTIL_GLIST_H_
#define _LIBUTIL_GLIST_H_

#include <stdlib.h>
/* Win32/WinCE DLL gunk */
#include <sphinxbase/sphinxbase_export.h>
#include <sphinxbase/prim_type.h>

#ifdef __cplusplus
extern "C" {
#endif
#if 0
/* Fool Emacs. */
}
#endif

/** A node in a generic list 
 */
typedef struct gnode_s {
	anytype_t data;		/** See prim_type.h */
	struct gnode_s *next;	/** Next node in list */
} gnode_t;
typedef gnode_t *glist_t;	/** Head of a list of gnodes */


/** Access macros, for convenience 
 */
#define gnode_ptr(g)		((g)->data.ptr)
#define gnode_int32(g)		((g)->data.i)
#define gnode_uint32(g)		((g)->data.ui)
#define gnode_float32(g)	((float32)(g)->data.fl)
#define gnode_float64(g)	((g)->data.fl)
#define gnode_next(g)		((g)->next)


/**
 * Create and prepend a new list node, with the given user-defined data, at the HEAD
 * of the given generic list.  Return the new list thus formed.
 * g may be NULL to indicate an initially empty list.
 */
SPHINXBASE_EXPORT
glist_t glist_add_ptr (glist_t g,  /**< a link list */
		       void *ptr   /**< a pointer */
	);

/**
 * Create and prepend a new list node containing an integer.
 */  
SPHINXBASE_EXPORT
glist_t glist_add_int32 (glist_t g, /**< a link list */
			 int32 val  /**< an integer value */
	);
/**
 * Create and prepend a new list node containing an unsigned integer.
 */  
SPHINXBASE_EXPORT
glist_t glist_add_uint32 (glist_t g,  /**< a link list */
			  uint32 val  /**< an unsigned integer value */
	);
/**
 * Create and prepend a new list node containing a single-precision float.
 */  
SPHINXBASE_EXPORT
glist_t glist_add_float32 (glist_t g, /**< a link list */
			   float32 val /**< a float32 vlaue */
	);
/**
 * Create and prepend a new list node containing a double-precision float.
 */  
SPHINXBASE_EXPORT
glist_t glist_add_float64 (glist_t g, /**< a link list */
			   float64 val  /**< a float64 vlaue */
	);



/**
 * Create and insert a new list node, with the given user-defined data, after
 * the given generic node gn.  gn cannot be NULL.
 * Return ptr to the newly created gnode_t.
 */
SPHINXBASE_EXPORT
gnode_t *glist_insert_ptr (gnode_t *gn, /**< a generic node which ptr will be inserted after it*/
			   void *ptr /**< pointer inserted */
	);
/**
 * Create and insert a new list node containing an integer.
 */  
SPHINXBASE_EXPORT
gnode_t *glist_insert_int32 (gnode_t *gn, /**< a generic node which a value will be inserted after it*/
			     int32 val /**< int32 inserted */
	);
/**
 * Create and insert a new list node containing an unsigned integer.
 */  
SPHINXBASE_EXPORT
gnode_t *glist_insert_uint32 (gnode_t *gn, /**< a generic node which a value will be inserted after it*/
			      uint32 val /**< uint32 inserted */
	);
/**
 * Create and insert a new list node containing a single-precision float.
 */  
SPHINXBASE_EXPORT
gnode_t *glist_insert_float32 (gnode_t *gn, /**< a generic node which a value will be inserted after it*/
			       float32 val /**< float32 inserted */
	);
/**
 * Create and insert a new list node containing a double-precision float.
 */  
SPHINXBASE_EXPORT
gnode_t *glist_insert_float64 (gnode_t *gn, /**< a generic node which a value will be inserted after it*/
			       float64 val /**< float64 inserted */
	);

/**
 * Reverse the order of the given glist.  (glist_add() adds to the head; one might
 * ultimately want the reverse of that.)
 * NOTE: The list is reversed "in place"; i.e., no new memory is allocated.
 * @return: The head of the new list.
 */
SPHINXBASE_EXPORT
glist_t glist_reverse (glist_t g /**< input link list */
	);


/**
   Count the number of element in a given link list 
   @return the number of elements in the given glist_t 
*/
SPHINXBASE_EXPORT
int32 glist_count (glist_t g /**< input link list */
	);

/**
 * Free the given generic list; user-defined data contained within is not
 * automatically freed.  The caller must have done that already.
 */
SPHINXBASE_EXPORT
void glist_free (glist_t g);


/**
 * Free the given node, gn, of a glist, pred being its predecessor in the list.
 * Return ptr to the next node in the list after the freed node.
 */
SPHINXBASE_EXPORT
gnode_t *gnode_free(gnode_t *gn, 
		    gnode_t *pred
	);

/**
 * Return the last node in the given list.
 */
SPHINXBASE_EXPORT
gnode_t *glist_tail (glist_t g);

#ifdef __cplusplus
}
#endif

#endif
