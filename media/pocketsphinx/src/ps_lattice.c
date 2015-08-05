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
 * @file ps_lattice.c Word graph search.
 */

/* System headers. */
#include <assert.h>
#include <string.h>
#include <math.h>

/* SphinxBase headers. */
#include <sphinxbase/ckd_alloc.h>
#include <sphinxbase/listelem_alloc.h>
#include <sphinxbase/strfuncs.h>
#include <sphinxbase/err.h>
#include <sphinxbase/pio.h>

/* Local headers. */
#include "pocketsphinx_internal.h"
#include "ps_lattice_internal.h"
#include "ngram_search.h"
#include "dict.h"

/*
 * Create a directed link between "from" and "to" nodes, but if a link already exists,
 * choose one with the best ascr.
 */
void
ps_lattice_link(ps_lattice_t *dag, ps_latnode_t *from, ps_latnode_t *to,
                int32 score, int32 ef)
{
    latlink_list_t *fwdlink;

    /* Look for an existing link between "from" and "to" nodes */
    for (fwdlink = from->exits; fwdlink; fwdlink = fwdlink->next)
        if (fwdlink->link->to == to)
            break;

    if (fwdlink == NULL) {
        latlink_list_t *revlink;
        ps_latlink_t *link;

        /* No link between the two nodes; create a new one */
        link = listelem_malloc(dag->latlink_alloc);
        fwdlink = listelem_malloc(dag->latlink_list_alloc);
        revlink = listelem_malloc(dag->latlink_list_alloc);

        link->from = from;
        link->to = to;
        link->ascr = score;
        link->ef = ef;
        link->best_prev = NULL;

        fwdlink->link = revlink->link = link;
        fwdlink->next = from->exits;
        from->exits = fwdlink;
        revlink->next = to->entries;
        to->entries = revlink;
    }
    else {
        /* Link already exists; just retain the best ascr */
        if (score BETTER_THAN fwdlink->link->ascr) {
            fwdlink->link->ascr = score;
            fwdlink->link->ef = ef;
        }
    }           
}

void
ps_lattice_penalize_fillers(ps_lattice_t *dag, int32 silpen, int32 fillpen)
{
    ps_latnode_t *node;

    for (node = dag->nodes; node; node = node->next) {
        latlink_list_t *linklist;
        if (node != dag->start && node != dag->end && dict_filler_word(dag->dict, node->basewid)) {
            for (linklist = node->entries; linklist; linklist = linklist->next)
                linklist->link->ascr += (node->basewid == dag->silence) ? silpen : fillpen;
        }
    }
}

static void
delete_node(ps_lattice_t *dag, ps_latnode_t *node)
{
    latlink_list_t *x, *next_x;

    for (x = node->exits; x; x = next_x) {
        next_x = x->next;
        x->link->from = NULL;
        listelem_free(dag->latlink_list_alloc, x);
    }
    for (x = node->entries; x; x = next_x) {
        next_x = x->next;
        x->link->to = NULL;
        listelem_free(dag->latlink_list_alloc, x);
    }
    listelem_free(dag->latnode_alloc, node);
}


static void
remove_dangling_links(ps_lattice_t *dag, ps_latnode_t *node)
{
    latlink_list_t *x, *prev_x, *next_x;

    prev_x = NULL;
    for (x = node->exits; x; x = next_x) {
        next_x = x->next;
        if (x->link->to == NULL) {
            if (prev_x)
                prev_x->next = next_x;
            else
                node->exits = next_x;
            listelem_free(dag->latlink_alloc, x->link);
            listelem_free(dag->latlink_list_alloc, x);
        }
        else
            prev_x = x;
    }
    prev_x = NULL;
    for (x = node->entries; x; x = next_x) {
        next_x = x->next;
        if (x->link->from == NULL) {
            if (prev_x)
                prev_x->next = next_x;
            else
                node->entries = next_x;
            listelem_free(dag->latlink_alloc, x->link);
            listelem_free(dag->latlink_list_alloc, x);
        }
        else
            prev_x = x;
    }
}

void
ps_lattice_delete_unreachable(ps_lattice_t *dag)
{
    ps_latnode_t *node, *prev_node, *next_node;
    int i;

    /* Remove unreachable nodes from the list of nodes. */
    prev_node = NULL;
    for (node = dag->nodes; node; node = next_node) {
        next_node = node->next;
        if (!node->reachable) {
            if (prev_node)
                prev_node->next = next_node;
            else
                dag->nodes = next_node;
            /* Delete this node and NULLify links to it. */
            delete_node(dag, node);
        }
        else
            prev_node = node;
    }

    /* Remove all links to and from unreachable nodes. */
    i = 0;
    for (node = dag->nodes; node; node = node->next) {
        /* Assign sequence numbers. */
        node->id = i++;

        /* We should obviously not encounter unreachable nodes here! */
        assert(node->reachable);

        /* Remove all links that go nowhere. */
        remove_dangling_links(dag, node);
    }
}

int32
ps_lattice_write(ps_lattice_t *dag, char const *filename)
{
    FILE *fp;
    int32 i;
    ps_latnode_t *d, *initial, *final;

    initial = dag->start;
    final = dag->end;

    E_INFO("Writing lattice file: %s\n", filename);
    if ((fp = fopen(filename, "w")) == NULL) {
        E_ERROR_SYSTEM("Failed to open lattice file '%s' for writing", filename);
        return -1;
    }

    /* Stupid Sphinx-III lattice code expects 'getcwd:' here */
    fprintf(fp, "# getcwd: /this/is/bogus\n");
    fprintf(fp, "# -logbase %e\n", logmath_get_base(dag->lmath));
    fprintf(fp, "#\n");

    fprintf(fp, "Frames %d\n", dag->n_frames);
    fprintf(fp, "#\n");

    for (i = 0, d = dag->nodes; d; d = d->next, i++);
    fprintf(fp,
            "Nodes %d (NODEID WORD STARTFRAME FIRST-ENDFRAME LAST-ENDFRAME)\n",
            i);
    for (i = 0, d = dag->nodes; d; d = d->next, i++) {
        d->id = i;
        fprintf(fp, "%d %s %d %d %d ; %d\n",
                i, dict_wordstr(dag->dict, d->wid),
                d->sf, d->fef, d->lef, d->node_id);
    }
    fprintf(fp, "#\n");

    fprintf(fp, "Initial %d\nFinal %d\n", initial->id, final->id);
    fprintf(fp, "#\n");

    /* Don't bother with this, it's not used by anything. */
    fprintf(fp, "BestSegAscr %d (NODEID ENDFRAME ASCORE)\n",
            0 /* #BPTable entries */ );
    fprintf(fp, "#\n");

    fprintf(fp, "Edges (FROM-NODEID TO-NODEID ASCORE)\n");
    for (d = dag->nodes; d; d = d->next) {
        latlink_list_t *l;
        for (l = d->exits; l; l = l->next) {
            if (l->link->ascr WORSE_THAN WORST_SCORE || l->link->ascr BETTER_THAN 0)
                continue;
            fprintf(fp, "%d %d %d\n",
                    d->id, l->link->to->id, l->link->ascr << SENSCR_SHIFT);
        }
    }
    fprintf(fp, "End\n");
    fclose(fp);

    return 0;
}

