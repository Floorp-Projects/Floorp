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
#include "secitem.h"
#include "secerr.h"

static const PRTime January1st2050  = LL_INIT(0x0008f81e, 0x1b098000);

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

char *
CERT_UTCTime2FormattedAscii (int64 utcTime, char *format)
{
    PRExplodedTime printableTime; 
    char *timeString;
   
    /* Converse time to local time and decompose it into components */
    PR_ExplodeTime(utcTime, PR_LocalTimeParameters, &printableTime);
    
    timeString = (char *)PORT_Alloc(256);

    if ( timeString ) {
        if ( ! PR_FormatTime( timeString, 256, format, &printableTime )) {
            PORT_Free(timeString);
            timeString = NULL;
        }
    }
    
    return (timeString);
}

char *CERT_GenTime2FormattedAscii (int64 genTime, char *format)
{
    PRExplodedTime printableTime; 
    char *timeString;
   
    /* Decompose time into components */
    PR_ExplodeTime(genTime, PR_GMTParameters, &printableTime);
    
    timeString = (char *)PORT_Alloc(256);

    if ( timeString ) {
        if ( ! PR_FormatTime( timeString, 256, format, &printableTime )) {
            PORT_Free(timeString);
            timeString = NULL;
            PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        }
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
