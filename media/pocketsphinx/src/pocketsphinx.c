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

/* System headers. */
#include <stdio.h>
#include <assert.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* SphinxBase headers. */
#include <sphinxbase/err.h>
#include <sphinxbase/strfuncs.h>
#include <sphinxbase/filename.h>
#include <sphinxbase/pio.h>
#include <sphinxbase/jsgf.h>
#include <sphinxbase/hash_table.h>

/* Local headers. */
#include "cmdln_macro.h"
#include "pocketsphinx.h"
#include "pocketsphinx_internal.h"
#include "ps_lattice_internal.h"
#include "phone_loop_search.h"
#include "kws_search.h"
#include "fsg_search_internal.h"
#include "ngram_search.h"
#include "ngram_search_fwdtree.h"
#include "ngram_search_fwdflat.h"
#include "allphone_search.h"

static const arg_t ps_args_def[] = {
    POCKETSPHINX_OPTIONS,
    CMDLN_EMPTY_OPTION
};

/* I'm not sure what the portable way to do this is. */
static int
file_exists(const char *path)
{
    FILE *tmp;

    tmp = fopen(path, "rb");
    if (tmp) fclose(tmp);
    return (tmp != NULL);
}

#ifdef MODELDIR
static int
hmmdir_exists(const char *path)
{
    FILE *tmp;
    char *mdef = string_join(path, "/mdef", NULL);

    tmp = fopen(mdef, "rb");
    if (tmp) fclose(tmp);
    ckd_free(mdef);
    return (tmp != NULL);
}
#endif

static void
ps_add_file(ps_decoder_t *ps, const char *arg,
            const char *hmmdir, const char *file)
{
    char *tmp = string_join(hmmdir, "/", file, NULL);

    if (cmd_ln_str_r(ps->config, arg) == NULL && file_exists(tmp))
        cmd_ln_set_str_r(ps->config, arg, tmp);
    ckd_free(tmp);
}

static void
ps_init_defaults(ps_decoder_t *ps)
{
    /* Disable memory mapping on Blackfin (FIXME: should be uClinux in general). */
#ifdef __ADSPBLACKFIN__
    E_INFO("Will not use mmap() on uClinux/Blackfin.");
    cmd_ln_set_boolean_r(ps->config, "-mmap", FALSE);
#endif

    char const *hmmdir;
    /* Get acoustic model filenames and add them to the command-line */
    if ((hmmdir = cmd_ln_str_r(ps->config, "-hmm")) != NULL) {
        ps_add_file(ps, "-mdef", hmmdir, "mdef");
        ps_add_file(ps, "-mean", hmmdir, "means");
        ps_add_file(ps, "-var", hmmdir, "variances");
        ps_add_file(ps, "-tmat", hmmdir, "transition_matrices");
        ps_add_file(ps, "-mixw", hmmdir, "mixture_weights");
        ps_add_file(ps, "-sendump", hmmdir, "sendump");
        ps_add_file(ps, "-fdict", hmmdir, "noisedict");
        ps_add_file(ps, "-lda", hmmdir, "feature_transform");
        ps_add_file(ps, "-featparams", hmmdir, "feat.params");
        ps_add_file(ps, "-senmgau", hmmdir, "senmgau");
    }
}

static void
ps_free_searches(ps_decoder_t *ps)
{
    if (ps->searches) {
        /* Release keys manually as we used ckd_salloc to add them, release every search too. */
        hash_iter_t *search_it;
        for (search_it = hash_table_iter(ps->searches); search_it;
             search_it = hash_table_iter_next(search_it)) {
            ckd_free((char *) hash_entry_key(search_it->ent));
            ps_search_free(hash_entry_val(search_it->ent));
        }

        hash_table_empty(ps->searches);
        hash_table_free(ps->searches);
    }

    ps->searches = NULL;
    ps->search = NULL;
}

static ps_search_t *
ps_find_search(ps_decoder_t *ps, char const *name)
{
    void *search = NULL;
    hash_table_lookup(ps->searches, name, &search);

    return (ps_search_t *) search;
}

void
ps_default_search_args(cmd_ln_t *config)
{
#ifdef MODELDIR
    /* Set default acoustic and language models. */
    const char *hmmdir = cmd_ln_str_r(config, "-hmm");
    if (hmmdir == NULL && hmmdir_exists(MODELDIR "/en-us/en-us")) {
        hmmdir = MODELDIR "/en-us/en-us";
        cmd_ln_set_str_r(config, "-hmm", hmmdir);
    }

    const char *lmfile = cmd_ln_str_r(config, "-lm");

    if (lmfile == NULL && !cmd_ln_str_r(config, "-fsg")
        && !cmd_ln_str_r(config, "-jsgf")
        && !cmd_ln_str_r(config, "-lmctl")
        && !cmd_ln_str_r(config, "-kws")
        && !cmd_ln_str_r(config, "-keyphrase")
        && file_exists(MODELDIR "/en-us/en-us.lm.dmp")) {
        lmfile = MODELDIR "/en-us/en-us.lm.dmp";
        cmd_ln_set_str_r(config, "-lm", lmfile);
    }

    const char *dictfile = cmd_ln_str_r(config, "-dict");
    if (dictfile == NULL && file_exists(MODELDIR "/en-us/cmudict-en-us.dict")) {
        dictfile = MODELDIR "/en-us/cmudict-en-us.dict";
        cmd_ln_set_str_r(config, "-dict", dictfile);
    }

    /* Expand acoustic and language model filenames relative to installation
     * path. */
    if (hmmdir && !path_is_absolute(hmmdir) && !hmmdir_exists(hmmdir)) {
        char *tmphmm = string_join(MODELDIR "/hmm/", hmmdir, NULL);
        if (hmmdir_exists(tmphmm)) {
            cmd_ln_set_str_r(config, "-hmm", tmphmm);
        } else {
            E_ERROR("Failed to find mdef file inside the model folder "
                    "specified with -hmm `%s'\n", hmmdir);
        }
        ckd_free(tmphmm);
    }
    if (lmfile && !path_is_absolute(lmfile) && !file_exists(lmfile)) {
        char *tmplm = string_join(MODELDIR "/lm/", lmfile, NULL);
        cmd_ln_set_str_r(config, "-lm", tmplm);
        ckd_free(tmplm);
    }
    if (dictfile && !path_is_absolute(dictfile) && !file_exists(dictfile)) {
        char *tmpdict = string_join(MODELDIR "/lm/", dictfile, NULL);
        cmd_ln_set_str_r(config, "-dict", tmpdict);
        ckd_free(tmpdict);
    }
#endif
}

