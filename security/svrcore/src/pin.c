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
 * pin.c - SVRCORE module implementing PK11 pin callback support
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <svrcore.h>

#include <string.h>
#include <pk11func.h>
#include <seccomon.h>

/*
 * Global state
 */
static SVRCOREPinObj *pinObj = 0;

/*
 * SVRCORE_Pk11PasswordFunc
 *
 * DEPRECATED public interface.
 */
static char *
SVRCORE_Pk11PasswordFunc(PK11SlotInfo *slot, PRBool retry, void *ctx)
{
  char *passwd;

  /* If the user has not installed a cbk, then return failure (cancel) */
  if (pinObj == 0) return 0;

  /* Invoke the callback function, translating slot into token name */
  passwd = SVRCORE_GetPin(pinObj, PK11_GetTokenName(slot), retry);

  return passwd;
}

/*
 * SVRCORE_RegisterPinObj
 */
void
SVRCORE_RegisterPinObj(SVRCOREPinObj *obj)
{
  /* Set PK11 callback function to call back here */
  PK11_SetPasswordFunc(SVRCORE_Pk11PasswordFunc);

  /* Set object to use for getPin method */
  pinObj = obj;
}

/*
 * SVRCORE_GetRegisteredPinObj
 */
SVRCOREPinObj *
SVRCORE_GetRegisteredPinObj(void)
{
  return pinObj;
}
