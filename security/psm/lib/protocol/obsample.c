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
/* SAMPLE CODE
** Illustrates use of SSMObscure object methods.
**
** Author:  Nelson Bolyard	June 1999
*/

#include <stdio.h>
#include "obscure.h"


/* On error, returns -1.
** On success, returns non-negative number of unobscured bytes in buf
int
RecvInitObscureData(int fd, SSMObscureObject * obj, void * buf, int bufSize )
{
    SSMObscureBool done = 0;

    do {
	int cc;
	int rv;
	cc = read(fd, buf, bufSize);
	if (cc <= 0)
	    return -1;
	rv = SSMObscure_RecvInit(obj, buf, cc, &done);
    } while (!done);
    return rv;
}


/* returns -1 on error, 0 on success. */
int
SendInitObscureData(int fd, SSMObscureObject * obj)
{
    unsigned char * initBuf = NULL;
    int             rv      = -1;

    do {
	int bufLen;
	int len;
	int cc;

	bufLen = SSMObscure_SendInit(obj, NULL);
	if (bufLen <= 0)
	    break;

	initBuf = malloc(bufLen);
	if (!initBuf)
	    break;

	len = SSMObscure_SendInit(obj, initBuf);
	if (len != bufLen)
	    break;

	cc = write(fd, initBuf, len);

	/* Note, this code assumes a blocking socket, 
	** and hence doesn't deal with short writes.
	*/
	if (cc < len) 
	    break;

    	rv = 0;

    } while (0);

    if (initBuf) {
	free(initBuf);
    	initBuf = NULL;
    }
    return rv;
}

/* This is like write, but it obscures the data first. */
/* This code assumes a blocking socket, and so it doesn't handle short 
** writes.
*/
int 
obscuredWrite(SSMObscureObject * obj, int fd, void * buf, int len)
{
    int rv;
    int cc;

    cc = SSMObscure_Send(obj, buf, len);
    if (cc <= 0)
    	return cc;
    rv = write(fd, buf, cc);
    ASSERT(rv == cc || rv < 0);
    return rv;
}

/* This is like read, but it unobscures the data after reading it. */
int 
obscuredRead(SSMObscureObject * obj, int fd, void * buf, int len)
{
    int rv;
    int cc;

    do {
	cc = read(fd, buf, len);
	if (cc <= 0)
	    return cc;
	rv = SSMObscure_Recv(obj, buf, len);
    } while (rv == 0);
    return rv;
}

SSMObscureObject * sobj;
unsigned char buf[8192];

/* Call this with fd for socket that has just been accepted.
** returns -1 on error, 
** On success, returns non-negative number of bytes received in buf. 
*/
int
InitClientObscureObject(int fd) 
{
    int rv;

    sobj = SSMObscure_Create(0);
    if (!sobj)
    	return -1;

    rv = SendInitObscureData(fd, sobj);
    if (rv < 0)
    	return rv;

    rv = RecvInitObscureData(fd, sobj, buf, sizeof buf);
    return rv;
}

/* Call this with fd for socket that has just been connected.
** returns -1 on error, 
** On success, returns non-negative number of bytes received in buf. 
*/
int
InitServerObscureObject(int fd)
{
    int cc;

    sobj = SSMObscure_Create(1);
    if (!sobj)
    	return -1;

    cc = RecvInitObscureData(fd, sobj, buf, sizeof buf);
    if (cc < 0) 
    	return cc;


    rv = SendInitObscureData(fd, sobj);
    if (rv < 0)
    	return rv;

    return cc;
}

