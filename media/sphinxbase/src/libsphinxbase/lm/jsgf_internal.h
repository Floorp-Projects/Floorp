/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 2007 Carnegie Mellon University.  All rights
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

#ifndef __JSGF_INTERNAL_H__
#define __JSGF_INTERNAL_H__

/**
 * @file jsgf_internal.h Internal definitions for JSGF grammar compiler
 */

#include <stdio.h>

#include <sphinxbase/hash_table.h>
#include <sphinxbase/glist.h>
#include <sphinxbase/fsg_model.h>
#include <sphinxbase/logmath.h>
#include <sphinxbase/strfuncs.h>
#include <sphinxbase/jsgf.h>


/* Flex uses strdup which is missing on WinCE */
#if defined(_WIN32) || defined(_WIN32_WCE)
#define strdup _strdup
#endif

#ifdef __cplusplus
extern "C" {
#endif
#if 0
/* Fool Emacs. */
}
#endif

#define YY_NO_INPUT /* Silence a compiler warning. */

typedef struct jsgf_rhs_s jsgf_rhs_t;
typedef struct jsgf_atom_s jsgf_atom_t;
typedef struct jsgf_link_s jsgf_link_t;
typedef struct jsgf_rule_stack_s jsgf_rule_stack_t;

struct jsgf_s {
    char *version;  /**< JSGF version (from header) */
    char *charset;  /**< JSGF charset (default UTF-8) */
    char *locale;   /**< JSGF locale (default C) */
    char *name;     /**< Grammar name */

    hash_table_t *rules;   /**< Defined or imported rules in this grammar. */
    hash_table_t *imports; /**< Pointers to imported grammars. */
    jsgf_t *parent;        /**< Parent grammar (if this is an imported one) */
    glist_t searchpath;    /**< List of directories to search for grammars. */

    /* Scratch variables for FSG conversion. */
    int nstate;            /**< Number of generated states. */
    glist_t links;	   /**< Generated FSG links. */
    glist_t rulestack;     /**< Stack of currently expanded rules. */
};

/* A type to keep track of the stack of rules currently being expanded. */
struct jsgf_rule_stack_s {
    jsgf_rule_t *rule;  /**< The rule being expanded */
    int entry;          /**< The entry-state for this expansion */
};

struct jsgf_rule_s {
    int refcnt;      /**< Reference count. */
    char *name;      /**< Rule name (NULL for an alternation/grouping) */
    int is_public;   /**< Is this rule marked 'public'? */
    jsgf_rhs_t *rhs; /**< Expansion */
};

struct jsgf_rhs_s {
    glist_t atoms;   /**< Sequence of items */
    jsgf_rhs_t *alt; /**< Linked list of alternates */
};

struct jsgf_atom_s {
    char *name;        /**< Rule or token name */
    glist_t tags;      /**< Tags, if any (glist_t of char *) */
    float weight;      /**< Weight (default 1) */
};

struct jsgf_link_s {
    jsgf_atom_t *atom; /**< Name, tags, weight */
    int from;          /**< From state */
    int to;            /**< To state */
};

#define jsgf_atom_is_rule(atom) ((atom)->name[0] == '<')

void jsgf_add_link(jsgf_t *grammar, jsgf_atom_t *atom, int from, int to);
jsgf_atom_t *jsgf_atom_new(char *name, float weight);
jsgf_atom_t *jsgf_kleene_new(jsgf_t *jsgf, jsgf_atom_t *atom, int plus);
jsgf_rule_t *jsgf_optional_new(jsgf_t *jsgf, jsgf_rhs_t *exp);
jsgf_rule_t *jsgf_define_rule(jsgf_t *jsgf, char *name, jsgf_rhs_t *rhs, int is_public);
jsgf_rule_t *jsgf_import_rule(jsgf_t *jsgf, char *name);

int jsgf_atom_free(jsgf_atom_t *atom);
int jsgf_rule_free(jsgf_rule_t *rule);
jsgf_rule_t *jsgf_rule_retain(jsgf_rule_t *rule);

#ifdef __cplusplus
}
#endif


#endif /* __JSGF_H__ */
