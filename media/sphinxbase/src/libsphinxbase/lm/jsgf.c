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

#include <string.h>
#include <assert.h>

#include "sphinxbase/ckd_alloc.h"
#include "sphinxbase/strfuncs.h"
#include "sphinxbase/hash_table.h"
#include "sphinxbase/filename.h"
#include "sphinxbase/err.h"
#include "sphinxbase/jsgf.h"

#include "jsgf_internal.h"
#include "jsgf_parser.h"
#include "jsgf_scanner.h"

extern int yyparse (void* scanner, jsgf_t* jsgf);

/**
 * \file jsgf.c
 *
 * This file implements the data structures for parsing JSGF grammars
 * into Sphinx finite-state grammars.
 **/

static int expand_rule(jsgf_t *grammar, jsgf_rule_t *rule, int rule_entry, int rule_exit);

jsgf_atom_t *
jsgf_atom_new(char *name, float weight)
{
    jsgf_atom_t *atom;

    atom = ckd_calloc(1, sizeof(*atom));
    atom->name = ckd_salloc(name);
    atom->weight = weight;
    return atom;
}

int
jsgf_atom_free(jsgf_atom_t *atom)
{
    if (atom == NULL)
        return 0;
    ckd_free(atom->name);
    ckd_free(atom);
    return 0;
}

jsgf_t *
jsgf_grammar_new(jsgf_t *parent)
{
    jsgf_t *grammar;

    grammar = ckd_calloc(1, sizeof(*grammar));
    /* If this is an imported/subgrammar, then we will share a global
     * namespace with the parent grammar. */
    if (parent) {
        grammar->rules = parent->rules;
        grammar->imports = parent->imports;
        grammar->searchpath = parent->searchpath;
        grammar->parent = parent;
    }
    else {
        grammar->rules = hash_table_new(64, 0);
        grammar->imports = hash_table_new(16, 0);
    }

    return grammar;
}

void
jsgf_grammar_free(jsgf_t *jsgf)
{
    /* FIXME: Probably should just use refcounting instead. */
    if (jsgf->parent == NULL) {
        hash_iter_t *itor;
        gnode_t *gn;

        for (itor = hash_table_iter(jsgf->rules); itor;
             itor = hash_table_iter_next(itor)) {
            ckd_free((char *)itor->ent->key);
            jsgf_rule_free((jsgf_rule_t *)itor->ent->val);
        }
        hash_table_free(jsgf->rules);
        for (itor = hash_table_iter(jsgf->imports); itor;
             itor = hash_table_iter_next(itor)) {
            ckd_free((char *)itor->ent->key);
            jsgf_grammar_free((jsgf_t *)itor->ent->val);
        }
        hash_table_free(jsgf->imports);
        for (gn = jsgf->searchpath; gn; gn = gnode_next(gn))
            ckd_free(gnode_ptr(gn));
        glist_free(jsgf->searchpath);
        for (gn = jsgf->links; gn; gn = gnode_next(gn))
            ckd_free(gnode_ptr(gn));
        glist_free(jsgf->links);
    }
    ckd_free(jsgf->name);
    ckd_free(jsgf->version);
    ckd_free(jsgf->charset);
    ckd_free(jsgf->locale);
    ckd_free(jsgf);
}

static void
jsgf_rhs_free(jsgf_rhs_t *rhs)
{
    gnode_t *gn;

    if (rhs == NULL)
        return;

    jsgf_rhs_free(rhs->alt);
    for (gn = rhs->atoms; gn; gn = gnode_next(gn))
        jsgf_atom_free(gnode_ptr(gn));
    glist_free(rhs->atoms);
    ckd_free(rhs);
}

