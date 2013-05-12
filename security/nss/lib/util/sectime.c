/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prtime.h"
#include "secder.h"
#include "secitem.h"
#include "secerr.h"

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
CERT_UTCTime2FormattedAscii(PRTime utcTime, char *format)
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

char *CERT_GenTime2FormattedAscii(PRTime genTime, char *format)
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
    PRTime utcTime;
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

SECStatus DER_EncodeTimeChoice(PLArenaPool* arena, SECItem* output, PRTime input)
{
    SECStatus rv;

    rv = DER_TimeToUTCTimeArena(arena, output, input);
    if (rv == SECSuccess || PORT_GetError() != SEC_ERROR_INVALID_ARGS) {
        return rv;
    }
    return DER_TimeToGeneralizedTimeArena(arena, output, input);
}