int
ps_reinit(ps_decoder_t *ps, cmd_ln_t *config)
{
    const char *path;
    const char *keyphrase;
    int32 lw;

    if (config && config != ps->config) {
        cmd_ln_free_r(ps->config);
        ps->config = cmd_ln_retain(config);
    }

    err_set_debug_level(cmd_ln_int32_r(ps->config, "-debug"));
    ps->mfclogdir = cmd_ln_str_r(ps->config, "-mfclogdir");
    ps->rawlogdir = cmd_ln_str_r(ps->config, "-rawlogdir");
    ps->senlogdir = cmd_ln_str_r(ps->config, "-senlogdir");

    /* Fill in some default arguments. */
    ps_init_defaults(ps);

    /* Free old searches (do this before other reinit) */
    ps_free_searches(ps);
    ps->searches = hash_table_new(3, HASH_CASE_YES);

    /* Free old acmod. */
    acmod_free(ps->acmod);
    ps->acmod = NULL;

    /* Free old dictionary (must be done after the two things above) */
    dict_free(ps->dict);
    ps->dict = NULL;

    /* Free d2p */
    dict2pid_free(ps->d2p);
    ps->d2p = NULL;

    /* Logmath computation (used in acmod and search) */
    if (ps->lmath == NULL
        || (logmath_get_base(ps->lmath) !=
            (float64)cmd_ln_float32_r(ps->config, "-logbase"))) {
        if (ps->lmath)
            logmath_free(ps->lmath);
        ps->lmath = logmath_init
            ((float64)cmd_ln_float32_r(ps->config, "-logbase"), 0,
             cmd_ln_boolean_r(ps->config, "-bestpath"));
    }

    /* Acoustic model (this is basically everything that
     * uttproc.c, senscr.c, and others used to do) */
    if ((ps->acmod = acmod_init(ps->config, ps->lmath, NULL, NULL)) == NULL)
        return -1;

    if (cmd_ln_int32_r(ps->config, "-pl_window") > 0) {
        /* Initialize an auxiliary phone loop search, which will run in
         * "parallel" with FSG or N-Gram search. */
        if ((ps->phone_loop =
             phone_loop_search_init(ps->config, ps->acmod, ps->dict)) == NULL)
            return -1;
        hash_table_enter(ps->searches,
                         ckd_salloc(ps_search_name(ps->phone_loop)),
                         ps->phone_loop);
    }

    /* Dictionary and triphone mappings (depends on acmod). */
    /* FIXME: pass config, change arguments, implement LTS, etc. */
    if ((ps->dict = dict_init(ps->config, ps->acmod->mdef, ps->acmod->lmath)) == NULL)
        return -1;
    if ((ps->d2p = dict2pid_build(ps->acmod->mdef, ps->dict)) == NULL)
        return -1;

    lw = cmd_ln_float32_r(config, "-lw");

    /* Determine whether we are starting out in FSG or N-Gram search mode.
     * If neither is used skip search initialization. */

    /* Load KWS if one was specified in config */
    if ((keyphrase = cmd_ln_str_r(config, "-keyphrase"))) {
        if (ps_set_keyphrase(ps, PS_DEFAULT_SEARCH, keyphrase))
            return -1;
        ps_set_search(ps, PS_DEFAULT_SEARCH);
    }

    if ((path = cmd_ln_str_r(config, "-kws"))) {
        if (ps_set_kws(ps, PS_DEFAULT_SEARCH, path))
            return -1;
        ps_set_search(ps, PS_DEFAULT_SEARCH);
    }

    /* Load an FSG if one was specified in config */
    if ((path = cmd_ln_str_r(config, "-fsg"))) {
        fsg_model_t *fsg = fsg_model_readfile(path, ps->lmath, lw);
        if (!fsg)
            return -1;
        if (ps_set_fsg(ps, PS_DEFAULT_SEARCH, fsg))
            return -1;
        ps_set_search(ps, PS_DEFAULT_SEARCH);
    }
    
    /* Or load a JSGF grammar */
    if ((path = cmd_ln_str_r(config, "-jsgf"))) {
        if (ps_set_jsgf_file(ps, PS_DEFAULT_SEARCH, path)
            || ps_set_search(ps, PS_DEFAULT_SEARCH))
            return -1;
    }

    if ((path = cmd_ln_str_r(ps->config, "-allphone"))) {
        if (ps_set_allphone_file(ps, PS_DEFAULT_SEARCH, path)
                || ps_set_search(ps, PS_DEFAULT_SEARCH))
                return -1;
    }

    if ((path = cmd_ln_str_r(ps->config, "-lm")) && 
        !cmd_ln_boolean_r(ps->config, "-allphone")) {
        if (ps_set_lm_file(ps, PS_DEFAULT_SEARCH, path)
            || ps_set_search(ps, PS_DEFAULT_SEARCH))
            return -1;
    }

    if ((path = cmd_ln_str_r(ps->config, "-lmctl"))) {
        const char *name;
        ngram_model_t *lmset;
        ngram_model_set_iter_t *lmset_it;

        if (!(lmset = ngram_model_set_read(ps->config, path, ps->lmath))) {
            E_ERROR("Failed to read language model control file: %s\n", path);
            return -1;
        }

        for(lmset_it = ngram_model_set_iter(lmset);
            lmset_it; lmset_it = ngram_model_set_iter_next(lmset_it)) {
            
            ngram_model_t *lm = ngram_model_set_iter_model(lmset_it, &name);            
            E_INFO("adding search %s\n", name);
            if (ps_set_lm(ps, name, lm)) {
    		ngram_model_free(lm);
                ngram_model_set_iter_free(lmset_it);
                return -1;
            }
	    ngram_model_free(lm);
        }

        name = cmd_ln_str_r(config, "-lmname");
        if (name)
            ps_set_search(ps, name);
        else {
            E_ERROR("No default LM name (-lmname) for `-lmctl'\n");
            return -1;
        }
    }

    /* Initialize performance timer. */
    ps->perf.name = "decode";
    ptmr_init(&ps->perf);

    return 0;
}

