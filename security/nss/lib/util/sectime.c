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

#include "prlong.h"
#include "prtime.h"
#include "secder.h"
#include "cert.h"
#include "secitem.h"

const SEC_ASN1Template CERT_ValidityTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTValidity) },
    { SEC_ASN1_UTC_TIME,
	  offsetof(CERTValidity,notBefore) },
    { SEC_ASN1_UTC_TIME,
	  offsetof(CERTValidity,notAfter) },
    { 0 }
};

DERTemplate CERTValidityTemplate[] = {
    { DER_SEQUENCE,
	  0, NULL, sizeof(CERTValidity) },
    { DER_UTC_TIME,
	  offsetof(CERTValidity,notBefore), },
    { DER_UTC_TIME,
	  offsetof(CERTValidity,notAfter), },
    { 0, }
};

static char *DecodeUTCTime2FormattedAscii (SECItem *utcTimeDER, char *format);

/* convert DER utc time to ascii time string */
char *
DER_UTCTimeToAscii(SECItem *utcTime)
{
    return (DecodeUTCTime2FormattedAscii (utcTime, "%a %b %d %H:%M:%S %Y"));
}

/* convert DER utc time to ascii time string, only include day, not time */
char *
DER_UTCDayToAscii(SECItem *utctime)
{
    return (DecodeUTCTime2FormattedAscii (utctime, "%a %b %d, %Y"));
}

CERTValidity *
CERT_CreateValidity(int64 notBefore, int64 notAfter)
{
    CERTValidity *v;
    int rv;
    PRArenaPool *arena;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    
    if ( !arena ) {
	return(0);
    }
    
    v = (CERTValidity*) PORT_ArenaZAlloc(arena, sizeof(CERTValidity));
    if (v) {
	v->arena = arena;
	rv = DER_TimeToUTCTime(&v->notBefore, notBefore);
	if (rv) goto loser;
	rv = DER_TimeToUTCTime(&v->notAfter, notAfter);
	if (rv) goto loser;
    }
    return v;

  loser:
    CERT_DestroyValidity(v);
    return 0;
}

SECStatus
CERT_CopyValidity(PRArenaPool *arena, CERTValidity *to, CERTValidity *from)
{
    SECStatus rv;

    CERT_DestroyValidity(to);
    to->arena = arena;
    
    rv = SECITEM_CopyItem(arena, &to->notBefore, &from->notBefore);
    if (rv) return rv;
    rv = SECITEM_CopyItem(arena, &to->notAfter, &from->notAfter);
    return rv;
}

void
CERT_DestroyValidity(CERTValidity *v)
{
    if (v && v->arena) {
	PORT_FreeArena(v->arena, PR_FALSE);
    }
    return;
}

char *
CERT_UTCTime2FormattedAscii (int64 utcTime, char *format)
{
    PRExplodedTime printableTime; 
    char *timeString;
   
    /* Converse time to local time and decompose it into components */
    PR_ExplodeTime(utcTime, PR_LocalTimeParameters, &printableTime);
    
    timeString = (char *)PORT_Alloc(100);

    if ( timeString ) {
        PR_FormatTime( timeString, 100, format, &printableTime );
    }
    
    return (timeString);
}

char *CERT_GenTime2FormattedAscii (int64 genTime, char *format)
{
    PRExplodedTime printableTime; 
    char *timeString;
   
    /* Decompose time into components */
    PR_ExplodeTime(genTime, PR_GMTParameters, &printableTime);
    
    timeString = (char *)PORT_Alloc(100);

    if ( timeString ) {
        PR_FormatTime( timeString, 100, format, &printableTime );
    }
    
    return (timeString);
}


/* convert DER utc time to ascii time string, The format of the time string
   depends on the input "format"
 */
static char *
DecodeUTCTime2FormattedAscii (SECItem *utcTimeDER,  char *format)
{
    int64 utcTime;
    int rv;
   
    rv = DER_UTCTimeToTime(&utcTime, utcTimeDER);
    if (rv) {
        return(NULL);
    }
    return (CERT_UTCTime2FormattedAscii (utcTime, format));
}
