/*
 *
 *    registrycb.c
 *
 *    $Source: /Users/ekr/tmp/nrappkit-dump/nrappkit/src/registry/registrycb.c,v $
 *    $Revision: 1.3 $
 *    $Date: 2007/06/26 22:37:51 $
 *
 *    Callback-related functions
 *
 *    
 *    Copyright (C) 2005, Network Resonance, Inc.
 *    Copyright (C) 2006, Network Resonance, Inc.
 *    All Rights Reserved
 *    
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions
 *    are met:
 *    
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of Network Resonance, Inc. nor the name of any
 *       contributors to this software may be used to endorse or promote 
 *       products derived from this software without specific prior written
 *       permission.
 *    
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 *    
 *
 */

#include <assert.h>
#include <string.h>
#include "registry.h"
#include "registry_int.h"
#include "r_assoc.h"
#include "r_errors.h"
#include "nr_common.h"
#include "r_log.h"
#include "r_macros.h"

static char CB_ACTIONS[] = { NR_REG_CB_ACTION_ADD,
                             NR_REG_CB_ACTION_DELETE,
                             NR_REG_CB_ACTION_CHANGE,
                             NR_REG_CB_ACTION_FINAL };

typedef struct nr_reg_cb_info_ {
     char            action;
     void          (*cb)(void *cb_arg, char action, NR_registry name);
     void           *cb_arg;
     NR_registry     name;
} nr_reg_cb_info;

/* callbacks that are registered, a mapping from names like "foo.bar.baz"
 * to an r_assoc which contains possibly several nr_reg_cb_info*'s */
static r_assoc     *nr_registry_callbacks = 0;

//static size_t SIZEOF_CB_ID = (sizeof(void (*)()) + 1);
#define SIZEOF_CB_ID  (sizeof(void (*)()) + 1)

static int nr_reg_validate_action(char action);
static int nr_reg_assoc_destroy(void *ptr);
static int compute_cb_id(void *cb, char action, unsigned char cb_id[SIZEOF_CB_ID]);
static int nr_reg_info_free(void *ptr);
static int nr_reg_raise_event_recurse(char *name, char *tmp, int action);
static int nr_reg_register_callback(NR_registry name, char action, void (*cb)(void *cb_arg, char action, NR_registry name), void *cb_arg);
static int nr_reg_unregister_callback(char *name, char action, void (*cb)(void *cb_arg, char action, NR_registry name));

int
nr_reg_cb_init()
{
    int r, _status;

    if (nr_registry_callbacks == 0) {
        if ((r=r_assoc_create(&nr_registry_callbacks, r_assoc_crc32_hash_compute, 12)))
            ABORT(r);
    }

    _status=0;
  abort:
    if (_status) {
       r_log(NR_LOG_REGISTRY, LOG_DEBUG, "Couldn't init notifications: %s", nr_strerror(_status));
    }
    return(_status);
}

int
nr_reg_validate_action(char action)
{
    int _status;
    int i;

    for (i = 0; i < sizeof(CB_ACTIONS); ++i) {
        if (action == CB_ACTIONS[i])
            return 0;
    }
    ABORT(R_BAD_ARGS);

    _status=0;
  abort:
    return(_status);
}