ps_decoder_t *
ps_init(cmd_ln_t *config)
{
    ps_decoder_t *ps;

    ps = ckd_calloc(1, sizeof(*ps));
    ps->refcount = 1;
    if (ps_reinit(ps, config) < 0) {
        ps_free(ps);
        return NULL;
    }
    return ps;
}

arg_t const *
ps_args(void)
{
    return ps_args_def;
}

ps_decoder_t *
ps_retain(ps_decoder_t *ps)
{
    ++ps->refcount;
    return ps;
}

int
ps_free(ps_decoder_t *ps)
{
    if (ps == NULL)
        return 0;
    if (--ps->refcount > 0)
        return ps->refcount;
    ps_free_searches(ps);
    dict_free(ps->dict);
    dict2pid_free(ps->d2p);
    acmod_free(ps->acmod);
    logmath_free(ps->lmath);
    cmd_ln_free_r(ps->config);
    ckd_free(ps);
    return 0;
}

cmd_ln_t *
ps_get_config(ps_decoder_t *ps)
{
    return ps->config;
}

logmath_t *
ps_get_logmath(ps_decoder_t *ps)
{
    return ps->lmath;
}

fe_t *
ps_get_fe(ps_decoder_t *ps)
{
    return ps->acmod->fe;
}

feat_t *
ps_get_feat(ps_decoder_t *ps)
{
    return ps->acmod->fcb;
}

ps_mllr_t *
ps_update_mllr(ps_decoder_t *ps, ps_mllr_t *mllr)
{
    return acmod_update_mllr(ps->acmod, mllr);
}

int
ps_set_search(ps_decoder_t *ps, const char *name)
{
    ps_search_t *search = ps_find_search(ps, name);
    if (search)
        ps->search = search;
    
    /* Set pl window depending on the search */
    if (!strcmp(PS_SEARCH_NGRAM, ps_search_name(search))) {
	ps->pl_window = cmd_ln_int32_r(ps->config, "-pl_window");
    } else {
	ps->pl_window = 0;
    }
    
    return search ? 0 : -1;
}

const char*
ps_get_search(ps_decoder_t *ps)
{
    hash_iter_t *search_it;
    const char* name = NULL;
    for (search_it = hash_table_iter(ps->searches); search_it;
        search_it = hash_table_iter_next(search_it)) {
        if (hash_entry_val(search_it->ent) == ps->search) {
    	    name = hash_entry_key(search_it->ent);
    	    break;
        }
    }
    return name;
}

int 
ps_unset_search(ps_decoder_t *ps, const char *name)
{
    ps_search_t *search = hash_table_delete(ps->searches, name);
    if (!search)
        return -1;
    if (ps->search == search)
        ps->search = NULL;
    ps_search_free(search);
    return 0;
}

ps_search_iter_t *
ps_search_iter(ps_decoder_t *ps)
{
   return (ps_search_iter_t *)hash_table_iter(ps->searches);
}

ps_search_iter_t *
ps_search_iter_next(ps_search_iter_t *itor)
{
   return (ps_search_iter_t *)hash_table_iter_next((hash_iter_t *)itor);
}

const char* 
ps_search_iter_val(ps_search_iter_t *itor)
{
   return (const char*)(((hash_iter_t *)itor)->ent->key);
}

void 
ps_search_iter_free(ps_search_iter_t *itor)
{
    hash_table_iter_free((hash_iter_t *)itor);
}

ngram_model_t *
ps_get_lm(ps_decoder_t *ps, const char *name)
{
    ps_search_t *search =  ps_find_search(ps, name);
    if (search && strcmp(PS_SEARCH_NGRAM, ps_search_name(search)))
        return NULL;
    return search ? ((ngram_search_t *) search)->lmset : NULL;
}