jsgf_atom_t *
jsgf_kleene_new(jsgf_t *jsgf, jsgf_atom_t *atom, int plus)
{
    jsgf_rule_t *rule;
    jsgf_atom_t *rule_atom;
    jsgf_rhs_t *rhs;

    /* Generate an "internal" rule of the form (<NULL> | <name> <g0006>) */
    /* Or if plus is true, (<name> | <name> <g0006>) */
    rhs = ckd_calloc(1, sizeof(*rhs));
    if (plus)
        rhs->atoms = glist_add_ptr(NULL, jsgf_atom_new(atom->name, 1.0));
    else
        rhs->atoms = glist_add_ptr(NULL, jsgf_atom_new("<NULL>", 1.0));
    rule = jsgf_define_rule(jsgf, NULL, rhs, 0);
    rule_atom = jsgf_atom_new(rule->name, 1.0);
    rhs = ckd_calloc(1, sizeof(*rhs));
    rhs->atoms = glist_add_ptr(NULL, rule_atom);
    rhs->atoms = glist_add_ptr(rhs->atoms, atom);
    rule->rhs->alt = rhs;

    return jsgf_atom_new(rule->name, 1.0);
}

jsgf_rule_t *
jsgf_optional_new(jsgf_t *jsgf, jsgf_rhs_t *exp)
{
    jsgf_rhs_t *rhs = ckd_calloc(1, sizeof(*rhs));
    jsgf_atom_t *atom = jsgf_atom_new("<NULL>", 1.0);
    rhs->alt = exp;
    rhs->atoms = glist_add_ptr(NULL, atom);
    return jsgf_define_rule(jsgf, NULL, rhs, 0);
}

void
jsgf_add_link(jsgf_t *grammar, jsgf_atom_t *atom, int from, int to)
{
    jsgf_link_t *link;

    link = ckd_calloc(1, sizeof(*link));
    link->from = from;
    link->to = to;
    link->atom = atom;
    grammar->links = glist_add_ptr(grammar->links, link);
}

static char *
extract_grammar_name(char *rule_name)
{
    char* dot_pos;
    char* grammar_name = ckd_salloc(rule_name + 1);
    if ((dot_pos = strrchr(grammar_name + 1, '.')) == NULL) {
        ckd_free(grammar_name);
        return NULL;
    }
    *dot_pos='\0';
    return grammar_name;
}

char const *
jsgf_grammar_name(jsgf_t *jsgf)
{
    return jsgf->name;
}

static char *
jsgf_fullname(jsgf_t *jsgf, const char *name)
{
    char *fullname;

    /* Check if it is already qualified */
    if (strchr(name + 1, '.'))
        return ckd_salloc(name);

    /* Skip leading < in name */
    fullname = ckd_malloc(strlen(jsgf->name) + strlen(name) + 4);
    sprintf(fullname, "<%s.%s", jsgf->name, name + 1);
    return fullname;
}

static char *
jsgf_fullname_from_rule(jsgf_rule_t *rule, const char *name)
{
    char *fullname, *grammar_name;

    /* Check if it is already qualified */
    if (strchr(name + 1, '.'))
        return ckd_salloc(name);

    /* Skip leading < in name */
    if ((grammar_name = extract_grammar_name(rule->name)) == NULL)
        return ckd_salloc(name);
    fullname = ckd_malloc(strlen(grammar_name) + strlen(name) + 4);
    sprintf(fullname, "<%s.%s", grammar_name, name + 1);
    ckd_free(grammar_name);

    return fullname;
}

/* Extract as rulename everything after the secondlast dot, if existent. 
 * Because everything before the secondlast dot is the path-specification. */
static char *
importname2rulename(char *importname)
{
    char *rulename = ckd_salloc(importname);
    char *last_dotpos;
    char *secondlast_dotpos;

    if ((last_dotpos = strrchr(rulename+1, '.')) != NULL) {
        *last_dotpos='\0';
        if ((secondlast_dotpos = strrchr(rulename+1, '.')) != NULL) {
            *last_dotpos='.';
            *secondlast_dotpos='<';
            secondlast_dotpos = ckd_salloc(secondlast_dotpos);
            ckd_free(rulename);
            return secondlast_dotpos;
        }
        else {
            *last_dotpos='.';
            return rulename;
        }
    }
    else {
        return rulename;
    }
}

#define NO_NODE -1
#define RECURSIVE_NODE -2

/**
 *
 * Expand a right-hand-side of a rule (i.e. a single alternate).
 *
 * @returns the FSG state at the end of this rule, NO_NODE if there's an
 * error, and RECURSIVE_NODE if the right-hand-side ended in right-recursion (i.e.
 * a link to an earlier FSG state).
 */