int32
ps_lattice_write_htk(ps_lattice_t *dag, char const *filename)
{
    FILE *fp;
    ps_latnode_t *d, *initial, *final;
    int32 j, n_links, n_nodes;

    initial = dag->start;
    final = dag->end;

    E_INFO("Writing lattice file: %s\n", filename);
    if ((fp = fopen(filename, "w")) == NULL) {
        E_ERROR_SYSTEM("Failed to open lattice file '%s' for writing", filename);
        return -1;
    }

    for (n_links = n_nodes = 0, d = dag->nodes; d; d = d->next) {
        latlink_list_t *l;
        if (!d->reachable)
            continue;
        d->id = n_nodes;
        for (l = d->exits; l; l = l->next) {
            if (l->link->to == NULL || !l->link->to->reachable)
                continue;
            if (l->link->ascr WORSE_THAN WORST_SCORE || l->link->ascr BETTER_THAN 0)
                continue;
            ++n_links;
        }
        ++n_nodes;
    }

    fprintf(fp, "# Lattice generated by PocketSphinx\n");
    fprintf(fp, "#\n# Header\n#\n");
    fprintf(fp, "VERSION=1.0\n");
    fprintf(fp, "start=%d\n", initial->id);
    fprintf(fp, "end=%d\n", final->id);
    fprintf(fp, "#\n");

    fprintf(fp, "N=%d\tL=%d\n", n_nodes, n_links);
    fprintf(fp, "#\n# Node definitions\n#\n");
    for (d = dag->nodes; d; d = d->next) {
        char const *word = dict_wordstr(dag->dict, d->wid);
        char const *c = strrchr(word, '(');
        int altpron = 1;
        if (!d->reachable)
            continue;
        if (c)
            altpron = atoi(c + 1);
        word = dict_basestr(dag->dict, d->wid);
        if (d->wid == dict_startwid(dag->dict))
            word = "!SENT_START";
        else if (d->wid == dict_finishwid(dag->dict))
            word = "!SENT_END";
        else if (dict_filler_word(dag->dict, d->wid))
            word = "!NULL";
        fprintf(fp, "I=%d\tt=%.2f\tW=%s\tv=%d\n",
                d->id, (double)d->sf / dag->frate,
                word, altpron);
    }
    fprintf(fp, "#\n# Link definitions\n#\n");
    for (j = 0, d = dag->nodes; d; d = d->next) {
        latlink_list_t *l;
        if (!d->reachable)
            continue;
        for (l = d->exits; l; l = l->next) {
            if (l->link->to == NULL || !l->link->to->reachable)
                continue;
            if (l->link->ascr WORSE_THAN WORST_SCORE || l->link->ascr BETTER_THAN 0)
                continue;
            fprintf(fp, "J=%d\tS=%d\tE=%d\ta=%f\tp=%g\n", j++,
                    d->id, l->link->to->id,
                    logmath_log_to_ln(dag->lmath, l->link->ascr << SENSCR_SHIFT),
                    logmath_exp(dag->lmath, l->link->alpha + l->link->beta - dag->norm));
        }
    }
    fclose(fp);

    return 0;
}

/* Read parameter from a lattice file*/
static int
dag_param_read(lineiter_t *li, char *param)
{
    int32 n;

    while ((li = lineiter_next(li)) != NULL) {
        char *c;

        /* Ignore comments. */
        if (li->buf[0] == '#')
            continue;

        /* Find the first space. */
        c = strchr(li->buf, ' ');
        if (c == NULL) continue;

        /* Check that the first field equals param and that there's a number after it. */
        if (strncmp(li->buf, param, strlen(param)) == 0
            && sscanf(c + 1, "%d", &n) == 1)
            return n;
    }
    return -1;
}

/* Mark every node that has a path to the argument dagnode as "reachable". */
static void
dag_mark_reachable(ps_latnode_t * d)
{
    latlink_list_t *l;

    d->reachable = 1;
    for (l = d->entries; l; l = l->next)
        if (l->link->from && !l->link->from->reachable)
            dag_mark_reachable(l->link->from);
}