fsg_model_t *
ps_get_fsg(ps_decoder_t *ps, const char *name)
{
    ps_search_t *search = ps_find_search(ps, name);
    if (search && strcmp(PS_SEARCH_FSG, ps_search_name(search)))
        return NULL;
    return search ? ((fsg_search_t *) search)->fsg : NULL;
}

const char*
ps_get_kws(ps_decoder_t *ps, const char* name)
{
    ps_search_t *search = ps_find_search(ps, name);
    if (search && strcmp(PS_SEARCH_KWS, ps_search_name(search)))
        return NULL;
    return search ? kws_search_get_keywords(search) : NULL;
}

static int
set_search_internal(ps_decoder_t *ps, const char *name, ps_search_t *search)
{
    ps_search_t *old_search;
    
    if (!search)
	return 1;

    search->pls = ps->phone_loop;
    old_search = (ps_search_t *) hash_table_replace(ps->searches, ckd_salloc(name), search);
    if (old_search != search)
        ps_search_free(old_search);

    return 0;
}

int
ps_set_lm(ps_decoder_t *ps, const char *name, ngram_model_t *lm)
{
    ps_search_t *search;
    search = ngram_search_init(lm, ps->config, ps->acmod, ps->dict, ps->d2p);
    return set_search_internal(ps, name, search);
}

int
ps_set_lm_file(ps_decoder_t *ps, const char *name, const char *path)
{
  ngram_model_t *lm;
  int result;

  lm = ngram_model_read(ps->config, path, NGRAM_AUTO, ps->lmath);
  if (!lm)
      return -1;

  result = ps_set_lm(ps, name, lm);
  ngram_model_free(lm);
  return result;
}

int
ps_set_allphone(ps_decoder_t *ps, const char *name, ngram_model_t *lm)
{
    ps_search_t *search;
    search = allphone_search_init(lm, ps->config, ps->acmod, ps->dict, ps->d2p);
    return set_search_internal(ps, name, search);
}

int
ps_set_allphone_file(ps_decoder_t *ps, const char *name, const char *path)
{
  ngram_model_t *lm;
  int result;

  lm = NULL;
  if (path)
    lm = ngram_model_read(ps->config, path, NGRAM_AUTO, ps->lmath);
  result = ps_set_allphone(ps, name, lm);
  if (lm)
      ngram_model_free(lm);
  return result;
}

int
ps_set_kws(ps_decoder_t *ps, const char *name, const char *keyfile)
{
    ps_search_t *search;
    search = kws_search_init(NULL, keyfile, ps->config, ps->acmod, ps->dict, ps->d2p);
    return set_search_internal(ps, name, search);
}

int
ps_set_keyphrase(ps_decoder_t *ps, const char *name, const char *keyphrase)
{
    ps_search_t *search;
    search = kws_search_init(keyphrase, NULL, ps->config, ps->acmod, ps->dict, ps->d2p);
    return set_search_internal(ps, name, search);
}

int
ps_set_fsg(ps_decoder_t *ps, const char *name, fsg_model_t *fsg)
{
    ps_search_t *search;
    search = fsg_search_init(fsg, ps->config, ps->acmod, ps->dict, ps->d2p);
    return set_search_internal(ps, name, search);
}

int 
ps_set_jsgf_file(ps_decoder_t *ps, const char *name, const char *path)
{
  fsg_model_t *fsg;
  jsgf_rule_t *rule;
  char const *toprule;
  jsgf_t *jsgf = jsgf_parse_file(path, NULL);
  float lw;
  int result;

  if (!jsgf)
      return -1;

  rule = NULL;
  /* Take the -toprule if specified. */
  if ((toprule = cmd_ln_str_r(ps->config, "-toprule"))) {
      rule = jsgf_get_rule(jsgf, toprule);
      if (rule == NULL) {
          E_ERROR("Start rule %s not found\n", toprule);
          return -1;
      }
  } else {
      rule = jsgf_get_public_rule(jsgf);
      if (rule == NULL) {
          E_ERROR("No public rules found in %s\n", path);
          return -1;
      }
  }

  lw = cmd_ln_float32_r(ps->config, "-lw");
  fsg = jsgf_build_fsg(jsgf, rule, ps->lmath, lw);
  result = ps_set_fsg(ps, name, fsg);
  fsg_model_free(fsg);
  return result;
}

int 
ps_set_jsgf_string(ps_decoder_t *ps, const char *name, const char *jsgf_string)
{
  fsg_model_t *fsg;
  jsgf_rule_t *rule;
  char const *toprule;
  jsgf_t *jsgf = jsgf_parse_string(jsgf_string, NULL);
  float lw;
  int result;

  if (!jsgf)
      return -1;

  rule = NULL;
  /* Take the -toprule if specified. */
  if ((toprule = cmd_ln_str_r(ps->config, "-toprule"))) {
      rule = jsgf_get_rule(jsgf, toprule);
      if (rule == NULL) {
          E_ERROR("Start rule %s not found\n", toprule);
          return -1;
      }
  } else {
      rule = jsgf_get_public_rule(jsgf);
      if (rule == NULL) {
          E_ERROR("No public rules found in input string\n");
          return -1;
      }
  }

  lw = cmd_ln_float32_r(ps->config, "-lw");
  fsg = jsgf_build_fsg(jsgf, rule, ps->lmath, lw);
  result = ps_set_fsg(ps, name, fsg);
  fsg_model_free(fsg);
  return result;
}


