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

/**
 * @file hmm.h Hidden Markov Model base structures.
 */

#ifndef __HMM_H__
#define __HMM_H__

/* System headers. */
#include <stdio.h>

/* SphinxBase headers. */
#include <sphinxbase/fixpoint.h>
#include <sphinxbase/listelem_alloc.h>

/* PocketSphinx headers. */
#include "bin_mdef.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * Type for frame index values. Used in HMM indexes and 
 * backpointers and affects memory required.Due to limitations of FSG
 * search implementation this value needs to be signed.
 */
typedef int32 frame_idx_t;

/**
 * Maximum number of frames in index, should be in sync with above.
 */
#define MAX_N_FRAMES MAX_INT32


/** Shift count for senone scores. */
#define SENSCR_SHIFT 10

/**
 * Large "bad" score.
 *
 * This number must be "bad" enough so that 4 times WORST_SCORE will
 * not overflow. The reason for this is that the search doesn't check
 * the scores in a model before evaluating the model and it may
 * require as many was 4 plies before the new 'good' score can wipe
 * out the initial WORST_SCORE initialization.
 */
#define WORST_SCORE		((int)0xE0000000)

/**
 * Watch out, though!  Transition matrix entries that are supposed to
 * be "zero" don't actually get that small due to quantization.
 */
#define TMAT_WORST_SCORE	(-255)

/**
 * Is one score better than another?
 */
#define BETTER_THAN >

/**
 * Is one score worse than another?
 */
#define WORSE_THAN <

/** \file hmm.h
 * \brief HMM data structure and operation
 *
 * For efficiency, this version is hardwired for two possible HMM
 * topologies, but will fall back to others:
 * 
 * 5-state left-to-right HMMs: (0 is the *emitting* entry state and E
 * is a non-emitting exit state; the x's indicate allowed transitions
 * between source and destination states):
 * 
 * <pre>
 *               0   1   2   3   4   E (destination-states)
 *           0   x   x   x
 *           1       x   x   x
 *           2           x   x   x
 *           3               x   x   x
 *           4                   x   x
 *    (source-states)
 * </pre>
 *
 * 5-state topologies that contain a subset of the above transitions should work as well.
 * 
 * 3-state left-to-right HMMs (similar notation as the 5-state topology above):
 * 
 * <pre>
 *               0   1   2   E (destination-states)
 *           0   x   x   x
 *           1       x   x   x
 *           2           x   x 
 *    (source-states)
 * </pre>
 *
 * 3-state topologies that contain a subset of the above transitions should work as well. 
 */

/**
 * @struct hmm_context_t
 * @brief Shared information between a set of HMMs.
 *
 * We assume that the initial state is emitting and that the
 * transition matrix is n_emit_state x (n_emit_state+1), where the
 * extra destination dimension correponds to the non-emitting final or
 * exit state.
 */
typedef struct hmm_context_s {
    int32 n_emit_state;     /**< Number of emitting states in this set of HMMs. */
    uint8 ** const *tp;	    /**< State transition scores tp[id][from][to] (logs3 values). */
    int16 const *senscore;  /**< State emission scores senscore[senid]
                               (negated scaled logs3 values). */
    uint16 * const *sseq;   /**< Senone sequence mapping. */
    int32 *st_sen_scr;      /**< Temporary array of senone scores (for some topologies). */
    listelem_alloc_t *mpx_ssid_alloc; /**< Allocator for senone sequence ID arrays. */
    void *udata;            /**< Whatever you feel like, gosh. */
} hmm_context_t;

/**
 * Hard-coded limit on the number of emitting states.
 */
#define HMM_MAX_NSTATE 5

/**
 * @struct hmm_t
 * @brief An individual HMM among the HMM search space.
 *
 * An individual HMM among the HMM search space.  An HMM with N
 * emitting states consists of N+1 internal states including the
 * non-emitting exit (out) state.
 */
typedef struct hmm_s {
    hmm_context_t *ctx;            /**< Shared context data for this HMM. */
    int32 score[HMM_MAX_NSTATE];   /**< State scores for emitting states. */
    int32 history[HMM_MAX_NSTATE]; /**< History indices for emitting states. */
    int32 out_score;               /**< Score for non-emitting exit state. */
    int32 out_history;             /**< History index for non-emitting exit state. */
    uint16 ssid;                   /**< Senone sequence ID (for non-MPX) */
    uint16 senid[HMM_MAX_NSTATE];  /**< Senone IDs (non-MPX) or sequence IDs (MPX) */
    int32 bestscore;	/**< Best [emitting] state score in current frame (for pruning). */
    int16 tmatid;       /**< Transition matrix ID (see hmm_context_t). */
    frame_idx_t frame;  /**< Frame in which this HMM was last active; <0 if inactive */
    uint8 mpx;          /**< Is this HMM multiplex? (hoisted for speed) */
    uint8 n_emit_state; /**< Number of emitting states (hoisted for speed) */
} hmm_t;

