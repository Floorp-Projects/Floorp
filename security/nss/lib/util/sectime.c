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

#include "prlong.h"
#include "prtime.h"
#include "secder.h"
#include "cert.h"
#include "secitem.h"
#include "secerr.h"

const SEC_ASN1Template CERT_TimeChoiceTemplate[] = {
  { SEC_ASN1_CHOICE, offsetof(SECItem, type), 0, sizeof(SECItem) },
  { SEC_ASN1_UTC_TIME, 0, 0, siUTCTime },
  { SEC_ASN1_GENERALIZED_TIME, 0, 0, siGeneralizedTime },
  { 0 }
};

SEC_ASN1_CHOOSER_IMPLEMENT(CERT_TimeChoiceTemplate)

const SEC_ASN1Template CERT_ValidityTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTValidity) },
    { SEC_ASN1_INLINE,
	  offsetof(CERTValidity,notBefore), CERT_TimeChoiceTemplate, 0 },
    { SEC_ASN1_INLINE,
	  offsetof(CERTValidity,notAfter), CERT_TimeChoiceTemplate, 0 },
    { 0 }
};

PRTime January1st2050 = LL_INIT(0x0008f81e,0x1b098000);

static char *DecodeUTCTime2FormattedAscii (SECItem *utcTimeDER, char *format);
static char *DecodeGeneralizedTime2FormattedAscii (SECItem *generalizedTimeDER, char *format);

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

/* convert DER generalized time to ascii time string, only include day,
   not time */
char *
DER_GeneralizedDayToAscii(SECItem *gentime)
{
    return (DecodeGeneralizedTime2FormattedAscii (gentime, "%a %b %d, %Y"));
}

/* convert DER generalized or UTC time to ascii time string, only include
   day, not time */
char *
DER_TimeChoiceDayToAscii(SECItem *timechoice)
{
    switch (timechoice->type) {

    case siUTCTime:
        return DER_UTCDayToAscii(timechoice);

    case siGeneralizedTime:
        return DER_GeneralizedDayToAscii(timechoice);

    default:
        PORT_Assert(0);
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }
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
	rv = DER_EncodeTimeChoice(arena, &v->notBefore, notBefore);
	if (rv) goto loser;
	rv = DER_EncodeTimeChoice(arena, &v->notAfter, notAfter);
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

/* convert DER utc time to ascii time string, The format of the time string
   depends on the input "format"
 */
static char *
DecodeGeneralizedTime2FormattedAscii (SECItem *generalizedTimeDER,  char *format)
{
    PRTime generalizedTime;
    int rv;
   
    rv = DER_GeneralizedTimeToTime(&generalizedTime, generalizedTimeDER);
    if (rv) {
        return(NULL);
    }
    return (CERT_GeneralizedTime2FormattedAscii (generalizedTime, format));
}

/* decode a SECItem containing either a SEC_ASN1_GENERALIZED_TIME 
   or a SEC_ASN1_UTC_TIME */

SECStatus DER_DecodeTimeChoice(PRTime* output, const SECItem* input)
{
    switch (input->type) {
        case siGeneralizedTime:
            return DER_GeneralizedTimeToTime(output, input);

        case siUTCTime:
            return DER_UTCTimeToTime(output, input);

        default:
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            PORT_Assert(0);
            return SECFailure;
    }
}

/* encode a PRTime to an ASN.1 DER SECItem containing either a
   SEC_ASN1_GENERALIZED_TIME or a SEC_ASN1_UTC_TIME */

SECStatus DER_EncodeTimeChoice(PRArenaPool* arena, SECItem* output, PRTime input)
{
    if (LL_CMP(input, >, January1st2050)) {
        return DER_TimeToGeneralizedTimeArena(arena, output, input);
    } else {
        return DER_TimeToUTCTimeArena(arena, output, input);
    }
}