int
ps_load_dict(ps_decoder_t *ps, char const *dictfile,
             char const *fdictfile, char const *format)
{
    cmd_ln_t *newconfig;
    dict2pid_t *d2p;
    dict_t *dict;
    hash_iter_t *search_it;

    /* Create a new scratch config to load this dict (so existing one
     * won't be affected if it fails) */
    newconfig = cmd_ln_init(NULL, ps_args(), TRUE, NULL);
    cmd_ln_set_boolean_r(newconfig, "-dictcase",
                         cmd_ln_boolean_r(ps->config, "-dictcase"));
    cmd_ln_set_str_r(newconfig, "-dict", dictfile);
    if (fdictfile)
        cmd_ln_set_str_r(newconfig, "-fdict", fdictfile);
    else
        cmd_ln_set_str_r(newconfig, "-fdict",
                         cmd_ln_str_r(ps->config, "-fdict"));

    /* Try to load it. */
    if ((dict = dict_init(newconfig, ps->acmod->mdef, ps->acmod->lmath)) == NULL) {
        cmd_ln_free_r(newconfig);
        return -1;
    }

    /* Reinit the dict2pid. */
    if ((d2p = dict2pid_build(ps->acmod->mdef, dict)) == NULL) {
        cmd_ln_free_r(newconfig);
        return -1;
    }

    /* Success!  Update the existing config to reflect new dicts and
     * drop everything into place. */
    cmd_ln_free_r(newconfig);
    cmd_ln_set_str_r(ps->config, "-dict", dictfile);
    if (fdictfile)
        cmd_ln_set_str_r(ps->config, "-fdict", fdictfile);
    dict_free(ps->dict);
    ps->dict = dict;
    dict2pid_free(ps->d2p);
    ps->d2p = d2p;

    /* And tell all searches to reconfigure themselves. */
    for (search_it = hash_table_iter(ps->searches); search_it;
       search_it = hash_table_iter_next(search_it)) {
        if (ps_search_reinit(hash_entry_val(search_it->ent), dict, d2p) < 0) {
            hash_table_iter_free(search_it);
            return -1;
        }
    }

    return 0;
}

int
ps_save_dict(ps_decoder_t *ps, char const *dictfile,
             char const *format)
{
    return dict_write(ps->dict, dictfile, format);
}

int
ps_add_word(ps_decoder_t *ps,
            char const *word,
            char const *phones,
            int update)
{
    int32 wid;
    s3cipid_t *pron;
    hash_iter_t *search_it;
    char **phonestr, *tmp;
    int np, i, rv;

    /* Parse phones into an array of phone IDs. */
    tmp = ckd_salloc(phones);
    np = str2words(tmp, NULL, 0);
    phonestr = ckd_calloc(np, sizeof(*phonestr));
    str2words(tmp, phonestr, np);
    pron = ckd_calloc(np, sizeof(*pron));
    for (i = 0; i < np; ++i) {
        pron[i] = bin_mdef_ciphone_id(ps->acmod->mdef, phonestr[i]);
        if (pron[i] == -1) {
            E_ERROR("Unknown phone %s in phone string %s\n",
                    phonestr[i], tmp);
            ckd_free(phonestr);
            ckd_free(tmp);
            ckd_free(pron);
            return -1;
        }
    }
    /* No longer needed. */
    ckd_free(phonestr);
    ckd_free(tmp);

    /* Add it to the dictionary. */
    if ((wid = dict_add_word(ps->dict, word, pron, np)) == -1) {
        ckd_free(pron);
        return -1;
    }
    /* No longer needed. */
    ckd_free(pron);

    /* Now we also have to add it to dict2pid. */
    dict2pid_add_word(ps->d2p, wid);

    /* TODO: we definitely need to refactor this */
    for (search_it = hash_table_iter(ps->searches); search_it;
         search_it = hash_table_iter_next(search_it)) {
        ps_search_t *search = hash_entry_val(search_it->ent);
        if (!strcmp(PS_SEARCH_NGRAM, ps_search_name(search))) {
            ngram_model_t *lmset = ((ngram_search_t *) search)->lmset;
            if (ngram_model_add_word(lmset, word, 1.0) == NGRAM_INVALID_WID) {
                hash_table_iter_free(search_it);
                return -1;
            }
        }

        if (update) {
            if ((rv = ps_search_reinit(search, ps->dict, ps->d2p) < 0)) {
                hash_table_iter_free(search_it);
                return rv;
            }
        }
    }

    /* Rebuild the widmap and search tree if requested. */
    return wid;
}

char *
ps_lookup_word(ps_decoder_t *ps, const char *word)
{
    s3wid_t wid;
    int32 phlen, j;
    char *phones;
    dict_t *dict = ps->dict;
    
    wid = dict_wordid(dict, word);
    if (wid == BAD_S3WID)
	return NULL;

    for (phlen = j = 0; j < dict_pronlen(dict, wid); ++j)
        phlen += strlen(dict_ciphone_str(dict, wid, j)) + 1;
    phones = ckd_calloc(1, phlen);
    for (j = 0; j < dict_pronlen(dict, wid); ++j) {
        strcat(phones, dict_ciphone_str(dict, wid, j));
        if (j != dict_pronlen(dict, wid) - 1)
            strcat(phones, " ");
    }
    return phones;
}