static int
expand_rhs(jsgf_t *grammar, jsgf_rule_t *rule, jsgf_rhs_t *rhs,
           int rule_entry, int rule_exit)
{
    gnode_t *gn;
    int lastnode;

    /* Last node expanded in this sequence. */
    lastnode = rule_entry;

    /* Iterate over atoms in rhs and generate links/nodes */
    for (gn = rhs->atoms; gn; gn = gnode_next(gn)) {
        jsgf_atom_t *atom = gnode_ptr(gn);

        if (jsgf_atom_is_rule(atom)) {
            jsgf_rule_t *subrule;
            char *fullname;
            gnode_t *subnode;
            jsgf_rule_stack_t *rule_stack_entry = NULL;

            /* Special case for <NULL> and <VOID> pseudo-rules             
               If this is the only atom in the rhs, and it's the 
               first rhs in the rule, then emit a null transition, 
               creating an exit state if needed. */
            if (0 == strcmp(atom->name, "<NULL>")) {
                if (gn == rhs->atoms && gnode_next(gn) == NULL) {
                    if (rule_exit == NO_NODE) {
                        jsgf_add_link(grammar, atom,
                                      lastnode, grammar->nstate);
                        rule_exit = lastnode = grammar->nstate;
                        ++grammar->nstate;
                    } else {
                        jsgf_add_link(grammar, atom,
                                      lastnode, rule_exit);
                    }
                }
                continue;
            }
            else if (0 == strcmp(atom->name, "<VOID>")) {
                /* Make this entire RHS unspeakable */
                return NO_NODE;
            }

            fullname = jsgf_fullname_from_rule(rule, atom->name);
            if (hash_table_lookup(grammar->rules, fullname, (void**)&subrule) == -1) {
                E_ERROR("Undefined rule in RHS: %s\n", fullname);
                ckd_free(fullname);
                return NO_NODE;
            }
            ckd_free(fullname);

            /* Look for this subrule in the stack of expanded rules */
            for (subnode = grammar->rulestack; subnode; subnode = gnode_next(subnode)) {
                rule_stack_entry = (jsgf_rule_stack_t *)gnode_ptr(subnode);
                if (rule_stack_entry->rule == subrule)
                    break;
            }

            if (subnode != NULL) {
                /* Allow right-recursion only. */
                if (gnode_next(gn) != NULL) {
                    E_ERROR("Only right-recursion is permitted (in %s.%s)\n",
                            grammar->name, rule->name);
                    return NO_NODE;
                }
                /* Add a link back to the beginning of this rule instance */
                E_INFO("Right recursion %s %d => %d\n", atom->name, lastnode, rule_stack_entry->entry);
                jsgf_add_link(grammar, atom, lastnode, rule_stack_entry->entry);

                /* Let our caller know that this rhs didn't reach an
                   end state. */
                lastnode = RECURSIVE_NODE;
            }
            else {
                /* If this is the last atom in this rhs, link its
                   expansion to the parent rule's exit state.
                   Otherwise, create a new exit state for it. */
                int subruleexit = NO_NODE;
                if (gnode_next(gn) == NULL && rule_exit >= 0)
                    subruleexit = rule_exit;

                /* Expand the subrule */
                lastnode = expand_rule(grammar, subrule, lastnode, subruleexit);
                
                if (lastnode == NO_NODE)
                     return NO_NODE;
            }
        }
        else {
            /* An exit-state is created if this isn't the last atom
               in the rhs, or if the containing rule doesn't have an
               exit state yet.
               Otherwise, the rhs's exit state becomes the containing
               rule's exit state. */
            int exitstate;
            if (gnode_next(gn) == NULL && rule_exit >= 0) {
                exitstate = rule_exit;
            } else {
                exitstate = grammar->nstate;
                ++grammar->nstate;
            }

            /* Add a link for this token */
            jsgf_add_link(grammar, atom,
                          lastnode, exitstate);
            lastnode = exitstate;
        }
    }

    return lastnode;
}

