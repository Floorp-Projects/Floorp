/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 1999-2013 Carnegie Mellon University.  All rights
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
 * fsg_lextree.h -- The collection of all the lextrees for the entire FSM.
 *
 */

#ifndef __S2_FSG_LEXTREE_H__
#define __S2_FSG_LEXTREE_H__

/* SphinxBase headers. */
#include <sphinxbase/cmd_ln.h>
#include <sphinxbase/fsg_model.h>

/* Local headers. */
#include "hmm.h"
#include "dict.h"
#include "dict2pid.h"

/*
 * Compile-time constant determining the size of the
 * bitvector fsg_pnode_t.fsg_pnode_ctxt_t.bv.  (See below.)
 * But it makes memory allocation simpler and more efficient.
 * Make it smaller (2) to save memory if your phoneset has less than
 * 64 phones.
 */
#define FSG_PNODE_CTXT_BVSZ	4

typedef struct {
    uint32 bv[FSG_PNODE_CTXT_BVSZ];
} fsg_pnode_ctxt_t;


/*
 * All transitions (words) out of any given FSG state represented are by a
 * phonetic prefix lextree (except for epsilon or null transitions; they
 * are not part of the lextree).  Lextree leaf nodes represent individual
 * FSG transitions, so no sharing is allowed at the leaf nodes.  The FSG
 * transition probs are distributed along the lextree: the prob at a node
 * is the max of the probs of all leaf nodes (and, hence, FSG transitions)
 * reachable from that node.
 * 
 * To conserve memory, the underlying HMMs with state-level information are
 * allocated only as needed.  Root and leaf nodes must also account for all
 * the possible phonetic contexts, with an independent HMM for each distinct
 * context.
 */
typedef struct fsg_pnode_s {
    /*
     * If this is not a leaf node, the first successor (child) node.  Otherwise
     * the parent FSG transition for which this is the leaf node (for figuring
     * the FSG destination state, and word emitted by the transition).  A node
     * may have several children.  The succ ptr gives just the first; the rest
     * are linked via the sibling ptr below.
     */
    union {
        struct fsg_pnode_s *succ;
        fsg_link_t *fsglink;
    } next;
  
    /*
     * For simplicity of memory management (i.e., freeing the pnodes), all
     * pnodes allocated for all transitions out of a state are maintained in a
     * linear linked list through the alloc_next pointer.
     */
    struct fsg_pnode_s *alloc_next;
  
    /*
     * The next node that is also a child of the parent of this node; NULL if
     * none.
     */
    struct fsg_pnode_s *sibling;

    /*
     * The transition (log) probability to be incurred upon transitioning to
     * this node.  (Transition probabilities are really associated with the
     * transitions.  But a lextree node has exactly one incoming transition.
     * Hence, the prob can be associated with the node.)
     * This is a logs2(prob) value, and includes the language weight.
     */
    int32 logs2prob;
  
    /*
     * The root and leaf positions associated with any transition have to deal
     * with multiple phonetic contexts.  However, different contexts may result
     * in the same SSID (senone-seq ID), and can share a single pnode with that
     * SSID.  But the pnode should track the set of context CI phones that share
     * it.  Hence the fsg_pnode_ctxt_t bit-vector set-representation.  (For
     * simplicity of implementation, its size is a compile-time constant for
     * now.)  Single phone words would need a 2-D array of context, but that's
     * too expensive.  For now, they simply use SIL as right context, so only
     * the left context is properly modelled.
     * (For word-internal phones, this field is unused, of course.)
     */
    fsg_pnode_ctxt_t ctxt;
  
    uint16 ci_ext;	/* This node's CIphone as viewed externally (context) */
    uint8 ppos;	/* Phoneme position in pronunciation */
    uint8 leaf;	/* Whether this is a leaf node */
  
    /* HMM-state-level stuff here */
    hmm_context_t *ctx;
    hmm_t hmm;
} fsg_pnode_t;

/* Access macros */
#define fsg_pnode_leaf(p)	((p)->leaf)
#define fsg_pnode_logs2prob(p)	((p)->logs2prob)
#define fsg_pnode_succ(p)	((p)->next.succ)
#define fsg_pnode_fsglink(p)	((p)->next.fsglink)
#define fsg_pnode_sibling(p)	((p)->sibling)
#define fsg_pnode_hmmptr(p)	(&((p)->hmm))
#define fsg_pnode_ci_ext(p)	((p)->ci_ext)
#define fsg_pnode_ppos(p)	((p)->ppos)
#define fsg_pnode_leaf(p)	((p)->leaf)
#define fsg_pnode_ctxt(p)	((p)->ctxt)

#define fsg_pnode_add_ctxt(p,c)	((p)->ctxt.bv[(c)>>5] |= (1 << ((c)&0x001f)))

