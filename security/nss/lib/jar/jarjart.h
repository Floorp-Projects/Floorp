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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
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
 *  jarjart.h
 *
 *  Functions for the Jartool, which is written in Java and
 *  requires wrappers located elsewhere in the client.
 *
 *  Do not call these unless you are the Jartool, no matter
 *  how convenient they may appear.
 *
 */

#ifndef _JARJART_H_
#define _JARJART_H_

/* return a list of certificate nicknames, separated by \n's */
extern char *JAR_JAR_list_certs (void);

/* validate archive, simple api */
extern int JAR_JAR_validate_archive (char *filename);

/* begin hash */
extern void *JAR_JAR_new_hash (int alg);

/* hash a streaming pile */
extern void *JAR_JAR_hash (int alg, void *cookie, int length, void *data);

/* end hash */
extern void *JAR_JAR_end_hash (int alg, void *cookie);

/* sign the archive (given an .SF file) with the given cert.
   The password argument is a relic, PKCS11 now handles that. */

extern int JAR_JAR_sign_archive 
   (char *nickname, char *password, char *sf, char *outsig);

/* convert status to text */
extern char *JAR_JAR_get_error (int status);

#endif