static int
expand_rule(jsgf_t *grammar, jsgf_rule_t *rule, int rule_entry,
    int rule_exit)
{
    jsgf_rule_stack_t* rule_stack_entry;
    jsgf_rhs_t *rhs;

    /* Push this rule onto the stack */
    rule_stack_entry = (jsgf_rule_stack_t*)ckd_calloc(1, sizeof (jsgf_rule_stack_t));
    rule_stack_entry->rule = rule;
    rule_stack_entry->entry = rule_entry;
    grammar->rulestack = glist_add_ptr(grammar->rulestack,
        rule_stack_entry);

    for (rhs = rule->rhs; rhs; rhs = rhs->alt) {
        int lastnode;

        lastnode = expand_rhs(grammar, rule, rhs,
            rule_entry, rule_exit);

        if (lastnode == NO_NODE) {
            return NO_NODE;
        } else if (lastnode == RECURSIVE_NODE) {
            /* The rhs ended with right-recursion, i.e. a transition to
               an earlier state. Nothing needs to happen at this level. */
            ;
        } else if (rule_exit == NO_NODE) {
            /* If this rule doesn't have an exit state yet, use the exit
             state of its first right-hand-side.
             All other right-hand-sides will use this exit state. */
            assert (lastnode >= 0);
            rule_exit = lastnode;
        }
    }

    /* If no exit-state was created, use the entry-state. */
    if (rule_exit == NO_NODE) {
        rule_exit = rule_entry;
    }

    /* Pop this rule from the rule stack */
    ckd_free(gnode_ptr(grammar->rulestack));
    grammar->rulestack = gnode_free(grammar->rulestack, NULL);

    return rule_exit;
}

jsgf_rule_iter_t *
jsgf_rule_iter(jsgf_t *grammar)
{
    return hash_table_iter(grammar->rules);
}

jsgf_rule_t *
jsgf_get_rule(jsgf_t *grammar, char const *name)
{
    void *val;
    char *fullname;
    
    fullname = string_join("<", name, ">", NULL);
    if (hash_table_lookup(grammar->rules, fullname, &val) < 0) {
	ckd_free(fullname);
        return NULL;
    }
    ckd_free(fullname);
    return (jsgf_rule_t *)val;
}

jsgf_rule_t *
jsgf_get_public_rule(jsgf_t *grammar)
{
    jsgf_rule_iter_t *itor;
    jsgf_rule_t *public_rule = NULL;

    for (itor = jsgf_rule_iter(grammar); itor;
         itor = jsgf_rule_iter_next(itor)) {
        jsgf_rule_t *rule = jsgf_rule_iter_rule(itor);
        if (jsgf_rule_public(rule)) {
    	    const char *rule_name = jsgf_rule_name(rule);
    	    char *dot_pos;
    	    if ((dot_pos = strrchr(rule_name + 1, '.')) == NULL) {
    		public_rule = rule;
                jsgf_rule_iter_free(itor);
                break;
    	    }
    	    if (0 == strncmp(rule_name + 1, jsgf_grammar_name(grammar), dot_pos - rule_name - 1)) {
    		public_rule = rule;
                jsgf_rule_iter_free(itor);
                break;    		
    	    }
        }
    }
    return public_rule;
}

char const *
jsgf_rule_name(jsgf_rule_t *rule)
{
    return rule->name;
}

int
jsgf_rule_public(jsgf_rule_t *rule)
{
    return rule->is_public;
}

