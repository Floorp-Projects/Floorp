/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 1999-2001 Carnegie Mellon University.  All rights
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
 * profile.h -- For timing and event counting.
 *
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1999 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 * 
 * HISTORY
 * $Log: profile.h,v $
 * Revision 1.10  2005/06/22 03:10:59  arthchan2003
 * 1, Fixed doxygen documentation, 2, Added  keyword.
 *
 * Revision 1.5  2005/06/15 04:21:47  archan
 * 1, Fixed doxygen-documentation, 2, Add  keyword such that changes will be logged into a file.
 *
 * Revision 1.4  2005/04/25 19:22:48  archan
 * Refactor out the code of rescoring from lexical tree. Potentially we want to turn off the rescoring if we need.
 *
 * Revision 1.3  2005/03/30 01:22:48  archan
 * Fixed mistakes in last updates. Add
 *
 * 
 * 11-Mar-1999	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Added ptmr_init().
 * 
 * 19-Jun-97	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Created from earlier Sphinx-3 version.
 */


#ifndef _LIBUTIL_PROFILE_H_
#define _LIBUTIL_PROFILE_H_

#ifdef __cplusplus
extern "C" {
#endif
#if 0
} /* Fool Emacs into not indenting things. */
#endif
  
/** \file profile.h
 * \brief Implementation of profiling, include counting , timing, cpu clock checking
 *  
 * Currently, function host_endian is also in this function. It is
 * not documented.  
 */

#include <stdio.h>

/* Win32/WinCE DLL gunk */
#include <sphinxbase/sphinxbase_export.h>
#include <sphinxbase/prim_type.h>


/**
 * \struct pctr_t 
 *
 * Generic event counter for profiling.  User is responsible for allocating an array
 * of the desired number.  There should be a sentinel with name = NULL.
 */
typedef struct {
	char *name;		/**< Counter print name; NULL 
				   terminates array of counters
				   Used by pctr_print_all */
	int32 count;		/**< Counter value */
} pctr_t;

/**
 * operations of pctr_t
 */

/**
 * Initialize a counter
 * @return an initialized counter 
 */ 
SPHINXBASE_EXPORT
pctr_t* pctr_new (
	char *name   /**< The name of the counter */
	);

/**
 * Reset a counter
 */ 

SPHINXBASE_EXPORT
void pctr_reset (pctr_t *ctr  /**< A pointer of a counter */
	);

/**
 * Print a counter
 */ 
SPHINXBASE_EXPORT
void pctr_print(FILE *fp,      /**< A file pointer */
		pctr_t *ctr   /**< A pointer of a counter */
	);

/**
 * Increment a counter
 */ 
SPHINXBASE_EXPORT
void pctr_increment (pctr_t *ctr, /**< A pointer of a counter */
		     int32 inc   /**< The increment of the counter */
	);

/**
   Free the counter 
*/
SPHINXBASE_EXPORT
void pctr_free(pctr_t* ctr /**< A pointer of a counter */ 
	);


/**
 * \struct ptmr_t
 * Generic timer structures and functions for coarse-grained performance measurements
 * using standard system calls.
 */
typedef struct {
	const char *name;		/**< Timer print name; NULL terminates an array of timers.
					   Used by ptmr_print_all */
	float64 t_cpu;		/**< CPU time accumulated since most recent reset op */
	float64 t_elapsed;		/**< Elapsed time accumulated since most recent reset */
	float64 t_tot_cpu;		/**< Total CPU time since creation */
	float64 t_tot_elapsed;	/**< Total elapsed time since creation */
	float64 start_cpu;		/**< ---- FOR INTERNAL USE ONLY ---- */
	float64 start_elapsed;	/**< ---- FOR INTERNAL USE ONLY ---- */
} ptmr_t;



/** Start timing using tmr */
SPHINXBASE_EXPORT
void ptmr_start (ptmr_t *tmr /**< The timer*/
	);

/** Stop timing and accumulate tmr->{t_cpu, t_elapsed, t_tot_cpu, t_tot_elapsed} */
SPHINXBASE_EXPORT
void ptmr_stop (ptmr_t *tmr  /**< The timer*/
	);

/** Reset tmr->{t_cpu, t_elapsed} to 0.0 */
SPHINXBASE_EXPORT
void ptmr_reset (ptmr_t *tmr  /**< The timer*/
	);

/** Reset tmr->{t_cpu, t_elapsed, t_tot_cpu, t_tot_elapsed} to 0.0 
 */
SPHINXBASE_EXPORT
void ptmr_init (ptmr_t *tmr /**< The timer*/
	);


/**
 * Reset t_cpu, t_elapsed of all timer modules in array tmr[] to 0.0.
 * The array should be terminated with a sentinel with .name = NULL.
 */
SPHINXBASE_EXPORT
void ptmr_reset_all (ptmr_t *tmr /**< The timer*/
	);

/**
 * Print t_cpu for all timer modules in tmr[], normalized by norm (i.e., t_cpu/norm).
 * The array should be terminated with a sentinel with .name = NULL.
 */
SPHINXBASE_EXPORT
void ptmr_print_all (FILE *fp,    /**< The file pointer */
		     ptmr_t *tmr, /**< The timer*/
		     float64 norm
	);


/**
 * Return the processor clock speed (in MHz); only available on some machines (Alphas).
 * The dummy argument can be any integer value.
 */
SPHINXBASE_EXPORT
int32 host_pclk (int32 dummy);


/*
 * Check the native byte-ordering of the machine by writing a magic
 * number to a temporary file and reading it back.  * Return value:
 * 0 if BIG-ENDIAN, 1 if LITTLE-ENDIAN, -1 if error.  
 */
SPHINXBASE_EXPORT
int32 host_endian ( void );

#ifdef __cplusplus
}
#endif

#endif
