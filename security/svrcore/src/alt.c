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
 * alt.c - SVRCORE module for reading a PIN from one of two alternate
 *   sources.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <svrcore.h>

/* ------------------------------------------------------------ */
/*
 * SVRCOREAltPinObj implementation
 */
struct SVRCOREAltPinObj
{
  SVRCOREPinObj base;
  SVRCOREPinObj *primary;
  SVRCOREPinObj *alt;
};
static const SVRCOREPinMethods vtable;

/* ------------------------------------------------------------ */
SVRCOREError
SVRCORE_CreateAltPinObj(
  SVRCOREAltPinObj **out,
  SVRCOREPinObj *primary, SVRCOREPinObj *alt)
{
  SVRCOREError err = SVRCORE_Success;
  SVRCOREAltPinObj *obj = 0;

  do {
    obj = (SVRCOREAltPinObj *)malloc(sizeof (SVRCOREAltPinObj));
    if (!obj) { err = SVRCORE_NoMemory_Error; break; }

    obj->base.methods = &vtable;

    obj->primary = primary;
    obj->alt = alt;
  } while(0);

  if (err != SVRCORE_Success)
  {
    SVRCORE_DestroyAltPinObj(obj);
  }

  *out = obj;

  return err;
}

void
SVRCORE_DestroyAltPinObj(
  SVRCOREAltPinObj *obj)
{
  if (!obj) return;

  free(obj);
}

/* ------------------------------------------------------------ */
/*
 * vtable methods
 */
static void
destroyObject(SVRCOREPinObj *obj)
{
  SVRCORE_DestroyAltPinObj((SVRCOREAltPinObj*)obj);
}

static char *
getPin(SVRCOREPinObj *pinObj, const char *tokenName, PRBool retry)
{
  SVRCOREAltPinObj *obj = (SVRCOREAltPinObj*)pinObj;
  char *res = 0;

  do {
    /* Try primary first */
    res = SVRCORE_GetPin(obj->primary, tokenName, retry);
    if (res) break;

    /* If unsucessful, try alternate source */
    res = SVRCORE_GetPin(obj->alt, tokenName, retry);
  } while(0);

  return res;
}

/*
 * VTable
 */
static const SVRCOREPinMethods vtable =
{ 0, 0, destroyObject, getPin };