static fsg_model_t *
jsgf_build_fsg_internal(jsgf_t *grammar, jsgf_rule_t *rule,
                        logmath_t *lmath, float32 lw, int do_closure)
{
    fsg_model_t *fsg;
    glist_t nulls;
    gnode_t *gn;
    int rule_entry, rule_exit;

    /* Clear previous links */
    for (gn = grammar->links; gn; gn = gnode_next(gn)) {
        ckd_free(gnode_ptr(gn));
    }
    glist_free(grammar->links);
    grammar->links = NULL;
    grammar->nstate = 0;

    /* Create the top-level entry state, and expand the
       top-level rule. */
    rule_entry = grammar->nstate++;
    rule_exit = expand_rule(grammar, rule, rule_entry, NO_NODE);

    /* If no exit-state was created, create one. */
    if (rule_exit == NO_NODE) {
        rule_exit = grammar->nstate++;
        jsgf_add_link(grammar, NULL, rule_entry, rule_exit);
    }

    fsg = fsg_model_init(rule->name, lmath, lw, grammar->nstate);
    fsg->start_state = rule_entry;
    fsg->final_state = rule_exit;
    grammar->links = glist_reverse(grammar->links);
    for (gn = grammar->links; gn; gn = gnode_next(gn)) {
        jsgf_link_t *link = gnode_ptr(gn);

        if (link->atom) {
            if (jsgf_atom_is_rule(link->atom)) {
                fsg_model_null_trans_add(fsg, link->from, link->to,
                                        logmath_log(lmath, link->atom->weight));
            }
            else {
                int wid = fsg_model_word_add(fsg, link->atom->name);
                fsg_model_trans_add(fsg, link->from, link->to,
                                   logmath_log(lmath, link->atom->weight), wid);
            }
        }
        else {
            fsg_model_null_trans_add(fsg, link->from, link->to, 0);
        }            
    }
    if (do_closure) {
        nulls = fsg_model_null_trans_closure(fsg, NULL);
        glist_free(nulls);
    }

    return fsg;
}

fsg_model_t *
jsgf_build_fsg(jsgf_t *grammar, jsgf_rule_t *rule,
               logmath_t *lmath, float32 lw)
{
    return jsgf_build_fsg_internal(grammar, rule, lmath, lw, TRUE);
}

fsg_model_t *
jsgf_build_fsg_raw(jsgf_t *grammar, jsgf_rule_t *rule,
                   logmath_t *lmath, float32 lw)
{
    return jsgf_build_fsg_internal(grammar, rule, lmath, lw, FALSE);
}

fsg_model_t *
jsgf_read_file(const char *file, logmath_t * lmath, float32 lw)
{
    fsg_model_t *fsg;
    jsgf_rule_t *rule;
    jsgf_t *jsgf;
    jsgf_rule_iter_t *itor;

    if ((jsgf = jsgf_parse_file(file, NULL)) == NULL) {
        E_ERROR("Error parsing file: %s\n", file);
        return NULL;
    }

    rule = NULL;
    for (itor = jsgf_rule_iter(jsgf); itor;
         itor = jsgf_rule_iter_next(itor)) {
        rule = jsgf_rule_iter_rule(itor);
        if (jsgf_rule_public(rule)) {
            jsgf_rule_iter_free(itor);
            break;
        }
    }
    if (rule == NULL) {
        E_ERROR("No public rules found in %s\n", file);
        return NULL;
    }
    fsg = jsgf_build_fsg(jsgf, rule, lmath, lw);
    jsgf_grammar_free(jsgf);
    return fsg;
}

fsg_model_t *
jsgf_read_string(const char *string, logmath_t * lmath, float32 lw)
{
    fsg_model_t *fsg;
    jsgf_rule_t *rule;
    jsgf_t *jsgf;
    jsgf_rule_iter_t *itor;

    if ((jsgf = jsgf_parse_string(string, NULL)) == NULL) {
        E_ERROR("Error parsing input string\n");
        return NULL;
    }

    rule = NULL;
    for (itor = jsgf_rule_iter(jsgf); itor;
         itor = jsgf_rule_iter_next(itor)) {
        rule = jsgf_rule_iter_rule(itor);
        if (jsgf_rule_public(rule)) {
            jsgf_rule_iter_free(itor);
            break;
        }
    }
    if (rule == NULL) {
        jsgf_grammar_free(jsgf);
        E_ERROR("No public rules found in input string\n");
        return NULL;
    }
    fsg = jsgf_build_fsg(jsgf, rule, lmath, lw);
    jsgf_grammar_free(jsgf);
    return fsg;
}


int
jsgf_write_fsg(jsgf_t *grammar, jsgf_rule_t *rule, FILE *outfh)
{
    fsg_model_t *fsg;
    logmath_t *lmath = logmath_init(1.0001, 0, 0);

    if ((fsg = jsgf_build_fsg_raw(grammar, rule, lmath, 1.0)) == NULL)
        goto error_out;

    fsg_model_write(fsg, outfh);
    logmath_free(lmath);
    return 0;

error_out:
    logmath_free(lmath);
    return -1;
}

