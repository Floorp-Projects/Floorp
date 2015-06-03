/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 2008 Carnegie Mellon University.  All rights
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
 * @file ps_lattice.h Word graph search
 */

#ifndef __PS_LATTICE_H__
#define __PS_LATTICE_H__

/* SphinxBase headers. */
#include <sphinxbase/prim_type.h>
#include <sphinxbase/ngram_model.h>

/* PocketSphinx headers. */
#include <pocketsphinx_export.h>

/**
 * Word graph structure used in bestpath/nbest search.
 */
typedef struct ps_lattice_s ps_lattice_t;

/**
 * DAG nodes.
 *
 * A node corresponds to a number of hypothesized instances of a word
 * which all share the same starting point.
 */
typedef struct ps_latnode_s ps_latnode_t;

/**
 * Iterator over DAG nodes.
 */
typedef struct ps_latnode_s ps_latnode_iter_t; /* pay no attention to the man behind the curtain */

/**
 * Links between DAG nodes.
 *
 * A link corresponds to a single hypothesized instance of a word with
 * a given start and end point.
 */
typedef struct ps_latlink_s ps_latlink_t;

/**
 * Iterator over DAG links.
 */
typedef struct latlink_list_s ps_latlink_iter_t;

/* Forward declaration needed to avoid circular includes */
struct ps_decoder_s;

/**
 * Read a lattice from a file on disk.
 *
 * @param ps Decoder to use for processing this lattice, or NULL.
 * @param file Path to lattice file.
 * @return Newly created lattice, or NULL for failure.
 */
POCKETSPHINX_EXPORT
ps_lattice_t *ps_lattice_read(struct ps_decoder_s *ps,
                              char const *file);

/**
 * Retain a lattice.
 *
 * This function retains ownership of a lattice for the caller,
 * preventing it from being freed automatically.  You must call
 * ps_lattice_free() to free it after having called this function.
 *
 * @return pointer to the retained lattice.
 */
POCKETSPHINX_EXPORT
ps_lattice_t *ps_lattice_retain(ps_lattice_t *dag);

/**
 * Free a lattice.
 *
 * @return new reference count (0 if dag was freed)
 */
POCKETSPHINX_EXPORT
int ps_lattice_free(ps_lattice_t *dag);

/**
 * Write a lattice to disk.
 *
 * @return 0 for success, <0 on failure.
 */
POCKETSPHINX_EXPORT
int ps_lattice_write(ps_lattice_t *dag, char const *filename);

/**
 * Write a lattice to disk in HTK format
 *
 * @return 0 for success, <0 on failure.
 */
POCKETSPHINX_EXPORT
int ps_lattice_write_htk(ps_lattice_t *dag, char const *filename);

/**
 * Get the log-math computation object for this lattice
 *
 * @return The log-math object for this lattice.  The lattice retains
 *         ownership of this pointer, so you should not attempt to
 *         free it manually.  Use logmath_retain() if you wish to
 *         reuse it elsewhere.
 */
POCKETSPHINX_EXPORT
logmath_t *ps_lattice_get_logmath(ps_lattice_t *dag);


/**
 * Start iterating over nodes in the lattice.
 *
 * @note No particular order of traversal is guaranteed, and you
 * should not depend on this.
 *
 * @param dag Lattice to iterate over.
 * @return Iterator over lattice nodes.
 */
POCKETSPHINX_EXPORT
ps_latnode_iter_t *ps_latnode_iter(ps_lattice_t *dag);

/**
 * Move to next node in iteration.
 * @param itor Node iterator.
 * @return Updated node iterator, or NULL if finished
 */
POCKETSPHINX_EXPORT
ps_latnode_iter_t *ps_latnode_iter_next(ps_latnode_iter_t *itor);

/**
 * Stop iterating over nodes.
 * @param itor Node iterator.
 */
POCKETSPHINX_EXPORT
void ps_latnode_iter_free(ps_latnode_iter_t *itor);

/**
 * Get node from iterator.
 */
POCKETSPHINX_EXPORT
ps_latnode_t *ps_latnode_iter_node(ps_latnode_iter_t *itor);

/**
 * Get start and end time range for a node.
 *
 * @param node Node inquired about.
 * @param out_fef Output: End frame of first exit from this node.
 * @param out_lef Output: End frame of last exit from this node.
 * @return Start frame for all edges exiting this node.
 */
POCKETSPHINX_EXPORT
int ps_latnode_times(ps_latnode_t *node, int16 *out_fef, int16 *out_lef);