ps_lattice_t *
ps_lattice_read(ps_decoder_t *ps,
                char const *file)
{
    FILE *fp;
    int32 ispipe;
    lineiter_t *line;
    float64 lb;
    float32 logratio;
    ps_latnode_t *tail;
    ps_latnode_t **darray;
    ps_lattice_t *dag;
    int i, k, n_nodes;
    int32 pip, silpen, fillpen;

    dag = ckd_calloc(1, sizeof(*dag));

    if (ps) {
        dag->search = ps->search;
        dag->dict = dict_retain(ps->dict);
        dag->lmath = logmath_retain(ps->lmath);
        dag->dict = dict_init(NULL, NULL, dag->lmath);
        dag->frate = cmd_ln_int32_r(dag->search->config, "-frate");
    }
    else {
        dag->dict = dict_init(NULL, NULL, dag->lmath);
        dag->lmath = logmath_init(1.0001, 0, FALSE);
        dag->frate = 100;
    }
    dag->silence = dict_silwid(dag->dict);
    dag->latnode_alloc = listelem_alloc_init(sizeof(ps_latnode_t));
    dag->latlink_alloc = listelem_alloc_init(sizeof(ps_latlink_t));
    dag->latlink_list_alloc = listelem_alloc_init(sizeof(latlink_list_t));
    dag->refcount = 1;

    tail = NULL;
    darray = NULL;

    E_INFO("Reading DAG file: %s\n", file);
    if ((fp = fopen_compchk(file, &ispipe)) == NULL) {
        E_ERROR_SYSTEM("Failed to open DAG file '%s' for reading", file);
        return NULL;
    }
    line = lineiter_start(fp);

    /* Read and verify logbase (ONE BIG HACK!!) */
    if (line == NULL) {
        E_ERROR("Premature EOF(%s)\n", file);
        goto load_error;
    }
    if (strncmp(line->buf, "# getcwd: ", 10) != 0) {
        E_ERROR("%s does not begin with '# getcwd: '\n%s", file, line->buf);
        goto load_error;
    }
    if ((line = lineiter_next(line)) == NULL) {
        E_ERROR("Premature EOF(%s)\n", file);
        goto load_error;
    }
    if ((strncmp(line->buf, "# -logbase ", 11) != 0)
        || (sscanf(line->buf + 11, "%lf", &lb) != 1)) {
        E_WARN("%s: Cannot find -logbase in header\n", file);
        lb = 1.0001;
    }
    logratio = 1.0f;
    if (dag->lmath == NULL)
        dag->lmath = logmath_init(lb, 0, TRUE);
    else {
        float32 pb = logmath_get_base(dag->lmath);
        if (fabs(lb - pb) >= 0.0001) {
            E_WARN("Inconsistent logbases: %f vs %f: will compensate\n", lb, pb);
            logratio = (float32)(log(lb) / log(pb));
            E_INFO("Lattice log ratio: %f\n", logratio);
        }
    }
    /* Read Frames parameter */
    dag->n_frames = dag_param_read(line, "Frames");
    if (dag->n_frames <= 0) {
        E_ERROR("Frames parameter missing or invalid\n");
        goto load_error;
    }
    /* Read Nodes parameter */
    n_nodes = dag_param_read(line, "Nodes");
    if (n_nodes <= 0) {
        E_ERROR("Nodes parameter missing or invalid\n");
        goto load_error;
    }

    /* Read nodes */
    darray = ckd_calloc(n_nodes, sizeof(*darray));
    for (i = 0; i < n_nodes; i++) {
        ps_latnode_t *d;
        int32 w;
        int seqid, sf, fef, lef;
        char wd[256];

        if ((line = lineiter_next(line)) == NULL) {
            E_ERROR("Premature EOF while loading Nodes(%s)\n", file);
            goto load_error;
        }

        if ((k =
             sscanf(line->buf, "%d %255s %d %d %d", &seqid, wd, &sf, &fef,
                    &lef)) != 5) {
            E_ERROR("Cannot parse line: %s, value of count %d\n", line->buf, k);
            goto load_error;
        }

        w = dict_wordid(dag->dict, wd);
        if (w < 0) {
            if (dag->search == NULL) {
                char *ww = ckd_salloc(wd);
                if (dict_word2basestr(ww) != -1) {
                    if (dict_wordid(dag->dict, ww) == BAD_S3WID)
                        dict_add_word(dag->dict, ww, NULL, 0);
                }
                ckd_free(ww);
                w = dict_add_word(dag->dict, wd, NULL, 0);
            }
            if (w < 0) {
                E_ERROR("Unknown word in line: %s\n", line->buf);
                goto load_error;
            }
        }

        if (seqid != i) {
            E_ERROR("Seqno error: %s\n", line->buf);
            goto load_error;
        }

        d = listelem_malloc(dag->latnode_alloc);
        darray[i] = d;
        d->wid = w;
        d->basewid = dict_basewid(dag->dict, w);
        d->id = seqid;
        d->sf = sf;
        d->fef = fef;
        d->lef = lef;
        d->reachable = 0;
        d->exits = d->entries = NULL;
        d->next = NULL;

        if (!dag->nodes)
            dag->nodes = d;
        else
            tail->next = d;
        tail = d;
    }

    /* Read initial node ID */
    k = dag_param_read(line, "Initial");
    if ((k < 0) || (k >= n_nodes)) {
        E_ERROR("Initial node parameter missing or invalid\n");
        goto load_error;
    }
    dag->start = darray[k];

    /* Read final node ID */
    k = dag_param_read(line, "Final");
    if ((k < 0) || (k >= n_nodes)) {
        E_ERROR("Final node parameter missing or invalid\n");
        goto load_error;
    }
    dag->end = darray[k];

    /* Read bestsegscore entries and ignore them. */
    if ((k = dag_param_read(line, "BestSegAscr")) < 0) {
        E_ERROR("BestSegAscr parameter missing\n");
        goto load_error;
    }
    for (i = 0; i < k; i++) {
        if ((line = lineiter_next(line)) == NULL) {
            E_ERROR("Premature EOF while (%s) ignoring BestSegAscr\n",
                    line);
            goto load_error;
        }
    }

    /* Read in edges. */
    while ((line = lineiter_next(line)) != NULL) {
        if (line->buf[0] == '#')
            continue;
        if (0 == strncmp(line->buf, "Edges", 5))
            break;
    }
    if (line == NULL) {
        E_ERROR("Edges missing\n");
        goto load_error;
    }
    while ((line = lineiter_next(line)) != NULL) {
        int from, to, ascr;
        ps_latnode_t *pd, *d;

        if (sscanf(line->buf, "%d %d %d", &from, &to, &ascr) != 3)
            break;
        if (ascr WORSE_THAN WORST_SCORE)
            continue;
        pd = darray[from];
        d = darray[to];
        if (logratio != 1.0f)
            ascr = (int32)(ascr * logratio);
        ps_lattice_link(dag, pd, d, ascr, d->sf - 1);
    }
    if (strcmp(line->buf, "End\n") != 0) {
        E_ERROR("Terminating 'End' missing\n");
        goto load_error;
    }
    lineiter_free(line);
    fclose_comp(fp, ispipe);
    ckd_free(darray);

    /* Minor hack: If the final node is a filler word and not </s>,
     * then set its base word ID to </s>, so that the language model
     * scores won't be screwed up. */
    if (dict_filler_word(dag->dict, dag->end->wid))
        dag->end->basewid = dag->search
            ? ps_search_finish_wid(dag->search)
            : dict_wordid(dag->dict, S3_FINISH_WORD);

    /* Mark reachable from dag->end */
    dag_mark_reachable(dag->end);

    /* Free nodes unreachable from dag->end and their links */
    ps_lattice_delete_unreachable(dag);

    if (ps) {
        /* Build links around silence and filler words, since they do
         * not exist in the language model.  FIXME: This is
         * potentially buggy, as we already do this before outputing
         * lattices. */
        pip = logmath_log(dag->lmath, cmd_ln_float32_r(ps->config, "-pip"));
        silpen = pip + logmath_log(dag->lmath,
                                   cmd_ln_float32_r(ps->config, "-silprob"));
        fillpen = pip + logmath_log(dag->lmath,
                                    cmd_ln_float32_r(ps->config, "-fillprob"));
        ps_lattice_penalize_fillers(dag, silpen, fillpen);
    }

    return dag;

  load_error:
    E_ERROR("Failed to load %s\n", file);
    lineiter_free(line);
    if (fp) fclose_comp(fp, ispipe);
    ckd_free(darray);
    return NULL;
}

int
ps_lattice_n_frames(ps_lattice_t *dag)
{
    return dag->n_frames;
}

ps_lattice_t *
ps_lattice_init_search(ps_search_t *search, int n_frame)
{
    ps_lattice_t *dag;

    dag = ckd_calloc(1, sizeof(*dag));
    dag->search = search;
    dag->dict = dict_retain(search->dict);
    dag->lmath = logmath_retain(search->acmod->lmath);
    dag->frate = cmd_ln_int32_r(dag->search->config, "-frate");
    dag->silence = dict_silwid(dag->dict);
    dag->n_frames = n_frame;
    dag->latnode_alloc = listelem_alloc_init(sizeof(ps_latnode_t));
    dag->latlink_alloc = listelem_alloc_init(sizeof(ps_latlink_t));
    dag->latlink_list_alloc = listelem_alloc_init(sizeof(latlink_list_t));
    dag->refcount = 1;
    return dag;
}

ps_lattice_t *
ps_lattice_retain(ps_lattice_t *dag)
{
    ++dag->refcount;
    return dag;
}

int
ps_lattice_free(ps_lattice_t *dag)
{
    if (dag == NULL)
        return 0;
    if (--dag->refcount > 0)
        return dag->refcount;
    logmath_free(dag->lmath);
    dict_free(dag->dict);
    listelem_alloc_free(dag->latnode_alloc);
    listelem_alloc_free(dag->latlink_alloc);
    listelem_alloc_free(dag->latlink_list_alloc);    
    ckd_free(dag->hyp_str);
    ckd_free(dag);
    return 0;
}

logmath_t *
ps_lattice_get_logmath(ps_lattice_t *dag)
{
    return dag->lmath;
}

ps_latnode_iter_t *
ps_latnode_iter(ps_lattice_t *dag)
{
    return dag->nodes;
}

ps_latnode_iter_t *
ps_latnode_iter_next(ps_latnode_iter_t *itor)
{
    return itor->next;
}

void
ps_latnode_iter_free(ps_latnode_iter_t *itor)
{
    /* Do absolutely nothing. */
}

ps_latnode_t *
ps_latnode_iter_node(ps_latnode_iter_t *itor)
{
    return itor;
}

int
ps_latnode_times(ps_latnode_t *node, int16 *out_fef, int16 *out_lef)
{
    if (out_fef) *out_fef = (int16)node->fef;
    if (out_lef) *out_lef = (int16)node->lef;
    return node->sf;
}

char const *
ps_latnode_word(ps_lattice_t *dag, ps_latnode_t *node)
{
    return dict_wordstr(dag->dict, node->wid);
}

char const *
ps_latnode_baseword(ps_lattice_t *dag, ps_latnode_t *node)
{
    return dict_wordstr(dag->dict, node->basewid);
}

int32
ps_latnode_prob(ps_lattice_t *dag, ps_latnode_t *node,
                ps_latlink_t **out_link)
{
    latlink_list_t *links;
    int32 bestpost = logmath_get_zero(dag->lmath);

    for (links = node->exits; links; links = links->next) {
        int32 post = links->link->alpha + links->link->beta - dag->norm;
        if (post > bestpost) {
            if (out_link) *out_link = links->link;
            bestpost = post;
        }
    }
    return bestpost;
}