long
ps_decode_raw(ps_decoder_t *ps, FILE *rawfh,
              long maxsamps)
{
    int16 *data;
    long total, pos, endpos;

    ps_start_stream(ps);
    ps_start_utt(ps);

    /* If this file is seekable or maxsamps is specified, then decode
     * the whole thing at once. */
    if (maxsamps != -1) {
        data = ckd_calloc(maxsamps, sizeof(*data));
        total = fread(data, sizeof(*data), maxsamps, rawfh);
        ps_process_raw(ps, data, total, FALSE, TRUE);
        ckd_free(data);
    } else if ((pos = ftell(rawfh)) >= 0) {
        fseek(rawfh, 0, SEEK_END);
        endpos = ftell(rawfh);
        fseek(rawfh, pos, SEEK_SET);
        maxsamps = endpos - pos;

        data = ckd_calloc(maxsamps, sizeof(*data));
        total = fread(data, sizeof(*data), maxsamps, rawfh);
        ps_process_raw(ps, data, total, FALSE, TRUE);
        ckd_free(data);
    } else {
        /* Otherwise decode it in a stream. */
        total = 0;
        while (!feof(rawfh)) {
            int16 data[256];
            size_t nread;

            nread = fread(data, sizeof(*data), sizeof(data)/sizeof(*data), rawfh);
            ps_process_raw(ps, data, nread, FALSE, FALSE);
            total += nread;
        }
    }
    ps_end_utt(ps);
    return total;
}

int
ps_start_stream(ps_decoder_t *ps)
{
    acmod_start_stream(ps->acmod);
    return 0;
}

int
ps_start_utt(ps_decoder_t *ps)
{
    int rv;
    char uttid[16];
    
    if (ps->search == NULL) {
        E_ERROR("No search module is selected, did you forget to "
                "specify a language model or grammar?\n");
        return -1;
    }

    ptmr_reset(&ps->perf);
    ptmr_start(&ps->perf);

    sprintf(uttid, "%09u", ps->uttno);
    ++ps->uttno;

    /* Remove any residual word lattice and hypothesis. */
    ps_lattice_free(ps->search->dag);
    ps->search->dag = NULL;
    ps->search->last_link = NULL;
    ps->search->post = 0;
    ckd_free(ps->search->hyp_str);
    ps->search->hyp_str = NULL;

    if ((rv = acmod_start_utt(ps->acmod)) < 0)
        return rv;

    /* Start logging features and audio if requested. */
    if (ps->mfclogdir) {
        char *logfn = string_join(ps->mfclogdir, "/",
                                  uttid, ".mfc", NULL);
        FILE *mfcfh;
        E_INFO("Writing MFCC log file: %s\n", logfn);
        if ((mfcfh = fopen(logfn, "wb")) == NULL) {
            E_ERROR_SYSTEM("Failed to open MFCC log file %s", logfn);
            ckd_free(logfn);
            return -1;
        }
        ckd_free(logfn);
        acmod_set_mfcfh(ps->acmod, mfcfh);
    }
    if (ps->rawlogdir) {
        char *logfn = string_join(ps->rawlogdir, "/",
                                  uttid, ".raw", NULL);
        FILE *rawfh;
        E_INFO("Writing raw audio log file: %s\n", logfn);
        if ((rawfh = fopen(logfn, "wb")) == NULL) {
            E_ERROR_SYSTEM("Failed to open raw audio log file %s", logfn);
            ckd_free(logfn);
            return -1;
        }
        ckd_free(logfn);
        acmod_set_rawfh(ps->acmod, rawfh);
    }
    if (ps->senlogdir) {
        char *logfn = string_join(ps->senlogdir, "/",
                                  uttid, ".sen", NULL);
        FILE *senfh;
        E_INFO("Writing senone score log file: %s\n", logfn);
        if ((senfh = fopen(logfn, "wb")) == NULL) {
            E_ERROR_SYSTEM("Failed to open senone score log file %s", logfn);
            ckd_free(logfn);
            return -1;
        }
        ckd_free(logfn);
        acmod_set_senfh(ps->acmod, senfh);
    }

    /* Start auxiliary phone loop search. */
    if (ps->phone_loop)
        ps_search_start(ps->phone_loop);

    return ps_search_start(ps->search);
}

static int
ps_search_forward(ps_decoder_t *ps)
{
    int nfr;

    nfr = 0;
    while (ps->acmod->n_feat_frame > 0) {
        int k;
        if (ps->pl_window > 0)
            if ((k = ps_search_step(ps->phone_loop, ps->acmod->output_frame)) < 0)
                return k;
        if (ps->acmod->output_frame >= ps->pl_window)
            if ((k = ps_search_step(ps->search,
                                    ps->acmod->output_frame - ps->pl_window)) < 0)
                return k;
        acmod_advance(ps->acmod);
        ++ps->n_frame;
        ++nfr;
    }
    return nfr;
}

int
ps_decode_senscr(ps_decoder_t *ps, FILE *senfh)
{
    int nfr, n_searchfr;

    ps_start_utt(ps);
    n_searchfr = 0;
    acmod_set_insenfh(ps->acmod, senfh);
    while ((nfr = acmod_read_scores(ps->acmod)) > 0) {
        if ((nfr = ps_search_forward(ps)) < 0) {
            ps_end_utt(ps);
            return nfr;
        }
        n_searchfr += nfr;
    }
    ps_end_utt(ps);
    acmod_set_insenfh(ps->acmod, NULL);

    return n_searchfr;
}

int
ps_process_raw(ps_decoder_t *ps,
               int16 const *data,
               size_t n_samples,
               int no_search,
               int full_utt)
{
    int n_searchfr = 0;

    if (ps->acmod->state == ACMOD_IDLE) {
	E_ERROR("Failed to process data, utterance is not started. Use start_utt to start it\n");
	return 0;
    }

    if (no_search)
        acmod_set_grow(ps->acmod, TRUE);

    while (n_samples) {
        int nfr;

        /* Process some data into features. */
        if ((nfr = acmod_process_raw(ps->acmod, &data,
                                     &n_samples, full_utt)) < 0)
            return nfr;

        /* Score and search as much data as possible */
        if (no_search)
            continue;
        if ((nfr = ps_search_forward(ps)) < 0)
            return nfr;
        n_searchfr += nfr;
    }

    return n_searchfr;
}