/**
 * Get word string for this node.
 *
 * @param dag Lattice to which node belongs.
 * @param node Node inquired about.
 * @return Word string for this node (possibly a pronunciation variant).
 */
POCKETSPHINX_EXPORT
char const *ps_latnode_word(ps_lattice_t *dag, ps_latnode_t *node);

/**
 * Get base word string for this node.
 *
 * @param dag Lattice to which node belongs.
 * @param node Node inquired about.
 * @return Base word string for this node.
 */
POCKETSPHINX_EXPORT
char const *ps_latnode_baseword(ps_lattice_t *dag, ps_latnode_t *node);

/**
 * Iterate over exits from this node.
 *
 * @param node Node inquired about.
 * @return Iterator over exit links from this node.
 */
POCKETSPHINX_EXPORT
ps_latlink_iter_t *ps_latnode_exits(ps_latnode_t *node);

/**
 * Iterate over entries to this node.
 *
 * @param node Node inquired about.
 * @return Iterator over entry links to this node.
 */
POCKETSPHINX_EXPORT
ps_latlink_iter_t *ps_latnode_entries(ps_latnode_t *node);

/**
 * Get best posterior probability and associated acoustic score from a lattice node.
 *
 * @param dag Lattice to which node belongs.
 * @param node Node inquired about.
 * @param out_link Output: exit link with highest posterior probability
 * @return Posterior probability of the best link exiting this node.
 *         Log is expressed in the log-base used in the decoder.  To
 *         convert to linear floating-point, use
 *         logmath_exp(ps_lattice_get_logmath(), pprob).
 */
POCKETSPHINX_EXPORT
int32 ps_latnode_prob(ps_lattice_t *dag, ps_latnode_t *node,
                      ps_latlink_t **out_link);

/**
 * Get next link from a lattice link iterator.
 *
 * @param itor Iterator.
 * @return Updated iterator, or NULL if finished.
 */
POCKETSPHINX_EXPORT
ps_latlink_iter_t *ps_latlink_iter_next(ps_latlink_iter_t *itor);

/**
 * Stop iterating over links.
 * @param itor Link iterator.
 */
POCKETSPHINX_EXPORT
void ps_latlink_iter_free(ps_latlink_iter_t *itor);

/**
 * Get link from iterator.
 */
POCKETSPHINX_EXPORT
ps_latlink_t *ps_latlink_iter_link(ps_latlink_iter_t *itor);

/**
 * Get start and end times from a lattice link.
 *
 * @note these are <strong>inclusive</strong> - i.e. the last frame of
 * this word is ef, not ef-1.
 *
 * @param link Link inquired about.
 * @param out_sf Output: (optional) start frame of this link.
 * @return End frame of this link.
 */
POCKETSPHINX_EXPORT
int ps_latlink_times(ps_latlink_t *link, int16 *out_sf);

/**
 * Get destination and source nodes from a lattice link
 *
 * @param link Link inquired about
 * @param out_src Output: (optional) source node.
 * @return destination node
 */
POCKETSPHINX_EXPORT
ps_latnode_t *ps_latlink_nodes(ps_latlink_t *link, ps_latnode_t **out_src);

/**
 * Get word string from a lattice link.
 *
 * @param dag Lattice to which node belongs.
 * @param link Link inquired about
 * @return Word string for this link (possibly a pronunciation variant).
 */
POCKETSPHINX_EXPORT
char const *ps_latlink_word(ps_lattice_t *dag, ps_latlink_t *link);

/**
 * Get base word string from a lattice link.
 *
 * @param dag Lattice to which node belongs.
 * @param link Link inquired about
 * @return Base word string for this link
 */
POCKETSPHINX_EXPORT
char const *ps_latlink_baseword(ps_lattice_t *dag, ps_latlink_t *link);

/**
 * Get predecessor link in best path.
 *
 * @param link Link inquired about
 * @return Best previous link from bestpath search, if any.  Otherwise NULL
 */
POCKETSPHINX_EXPORT
ps_latlink_t *ps_latlink_pred(ps_latlink_t *link);

/**
 * Get acoustic score and posterior probability from a lattice link.
 *
 * @param dag Lattice to which node belongs.
 * @param link Link inquired about
 * @param out_ascr Output: (optional) acoustic score.
 * @return Posterior probability for this link.  Log is expressed in
 *         the log-base used in the decoder.  To convert to linear
 *         floating-point, use logmath_exp(ps_lattice_get_logmath(), pprob).
 */