ps_latlink_iter_t *
ps_latnode_exits(ps_latnode_t *node)
{
    return node->exits;
}

ps_latlink_iter_t *
ps_latnode_entries(ps_latnode_t *node)
{
    return node->entries;
}

ps_latlink_iter_t *
ps_latlink_iter_next(ps_latlink_iter_t *itor)
{
    return itor->next;
}

void
ps_latlink_iter_free(ps_latlink_iter_t *itor)
{
    /* Do absolutely nothing. */
}

ps_latlink_t *
ps_latlink_iter_link(ps_latlink_iter_t *itor)
{
    return itor->link;
}

int
ps_latlink_times(ps_latlink_t *link, int16 *out_sf)
{
    if (out_sf) {
        if (link->from) {
            *out_sf = link->from->sf;
        }
        else {
            *out_sf = 0;
        }
    }
    return link->ef;
}

ps_latnode_t *
ps_latlink_nodes(ps_latlink_t *link, ps_latnode_t **out_src)
{
    if (out_src) *out_src = link->from;
    return link->to;
}

char const *
ps_latlink_word(ps_lattice_t *dag, ps_latlink_t *link)
{
    if (link->from == NULL)
        return NULL;
    return dict_wordstr(dag->dict, link->from->wid);
}

char const *
ps_latlink_baseword(ps_lattice_t *dag, ps_latlink_t *link)
{
    if (link->from == NULL)
        return NULL;
    return dict_wordstr(dag->dict, link->from->basewid);
}

ps_latlink_t *
ps_latlink_pred(ps_latlink_t *link)
{
    return link->best_prev;
}

int32
ps_latlink_prob(ps_lattice_t *dag, ps_latlink_t *link, int32 *out_ascr)
{
    int32 post = link->alpha + link->beta - dag->norm;
    if (out_ascr) *out_ascr = link->ascr << SENSCR_SHIFT;
    return post;
}

char const *
ps_lattice_hyp(ps_lattice_t *dag, ps_latlink_t *link)
{
    ps_latlink_t *l;
    size_t len;
    char *c;

    /* Backtrace once to get hypothesis length. */
    len = 0;
    /* FIXME: There may not be a search, but actually there should be a dict. */
    if (dict_real_word(dag->dict, link->to->basewid)) {
	char *wstr = dict_wordstr(dag->dict, link->to->basewid);
	if (wstr != NULL)
	    len += strlen(wstr) + 1;
    }
    for (l = link; l; l = l->best_prev) {
        if (dict_real_word(dag->dict, l->from->basewid)) {
    	    char *wstr = dict_wordstr(dag->dict, l->from->basewid);
            if (wstr != NULL)
        	len += strlen(wstr) + 1;
        }
    }

    /* Backtrace again to construct hypothesis string. */
    ckd_free(dag->hyp_str);
    dag->hyp_str = ckd_calloc(1, len+1); /* extra one incase the hyp is empty */
    c = dag->hyp_str + len - 1;
    if (dict_real_word(dag->dict, link->to->basewid)) {
	char *wstr = dict_wordstr(dag->dict, link->to->basewid);
	if (wstr != NULL) {
    	    len = strlen(wstr);
	    c -= len;
    	    memcpy(c, wstr, len);
    	    if (c > dag->hyp_str) {
        	--c;
        	*c = ' ';
	    }
        }
    }
    for (l = link; l; l = l->best_prev) {
        if (dict_real_word(dag->dict, l->from->basewid)) {
    	    char *wstr = dict_wordstr(dag->dict, l->from->basewid);
    	    if (wstr != NULL) {
	        len = strlen(wstr);            
    		c -= len;
    		memcpy(c, wstr, len);
        	if (c > dag->hyp_str) {
            	    --c;
            	    *c = ' ';
        	}
    	    }
        }
    }

    return dag->hyp_str;
}

static void
ps_lattice_compute_lscr(ps_seg_t *seg, ps_latlink_t *link, int to)
{
    ngram_model_t *lmset;

    /* Language model score is included in the link score for FSG
     * search.  FIXME: Of course, this is sort of a hack :( */
    if (0 != strcmp(ps_search_name(seg->search), PS_SEARCH_NGRAM)) {
        seg->lback = 1; /* Unigram... */
        seg->lscr = 0;
        return;
    }
        
    lmset = ((ngram_search_t *)seg->search)->lmset;

    if (link->best_prev == NULL) {
        if (to) /* Sentence has only two words. */
            seg->lscr = ngram_bg_score(lmset, link->to->basewid,
                                       link->from->basewid, &seg->lback)
                >> SENSCR_SHIFT;
        else {/* This is the start symbol, its lscr is always 0. */
            seg->lscr = 0;
            seg->lback = 1;
        }
    }
    else {
        /* Find the two predecessor words. */
        if (to) {
            seg->lscr = ngram_tg_score(lmset, link->to->basewid,
                                       link->from->basewid,
                                       link->best_prev->from->basewid,
                                       &seg->lback) >> SENSCR_SHIFT;
        }
        else {
            if (link->best_prev->best_prev)
                seg->lscr = ngram_tg_score(lmset, link->from->basewid,
                                           link->best_prev->from->basewid,
                                           link->best_prev->best_prev->from->basewid,
                                           &seg->lback) >> SENSCR_SHIFT;
            else
                seg->lscr = ngram_bg_score(lmset, link->from->basewid,
                                           link->best_prev->from->basewid,
                                           &seg->lback) >> SENSCR_SHIFT;
        }
    }
}

static void
ps_lattice_link2itor(ps_seg_t *seg, ps_latlink_t *link, int to)
{
    dag_seg_t *itor = (dag_seg_t *)seg;
    ps_latnode_t *node;

    if (to) {
        node = link->to;
        seg->ef = node->lef;
        seg->prob = 0; /* norm + beta - norm */
    }
    else {
        latlink_list_t *x;
        ps_latnode_t *n;
        logmath_t *lmath = ps_search_acmod(seg->search)->lmath;

        node = link->from;
        seg->ef = link->ef;
        seg->prob = link->alpha + link->beta - itor->norm;
        /* Sum over all exits for this word and any alternate
           pronunciations at the same frame. */
        for (n = node; n; n = n->alt) {
            for (x = n->exits; x; x = x->next) {
                if (x->link == link)
                    continue;
                seg->prob = logmath_add(lmath, seg->prob,
                                        x->link->alpha + x->link->beta - itor->norm);
            }
        }
    }
    seg->word = dict_wordstr(ps_search_dict(seg->search), node->wid);
    seg->sf = node->sf;
    seg->ascr = link->ascr << SENSCR_SHIFT;
    /* Compute language model score from best predecessors. */
    ps_lattice_compute_lscr(seg, link, to);
}

static void
ps_lattice_seg_free(ps_seg_t *seg)
{
    dag_seg_t *itor = (dag_seg_t *)seg;
    
    ckd_free(itor->links);
    ckd_free(itor);
}

static ps_seg_t *
ps_lattice_seg_next(ps_seg_t *seg)
{
    dag_seg_t *itor = (dag_seg_t *)seg;

    ++itor->cur;
    if (itor->cur == itor->n_links + 1) {
        ps_lattice_seg_free(seg);
        return NULL;
    }
    else if (itor->cur == itor->n_links) {
        /* Re-use the last link but with the "to" node. */
        ps_lattice_link2itor(seg, itor->links[itor->cur - 1], TRUE);
    }
    else {
        ps_lattice_link2itor(seg, itor->links[itor->cur], FALSE);
    }

    return seg;
}