int
ps_process_cep(ps_decoder_t *ps,
               mfcc_t **data,
               int32 n_frames,
               int no_search,
               int full_utt)
{
    int n_searchfr = 0;

    if (no_search)
        acmod_set_grow(ps->acmod, TRUE);

    while (n_frames) {
        int nfr;

        /* Process some data into features. */
        if ((nfr = acmod_process_cep(ps->acmod, &data,
                                     &n_frames, full_utt)) < 0)
            return nfr;

        /* Score and search as much data as possible */
        if (no_search)
            continue;
        if ((nfr = ps_search_forward(ps)) < 0)
            return nfr;
        n_searchfr += nfr;
    }

    return n_searchfr;
}

int
ps_end_utt(ps_decoder_t *ps)
{
    int rv, i;

    acmod_end_utt(ps->acmod);

    /* Search any remaining frames. */
    if ((rv = ps_search_forward(ps)) < 0) {
        ptmr_stop(&ps->perf);
        return rv;
    }
    /* Finish phone loop search. */
    if (ps->phone_loop) {
        if ((rv = ps_search_finish(ps->phone_loop)) < 0) {
            ptmr_stop(&ps->perf);
            return rv;
        }
    }
    /* Search any frames remaining in the lookahead window. */
    if (ps->acmod->output_frame >= ps->pl_window) {
        for (i = ps->acmod->output_frame - ps->pl_window;
             i < ps->acmod->output_frame; ++i)
            ps_search_step(ps->search, i);
    }
    /* Finish main search. */
    if ((rv = ps_search_finish(ps->search)) < 0) {
        ptmr_stop(&ps->perf);
        return rv;
    }
    ptmr_stop(&ps->perf);

    /* Log a backtrace if requested. */
    if (cmd_ln_boolean_r(ps->config, "-backtrace")) {
        const char* hyp;
        ps_seg_t *seg;
        int32 score;

        hyp = ps_get_hyp(ps, &score);
        
        if (hyp != NULL) {
    	    E_INFO("%s (%d)\n", hyp, score);
    	    E_INFO_NOFN("%-20s %-5s %-5s %-5s %-10s %-10s %-3s\n",
                    "word", "start", "end", "pprob", "ascr", "lscr", "lback");
    	    for (seg = ps_seg_iter(ps, &score); seg;
        	 seg = ps_seg_next(seg)) {
    	        char const *word;
        	int sf, ef;
        	int32 post, lscr, ascr, lback;

        	word = ps_seg_word(seg);
        	ps_seg_frames(seg, &sf, &ef);
        	post = ps_seg_prob(seg, &ascr, &lscr, &lback);
        	E_INFO_NOFN("%-20s %-5d %-5d %-1.3f %-10d %-10d %-3d\n",
                    	    word, sf, ef, logmath_exp(ps_get_logmath(ps), post),
                    	ascr, lscr, lback);
    	    }
        }
    }
    return rv;
}

char const *
ps_get_hyp(ps_decoder_t *ps, int32 *out_best_score)
{
    char const *hyp;

    ptmr_start(&ps->perf);
    hyp = ps_search_hyp(ps->search, out_best_score, NULL);
    ptmr_stop(&ps->perf);
    return hyp;
}

char const *
ps_get_hyp_final(ps_decoder_t *ps, int32 *out_is_final)
{
    char const *hyp;

    ptmr_start(&ps->perf);
    hyp = ps_search_hyp(ps->search, NULL, out_is_final);
    ptmr_stop(&ps->perf);
    return hyp;
}


int32
ps_get_prob(ps_decoder_t *ps)
{
    int32 prob;

    ptmr_start(&ps->perf);
    prob = ps_search_prob(ps->search);
    ptmr_stop(&ps->perf);
    return prob;
}

ps_seg_t *
ps_seg_iter(ps_decoder_t *ps, int32 *out_best_score)
{
    ps_seg_t *itor;

    ptmr_start(&ps->perf);
    itor = ps_search_seg_iter(ps->search, out_best_score);
    ptmr_stop(&ps->perf);
    return itor;
}

ps_seg_t *
ps_seg_next(ps_seg_t *seg)
{
    return ps_search_seg_next(seg);
}

char const *
ps_seg_word(ps_seg_t *seg)
{
    return seg->word;
}

void
ps_seg_frames(ps_seg_t *seg, int *out_sf, int *out_ef)
{
    int uf;
    uf = acmod_stream_offset(seg->search->acmod);
    if (out_sf) *out_sf = seg->sf + uf;
    if (out_ef) *out_ef = seg->ef + uf;
}

int32
ps_seg_prob(ps_seg_t *seg, int32 *out_ascr, int32 *out_lscr, int32 *out_lback)
{
    if (out_ascr) *out_ascr = seg->ascr;
    if (out_lscr) *out_lscr = seg->lscr;
    if (out_lback) *out_lback = seg->lback;
    return seg->prob;
}

void
ps_seg_free(ps_seg_t *seg)
{
    ps_search_seg_free(seg);
}

ps_lattice_t *
ps_get_lattice(ps_decoder_t *ps)
{
    return ps_search_lattice(ps->search);
}

