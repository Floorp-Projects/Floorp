/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
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
 *
 * Implements a link between the (cf) CFont Interface and
 * its C++ implementation viz (f) FontObject.
 *
 * dp Suresh <dp@netscape.com>
 */


#ifndef _Pcf_H_
#define _Pcf_H_

#include "Mnfrc.h"
#include "Mcf.h"			/* Generated header */

struct cfImpl {
  cfImplHeader	header;
  void *object;				/* FontObject * */
};

/* The generated getInterface used the wrong object IDS. So we
 * override them with ours.
 */
#define OVERRIDE_cf_getInterface

/* The generated finalize doesn't have provision to free the
 * private data that we create inside the object. So we
 * override the finalize method and implement destruction
 * of our private data.
 */
#define OVERRIDE_cf_finalize

/* We are doing much more with the reference counting mechanism.
 * cf has all these refcounting it
 *	1. the users of the nff
 *	2. the fonthandles in the nff
 *
 * Other places a pointer to nff is stored but these dont increment
 * the refcount:
 *	1. FontObjectCache
 *	2. FontObject has a self member which is the nff
 *
 * We override the release to put a hook in for out FontObject
 * Garbage Collector to run.
*/
#define OVERRIDE_cf_release
#endif /* _Pcf_H_ */