static ps_segfuncs_t ps_lattice_segfuncs = {
    /* seg_next */ ps_lattice_seg_next,
    /* seg_free */ ps_lattice_seg_free
};

ps_seg_t *
ps_lattice_seg_iter(ps_lattice_t *dag, ps_latlink_t *link, float32 lwf)
{
    dag_seg_t *itor;
    ps_latlink_t *l;
    int cur;

    /* Calling this an "iterator" is a bit of a misnomer since we have
     * to get the entire backtrace in order to produce it.
     */
    itor = ckd_calloc(1, sizeof(*itor));
    itor->base.vt = &ps_lattice_segfuncs;
    itor->base.search = dag->search;
    itor->base.lwf = lwf;
    itor->n_links = 0;
    itor->norm = dag->norm;

    for (l = link; l; l = l->best_prev) {
        ++itor->n_links;
    }
    if (itor->n_links == 0) {
        ckd_free(itor);
        return NULL;
    }

    itor->links = ckd_calloc(itor->n_links, sizeof(*itor->links));
    cur = itor->n_links - 1;
    for (l = link; l; l = l->best_prev) {
        itor->links[cur] = l;
        --cur;
    }

    ps_lattice_link2itor((ps_seg_t *)itor, itor->links[0], FALSE);
    return (ps_seg_t *)itor;
}

latlink_list_t *
latlink_list_new(ps_lattice_t *dag, ps_latlink_t *link, latlink_list_t *next)
{
    latlink_list_t *ll;

    ll = listelem_malloc(dag->latlink_list_alloc);
    ll->link = link;
    ll->next = next;

    return ll;
}

void
ps_lattice_pushq(ps_lattice_t *dag, ps_latlink_t *link)
{
    if (dag->q_head == NULL)
        dag->q_head = dag->q_tail = latlink_list_new(dag, link, NULL);
    else {
        dag->q_tail->next = latlink_list_new(dag, link, NULL);
        dag->q_tail = dag->q_tail->next;
    }

}

ps_latlink_t *
ps_lattice_popq(ps_lattice_t *dag)
{
    latlink_list_t *x;
    ps_latlink_t *link;

    if (dag->q_head == NULL)
        return NULL;
    link = dag->q_head->link;
    x = dag->q_head->next;
    listelem_free(dag->latlink_list_alloc, dag->q_head);
    dag->q_head = x;
    if (dag->q_head == NULL)
        dag->q_tail = NULL;
    return link;
}

void
ps_lattice_delq(ps_lattice_t *dag)
{
    while (ps_lattice_popq(dag)) {
        /* Do nothing. */
    }
}

ps_latlink_t *
ps_lattice_traverse_edges(ps_lattice_t *dag, ps_latnode_t *start, ps_latnode_t *end)
{
    ps_latnode_t *node;
    latlink_list_t *x;

    /* Cancel any unfinished traversal. */
    ps_lattice_delq(dag);

    /* Initialize node fanin counts and path scores. */
    for (node = dag->nodes; node; node = node->next)
        node->info.fanin = 0;
    for (node = dag->nodes; node; node = node->next) {
        for (x = node->exits; x; x = x->next)
            (x->link->to->info.fanin)++;
    }

    /* Initialize agenda with all exits from start. */
    if (start == NULL) start = dag->start;
    for (x = start->exits; x; x = x->next)
        ps_lattice_pushq(dag, x->link);

    /* Pull the first edge off the queue. */
    return ps_lattice_traverse_next(dag, end);
}

ps_latlink_t *
ps_lattice_traverse_next(ps_lattice_t *dag, ps_latnode_t *end)
{
    ps_latlink_t *next;

    next = ps_lattice_popq(dag);
    if (next == NULL)
        return NULL;

    /* Decrease fanin count for destination node and expand outgoing
     * edges if all incoming edges have been seen. */
    --next->to->info.fanin;
    if (next->to->info.fanin == 0) {
        latlink_list_t *x;

        if (end == NULL) end = dag->end;
        if (next->to == end) {
            /* If we have traversed all links entering the end node,
             * clear the queue, causing future calls to this function
             * to return NULL. */
            ps_lattice_delq(dag);
            return next;
        }

        /* Extend all outgoing edges. */
        for (x = next->to->exits; x; x = x->next)
            ps_lattice_pushq(dag, x->link);
    }
    return next;
}

ps_latlink_t *
ps_lattice_reverse_edges(ps_lattice_t *dag, ps_latnode_t *start, ps_latnode_t *end)
{
    ps_latnode_t *node;
    latlink_list_t *x;

    /* Cancel any unfinished traversal. */
    ps_lattice_delq(dag);

    /* Initialize node fanout counts and path scores. */
    for (node = dag->nodes; node; node = node->next) {
        node->info.fanin = 0;
        for (x = node->exits; x; x = x->next)
            ++node->info.fanin;
    }

    /* Initialize agenda with all entries from end. */
    if (end == NULL) end = dag->end;
    for (x = end->entries; x; x = x->next)
        ps_lattice_pushq(dag, x->link);

    /* Pull the first edge off the queue. */
    return ps_lattice_reverse_next(dag, start);
}

ps_latlink_t *
ps_lattice_reverse_next(ps_lattice_t *dag, ps_latnode_t *start)
{
    ps_latlink_t *next;

    next = ps_lattice_popq(dag);
    if (next == NULL)
        return NULL;

    /* Decrease fanout count for source node and expand incoming
     * edges if all incoming edges have been seen. */
    --next->from->info.fanin;
    if (next->from->info.fanin == 0) {
        latlink_list_t *x;

        if (start == NULL) start = dag->start;
        if (next->from == start) {
            /* If we have traversed all links entering the start node,
             * clear the queue, causing future calls to this function
             * to return NULL. */
            ps_lattice_delq(dag);
            return next;
        }

        /* Extend all outgoing edges. */
        for (x = next->from->entries; x; x = x->next)
            ps_lattice_pushq(dag, x->link);
    }
    return next;
}

/*
 * Find the best score from dag->start to end point of any link and
 * use it to update links further down the path.  This is like
 * single-source shortest path search, except that it is done over
 * edges rather than nodes, which allows us to do exact trigram scoring.
 *
 * Helpfully enough, we get half of the posterior probability
 * calculation for free that way too.  (interesting research topic: is
 * there a reliable Viterbi analogue to word-level Forward-Backward
 * like there is for state-level?  Or, is it just lattice density?)
 */
