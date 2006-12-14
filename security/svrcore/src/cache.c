/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape svrcore library.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * cache.c - SVRCORE module for caching PIN values
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <svrcore.h>

/* ------------------------------------------------------------ */
/*
 * Node - for maintaining link list of tokens with cached PINs
 */
typedef struct Node Node;
static void freeNode(Node *node);
static void freeList(Node *list);

struct Node
{
  Node *next;
  char *tokenName;
  SVRCOREPk11PinStore *store;
};

/* ------------------------------------------------------------ */
/*
 * SVRCORECachedPinObj implementation
 */
struct SVRCORECachedPinObj
{
  SVRCOREPinObj base;
  SVRCOREPinObj *alt;
  Node *pinList;
};
static const struct SVRCOREPinMethods vtable;

/* ------------------------------------------------------------ */
SVRCOREError
SVRCORE_CreateCachedPinObj(
  SVRCORECachedPinObj **out, SVRCOREPinObj *alt)
{
  SVRCOREError err = SVRCORE_Success;
  SVRCORECachedPinObj *obj;

  do {
    obj = (SVRCORECachedPinObj*)malloc(sizeof (SVRCORECachedPinObj));
    if (!obj) { err = SVRCORE_NoMemory_Error; break; }

    obj->base.methods = &vtable;

    obj->alt = alt;
    obj->pinList = 0;
  } while(0);

  *out = obj;
  return err;
}

void
SVRCORE_DestroyCachedPinObj(SVRCORECachedPinObj *obj)
{
  if (!obj) return;

  if (obj->pinList) freeList(obj->pinList);

  free(obj);
}

/* ------------------------------------------------------------ */
/*
 * vtable functions
 */
static void
destroyObject(SVRCOREPinObj *obj)
{
  SVRCORE_DestroyCachedPinObj((SVRCORECachedPinObj*)obj);
}

static char *
getPin(SVRCOREPinObj *ctx, const char *tokenName, PRBool retry)
{
  SVRCORECachedPinObj *obj = (SVRCORECachedPinObj*)ctx;
  Node **link, *node;
  char *pin = 0;

  /*
   * Look up the value in the cache.  Remove the entry if this is
   * a retry, or getting the stored value fails.  This loop terminates
   * with 'pin' set to any valid cached value.
   */
  for(link = &obj->pinList;(node = *link) != NULL;link = &node->next)
  {
    if (strcmp(node->tokenName, tokenName) != 0) continue;

    if (retry ||
        SVRCORE_Pk11StoreGetPin(&pin, node->store) != SVRCORE_Success)
    {
      *link = node->next;

      freeNode(node);
    }

    break;
  }

  /*
   * Now loop, attempting to read a pin from the alternate source
   * until cancelled, or a valid PIN is provided.
   */
  while(!pin)
  {
    SVRCOREError err;

    /* Call second level */
    pin = SVRCORE_GetPin(obj->alt, tokenName, retry);

    if (!pin) break; /* Cancel */

    /* Attempt to create a Pin Storage object.  This checks the
     * password. 
     */
    do {
      Node *node;

      node = (Node*)malloc(sizeof (Node));
      if (!node) { err = SVRCORE_NoMemory_Error; break; }

      node->tokenName = 0;
      node->store = 0;

      do {
        node->tokenName = strdup(tokenName);
        if (!node->tokenName) { err = SVRCORE_NoMemory_Error; break; }

        err = SVRCORE_CreatePk11PinStore(&node->store, tokenName, pin);
      } while(0);

      if (err) { freeNode(node); break; }

      node->next = obj->pinList;
      obj->pinList = node;
    } while(0);

    /* If node creation worked, then pin is correct */
    if (err == SVRCORE_Success) break;

    /* Quit on any error other than IncorrectPassword */
    if (err != SVRCORE_IncorrectPassword_Error) break;

    /* Password was incorrect, treat this as a retry */
    retry = PR_TRUE;
  }

  return pin;
}

/* ------------------------------------------------------------ */
/*
 * SVRCORE_CachedPinGetPin
 */
SVRCOREError
SVRCORE_CachedPinGetPin(
  char **out, SVRCORECachedPinObj *obj,
  const char *tokenName)
{
  SVRCOREError err;
  Node *node;

  *out = 0;

  do {
    /* Find a matching PIN node */
    for(node = obj->pinList;node;node = node->next)
    {
      if (strcmp(node->tokenName, tokenName) == 0) break;;
    }
    if (!node) { err = SVRCORE_NoSuchToken_Error; break; }

    err = SVRCORE_Pk11StoreGetPin(out, node->store);
  } while(0);

  return err;
}

static const struct SVRCOREPinMethods vtable =
{ 0, 0, destroyObject, getPin };

/* ------------------------------------------------------------ */
/*
 * Node implementation
 */
static void freeNode(Node *node)
{
  if (!node) return;

  if (node->store) SVRCORE_DestroyPk11PinStore(node->store);
  if (node->tokenName) free(node->tokenName);

  free(node);
}

static void freeList(Node *list)
{
  Node *n;

  while((n = list) != NULL)
  {
    list = n->next;

    free(n->tokenName);
    free(n);
  }
}