/*
 * The following is macroized because its called very frequently
 * ::: uint32 fsg_pnode_ctxt_sub (fsg_pnode_ctxt_t *src, fsg_pnode_ctxt_t *sub);
 */
/*
 * Subtract bitvector sub from bitvector src (src updated with the result).
 * Return 0 if result is all 0, non-zero otherwise.
 */

#if (FSG_PNODE_CTXT_BVSZ == 1)
    #define FSG_PNODE_CTXT_SUB(src,sub) \
    ((src)->bv[0] = (~((sub)->bv[0]) & (src)->bv[0]))
#elif (FSG_PNODE_CTXT_BVSZ == 2)
    #define FSG_PNODE_CTXT_SUB(src,sub) \
    (((src)->bv[0] = (~((sub)->bv[0]) & (src)->bv[0])) | \
     ((src)->bv[1] = (~((sub)->bv[1]) & (src)->bv[1])))
#elif (FSG_PNODE_CTXT_BVSZ == 4)
    #define FSG_PNODE_CTXT_SUB(src,sub) \
    (((src)->bv[0] = (~((sub)->bv[0]) & (src)->bv[0]))  | \
     ((src)->bv[1] = (~((sub)->bv[1]) & (src)->bv[1]))  | \
     ((src)->bv[2] = (~((sub)->bv[2]) & (src)->bv[2]))  | \
     ((src)->bv[3] = (~((sub)->bv[3]) & (src)->bv[3])))
#else
    #define FSG_PNODE_CTXT_SUB(src,sub) fsg_pnode_ctxt_sub_generic((src),(sub))
#endif

/**
 * Collection of lextrees for an FSG.
 */
typedef struct fsg_lextree_s {
    fsg_model_t *fsg;	/**< The fsg for which this lextree is built. */
    hmm_context_t *ctx; /**< HMM context structure. */
    dict_t *dict;     /**< Pronunciation dictionary for this FSG. */
    dict2pid_t *d2p;    /**< Context-dependent phone mappings for this FSG. */
    bin_mdef_t *mdef;   /**< Model definition (triphone mappings). */

    /*
     * Left and right CIphone sets for each state.
     * Left context CIphones for a state S: If word W transitions into S, W's
     * final CIphone is in S's {lc}.  Words transitioning out of S must consider
     * these left context CIphones.
     * Similarly, right contexts for state S: If word W transitions out of S,
     * W's first CIphone is in S's {rc}.  Words transitioning into S must consider
     * these right contexts.
     * 
     * NOTE: Words may transition into and out of S INDIRECTLY, with intermediate
     *   null transitions.
     * NOTE: Single-phone words are difficult; only SILENCE right context is
     *   modelled for them.
     * NOTE: Non-silence filler phones aren't included in these sets.  Filler
     *   words don't use context, and present the SILENCE phone as context to
     *   adjacent words.
     */
    int16 **lc;         /**< Left context triphone mappings for FSG. */
    int16 **rc;         /**< Right context triphone mappings for FSG. */

    fsg_pnode_t **root;	/* root[s] = lextree representing all transitions
			   out of state s.  Note that the "tree" for each
			   state is actually a collection of trees, linked
			   via fsg_pnode_t.sibling (root[s]->sibling) */
    fsg_pnode_t **alloc_head;	/* alloc_head[s] = head of linear list of all
				   pnodes allocated for state s */
    int32 n_pnode;	/* #HMM nodes in search structure */
    int32 wip;
    int32 pip;
} fsg_lextree_t;

/* Access macros */
#define fsg_lextree_root(lt,s)	((lt)->root[s])
#define fsg_lextree_n_pnode(lt)	((lt)->n_pnode)

/**
 * Create, initialize, and return a new phonetic lextree for the given FSG.
 */
fsg_lextree_t *fsg_lextree_init(fsg_model_t *fsg, dict_t *dict,
                                dict2pid_t *d2p,
				bin_mdef_t *mdef, hmm_context_t *ctx,
				int32 wip, int32 pip);

/**
 * Free lextrees for an FSG.
 */
void fsg_lextree_free(fsg_lextree_t *fsg);

/**
 * Print an FSG lextree to a file for debugging.
 */
void fsg_lextree_dump(fsg_lextree_t *fsg, FILE *fh);

/**
 * Mark the given pnode as inactive (for search).
 */
void fsg_psubtree_pnode_deactivate(fsg_pnode_t *pnode);

/**
 *  Set all flags on in the given context bitvector.
 */
void fsg_pnode_add_all_ctxt(fsg_pnode_ctxt_t *ctxt);

/**
 *  Generic variant for arbitrary size
 */
uint32 fsg_pnode_ctxt_sub_generic(fsg_pnode_ctxt_t *src, fsg_pnode_ctxt_t *sub);

#endif