ps_latlink_t *
ps_lattice_bestpath(ps_lattice_t *dag, ngram_model_t *lmset,
                    float32 lwf, float32 ascale)
{
    ps_search_t *search;
    ps_latnode_t *node;
    ps_latlink_t *link;
    ps_latlink_t *bestend;
    latlink_list_t *x;
    logmath_t *lmath;
    int32 bestescr;

    search = dag->search;
    lmath = dag->lmath;

    /* Initialize path scores for all links exiting dag->start, and
     * set all other scores to the minimum.  Also initialize alphas to
     * log-zero. */
    for (node = dag->nodes; node; node = node->next) {
        for (x = node->exits; x; x = x->next) {
            x->link->path_scr = MAX_NEG_INT32;
            x->link->alpha = logmath_get_zero(lmath);
        }
    }
    for (x = dag->start->exits; x; x = x->next) {
        int32 n_used;
        int16 to_is_fil;

        to_is_fil = dict_filler_word(ps_search_dict(search), x->link->to->basewid) && x->link->to != dag->end;

        /* Best path points to dag->start, obviously. */
        x->link->path_scr = x->link->ascr;
        if (lmset && !to_is_fil)
            x->link->path_scr += (ngram_bg_score(lmset, x->link->to->basewid,
                                ps_search_start_wid(search), &n_used) >> SENSCR_SHIFT) * lwf;
        x->link->best_prev = NULL;
        /* No predecessors for start links. */
        x->link->alpha = 0;
    }

    /* Traverse the edges in the graph, updating path scores. */
    for (link = ps_lattice_traverse_edges(dag, NULL, NULL);
         link; link = ps_lattice_traverse_next(dag, NULL)) {
        int32 bprob, n_used;
        int32 w3_wid, w2_wid;
        int16 w3_is_fil, w2_is_fil;
        ps_latlink_t *prev_link;

        /* Sanity check, we should not be traversing edges that
         * weren't previously updated, otherwise nasty overflows will result. */
        assert(link->path_scr != MAX_NEG_INT32);

        /* Find word predecessor if from-word is filler */
        w3_wid = link->from->basewid;
        w2_wid = link->to->basewid;
        w3_is_fil = dict_filler_word(ps_search_dict(search), link->from->basewid) && link->from != dag->start;
        w2_is_fil = dict_filler_word(ps_search_dict(search), w2_wid) && link->to != dag->end;
        prev_link = link;

        if (w3_is_fil) {
            while (prev_link->best_prev != NULL) {
                prev_link = prev_link->best_prev;
                w3_wid = prev_link->from->basewid;
                if (!dict_filler_word(ps_search_dict(search), w3_wid) || prev_link->from == dag->start) {
                    w3_is_fil = FALSE;
                    break;
                }
            }
        }

        /* Calculate common bigram probability for all alphas. */
        if (lmset && !w3_is_fil && !w2_is_fil)
            bprob = ngram_ng_prob(lmset, w2_wid, &w3_wid, 1, &n_used);
        else
            bprob = 0;
        /* Add in this link's acoustic score, which was a constant
           factor in previous computations (if any). */
        link->alpha += (link->ascr << SENSCR_SHIFT) * ascale;

        if (w2_is_fil) {
            w2_is_fil = w3_is_fil;
            w3_is_fil = TRUE;
            w2_wid = w3_wid;
            while (prev_link->best_prev != NULL) {
                prev_link = prev_link->best_prev;
                w3_wid = prev_link->from->basewid;
                if (!dict_filler_word(ps_search_dict(search), w3_wid) || prev_link->from == dag->start) {
                    w3_is_fil = FALSE;
                    break;
                }
            }
        }

        /* Update scores for all paths exiting link->to. */
        for (x = link->to->exits; x; x = x->next) {
            int32 score;
            int32 w1_wid;
            int16 w1_is_fil;

            w1_wid = x->link->to->basewid;
            w1_is_fil = dict_filler_word(ps_search_dict(search), w1_wid) && x->link->to != dag->end;

            /* Update alpha with sum of previous alphas. */
            x->link->alpha = logmath_add(lmath, x->link->alpha, link->alpha + bprob);

            /* Update link score with maximum link score. */
            score = link->path_scr + x->link->ascr;
            /* Calculate language score for bestpath if possible */
            if (lmset && !w1_is_fil && !w2_is_fil) {
                if (w3_is_fil)
                    //partial context available
                    score += (ngram_bg_score(lmset, w1_wid, w2_wid, &n_used) >> SENSCR_SHIFT) * lwf;
                else
                    //full context available
                    score += (ngram_tg_score(lmset, w1_wid, w2_wid, w3_wid, &n_used) >> SENSCR_SHIFT) * lwf;
            }

            if (score BETTER_THAN x->link->path_scr) {
                x->link->path_scr = score;
                x->link->best_prev = link;
            }
        }
    }

    /* Find best link entering final node, and calculate normalizer
     * for posterior probabilities. */
    bestend = NULL;
    bestescr = MAX_NEG_INT32;

    /* Normalizer is the alpha for the imaginary link exiting the
       final node. */
    dag->norm = logmath_get_zero(lmath);
    for (x = dag->end->entries; x; x = x->next) {
        int32 bprob, n_used;
        int32 from_wid;
        int16 from_is_fil;

        from_wid = x->link->from->basewid;
        from_is_fil = dict_filler_word(ps_search_dict(search), from_wid) && x->link->from != dag->start;
        if (from_is_fil) {
            ps_latlink_t *prev_link = x->link;
            while (prev_link->best_prev != NULL) {
                prev_link = prev_link->best_prev;
                from_wid = prev_link->from->basewid;
                if (!dict_filler_word(ps_search_dict(search), from_wid) || prev_link->from == dag->start) {
                    from_is_fil = FALSE;
                    break;
                }
            }
        }

        if (lmset && !from_is_fil)
            bprob = ngram_ng_prob(lmset,
                                  x->link->to->basewid,
                                  &from_wid, 1, &n_used);
        else
            bprob = 0;
        dag->norm = logmath_add(lmath, dag->norm, x->link->alpha + bprob);
        if (x->link->path_scr BETTER_THAN bestescr) {
            bestescr = x->link->path_scr;
            bestend = x->link;
        }
    }
    /* FIXME: floating point... */
    dag->norm += (int32)(dag->final_node_ascr << SENSCR_SHIFT) * ascale;

    E_INFO("Bestpath score: %d\n", bestescr);
    E_INFO("Normalizer P(O) = alpha(%s:%d:%d) = %d\n",
           dict_wordstr(dag->search->dict, dag->end->wid),
           dag->end->sf, dag->end->lef,
           dag->norm);
    return bestend;
}

static int32
ps_lattice_joint(ps_lattice_t *dag, ps_latlink_t *link, float32 ascale)
{
    ngram_model_t *lmset;
    int32 jprob;

    /* Sort of a hack... */
    if (dag->search && 0 == strcmp(ps_search_name(dag->search), PS_SEARCH_NGRAM))
        lmset = ((ngram_search_t *)dag->search)->lmset;
    else
        lmset = NULL;

    jprob = (dag->final_node_ascr << SENSCR_SHIFT) * ascale;
    while (link) {
        if (lmset) {
            int lback;
            int32 from_wid, to_wid;
            int16 from_is_fil, to_is_fil;

            from_wid = link->from->basewid;
            to_wid = link->to->basewid;
            from_is_fil = dict_filler_word(dag->dict, from_wid) && link->from != dag->start;
            to_is_fil = dict_filler_word(dag->dict, to_wid) && link->to != dag->end;

            /* Find word predecessor if from-word is filler */
            if (!to_is_fil && from_is_fil) {
                ps_latlink_t *prev_link = link;
                while (prev_link->best_prev != NULL) {
                    prev_link = prev_link->best_prev;
                    from_wid = prev_link->from->basewid;
                    if (!dict_filler_word(dag->dict, from_wid) || prev_link->from == dag->start) {
                        from_is_fil = FALSE;
                        break;
                    }
                }
            }

            /* Compute unscaled language model probability.  Note that
               this is actually not the language model probability
               that corresponds to this link, but that is okay,
               because we are just taking the sum over all links in
               the best path. */
            if (!from_is_fil && !to_is_fil)
                jprob += ngram_ng_prob(lmset, to_wid,
                                       &from_wid, 1, &lback);
        }
        /* If there is no language model, we assume that the language
           model probability (such as it is) has been included in the
           link score. */
        jprob += (link->ascr << SENSCR_SHIFT) * ascale;
        link = link->best_prev;
    }

    E_INFO("Joint P(O,S) = %d P(S|O) = %d\n", jprob, jprob - dag->norm);
    return jprob;
}

