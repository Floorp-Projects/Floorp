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
#include "blapit.h"
#include "secport.h"
#include "secerr.h"

/*
 * Prepare a buffer for DES encryption, growing to the appropriate boundary,
 * filling with the appropriate padding.
 *
 * NOTE: If arena is non-NULL, we re-allocate from there, otherwise
 * we assume (and use) XP memory (re)allocation.
 */
unsigned char *
DES_PadBuffer(PRArenaPool *arena, unsigned char *inbuf, unsigned int inlen,
	      unsigned int *outlen)
{
    unsigned char *outbuf;
    unsigned int   des_len;
    unsigned int   i;
    unsigned char  des_pad_len;

    /*
     * We need from 1 to DES_KEY_LENGTH bytes -- we *always* grow.
     * The extra bytes contain the value of the length of the padding:
     * if we have 2 bytes of padding, then the padding is "0x02, 0x02".
     */
    des_len = (inlen + DES_KEY_LENGTH) & ~(DES_KEY_LENGTH - 1);

    if (arena != NULL) {
	outbuf = (unsigned char*)PORT_ArenaGrow (arena, inbuf, inlen, des_len);
    } else {
	outbuf = (unsigned char*)PORT_Realloc (inbuf, des_len);
    }

    if (outbuf == NULL) {
	PORT_SetError (SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    des_pad_len = des_len - inlen;
    for (i = inlen; i < des_len; i++)
	outbuf[i] = des_pad_len;

    *outlen = des_len;
    return outbuf;
}
