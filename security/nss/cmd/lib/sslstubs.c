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
#include "secport.h"	/* for UUID */
#ifdef XP_WIN
#include <IO.H>
#endif

#ifndef NOT_NULL
char *NOT_NULL(const char *x)
{
	return((char *) x);
}
#endif

/* Override the macro definition in xpassert.h if necessary. */
#ifdef XP_AssertAtLine
#undef XP_AssertAtLine
#endif

void XP_AssertAtLine(char *pFileName, int iLine)
{
	abort();
}

void FE_Trace(const char *msg)
{
	fputs(msg, stderr);
}

#ifdef XP_WIN
RPC_STATUS __stdcall UuidCreate(UUID *Uuid)
{
	return 0;
}

void FEU_StayingAlive(void)
{
}

int dupsocket(int sock)
{
	return dup(sock);
}
#endif