int32
ps_lattice_posterior(ps_lattice_t *dag, ngram_model_t *lmset,
                     float32 ascale)
{
    logmath_t *lmath;
    ps_latnode_t *node;
    ps_latlink_t *link;
    latlink_list_t *x;
    ps_latlink_t *bestend;
    int32 bestescr;

    lmath = dag->lmath;

    /* Reset all betas to zero. */
    for (node = dag->nodes; node; node = node->next) {
        for (x = node->exits; x; x = x->next) {
            x->link->beta = logmath_get_zero(lmath);
        }
    }

    bestend = NULL;
    bestescr = MAX_NEG_INT32;
    /* Accumulate backward probabilities for all links. */
    for (link = ps_lattice_reverse_edges(dag, NULL, NULL);
         link; link = ps_lattice_reverse_next(dag, NULL)) {
        int32 bprob, n_used;
        int32 from_wid, to_wid;
        int16 from_is_fil, to_is_fil;

        from_wid = link->from->basewid;
        to_wid = link->to->basewid;
        from_is_fil = dict_filler_word(dag->dict, from_wid) && link->from != dag->start;
        to_is_fil = dict_filler_word(dag->dict, to_wid) && link->to != dag->end;

        /* Find word predecessor if from-word is filler */
        if (!to_is_fil && from_is_fil) {
            ps_latlink_t *prev_link = link;
            while (prev_link->best_prev != NULL) {
                prev_link = prev_link->best_prev;
                from_wid = prev_link->from->basewid;
                if (!dict_filler_word(dag->dict, from_wid) || prev_link->from == dag->start) {
                    from_is_fil = FALSE;
                    break;
                }
            }
        }

        /* Calculate LM probability. */
        if (lmset && !from_is_fil && !to_is_fil)
            bprob = ngram_ng_prob(lmset, to_wid, &from_wid, 1, &n_used);
        else
            bprob = 0;

        if (link->to == dag->end) {
            /* Track the best path - we will backtrace in order to
               calculate the unscaled joint probability for sentence
               posterior. */
            if (link->path_scr BETTER_THAN bestescr) {
                bestescr = link->path_scr;
                bestend = link;
            }
            /* Imaginary exit link from final node has beta = 1.0 */
            link->beta = bprob + (dag->final_node_ascr << SENSCR_SHIFT) * ascale;
        }
        else {
            /* Update beta from all outgoing betas. */
            for (x = link->to->exits; x; x = x->next) {
                link->beta = logmath_add(lmath, link->beta,
                                         x->link->beta + bprob
                                         + (x->link->ascr << SENSCR_SHIFT) * ascale);
            }
        }
    }

    /* Return P(S|O) = P(O,S)/P(O) */
    return ps_lattice_joint(dag, bestend, ascale) - dag->norm;
}

int32
ps_lattice_posterior_prune(ps_lattice_t *dag, int32 beam)
{
    ps_latlink_t *link;
    int npruned = 0;

    for (link = ps_lattice_traverse_edges(dag, dag->start, dag->end);
         link; link = ps_lattice_traverse_next(dag, dag->end)) {
        link->from->reachable = FALSE;
        if (link->alpha + link->beta - dag->norm < beam) {
            latlink_list_t *x, *tmp, *next;
            tmp = NULL;
            for (x = link->from->exits; x; x = next) {
                next = x->next;
                if (x->link == link) {
                    listelem_free(dag->latlink_list_alloc, x);
                }
                else {
                    x->next = tmp;
                    tmp = x;
                }
            }
            link->from->exits = tmp;
            tmp = NULL;
            for (x = link->to->entries; x; x = next) {
                next = x->next;
                if (x->link == link) {
                    listelem_free(dag->latlink_list_alloc, x);
                }
                else {
                    x->next = tmp;
                    tmp = x;
                }
            }
            link->to->entries = tmp;
            listelem_free(dag->latlink_alloc, link);
            ++npruned;
        }
    }
    dag_mark_reachable(dag->end);
    ps_lattice_delete_unreachable(dag);
    return npruned;
}


/* Parameters to prune n-best alternatives search */
#define MAX_PATHS	500     /* Max allowed active paths at any time */
#define MAX_HYP_TRIES	10000

/*
 * For each node in any path between from and end of utt, find the
 * best score from "from".sf to end of utt.  (NOTE: Uses bigram probs;
 * this is an estimate of the best score from "from".)  (NOTE #2: yes,
 * this is the "heuristic score" used in A* search)
 */
static int32
best_rem_score(ps_astar_t *nbest, ps_latnode_t * from)
{
    latlink_list_t *x;
    int32 bestscore, score;

    if (from->info.rem_score <= 0)
        return (from->info.rem_score);

    /* Best score from "from" to end of utt not known; compute from successors */
    bestscore = WORST_SCORE;
    for (x = from->exits; x; x = x->next) {
        int32 n_used;

        score = best_rem_score(nbest, x->link->to);
        score += x->link->ascr;
        if (nbest->lmset)
            score += (ngram_bg_score(nbest->lmset, x->link->to->basewid,
                                     from->basewid, &n_used) >> SENSCR_SHIFT)
                      * nbest->lwf;
        if (score BETTER_THAN bestscore)
            bestscore = score;
    }
    from->info.rem_score = bestscore;

    return bestscore;
}

/*
 * Insert newpath in sorted (by path score) list of paths.  But if newpath is
 * too far down the list, drop it (FIXME: necessary?)
 * total_score = path score (newpath) + rem_score to end of utt.
 */
static void
path_insert(ps_astar_t *nbest, ps_latpath_t *newpath, int32 total_score)
{
    ps_latpath_t *prev, *p;
    int32 i;

    prev = NULL;
    for (i = 0, p = nbest->path_list; (i < MAX_PATHS) && p; p = p->next, i++) {
        if ((p->score + p->node->info.rem_score) < total_score)
            break;
        prev = p;
    }

    /* newpath should be inserted between prev and p */
    if (i < MAX_PATHS) {
        /* Insert new partial hyp */
        newpath->next = p;
        if (!prev)
            nbest->path_list = newpath;
        else
            prev->next = newpath;
        if (!p)
            nbest->path_tail = newpath;

        nbest->n_path++;
        nbest->n_hyp_insert++;
        nbest->insert_depth += i;
    }
    else {
        /* newpath score too low; reject it and also prune paths beyond MAX_PATHS */
        nbest->path_tail = prev;
        prev->next = NULL;
        nbest->n_path = MAX_PATHS;
        listelem_free(nbest->latpath_alloc, newpath);

        nbest->n_hyp_reject++;
        for (; p; p = newpath) {
            newpath = p->next;
            listelem_free(nbest->latpath_alloc, p);
            nbest->n_hyp_reject++;
        }
    }
}

/* Find all possible extensions to given partial path */
static void
path_extend(ps_astar_t *nbest, ps_latpath_t * path)
{
    latlink_list_t *x;
    ps_latpath_t *newpath;
    int32 total_score, tail_score;

    /* Consider all successors of path->node */
    for (x = path->node->exits; x; x = x->next) {
        int32 n_used;

        /* Skip successor if no path from it reaches the final node */
        if (x->link->to->info.rem_score <= WORST_SCORE)
            continue;

        /* Create path extension and compute exact score for this extension */
        newpath = listelem_malloc(nbest->latpath_alloc);
        newpath->node = x->link->to;
        newpath->parent = path;
        newpath->score = path->score + x->link->ascr;
        if (nbest->lmset) {
            if (path->parent) {
                newpath->score += nbest->lwf
                    * (ngram_tg_score(nbest->lmset, newpath->node->basewid,
                                      path->node->basewid,
                                      path->parent->node->basewid, &n_used)
                       >> SENSCR_SHIFT);
            }
            else 
                newpath->score += nbest->lwf
                    * (ngram_bg_score(nbest->lmset, newpath->node->basewid,
                                      path->node->basewid, &n_used)
                       >> SENSCR_SHIFT);
        }

        /* Insert new partial path hypothesis into sorted path_list */
        nbest->n_hyp_tried++;
        total_score = newpath->score + newpath->node->info.rem_score;

        /* First see if hyp would be worse than the worst */
        if (nbest->n_path >= MAX_PATHS) {
            tail_score =
                nbest->path_tail->score
                + nbest->path_tail->node->info.rem_score;
            if (total_score < tail_score) {
                listelem_free(nbest->latpath_alloc, newpath);
                nbest->n_hyp_reject++;
                continue;
            }
        }

        path_insert(nbest, newpath, total_score);
    }
}

