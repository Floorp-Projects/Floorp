/* -*- Mode: C; c-file-style: "stroustrup"; comment-column: 40 -*- */
/* 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Mailstone utility, 
 * released March 17, 2000.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s):	Dan Christian <robodan@netscape.com>
 *			Marcel DePaolis <marcel@netcape.com>
 *			Mike Blakely
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License Version 2 or later (the "GPL"), in
 * which case the provisions of the GPL are applicable instead of
 * those above.  If you wish to allow use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GPL.  If you do not delete the
 * provisions above, a recipient may use your version of this file
 * under either the NPL or the GPL.
 */
/*
  Stuff that deals with timevals.
*/
#include "bench.h"

double
timevaldouble(struct timeval *tin)
{
    return ((double)tin->tv_sec + ((double)tin->tv_usec / USECINSEC));
}


void
doubletimeval(const double tin, struct timeval *tout)
{
    tout->tv_sec = (long)floor(tin);	
    tout->tv_usec = (long)((tin - tout->tv_sec) * USECINSEC );
}

/* Difference two timevals and return as a double (in seconds) */
/* Could be a macro */
double
compdifftime_double(struct timeval *EndTime, struct timeval *StartTime)
{
    /* doing the integeger differences first is supposed to prevent
       any loss in resolution (more important if we returned float instead) */
    double d = (EndTime->tv_sec - StartTime->tv_sec)
	+ ((double)(EndTime->tv_usec - StartTime->tv_usec)*(1.0/USECINSEC));

    if (d < 0.0) {
	D_PRINTF (stderr, "Woa! compdifftime negative start %lu.%06lu end %lu.%06lu\n",
		  StartTime->tv_sec, StartTime->tv_usec,
		  EndTime->tv_sec, EndTime->tv_usec);
	return 0.0;
    }

    return d;
}