int
nr_reg_register_callback(NR_registry name, char action, void (*cb)(void *cb_arg, char action, NR_registry name), void *cb_arg)
{
    int r, _status;
    r_assoc *assoc;
    int create_assoc = 0;
    nr_reg_cb_info *info;
    int create_info = 0;
    unsigned char cb_id[SIZEOF_CB_ID];

    if (name == 0 || cb == 0)
      ABORT(R_BAD_ARGS);

    if (nr_registry_callbacks == 0)
      ABORT(R_FAILED);

    if ((r=nr_reg_is_valid(name)))
      ABORT(r);

    if ((r=nr_reg_validate_action(action)))
      ABORT(r);

    if ((r=r_assoc_fetch(nr_registry_callbacks, name, strlen(name)+1, (void*)&assoc))) {
      if (r == R_NOT_FOUND)
        create_assoc = 1;
      else
        ABORT(r);
    }

    if (create_assoc) {
      if ((r=r_assoc_create(&assoc, r_assoc_crc32_hash_compute, 5)))
        ABORT(r);

      if ((r=r_assoc_insert(nr_registry_callbacks, name, strlen(name)+1, assoc, 0, nr_reg_assoc_destroy, R_ASSOC_NEW)))
        ABORT(r);
    }

    if ((r=compute_cb_id(cb, action, cb_id)))
      ABORT(r);

    if ((r=r_assoc_fetch(assoc, (char*)cb_id, SIZEOF_CB_ID, (void*)&info))) {
      if (r == R_NOT_FOUND)
        create_info = 1;
      else
        ABORT(r);
    }

    if (create_info) {
      if (!(info=(void*)RCALLOC(sizeof(*info))))
        ABORT(R_NO_MEMORY);
    }

    strncpy(info->name, name, sizeof(info->name));
    info->action = action;
    info->cb = cb;
    info->cb_arg = cb_arg;

    if (create_info) {
      if ((r=r_assoc_insert(assoc, (char*)cb_id, SIZEOF_CB_ID, info, 0, nr_reg_info_free, R_ASSOC_NEW)))
        ABORT(r);
    }

    _status=0;
  abort:
    r_log(NR_LOG_REGISTRY, LOG_DEBUG, "register callback %X on '%s' for '%s' %s", cb, name, nr_reg_action_name(action), (_status ? "FAILED" : "succeeded"));

    if (_status) {
      if (create_info && info) RFREE(info);
      if (create_assoc && assoc) nr_reg_assoc_destroy(&assoc);
    }
    return(_status);
}

int
nr_reg_unregister_callback(char *name, char action, void (*cb)(void *cb_arg, char action, NR_registry name))
{
    int r, _status;
    r_assoc *assoc;
    int size;
    unsigned char cb_id[SIZEOF_CB_ID];

    if (name == 0 || cb == 0)
      ABORT(R_BAD_ARGS);

    if (nr_registry_callbacks == 0)
      ABORT(R_FAILED);

    if ((r=nr_reg_is_valid(name)))
      ABORT(r);

    if ((r=nr_reg_validate_action(action)))
      ABORT(r);

    if ((r=r_assoc_fetch(nr_registry_callbacks, name, strlen(name)+1, (void*)&assoc))) {
      if (r != R_NOT_FOUND)
        ABORT(r);
    }
    else {
      if ((r=compute_cb_id(cb, action, cb_id)))
        ABORT(r);

      if ((r=r_assoc_delete(assoc, (char*)cb_id, SIZEOF_CB_ID))) {
        if (r != R_NOT_FOUND)
          ABORT(r);
      }

      if ((r=r_assoc_num_elements(assoc, &size)))
        ABORT(r);

      if (size == 0) {
        if ((r=r_assoc_delete(nr_registry_callbacks, name, strlen(name)+1)))
          ABORT(r);
      }
    }

    _status=0;
  abort:
    r_log(NR_LOG_REGISTRY, LOG_DEBUG, "unregister callback %X on '%s' for '%s' %s", cb, name, nr_reg_action_name(action), (_status ? "FAILED" : "succeeded"));

    return(_status);
}

int
compute_cb_id(void *cb, char action, unsigned char cb_id[SIZEOF_CB_ID])
{
   /* callbacks are identified by the pointer to the cb function plus
    * the action being watched */
   assert(sizeof(cb) == sizeof(void (*)()));
   assert(sizeof(cb) == (SIZEOF_CB_ID - 1));

   memcpy(cb_id, &(cb), sizeof(cb));
   cb_id[SIZEOF_CB_ID-1] = action;

   return 0;
}

char *
nr_reg_action_name(int action)
{
    char *name = "*Unknown*";

    switch (action) {
    case NR_REG_CB_ACTION_ADD:     name = "add";     break;
    case NR_REG_CB_ACTION_DELETE:  name = "delete";  break;
    case NR_REG_CB_ACTION_CHANGE:  name = "change";  break;
    case NR_REG_CB_ACTION_FINAL:   name = "final";  break;
    }

    return name;
}

int
nr_reg_assoc_destroy(void *ptr)
{
  return r_assoc_destroy((r_assoc**)&ptr);
}

int
nr_reg_info_free(void *ptr)
{
  RFREE(ptr);
  return 0;
}