ps_astar_t *
ps_astar_start(ps_lattice_t *dag,
                  ngram_model_t *lmset,
                  float32 lwf,
                  int sf, int ef,
                  int w1, int w2)
{
    ps_astar_t *nbest;
    ps_latnode_t *node;

    nbest = ckd_calloc(1, sizeof(*nbest));
    nbest->dag = dag;
    nbest->lmset = lmset;
    nbest->lwf = lwf;
    nbest->sf = sf;
    if (ef < 0)
        nbest->ef = dag->n_frames + 1;
    else
        nbest->ef = ef;
    nbest->w1 = w1;
    nbest->w2 = w2;
    nbest->latpath_alloc = listelem_alloc_init(sizeof(ps_latpath_t));

    /* Initialize rem_score (A* heuristic) to default values */
    for (node = dag->nodes; node; node = node->next) {
        if (node == dag->end)
            node->info.rem_score = 0;
        else if (node->exits == NULL)
            node->info.rem_score = WORST_SCORE;
        else
            node->info.rem_score = 1;   /* +ve => unknown value */
    }

    /* Create initial partial hypotheses list consisting of nodes starting at sf */
    nbest->path_list = nbest->path_tail = NULL;
    for (node = dag->nodes; node; node = node->next) {
        if (node->sf == sf) {
            ps_latpath_t *path;
            int32 n_used;

            best_rem_score(nbest, node);
            path = listelem_malloc(nbest->latpath_alloc);
            path->node = node;
            path->parent = NULL;
            if (nbest->lmset)
                path->score = nbest->lwf *
                    ((w1 < 0)
                    ? ngram_bg_score(nbest->lmset, node->basewid, w2, &n_used)
                    : ngram_tg_score(nbest->lmset, node->basewid, w2, w1, &n_used));
            else
                path->score = 0;
            path->score >>= SENSCR_SHIFT;
            path_insert(nbest, path, path->score + node->info.rem_score);
        }
    }

    return nbest;
}

ps_latpath_t *
ps_astar_next(ps_astar_t *nbest)
{
    ps_lattice_t *dag;

    dag = nbest->dag;

    /* Pop the top (best) partial hypothesis */
    while ((nbest->top = nbest->path_list) != NULL) {
        nbest->path_list = nbest->path_list->next;
        if (nbest->top == nbest->path_tail)
            nbest->path_tail = NULL;
        nbest->n_path--;

        /* Complete hypothesis? */
        if ((nbest->top->node->sf >= nbest->ef)
            || ((nbest->top->node == dag->end) &&
                (nbest->ef > dag->end->sf))) {
            /* FIXME: Verify that it is non-empty.  Also we may want
             * to verify that it is actually distinct from other
             * paths, since often this is not the case*/
            return nbest->top;
        }
        else {
            if (nbest->top->node->fef < nbest->ef)
                path_extend(nbest, nbest->top);
        }
    }

    /* Did not find any more paths to extend. */
    return NULL;
}

char const *
ps_astar_hyp(ps_astar_t *nbest, ps_latpath_t *path)
{
    ps_search_t *search;
    ps_latpath_t *p;
    size_t len;
    char *c;
    char *hyp;

    search = nbest->dag->search;

    /* Backtrace once to get hypothesis length. */
    len = 0;
    for (p = path; p; p = p->parent) {
        if (dict_real_word(ps_search_dict(search), p->node->basewid)) {
    	    char *wstr = dict_wordstr(ps_search_dict(search), p->node->basewid);
    	    if (wstr != NULL)
    	        len += strlen(wstr) + 1;
        }
    }

    if (len == 0) {
	return NULL;
    }

    /* Backtrace again to construct hypothesis string. */
    hyp = ckd_calloc(1, len);
    c = hyp + len - 1;
    for (p = path; p; p = p->parent) {
        if (dict_real_word(ps_search_dict(search), p->node->basewid)) {
    	    char *wstr = dict_wordstr(ps_search_dict(search), p->node->basewid);
    	    if (wstr != NULL) {
	        len = strlen(wstr);
    		c -= len;
        	memcpy(c, wstr, len);
    		if (c > hyp) {
            	    --c;
        	    *c = ' ';
    		}
    	    }
        }
    }

    nbest->hyps = glist_add_ptr(nbest->hyps, hyp);
    return hyp;
}

static void
ps_astar_node2itor(astar_seg_t *itor)
{
    ps_seg_t *seg = (ps_seg_t *)itor;
    ps_latnode_t *node;

    assert(itor->cur < itor->n_nodes);
    node = itor->nodes[itor->cur];
    if (itor->cur == itor->n_nodes - 1)
        seg->ef = node->lef;
    else
        seg->ef = itor->nodes[itor->cur + 1]->sf - 1;
    seg->word = dict_wordstr(ps_search_dict(seg->search), node->wid);
    seg->sf = node->sf;
    seg->prob = 0; /* FIXME: implement forward-backward */
}

static void
ps_astar_seg_free(ps_seg_t *seg)
{
    astar_seg_t *itor = (astar_seg_t *)seg;
    ckd_free(itor->nodes);
    ckd_free(itor);
}

static ps_seg_t *
ps_astar_seg_next(ps_seg_t *seg)
{
    astar_seg_t *itor = (astar_seg_t *)seg;

    ++itor->cur;
    if (itor->cur == itor->n_nodes) {
        ps_astar_seg_free(seg);
        return NULL;
    }
    else {
        ps_astar_node2itor(itor);
    }

    return seg;
}

static ps_segfuncs_t ps_astar_segfuncs = {
    /* seg_next */ ps_astar_seg_next,
    /* seg_free */ ps_astar_seg_free
};

ps_seg_t *
ps_astar_seg_iter(ps_astar_t *astar, ps_latpath_t *path, float32 lwf)
{
    astar_seg_t *itor;
    ps_latpath_t *p;
    int cur;

    /* Backtrace and make an iterator, this should look familiar by now. */
    itor = ckd_calloc(1, sizeof(*itor));
    itor->base.vt = &ps_astar_segfuncs;
    itor->base.search = astar->dag->search;
    itor->base.lwf = lwf;
    itor->n_nodes = itor->cur = 0;
    for (p = path; p; p = p->parent) {
        ++itor->n_nodes;
    }
    itor->nodes = ckd_calloc(itor->n_nodes, sizeof(*itor->nodes));
    cur = itor->n_nodes - 1;
    for (p = path; p; p = p->parent) {
        itor->nodes[cur] = p->node;
        --cur;
    }

    ps_astar_node2itor(itor);
    return (ps_seg_t *)itor;
}

void
ps_astar_finish(ps_astar_t *nbest)
{
    gnode_t *gn;

    /* Free all hyps. */
    for (gn = nbest->hyps; gn; gn = gnode_next(gn)) {
        ckd_free(gnode_ptr(gn));
    }
    glist_free(nbest->hyps);
    /* Free all paths. */
    listelem_alloc_free(nbest->latpath_alloc);
    /* Free the Henge. */
    ckd_free(nbest);
}

