/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
#ifndef __obscure_h__
#define __obscure_h__ 1
#ifdef __cplusplus
extern "C" {
#endif
typedef unsigned char SSMObscureBool;

typedef struct SSMObscureObjectStr 	SSMObscureObject;

/* 
** Create a new Obscuring object 
*/
extern SSMObscureObject * SSMObscure_Create(SSMObscureBool IsServer);


/* Prepare initial buffer with initial message to send to other side to 
** establish cryptographic * synchronization.
**
** If buf is NULL, function returns the size of the buffer that 
** the caller needs to allocate for sending the initial message.
** 
** If buf is non-null, function returns the number of bytes of data filled 
** into buf, the amount that the caller should then send to the other side.
**
*/
extern int SSMObscure_SendInit( SSMObscureObject * obj,
				void *             buf);

/* 
** Obscure "len" bytes in "buf" before sending it. 
*/
extern int SSMObscure_Send(	SSMObscureObject * obj, 
                            	void *             buf, 
				unsigned int       len);

/* 
** UnObscure "len" bytes in "buf" after receiving it. 
** This function may absorb some or all of the received bytes, leaving 
** fewer bytes (possibly none) in the buffer for the application to use 
** than were in the buffer when the function was called.
** Function returns the number of bytes of unobscured data remaining in
** buf.  Zero means all data was used internally and no data remains 
** for application use. Negative number means error occurred.
*/
extern int SSMObscure_Recv(	SSMObscureObject * obj, 
                                void *             buf, 
				unsigned int       len);

/* like _Recv, but returns a flag telling when all initialization info has
** been received.
*/
extern int SSMObscure_RecvInit(	SSMObscureObject * obj, 
                                void *             buf, 
				unsigned int       len,
				SSMObscureBool   * done);

/* 
** Destroy the Obscure Object 
*/
extern int SSMObscure_Destroy(SSMObscureObject * obj);

#ifdef __cplusplus
}
#endif

#endif /* __obscure_h__ */
