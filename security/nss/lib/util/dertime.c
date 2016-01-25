/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prtypes.h"
#include "prtime.h"
#include "secder.h"
#include "prlong.h"
#include "secerr.h"

#define HIDIGIT(v) (((v) / 10) + '0')
#define LODIGIT(v) (((v) % 10) + '0')

#define ISDIGIT(dig) (((dig) >= '0') && ((dig) <= '9'))
#define CAPTURE(var,p,label)				  \
{							  \
    if (!ISDIGIT((p)[0]) || !ISDIGIT((p)[1])) goto label; \
    (var) = ((p)[0] - '0') * 10 + ((p)[1] - '0');	  \
    p += 2; \
}

static const PRTime January1st1     = PR_INT64(0xff23400100d44000);
static const PRTime January1st1950  = PR_INT64(0xfffdc1f8793da000);
static const PRTime January1st2050  = PR_INT64(0x0008f81e1b098000);
static const PRTime January1st10000 = PR_INT64(0x0384440ccc736000);

/* gmttime must contains UTC time in micro-seconds unit */
SECStatus
DER_TimeToUTCTimeArena(PLArenaPool* arenaOpt, SECItem *dst, PRTime gmttime)
{
    PRExplodedTime printableTime;
    unsigned char *d;

    if ( (gmttime < January1st1950) || (gmttime >= January1st2050) ) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    dst->len = 13;
    if (arenaOpt) {
        dst->data = d = (unsigned char*) PORT_ArenaAlloc(arenaOpt, dst->len);
    } else {
        dst->data = d = (unsigned char*) PORT_Alloc(dst->len);
    }
    dst->type = siUTCTime;
    if (!d) {
	return SECFailure;
    }

    /* Convert a PRTime to a printable format.  */
    PR_ExplodeTime(gmttime, PR_GMTParameters, &printableTime);

    /* The month in UTC time is base one */
    printableTime.tm_month++;

    /* remove the century since it's added to the tm_year by the 
       PR_ExplodeTime routine, but is not needed for UTC time */
    printableTime.tm_year %= 100; 

    d[0] = HIDIGIT(printableTime.tm_year);
    d[1] = LODIGIT(printableTime.tm_year);
    d[2] = HIDIGIT(printableTime.tm_month);
    d[3] = LODIGIT(printableTime.tm_month);
    d[4] = HIDIGIT(printableTime.tm_mday);
    d[5] = LODIGIT(printableTime.tm_mday);
    d[6] = HIDIGIT(printableTime.tm_hour);
    d[7] = LODIGIT(printableTime.tm_hour);
    d[8] = HIDIGIT(printableTime.tm_min);
    d[9] = LODIGIT(printableTime.tm_min);
    d[10] = HIDIGIT(printableTime.tm_sec);
    d[11] = LODIGIT(printableTime.tm_sec);
    d[12] = 'Z';
    return SECSuccess;
}

SECStatus
DER_TimeToUTCTime(SECItem *dst, PRTime gmttime)
{
    return DER_TimeToUTCTimeArena(NULL, dst, gmttime);
}

static SECStatus /* forward */
der_TimeStringToTime(PRTime *dst, const char *string, int generalized,
                     const char **endptr);

#define GEN_STRING 2 /* TimeString is a GeneralizedTime */
#define UTC_STRING 0 /* TimeString is a UTCTime         */

/* The caller of DER_AsciiToItem MUST ENSURE that either
** a) "string" points to a null-terminated ASCII string, or
** b) "string" points to a buffer containing a valid UTCTime, 
**     whether null terminated or not, or
** c) "string" contains at least 19 characters, with or without null char.
** otherwise, this function may UMR and/or crash.
** It suffices to ensure that the input "string" is at least 17 bytes long.
*/
SECStatus
DER_AsciiToTime(PRTime *dst, const char *string)
{
    return der_TimeStringToTime(dst, string, UTC_STRING, NULL);
}

SECStatus
DER_UTCTimeToTime(PRTime *dst, const SECItem *time)
{
    /* Minimum valid UTCTime is yymmddhhmmZ       which is 11 bytes. 
    ** Maximum valid UTCTime is yymmddhhmmss+0000 which is 17 bytes.
    ** 20 should be large enough for all valid encoded times. 
    */
    unsigned int i;
    char localBuf[20];
    const char *end = NULL;
    SECStatus rv;

    if (!time || !time->data || time->len < 11 || time->len > 17) {
	PORT_SetError(SEC_ERROR_INVALID_TIME);
	return SECFailure;
    }

    for (i = 0; i < time->len; i++) {
	if (time->data[i] == '\0') {
	    PORT_SetError(SEC_ERROR_INVALID_TIME);
	    return SECFailure;
	}
	localBuf[i] = time->data[i];
    }
    localBuf[i] = '\0';

    rv = der_TimeStringToTime(dst, localBuf, UTC_STRING, &end);
    if (rv == SECSuccess && *end != '\0') {
	PORT_SetError(SEC_ERROR_INVALID_TIME);
	return SECFailure;
    }
    return rv;
}

/*
   gmttime must contains UTC time in micro-seconds unit.
   Note: the caller should make sure that Generalized time
   should only be used for certifiate validities after the
   year 2049.  Otherwise, UTC time should be used.  This routine
   does not check this case, since it can be used to encode
   certificate extension, which does not have this restriction. 
 */