ps_nbest_t *
ps_nbest(ps_decoder_t *ps, int sf, int ef,
         char const *ctx1, char const *ctx2)
{
    ps_lattice_t *dag;
    ngram_model_t *lmset;
    ps_astar_t *nbest;
    float32 lwf;
    int32 w1, w2;

    if (ps->search == NULL)
        return NULL;
    if ((dag = ps_get_lattice(ps)) == NULL)
        return NULL;

    /* FIXME: This is all quite specific to N-Gram search.  Either we
     * should make N-best a method for each search module or it needs
     * to be abstracted to work for N-Gram and FSG. */
    if (0 != strcmp(ps_search_name(ps->search), PS_SEARCH_NGRAM)) {
        lmset = NULL;
        lwf = 1.0f;
    } else {
        lmset = ((ngram_search_t *)ps->search)->lmset;
        lwf = ((ngram_search_t *)ps->search)->bestpath_fwdtree_lw_ratio;
    }

    w1 = ctx1 ? dict_wordid(ps_search_dict(ps->search), ctx1) : -1;
    w2 = ctx2 ? dict_wordid(ps_search_dict(ps->search), ctx2) : -1;
    nbest = ps_astar_start(dag, lmset, lwf, sf, ef, w1, w2);

    return (ps_nbest_t *)nbest;
}

void
ps_nbest_free(ps_nbest_t *nbest)
{
    ps_astar_finish(nbest);
}

ps_nbest_t *
ps_nbest_next(ps_nbest_t *nbest)
{
    ps_latpath_t *next;

    next = ps_astar_next(nbest);
    if (next == NULL) {
        ps_nbest_free(nbest);
        return NULL;
    }
    return nbest;
}

char const *
ps_nbest_hyp(ps_nbest_t *nbest, int32 *out_score)
{
    assert(nbest != NULL);

    if (nbest->top == NULL)
        return NULL;
    if (out_score) *out_score = nbest->top->score;
    return ps_astar_hyp(nbest, nbest->top);
}

ps_seg_t *
ps_nbest_seg(ps_nbest_t *nbest, int32 *out_score)
{
    if (nbest->top == NULL)
        return NULL;
    if (out_score) *out_score = nbest->top->score;
    return ps_astar_seg_iter(nbest, nbest->top, 1.0);
}

int
ps_get_n_frames(ps_decoder_t *ps)
{
    return ps->acmod->output_frame + 1;
}

void
ps_get_utt_time(ps_decoder_t *ps, double *out_nspeech,
                double *out_ncpu, double *out_nwall)
{
    int32 frate;

    frate = cmd_ln_int32_r(ps->config, "-frate");
    *out_nspeech = (double)ps->acmod->output_frame / frate;
    *out_ncpu = ps->perf.t_cpu;
    *out_nwall = ps->perf.t_elapsed;
}

void
ps_get_all_time(ps_decoder_t *ps, double *out_nspeech,
                double *out_ncpu, double *out_nwall)
{
    int32 frate;

    frate = cmd_ln_int32_r(ps->config, "-frate");
    *out_nspeech = (double)ps->n_frame / frate;
    *out_ncpu = ps->perf.t_tot_cpu;
    *out_nwall = ps->perf.t_tot_elapsed;
}

uint8 
ps_get_in_speech(ps_decoder_t *ps)
{
    return fe_get_vad_state(ps->acmod->fe);
}

void
ps_search_init(ps_search_t *search, ps_searchfuncs_t *vt,
               cmd_ln_t *config, acmod_t *acmod, dict_t *dict,
               dict2pid_t *d2p)
{
    search->vt = vt;
    search->config = config;
    search->acmod = acmod;
    if (d2p)
        search->d2p = dict2pid_retain(d2p);
    else
        search->d2p = NULL;
    if (dict) {
        search->dict = dict_retain(dict);
        search->start_wid = dict_startwid(dict);
        search->finish_wid = dict_finishwid(dict);
        search->silence_wid = dict_silwid(dict);
        search->n_words = dict_size(dict);
    }
    else {
        search->dict = NULL;
        search->start_wid = search->finish_wid = search->silence_wid = -1;
        search->n_words = 0;
    }
}

void
ps_search_base_reinit(ps_search_t *search, dict_t *dict,
                      dict2pid_t *d2p)
{
    dict_free(search->dict);
    dict2pid_free(search->d2p);
    /* FIXME: _retain() should just return NULL if passed NULL. */
    if (dict) {
        search->dict = dict_retain(dict);
        search->start_wid = dict_startwid(dict);
        search->finish_wid = dict_finishwid(dict);
        search->silence_wid = dict_silwid(dict);
        search->n_words = dict_size(dict);
    }
    else {
        search->dict = NULL;
        search->start_wid = search->finish_wid = search->silence_wid = -1;
        search->n_words = 0;
    }
    if (d2p)
        search->d2p = dict2pid_retain(d2p);
    else
        search->d2p = NULL;
}

void
ps_search_deinit(ps_search_t *search)
{
    /* FIXME: We will have refcounting on acmod, config, etc, at which
     * point we will free them here too. */
    dict_free(search->dict);
    dict2pid_free(search->d2p);
    ckd_free(search->hyp_str);
    ps_lattice_free(search->dag);
}

void
ps_set_rawdata_size(ps_decoder_t *ps, int32 size) 
{
    acmod_set_rawdata_size(ps->acmod, size);
}

void
ps_get_rawdata(ps_decoder_t *ps, int16 **buffer, int32 *size)
{
    acmod_get_rawdata(ps->acmod, buffer, size);
}