/** Access macros. */
#define hmm_context(h) (h)->ctx
#define hmm_is_mpx(h) (h)->mpx

#define hmm_in_score(h) (h)->score[0]
#define hmm_score(h,st) (h)->score[st]
#define hmm_out_score(h) (h)->out_score

#define hmm_in_history(h) (h)->history[0]
#define hmm_history(h,st) (h)->history[st]
#define hmm_out_history(h) (h)->out_history

#define hmm_bestscore(h) (h)->bestscore
#define hmm_frame(h) (h)->frame
#define hmm_mpx_ssid(h,st) (h)->senid[st]
#define hmm_nonmpx_ssid(h) (h)->ssid
#define hmm_ssid(h,st) (hmm_is_mpx(h)                                   \
                        ? hmm_mpx_ssid(h,st) : hmm_nonmpx_ssid(h))
#define hmm_mpx_senid(h,st) (hmm_mpx_ssid(h,st) == BAD_SENID \
                             ? BAD_SENID : (h)->ctx->sseq[hmm_mpx_ssid(h,st)][st])
#define hmm_nonmpx_senid(h,st) ((h)->senid[st])
#define hmm_senid(h,st) (hmm_is_mpx(h)                                  \
                         ? hmm_mpx_senid(h,st) : hmm_nonmpx_senid(h,st))
#define hmm_senscr(h,st) (hmm_senid(h,st) == BAD_SENID                  \
                          ? WORST_SCORE                                 \
                          : -(h)->ctx->senscore[hmm_senid(h,st)])
#define hmm_tmatid(h) (h)->tmatid
#define hmm_tprob(h,i,j) (-(h)->ctx->tp[hmm_tmatid(h)][i][j])
#define hmm_n_emit_state(h) ((h)->n_emit_state)
#define hmm_n_state(h) ((h)->n_emit_state + 1)

/**
 * Create an HMM context.
 **/
hmm_context_t *hmm_context_init(int32 n_emit_state,
                                uint8 ** const *tp,
                                int16 const *senscore,
                                uint16 * const *sseq);

/**
 * Change the senone score array for a context.
 **/
#define hmm_context_set_senscore(ctx, senscr) ((ctx)->senscore = (senscr))

/**
 * Free an HMM context.
 *
 * @note The transition matrices, senone scores, and senone sequence
 * mapping are all assumed to be allocated externally, and will NOT be
 * freed by this function.
 **/
void hmm_context_free(hmm_context_t *ctx);

/**
 * Populate a previously-allocated HMM structure, allocating internal data.
 **/
void hmm_init(hmm_context_t *ctx, hmm_t *hmm, int mpx, int ssid, int tmatid);

/**
 * Free an HMM structure, releasing internal data (but not the HMM structure itself).
 */
void hmm_deinit(hmm_t *hmm);

/**
 * Reset the states of the HMM to the invalid condition.

 * i.e., scores to WORST_SCORE and hist to undefined.
 */
void hmm_clear(hmm_t *h);

/**
 * Reset the scores of the HMM.
 */
void hmm_clear_scores(hmm_t *h);

/**
 * Renormalize the scores in this HMM based on the given best score.
 */
void hmm_normalize(hmm_t *h, int32 bestscr);

/**
 * Enter an HMM with the given path score and history ID.
 **/
void hmm_enter(hmm_t *h, int32 score,
               int32 histid, int frame);

/**
 * Viterbi evaluation of given HMM.
 *
 * @note If this module were being used for tracking state
 * segmentations, the dummy, non-emitting exit state would have to be
 * updated separately.  In the Viterbi DP diagram, transitions to the
 * exit state occur from the current time; they are vertical
 * transitions.  Hence they should be made only after the history has
 * been logged for the emitting states.  But we're not bothered with
 * state segmentations, for now.  So, we update the exit state as
 * well.
*/
int32 hmm_vit_eval(hmm_t *hmm);
  

/**
 * Like hmm_vit_eval, but dump HMM state and relevant senscr to fp first, for debugging;.
 */
int32 hmm_dump_vit_eval(hmm_t *hmm,  /**< In/Out: HMM being updated */
                        FILE *fp /**< An output file pointer */
    );

/** 
 * For debugging, dump the whole HMM out.
 */

void hmm_dump(hmm_t *h,  /**< In/Out: HMM being updated */
              FILE *fp /**< An output file pointer */
    );


#ifdef __cplusplus
}
#endif

#endif /* __HMM_H__ */