SECStatus
DER_TimeToGeneralizedTimeArena(PLArenaPool* arenaOpt, SECItem *dst, PRTime gmttime)
{
    PRExplodedTime printableTime;
    unsigned char *d;

    if ( (gmttime<January1st1) || (gmttime>=January1st10000) ) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    dst->len = 15;
    if (arenaOpt) {
        dst->data = d = (unsigned char*) PORT_ArenaAlloc(arenaOpt, dst->len);
    } else {
        dst->data = d = (unsigned char*) PORT_Alloc(dst->len);
    }
    dst->type = siGeneralizedTime;
    if (!d) {
	return SECFailure;
    }

    /* Convert a PRTime to a printable format.  */
    PR_ExplodeTime(gmttime, PR_GMTParameters, &printableTime);

    /* The month in Generalized time is base one */
    printableTime.tm_month++;

    d[0] = (printableTime.tm_year /1000) + '0';
    d[1] = ((printableTime.tm_year % 1000) / 100) + '0';
    d[2] = ((printableTime.tm_year % 100) / 10) + '0';
    d[3] = (printableTime.tm_year % 10) + '0';
    d[4] = HIDIGIT(printableTime.tm_month);
    d[5] = LODIGIT(printableTime.tm_month);
    d[6] = HIDIGIT(printableTime.tm_mday);
    d[7] = LODIGIT(printableTime.tm_mday);
    d[8] = HIDIGIT(printableTime.tm_hour);
    d[9] = LODIGIT(printableTime.tm_hour);
    d[10] = HIDIGIT(printableTime.tm_min);
    d[11] = LODIGIT(printableTime.tm_min);
    d[12] = HIDIGIT(printableTime.tm_sec);
    d[13] = LODIGIT(printableTime.tm_sec);
    d[14] = 'Z';
    return SECSuccess;
}

SECStatus
DER_TimeToGeneralizedTime(SECItem *dst, PRTime gmttime)
{
    return DER_TimeToGeneralizedTimeArena(NULL, dst, gmttime);
}


SECStatus
DER_GeneralizedTimeToTime(PRTime *dst, const SECItem *time)
{
    /* Minimum valid GeneralizedTime is ccyymmddhhmmZ       which is 13 bytes.
    ** Maximum valid GeneralizedTime is ccyymmddhhmmss+0000 which is 19 bytes.
    ** 20 should be large enough for all valid encoded times. 
    */
    unsigned int i;
    char localBuf[20];
    const char *end = NULL;
    SECStatus rv;

    if (!time || !time->data || time->len < 13 || time->len > 19) {
	PORT_SetError(SEC_ERROR_INVALID_TIME);
	return SECFailure;
    }

    for (i = 0; i < time->len; i++) {
	if (time->data[i] == '\0') {
	    PORT_SetError(SEC_ERROR_INVALID_TIME);
	    return SECFailure;
	}
	localBuf[i] = time->data[i];
    }
    localBuf[i] = '\0';

    rv = der_TimeStringToTime(dst, localBuf, GEN_STRING, &end);
    if (rv == SECSuccess && *end != '\0') {
	PORT_SetError(SEC_ERROR_INVALID_TIME);
	return SECFailure;
    }
    return rv;
}

static SECStatus
der_TimeStringToTime(PRTime *dst, const char *string, int generalized,
                     const char **endptr)
{
    PRExplodedTime genTime;
    long hourOff = 0, minOff = 0;
    PRUint16 century;
    char signum;

    if (string == NULL || dst == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    /* Verify time is formatted properly and capture information */
    memset(&genTime, 0, sizeof genTime);

    if (generalized == UTC_STRING) {
	CAPTURE(genTime.tm_year, string, loser);
	century = (genTime.tm_year < 50) ? 20 : 19;
    } else {
	CAPTURE(century, string, loser);
	CAPTURE(genTime.tm_year, string, loser);
    }
    genTime.tm_year += century * 100;

    CAPTURE(genTime.tm_month, string, loser);
    if ((genTime.tm_month == 0) || (genTime.tm_month > 12)) 
    	goto loser;

    /* NSPR month base is 0 */
    --genTime.tm_month;
    
    CAPTURE(genTime.tm_mday, string, loser);
    if ((genTime.tm_mday == 0) || (genTime.tm_mday > 31)) 
    	goto loser;
    
    CAPTURE(genTime.tm_hour, string, loser);
    if (genTime.tm_hour > 23) 
    	goto loser;
    
    CAPTURE(genTime.tm_min, string, loser);
    if (genTime.tm_min > 59) 
    	goto loser;
    
    if (ISDIGIT(string[0])) {
	CAPTURE(genTime.tm_sec, string, loser);
	if (genTime.tm_sec > 59) 
	    goto loser;
    }
    signum = *string++;
    if (signum == '+' || signum == '-') {
	CAPTURE(hourOff, string, loser);
	if (hourOff > 23) 
	    goto loser;
	CAPTURE(minOff, string, loser);
	if (minOff > 59) 
	    goto loser;
	if (signum == '-') {
	    hourOff = -hourOff;
	    minOff  = -minOff;
	}
    } else if (signum != 'Z') {
	goto loser;
    }

    if (endptr)
    	*endptr = string;

    /* Convert the GMT offset to seconds and save it in genTime
     * for the implode time call.
     */
    genTime.tm_params.tp_gmt_offset = (PRInt32)((hourOff * 60L + minOff) * 60L);
    *dst = PR_ImplodeTime(&genTime);
    return SECSuccess;

loser:
    PORT_SetError(SEC_ERROR_INVALID_TIME);
    return SECFailure;
}
