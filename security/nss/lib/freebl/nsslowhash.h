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
 * The Original Code is Red Hat, Inc.
 *
 * The Initial Developer of the Original Code is
 * Red Hat, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2008
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
 * Provide FIPS validated hashing for applications that only need hashing.
 * NOTE: mac'ing requires keys and will not work in this interface.
 * Also NOTE: this only works with Hashing. Only the FIPS interface is enabled.
 */

typedef struct NSSLOWInitContextStr NSSLOWInitContext;
typedef struct NSSLOWHASHContextStr NSSLOWHASHContext;

NSSLOWInitContext *NSSLOW_Init(void);
void NSSLOW_Shutdown(NSSLOWInitContext *context);
void NSSLOW_Reset(NSSLOWInitContext *context);
NSSLOWHASHContext *NSSLOWHASH_NewContext(
			NSSLOWInitContext *initContext, 
			HASH_HashType hashType);
void NSSLOWHASH_Begin(NSSLOWHASHContext *context);
void NSSLOWHASH_Update(NSSLOWHASHContext *context, 
			const unsigned char *buf, 
			unsigned int len);
void NSSLOWHASH_End(NSSLOWHASHContext *context, 
			unsigned char *buf, 
			unsigned int *ret, unsigned int len);
void NSSLOWHASH_Destroy(NSSLOWHASHContext *context);
unsigned int NSSLOWHASH_Length(NSSLOWHASHContext *context); 