/* call with tmp=0 */
int
nr_reg_raise_event_recurse(char *name, char *tmp, int action)
{
    int r, _status;
    r_assoc *assoc;
    nr_reg_cb_info *info;
    r_assoc_iterator iter;
    char *key;
    int keyl;
    char *c;
    int free_tmp = 0;
    int count;

    if (tmp == 0) {
      if (!(tmp = (char*)r_strdup(name)))
        ABORT(R_NO_MEMORY);
      free_tmp = 1;
    }

    if ((r=r_assoc_fetch(nr_registry_callbacks, tmp, strlen(tmp)+1, (void*)&assoc))) {
      if (r != R_NOT_FOUND)
        ABORT(r);

      r_log(NR_LOG_REGISTRY, LOG_DEBUG, "No callbacks found on '%s'", tmp);
    }
    else {
      if (!r_assoc_num_elements(assoc, &count)) {
          r_log(NR_LOG_REGISTRY, LOG_DEBUG, "%d callback%s found on '%s'",
                count, ((count == 1) ? "" : "s"), tmp);
      }

      if ((r=r_assoc_init_iter(assoc, &iter)))
          ABORT(r);

      for (;;) {
        if ((r=r_assoc_iter(&iter, (void*)&key, &keyl, (void*)&info))) {
          if (r == R_EOD)
            break;
          else
            ABORT(r);
        }

        if (info->action == action) {
          r_log(NR_LOG_REGISTRY, LOG_DEBUG,
                "Invoking callback %X for '%s'",
                info->cb,
                nr_reg_action_name(info->action));

          (void)info->cb(info->cb_arg, action, name);
        }
        else {
          r_log(NR_LOG_REGISTRY, LOG_DEBUG,
                "Skipping callback %X for '%s'",
                info->cb,
                nr_reg_action_name(info->action));
        }
      }
    }

    if (strlen(tmp) > 0) {
        c = strrchr(tmp, '.');
        if (c != 0)
          *c = '\0';
        else
          tmp[0] = '\0';

        if ((r=nr_reg_raise_event_recurse(name, tmp, action)))
          ABORT(r);
    }

    _status=0;
  abort:
    if (free_tmp && tmp != 0) RFREE(tmp);
    return(_status);
}


/* NON-STATIC METHODS */

int
nr_reg_raise_event(char *name, int action)
{
    int r, _status;
    int count;
    char *event = nr_reg_action_name(action);

    r_log(NR_LOG_REGISTRY, LOG_DEBUG, "raising event '%s' on '%s'", event, name);

    if (name == 0)
      ABORT(R_BAD_ARGS);

    if ((r=nr_reg_validate_action(action)))
      ABORT(r);

    if ((r=r_assoc_num_elements(nr_registry_callbacks, &count)))
      ABORT(r);

    if (count > 0) {
      if ((r=nr_reg_raise_event_recurse(name, 0, action)))
        ABORT(r);
    }
    else {
      r_log(NR_LOG_REGISTRY, LOG_DEBUG, "No callbacks found");
      return 0;
    }

    _status=0;
  abort:
    return(_status);
}


/* PUBLIC METHODS */

int
NR_reg_register_callback(NR_registry name, char action, void (*cb)(void *cb_arg, char action, NR_registry name), void *cb_arg)
{
    int r, _status;
    int i;

    for (i = 0; i < sizeof(CB_ACTIONS); ++i) {
        if (action & CB_ACTIONS[i]) {
            if ((r=nr_reg_register_callback(name, CB_ACTIONS[i], cb, cb_arg)))
                ABORT(r);

            action &= ~(CB_ACTIONS[i]);
        }
    }

    if (action)
        ABORT(R_BAD_ARGS);

    _status=0;
  abort:
    return(_status);
}

int
NR_reg_unregister_callback(char *name, char action, void (*cb)(void *cb_arg, char action, NR_registry name))
{
    int r, _status;
    int i;

    for (i = 0; i < sizeof(CB_ACTIONS); ++i) {
        if (action & CB_ACTIONS[i]) {
            if ((r=nr_reg_unregister_callback(name, CB_ACTIONS[i], cb)))
                ABORT(r);

            action &= ~(CB_ACTIONS[i]);
        }
    }

    if (action)
        ABORT(R_BAD_ARGS);

    _status=0;
  abort:
    return(_status);
}