jsgf_rule_t *
jsgf_define_rule(jsgf_t *jsgf, char *name, jsgf_rhs_t *rhs, int is_public)
{
    jsgf_rule_t *rule;
    void *val;

    if (name == NULL) {
        name = ckd_malloc(strlen(jsgf->name) + 16);
        sprintf(name, "<%s.g%05d>", jsgf->name, hash_table_inuse(jsgf->rules));
    }
    else {
        char *newname;

        newname = jsgf_fullname(jsgf, name);
        name = newname;
    }

    rule = ckd_calloc(1, sizeof(*rule));
    rule->refcnt = 1;
    rule->name = ckd_salloc(name);
    rule->rhs = rhs;
    rule->is_public = is_public;

    E_INFO("Defined rule: %s%s\n",
           rule->is_public ? "PUBLIC " : "",
           rule->name);
    val = hash_table_enter(jsgf->rules, name, rule);
    if (val != (void *)rule) {
        E_WARN("Multiply defined symbol: %s\n", name);
    }
    return rule;
}

jsgf_rule_t *
jsgf_rule_retain(jsgf_rule_t *rule)
{
    ++rule->refcnt;
    return rule;
}

int
jsgf_rule_free(jsgf_rule_t *rule)
{
    if (rule == NULL)
        return 0;
    if (--rule->refcnt > 0)
        return rule->refcnt;
    jsgf_rhs_free(rule->rhs);
    ckd_free(rule->name);
    ckd_free(rule);
    return 0;
}


/* FIXME: This should go in libsphinxutil */
static char *
path_list_search(glist_t paths, char *path)
{
    gnode_t *gn;

    for (gn = paths; gn; gn = gnode_next(gn)) {
        char *fullpath;
        FILE *tmp;

        fullpath = string_join(gnode_ptr(gn), "/", path, NULL);
        tmp = fopen(fullpath, "r");
        if (tmp != NULL) {
            fclose(tmp);
            return fullpath;
        }
        else {
            ckd_free(fullpath);
        }
    }
    return NULL;
}

jsgf_rule_t *
jsgf_import_rule(jsgf_t *jsgf, char *name)
{
    char *c, *path, *newpath;
    size_t namelen, packlen;
    void *val;
    jsgf_t *imp;
    int import_all;

    /* Trim the leading and trailing <> */
    namelen = strlen(name);
    path = ckd_malloc(namelen - 2 + 6); /* room for a trailing .gram */
    strcpy(path, name + 1);
    /* Split off the first part of the name */
    c = strrchr(path, '.');
    if (c == NULL) {
        E_ERROR("Imported rule is not qualified: %s\n", name);
        ckd_free(path);
        return NULL;
    }
    packlen = c - path;
    *c = '\0';

    /* Look for import foo.* */
    import_all = (strlen(name) > 2 && 0 == strcmp(name + namelen - 3, ".*>"));

    /* Construct a filename. */
    for (c = path; *c; ++c)
        if (*c == '.') *c = '/';
    strcat(path, ".gram");
    newpath = path_list_search(jsgf->searchpath, path);
    if (newpath == NULL) {
	E_ERROR("Failed to find grammar %s\n", path);
        ckd_free(path);
        return NULL;
    }
    ckd_free(path);

    path = newpath;
    E_INFO("Importing %s from %s to %s\n", name, path, jsgf->name);

    /* FIXME: Also, we need to make sure that path is fully qualified
     * here, by adding any prefixes from jsgf->name to it. */
    /* See if we have parsed it already */
    if (hash_table_lookup(jsgf->imports, path, &val) == 0) {
        E_INFO("Already imported %s\n", path);
        imp = val;
        ckd_free(path);
    }
    else {
        /* If not, parse it. */
        imp = jsgf_parse_file(path, jsgf);
        val = hash_table_enter(jsgf->imports, path, imp);
        if (val != (void *)imp) {
            E_WARN("Multiply imported file: %s\n", path);
        }
    }
    if (imp != NULL) {
        hash_iter_t *itor;
        /* Look for public rules matching rulename. */
        for (itor = hash_table_iter(imp->rules); itor;
             itor = hash_table_iter_next(itor)) {
            hash_entry_t *he = itor->ent;
            jsgf_rule_t *rule = hash_entry_val(he);
            int rule_matches;
            char *rule_name = importname2rulename(name);

            if (import_all) {
                /* Match package name (symbol table is shared) */
                rule_matches = !strncmp(rule_name, rule->name, packlen + 1);
            }
            else {
                /* Exact match */
                rule_matches = !strcmp(rule_name, rule->name);
            }
            ckd_free(rule_name);
            if (rule->is_public && rule_matches) {
                void *val;
                char *newname;

                /* Link this rule into the current namespace. */
                c = strrchr(rule->name, '.');
                assert(c != NULL);
                newname = jsgf_fullname(jsgf, c);

                E_INFO("Imported %s\n", newname);
                val = hash_table_enter(jsgf->rules, newname,
                                       jsgf_rule_retain(rule));
                if (val != (void *)rule) {
                    E_WARN("Multiply defined symbol: %s\n", newname);
                }
                if (!import_all) {
                    hash_table_iter_free(itor);
                    return rule;
                }
            }
        }
    }

    return NULL;
}