POCKETSPHINX_EXPORT
int32 ps_latlink_prob(ps_lattice_t *dag, ps_latlink_t *link, int32 *out_ascr);

/**
 * Create a directed link between "from" and "to" nodes, but if a link already exists,
 * choose one with the best link_scr.
 */
POCKETSPHINX_EXPORT
void ps_lattice_link(ps_lattice_t *dag, ps_latnode_t *from, ps_latnode_t *to,
                     int32 score, int32 ef);

/**
 * Start a forward traversal of edges in a word graph.
 *
 * @note A keen eye will notice an inconsistency in this API versus
 * other types of iterators in PocketSphinx.  The reason for this is
 * that the traversal algorithm is much more efficient when it is able
 * to modify the lattice structure.  Therefore, to avoid giving the
 * impression that multiple traversals are possible at once, no
 * separate iterator structure is provided.
 *
 * @param dag Lattice to be traversed.
 * @param start Start node (source) of traversal.
 * @param end End node (goal) of traversal.
 * @return First link in traversal.
 */
POCKETSPHINX_EXPORT
ps_latlink_t *ps_lattice_traverse_edges(ps_lattice_t *dag, ps_latnode_t *start, ps_latnode_t *end);

/**
 * Get the next link in forward traversal.
 *
 * @param dag Lattice to be traversed.
 * @param end End node (goal) of traversal.
 * @return Next link in traversal.
 */
POCKETSPHINX_EXPORT
ps_latlink_t *ps_lattice_traverse_next(ps_lattice_t *dag, ps_latnode_t *end);

/**
 * Start a reverse traversal of edges in a word graph.
 *
 * @note See ps_lattice_traverse_edges() for why this API is the way it is.
 *
 * @param dag Lattice to be traversed.
 * @param start Start node (goal) of traversal.
 * @param end End node (source) of traversal.
 * @return First link in traversal.
 */
POCKETSPHINX_EXPORT
ps_latlink_t *ps_lattice_reverse_edges(ps_lattice_t *dag, ps_latnode_t *start, ps_latnode_t *end);

/**
 * Get the next link in reverse traversal.
 *
 * @param dag Lattice to be traversed.
 * @param start Start node (goal) of traversal.
 * @return Next link in traversal.
 */
POCKETSPHINX_EXPORT
ps_latlink_t *ps_lattice_reverse_next(ps_lattice_t *dag, ps_latnode_t *start);

/**
 * Do N-Gram based best-path search on a word graph.
 *
 * This function calculates both the best path as well as the forward
 * probability used in confidence estimation.
 *
 * @return Final link in best path, NULL on error.
 */
POCKETSPHINX_EXPORT
ps_latlink_t *ps_lattice_bestpath(ps_lattice_t *dag, ngram_model_t *lmset,
                                  float32 lwf, float32 ascale);

/**
 * Calculate link posterior probabilities on a word graph.
 *
 * This function assumes that bestpath search has already been done.
 *
 * @return Posterior probability of the utterance as a whole.
 */
POCKETSPHINX_EXPORT
int32 ps_lattice_posterior(ps_lattice_t *dag, ngram_model_t *lmset,
                           float32 ascale);

/**
 * Prune all links (and associated nodes) below a certain posterior probability.
 *
 * This function assumes that ps_lattice_posterior() has already been called.
 *
 * @param beam Minimum posterior probability for links. This is
 *         expressed in the log-base used in the decoder.  To convert
 *         from linear floating-point, use
 *         logmath_log(ps_lattice_get_logmath(), prob).
 * @return number of arcs removed.
 */
POCKETSPHINX_EXPORT
int32 ps_lattice_posterior_prune(ps_lattice_t *dag, int32 beam);

#ifdef NOT_IMPLEMENTED_YET
/**
 * Expand lattice using an N-gram language model.
 *
 * This function expands the lattice such that each node represents a
 * unique N-gram history, and adds language model scores to the links.
 */
POCKETSPHINX_EXPORT
int32 ps_lattice_ngram_expand(ps_lattice_t *dag, ngram_model_t *lm);
#endif

/**
 * Get the number of frames in the lattice.
 *
 * @param dag The lattice in question.
 * @return Number of frames in this lattice.
 */
POCKETSPHINX_EXPORT
int ps_lattice_n_frames(ps_lattice_t *dag);

#endif /* __PS_LATTICE_H__ */