static void
jsgf_set_search_path(jsgf_t *jsgf, const char *filename)
{
     char *jsgf_path;

#if !defined(_WIN32_WCE)
     if ((jsgf_path = getenv("JSGF_PATH")) != NULL) {
	char *word, *c;
        /* FIXME: This should be a function in libsphinxbase. */
        word = jsgf_path = ckd_salloc(jsgf_path);
        while ((c = strchr(word, ':'))) {
            *c = '\0';
	     jsgf->searchpath = glist_add_ptr(jsgf->searchpath, word);
    	     word = c + 1;
        }
        jsgf->searchpath = glist_add_ptr(jsgf->searchpath, word);
        jsgf->searchpath = glist_reverse(jsgf->searchpath);
        return;
    }
#endif

    if (!filename) {
	jsgf->searchpath = glist_add_ptr(jsgf->searchpath, ckd_salloc("."));
	return;
    }
    
    jsgf_path = ckd_salloc(filename);
    path2dirname(filename, jsgf_path);
    jsgf->searchpath = glist_add_ptr(jsgf->searchpath, jsgf_path);
}

jsgf_t *
jsgf_parse_file(const char *filename, jsgf_t *parent)
{
    yyscan_t yyscanner;
    jsgf_t *jsgf;
    int yyrv;
    FILE *in = NULL;

    yylex_init(&yyscanner);
    if (filename == NULL) {
        yyset_in(stdin, yyscanner);
    }
    else {
        in = fopen(filename, "r");
        if (in == NULL) {
            E_ERROR_SYSTEM("Failed to open %s for parsing", filename);
            return NULL;
        }
        yyset_in(in, yyscanner);
    }

    jsgf = jsgf_grammar_new(parent);

    if (!parent)
        jsgf_set_search_path(jsgf, filename);

    yyrv = yyparse(yyscanner, jsgf);
    if (yyrv != 0) {
        E_ERROR("Failed to parse JSGF grammar from '%s'\n", filename ? filename : "(stdin)");
        jsgf_grammar_free(jsgf);
        yylex_destroy(yyscanner);
        return NULL;
    }
    if (in)
        fclose(in);
    yylex_destroy(yyscanner);

    return jsgf;
}

jsgf_t *
jsgf_parse_string(const char *string, jsgf_t * parent)
{
    yyscan_t yyscanner;
    jsgf_t *jsgf;
    int yyrv;
    YY_BUFFER_STATE buf;

    yylex_init(&yyscanner);
    buf = yy_scan_string(string, yyscanner);

    jsgf = jsgf_grammar_new(parent);
    if (!parent)
        jsgf_set_search_path(jsgf, NULL);

    yyrv = yyparse(yyscanner, jsgf);
    if (yyrv != 0) {
        E_ERROR("Failed to parse JSGF grammar from input string\n");
        jsgf_grammar_free(jsgf);
        yy_delete_buffer(buf, yyscanner);
        yylex_destroy(yyscanner);
        return NULL;
    }
    yy_delete_buffer(buf, yyscanner);
    yylex_destroy(yyscanner);

    return jsgf;
}
