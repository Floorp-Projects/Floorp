/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * file: y2k.c
 * description: Test for y2k compliance for NSPR.
 */
/***********************************************************************
** Includes
***********************************************************************/
/* Used to get the command line option */
#include "plgetopt.h"

#include "prinit.h"
#include "prtime.h"
#include "prprf.h"
#include "prlog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef XP_MAC
#include "prlog.h"
#include "macstdlibextras.h"
extern void SetupMacPrintfLog(char *logFile);
#endif

int failed_already=0;
PRBool debug_mode = PR_FALSE;

static char *dayOfWeek[] =
	{ "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "???" };
static char *month[] =
	{ "Jan", "Feb", "Mar", "Apr", "May", "Jun",
	  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", "???" };

PRLogModuleInfo *lm;

static void PrintExplodedTime(const PRExplodedTime *et) {
    PRInt32 totalOffset;
    PRInt32 hourOffset, minOffset;
    const char *sign;

    /* Print day of the week, month, day, hour, minute, and second */
    printf("%s %s %2ld %02ld:%02ld:%02ld ",
	    dayOfWeek[et->tm_wday], month[et->tm_month], et->tm_mday,
	    et->tm_hour, et->tm_min, et->tm_sec);

    /* Print year */
    printf("%hd ", et->tm_year);

    /* Print time zone */
    totalOffset = et->tm_params.tp_gmt_offset + et->tm_params.tp_dst_offset;
    if (totalOffset == 0) {
	printf("UTC ");
    } else {
        sign = "+";
        if (totalOffset < 0) {
	    totalOffset = -totalOffset;
	    sign = "-";
        }
        hourOffset = totalOffset / 3600;
        minOffset = (totalOffset % 3600) / 60;
        printf("%s%02ld%02ld ", sign, hourOffset, minOffset);
    }
#ifdef PRINT_DETAILS
	printf("{%d, %d, %d, %d, %d, %d, %d, %d, %d, { %d, %d}}\n",et->tm_usec,
											et->tm_sec,
											et->tm_min,
											et->tm_hour,
											et->tm_mday,
											et->tm_month,
											et->tm_year,
											et->tm_wday,
											et->tm_yday,
											et->tm_params.tp_gmt_offset,
											et->tm_params.tp_dst_offset);
#endif
}

static int ExplodedTimeIsEqual(const PRExplodedTime *et1,
	const PRExplodedTime *et2)
{
    if (et1->tm_usec == et2->tm_usec &&
	    et1->tm_sec == et2->tm_sec &&
	    et1->tm_min == et2->tm_min &&
	    et1->tm_hour == et2->tm_hour &&
	    et1->tm_mday == et2->tm_mday &&
	    et1->tm_month == et2->tm_month &&
	    et1->tm_year == et2->tm_year &&
	    et1->tm_wday == et2->tm_wday &&
	    et1->tm_yday == et2->tm_yday &&
	    et1->tm_params.tp_gmt_offset == et2->tm_params.tp_gmt_offset &&
	    et1->tm_params.tp_dst_offset == et2->tm_params.tp_dst_offset) {
        return 1;
    } else {
	return 0;
    }
}

/*
 * TEST 1: TestExplodeImplodeTime
 * Description:
 * For each given timestamp T (a PRTime value), call PR_ExplodeTime
 * with GMT, US Pacific, and local time parameters.  Compare the
 * resulting calendar (exploded) time values with the expected
 * values.
 *
 * Note: the expected local time values depend on the local time
 * zone.  The local time values stored in this test are for the US
 * Pacific Time Zone.  If you are running this test in a different
 * time zone, you need to modify the values in the localt array.
 * An example is provided below.
 *
 * Call PR_ImplodeTime for each of the exploded values and compare
 * the resulting PRTime values with the original input.
 * 
 * This test is run for the values of time T corresponding to the
 * following dates:
 * - 12/31/99 - before 2000 
 * - 01/01/00 - after 2000 
 * - Leap year - Feb 29, 2000 
 * - March 1st, 2001 (after 1 year) 
 * - March 1st, 2005 (after second leap year) 
 * - 09/09/99 (used by some programs as an end of file marker)
 *
 * Call PR_Now, convert to calendar time using PR_ExplodeTime and
 * manually check the result for correctness. The time should match
 * the system clock.
 *
 * Tested functions: PR_Now, PR_ExplodeTime, PR_ImplodeTime,
 * PR_LocalTimeParameters, PR_GMTParameters. 
 */

static PRTime prt[] = {
    LL_INIT(220405, 2133125120),  /* 946634400000000 */
    LL_INIT(220425, 2633779200),  /* 946720800000000 */
    LL_INIT(221612, 2107598848),  /* 951818400000000 */
    LL_INIT(228975, 663398400),  /* 983440800000000 */
    LL_INIT(258365, 1974568960),  /* 1109671200000000 */
    LL_INIT(218132, 1393788928)  /* 936871200000000 */
};

static PRExplodedTime gmt[] = {
    { 0, 0, 0, 10, 31, 11, 1999, 5, 364, {0, 0}}, /* 1999/12/31 10:00:00 GMT */
    { 0, 0, 0, 10, 1, 0, 2000, 6, 0, {0, 0}}, /* 2000/01/01 10:00:00 GMT */
    { 0, 0, 0, 10, 29, 1, 2000, 2, 59, {0, 0}}, /* 2000/02/29 10:00:00 GMT */
    { 0, 0, 0, 10, 1, 2, 2001, 4, 59, {0, 0}}, /* 2001/3/1 10:00:00 GMT */
    { 0, 0, 0, 10, 1, 2, 2005, 2, 59, {0, 0}}, /* 2005/3/1 10:00:00 GMT */
    { 0, 0, 0, 10, 9, 8, 1999, 4, 251, {0, 0}}  /* 1999/9/9 10:00:00 GMT */
};

static PRExplodedTime uspt[] = {
{ 0, 0, 0, 2, 31, 11, 1999, 5, 364, {-28800, 0}}, /* 1999/12/31 2:00:00 PST */
{ 0, 0, 0, 2, 1, 0, 2000, 6, 0, {-28800, 0}}, /* 2000/01/01 2:00:00 PST */
{ 0, 0, 0, 2, 29, 1, 2000, 2, 59, {-28800, 0}}, /* 2000/02/29 2:00:00 PST */
{ 0, 0, 0, 2, 1, 2, 2001, 4, 59, {-28800, 0}}, /* 2001/3/1 2:00:00 PST */
{ 0, 0, 0, 2, 1, 2, 2005, 2, 59, {-28800, 0}}, /* 2005/3/1 2:00:00 PST */
{ 0, 0, 0, 3, 9, 8, 1999, 4, 251, {-28800, 3600}}  /* 1999/9/9 3:00:00 PDT */
};

/*
 * This test assumes that we are in US Pacific Time Zone.
 * If you are running this test in a different time zone,
 * you need to modify the localt array and fill in the
 * expected results.  The localt array for US Eastern Time
 * Zone is provided as an example.
 */
static PRExplodedTime localt[] = {
{ 0, 0, 0, 2, 31, 11, 1999, 5, 364, {-28800, 0}}, /* 1999/12/31 2:00:00 PST */
{ 0, 0, 0, 2, 1, 0, 2000, 6, 0, {-28800, 0}}, /* 2000/01/01 2:00:00 PST */
{ 0, 0, 0, 2, 29, 1, 2000, 2, 59, {-28800, 0}}, /* 2000/02/29 2:00:00 PST */
{ 0, 0, 0, 2, 1, 2, 2001, 4, 59, {-28800, 0}}, /* 2001/3/1 2:00:00 PST */
{ 0, 0, 0, 2, 1, 2, 2005, 2, 59, {-28800, 0}}, /* 2005/3/1 2:00:00 PST */
{ 0, 0, 0, 3, 9, 8, 1999, 4, 251, {-28800, 3600}}  /* 1999/9/9 3:00:00 PDT */
};

#ifdef US_EASTERN_TIME
static PRExplodedTime localt[] = {
{ 0, 0, 0, 5, 31, 11, 1999, 5, 364, {-18000, 0}}, /* 1999/12/31 2:00:00 EST */
{ 0, 0, 0, 5, 1, 0, 2000, 6, 0, {-18000, 0}}, /* 2000/01/01 2:00:00 EST */
{ 0, 0, 0, 5, 29, 1, 2000, 2, 59, {-18000, 0}}, /* 2000/02/29 2:00:00 EST */
{ 0, 0, 0, 5, 1, 2, 2001, 4, 59, {-18000, 0}}, /* 2001/3/1 2:00:00 EST */
{ 0, 0, 0, 5, 1, 2, 2005, 2, 59, {-18000, 0}}, /* 2005/3/1 2:00:00 EST */
{ 0, 0, 0, 6, 9, 8, 1999, 4, 251, {-18000, 3600}}  /* 1999/9/9 3:00:00 EDT */
};
#endif

static PRStatus TestExplodeImplodeTime(void)
{
    PRTime prt_tmp;
    PRTime now;
    int idx;
    int array_size = sizeof(prt) / sizeof(PRTime);
    PRExplodedTime et_tmp;
    char buf[1024];

    for (idx = 0; idx < array_size; idx++) {
        PR_snprintf(buf, sizeof(buf), "%lld", prt[idx]);
        if (debug_mode) printf("Time stamp %s\n", buf); 
        PR_ExplodeTime(prt[idx], PR_GMTParameters, &et_tmp);
        if (!ExplodedTimeIsEqual(&et_tmp, &gmt[idx])) {
            fprintf(stderr, "GMT not equal\n");
            exit(1);
        }
        prt_tmp = PR_ImplodeTime(&et_tmp);
        if (LL_NE(prt_tmp, prt[idx])) {
            fprintf(stderr, "PRTime not equal\n");
            exit(1);
        }
        if (debug_mode) {
            printf("GMT: ");
            PrintExplodedTime(&et_tmp);
            printf("\n");
        }

        PR_ExplodeTime(prt[idx], PR_USPacificTimeParameters, &et_tmp);
        if (!ExplodedTimeIsEqual(&et_tmp, &uspt[idx])) {
            fprintf(stderr, "US Pacific Time not equal\n");
            exit(1);
        }
        prt_tmp = PR_ImplodeTime(&et_tmp);
        if (LL_NE(prt_tmp, prt[idx])) {
            fprintf(stderr, "PRTime not equal\n");
            exit(1);
        }
        if (debug_mode) {
            printf("US Pacific Time: ");
            PrintExplodedTime(&et_tmp);
            printf("\n");
        }

        PR_ExplodeTime(prt[idx], PR_LocalTimeParameters, &et_tmp);
        if (!ExplodedTimeIsEqual(&et_tmp, &localt[idx])) {
            fprintf(stderr, "not equal\n");
            exit(1);
        }
        prt_tmp = PR_ImplodeTime(&et_tmp);
        if (LL_NE(prt_tmp, prt[idx])) {
            fprintf(stderr, "not equal\n");
            exit(1);
        }
        if (debug_mode) {
            printf("Local time:");
            PrintExplodedTime(&et_tmp);
            printf("\n\n");
        }
    }

    now = PR_Now();
    PR_ExplodeTime(now, PR_GMTParameters, &et_tmp);
    printf("Current GMT is ");
    PrintExplodedTime(&et_tmp);
    printf("\n");
    prt_tmp = PR_ImplodeTime(&et_tmp);
    if (LL_NE(prt_tmp, now)) {
        fprintf(stderr, "not equal\n");
        exit(1);
    }
    PR_ExplodeTime(now, PR_USPacificTimeParameters, &et_tmp);
    printf("Current US Pacific Time is ");
    PrintExplodedTime(&et_tmp);
    printf("\n");
    prt_tmp = PR_ImplodeTime(&et_tmp);
    if (LL_NE(prt_tmp, now)) {
        fprintf(stderr, "not equal\n");
        exit(1);
    }
    PR_ExplodeTime(now, PR_LocalTimeParameters, &et_tmp);
    printf("Current local time is ");
    PrintExplodedTime(&et_tmp);
    printf("\n");
    prt_tmp = PR_ImplodeTime(&et_tmp);
    if (LL_NE(prt_tmp, now)) {
        fprintf(stderr, "not equal\n");
        exit(1);
    }
    printf("Please verify the results\n\n");

    if (debug_mode) printf("Test 1 passed\n");
    return PR_SUCCESS;
}
/* End of Test 1: TestExplodeImplodeTime */

/*
 * Test 2: Normalize Time
 */

/*
 * time increment for addition to PRExplodeTime
 */
typedef struct time_increment {
	PRInt32 ti_usec;
	PRInt32 ti_sec;
	PRInt32 ti_min;
	PRInt32 ti_hour;
} time_increment_t;

/*
 * Data for testing PR_Normalize
 *		Add the increment to base_time, normalize it to GMT and US Pacific
 *		Time zone.
 */
typedef struct normalize_test_data {
    PRExplodedTime		base_time; 
    time_increment_t  	increment;
    PRExplodedTime		expected_gmt_time;
    PRExplodedTime		expected_uspt_time;
} normalize_test_data_t;


/*
 * Test data -	the base time values cover dates of interest including y2k - 1,
 *				y2k + 1, y2k leap year, y2k leap date + 1year,
 *				y2k leap date + 4 years
 */
normalize_test_data_t normalize_test_array[] = {
  /*usec sec min hour  mday  mo  year  wday yday {gmtoff, dstoff }*/

	/* Fri 12/31/1999 19:32:48 PST */
	{{0, 48, 32, 19, 31, 11, 1999, 5, 364, { -28800, 0}},
    {0, 0, 30, 20},
	{0, 48, 2, 0, 2, 0, 2000, 0, 1, { 0, 0}},	/*Sun Jan 2 00:02:48 UTC 2000*/
	{0, 48, 2, 16, 1, 0, 2000, 6, 0, { -28800, 0}},/* Sat Jan 1 16:02:48
														PST 2000*/
	},
	/* Fri 99-12-31 23:59:02 GMT */
	{{0, 2, 59, 23, 31, 11, 1999, 5, 364, { 0, 0}},
    {0, 0, 45, 0},
	{0, 2, 44, 0, 1, 0, 2000, 6, 0, { 0, 0}},/* Sat Jan 1 00:44:02 UTC 2000*/
	{0, 2, 44, 16, 31, 11, 1999, 5, 364, { -28800, 0}}/*Fri Dec 31 16:44:02
														PST 1999*/
	},
	/* 99-12-25 12:00:00 GMT */
	{{0, 0, 0, 12, 25, 11, 1999, 6, 358, { 0, 0}},
    {0, 0, 0, 364 * 24},
	{0, 0, 0, 12, 23, 11, 2000, 6, 357, { 0, 0}},/*Sat Dec 23 12:00:00
													2000 UTC*/
	{0, 0, 0, 4, 23, 11, 2000, 6, 357, { -28800, 0}}/*Sat Dec 23 04:00:00
														2000 -0800*/
	},
	/* 00-01-1 00:00:00 PST */
    {{0, 0, 0, 0, 1, 0, 2000, 6, 0, { -28800, 0}},
    {0, 0, 0, 48},
    {0, 0, 0, 8, 3, 0, 2000, 1, 2, { 0, 0}},/*Mon Jan  3 08:00:00 2000 UTC*/
    {0, 0, 0, 0, 3, 0, 2000, 1, 2, { -28800, 0}}/*Mon Jan  3 00:00:00 2000
																-0800*/
	},
	/* 00-01-10 12:00:00 PST */
    {{0, 0, 0, 12, 10, 0, 2000, 1, 9, { -28800, 0}},
    {0, 0, 0, 364 * 5 * 24},
    {0, 0, 0, 20, 3, 0, 2005, 1, 2, { 0, 0}},/*Mon Jan  3 20:00:00 2005 UTC */
    {0, 0, 0, 12, 3, 0, 2005, 1, 2, { -28800, 0}}/*Mon Jan  3 12:00:00
														2005 -0800*/
	},
	/* 00-02-28 15:39 GMT */
	{{0, 0, 39, 15, 28, 1, 2000, 1, 58, { 0, 0}},
    {0,  0, 0, 24},
	{0, 0, 39, 15, 29, 1, 2000, 2, 59, { 0, 0}}, /*Tue Feb 29 15:39:00 2000
														UTC*/
	{0, 0, 39, 7, 29, 1, 2000, 2, 59, { -28800, 0}}/*Tue Feb 29 07:39:00
														2000 -0800*/
	},
	/* 01-03-01 12:00 PST */
    {{0, 0, 0, 12, 3, 0, 2001, 3, 2, { -28800, 0}},/*Wed Jan 3 12:00:00
													-0800 2001*/
    {0, 30, 30,45},
    {0, 30, 30, 17, 5, 0, 2001, 5, 4, { 0, 0}}, /*Fri Jan  5 17:30:30 2001
													UTC*/
    {0, 30, 30, 9, 5, 0, 2001, 5, 4, { -28800, 0}} /*Fri Jan  5 09:30:30
														2001 -0800*/
	},
	/* 2004-04-26 12:00 GMT */
	{{0, 0, 0, 20, 3, 0, 2001, 3, 2, { 0, 0}},
    {0, 0, 30,0},
	{0, 0, 30, 20, 3, 0, 2001, 3, 2, { 0, 0}},/*Wed Jan  3 20:30:00 2001 UTC*/ 
    {0, 0, 30, 12, 3, 0, 2001, 3, 2, { -28800, 0}}/*Wed Jan  3 12:30:00
														2001 -0800*/
	},
	/* 99-09-09 00:00 GMT */
	{{0, 0, 0, 0, 9, 8, 1999, 4, 251, { 0, 0}},
    {0, 0, 0, 12},
    {0, 0, 0, 12, 9, 8, 1999, 4, 251, { 0, 0}},/*Thu Sep  9 12:00:00 1999 UTC*/
    {0, 0, 0, 5, 9, 8, 1999, 4, 251, { -28800, 3600}}/*Thu Sep  9 05:00:00
														1999 -0700*/
	}
};

void add_time_increment(PRExplodedTime *et1, time_increment_t *it)
{
	et1->tm_usec += it->ti_usec;
	et1->tm_sec	+= it->ti_sec;
	et1->tm_min += it->ti_min;
	et1->tm_hour += it->ti_hour;
}

/*
** TestNormalizeTime() -- Test PR_NormalizeTime()
**		For each data item, add the time increment to the base_time and then
**		normalize it for GMT and local time zones. This test assumes that
**		the local time zone is the Pacific Time Zone. The normalized values
**		should match the expected values in the data item.
**
*/
PRStatus TestNormalizeTime(void)
{
int idx, count;
normalize_test_data_t *itemp;
time_increment_t *itp;

	count = sizeof(normalize_test_array)/sizeof(normalize_test_array[0]);
	for (idx = 0; idx < count; idx++) {
		itemp = &normalize_test_array[idx];
		if (debug_mode) {
			printf("%2d. %15s",idx +1,"Base time: ");
			PrintExplodedTime(&itemp->base_time);
			printf("\n");
		}
		itp = &itemp->increment;
		if (debug_mode) {
			printf("%20s %2d hrs %2d min %3d sec\n","Add",itp->ti_hour,
												itp->ti_min, itp->ti_sec);
		}
		add_time_increment(&itemp->base_time, &itemp->increment);
		PR_NormalizeTime(&itemp->base_time, PR_LocalTimeParameters);
		if (debug_mode) {
			printf("%19s","PST time: ");
			PrintExplodedTime(&itemp->base_time);
			printf("\n");
		}
		if (!ExplodedTimeIsEqual(&itemp->base_time,
									&itemp->expected_uspt_time)) {
			printf("PR_NormalizeTime failed\n");
			if (debug_mode)
				PrintExplodedTime(&itemp->expected_uspt_time);
			return PR_FAILURE;
		}
		PR_NormalizeTime(&itemp->base_time, PR_GMTParameters);
		if (debug_mode) {
			printf("%19s","GMT time: ");
			PrintExplodedTime(&itemp->base_time);
			printf("\n");
		}

		if (!ExplodedTimeIsEqual(&itemp->base_time,
									&itemp->expected_gmt_time)) {
			printf("PR_NormalizeTime failed\n");
			return PR_FAILURE;
		}
	}
	return PR_SUCCESS;
}


/*
** ParseTest. Structure defining a string time and a matching exploded time
**
*/
typedef struct ParseTest
{
    char            *sDate;     /* string to be converted using PR_ParseTimeString() */
    PRExplodedTime  et;         /* expected result of the conversion */
} ParseTest;

static ParseTest parseArray[] = 
{
    /*                                  |<----- expected result ------------------------------------------->| */
    /* "string to test"                   usec     sec min hour   day  mo  year  wday julian {gmtoff, dstoff }*/
    { "Thursday 1 Jan 1970 00:00:00",   { 000000,  00, 00, 00,     1,   0, 1970, 4,     0,   {-28800, 0 }}},
    { "1 Jan 1970 00:00:00",            { 000000,  00, 00, 00,     1,   0, 1970, 4,     0,   {-28800, 0 }}},
    { "1-Jan-1970 00:00:00",            { 000000,  00, 00, 00,     1,   0, 1970, 4,     0,   {-28800, 0 }}},
    { "01-Jan-1970 00:00:00",           { 000000,  00, 00, 00,     1,   0, 1970, 4,     0,   {-28800, 0 }}},
    { "January 1, 1970",                { 000000,  00, 00, 00,     1,   0, 1970, 4,     0,   {-28800, 0 }}},
    { "January 1, 1970 00:00",          { 000000,  00, 00, 00,     1,   0, 1970, 4,     0,   {-28800, 0 }}},
    { "January 01, 1970 00:00",         { 000000,  00, 00, 00,     1,   0, 1970, 4,     0,   {-28800, 0 }}},
    { "January 01 1970 00:00",          { 000000,  00, 00, 00,     1,   0, 1970, 4,     0,   {-28800, 0 }}},
    { "January 01 1970 00:00:00",       { 000000,  00, 00, 00,     1,   0, 1970, 4,     0,   {-28800, 0 }}},
    { "01-01-1970",                     { 000000,  00, 00, 00,     1,   0, 1970, 4,     0,   {-28800, 0 }}},
    { "01/01/1970",                     { 000000,  00, 00, 00,     1,   0, 1970, 4,     0,   {-28800, 0 }}},
    { "01/01/70",                       { 000000,  00, 00, 00,     1,   0, 1970, 4,     0,   {-28800, 0 }}},
    { "01/01/70 00:00:00",              { 000000,  00, 00, 00,     1,   0, 1970, 4,     0,   {-28800, 0 }}},
    { "70/01/01 00:00:00",              { 000000,  00, 00, 00,     1,   0, 1970, 4,     0,   {-28800, 0 }}},
    { "70/1/1 00:00:",                  { 000000,  00, 00, 00,     1,   0, 1970, 4,     0,   {-28800, 0 }}},
    { "00:00 Thursday, January 1, 1970",{ 000000,  00, 00, 00,     1,   0, 1970, 4,     0,   {-28800, 0 }}},
    { "1-Jan-70 00:00:00",              { 000000,  00, 00, 00,     1,   0, 1970, 4,     0,   {-28800, 0 }}},
    { "70-01-01 00:00:00",              { 000000,  00, 00, 00,     1,   0, 1970, 4,     0,   {-28800, 0 }}},
    { "70/01/01 00:00:00",              { 000000,  00, 00, 00,     1,   0, 1970, 4,     0,   {-28800, 0 }}},

    /* 31-Dec-1969 */
    { "Wed 31 Dec 1969 00:00:00",       { 000000,  00, 00, 00,    31,  11, 1969, 3,   364,   {-28800, 0 }}},
    { "31 Dec 1969 00:00:00",           { 000000,  00, 00, 00,    31,  11, 1969, 3,   364,   {-28800, 0 }}},
    { "12/31/69    00:00:00",           { 000000,  00, 00, 00,    31,  11, 2069, 2,   364,   {-28800, 0 }}},
    { "12/31/1969  00:00:00",           { 000000,  00, 00, 00,    31,  11, 1969, 3,   364,   {-28800, 0 }}},
    { "12-31-69    00:00:00",           { 000000,  00, 00, 00,    31,  11, 2069, 2,   364,   {-28800, 0 }}},
    { "12-31-1969  00:00:00",           { 000000,  00, 00, 00,    31,  11, 1969, 3,   364,   {-28800, 0 }}},
    { "69-12-31    00:00:00",           { 000000,  00, 00, 00,    31,  11, 2069, 2,   364,   {-28800, 0 }}},
    { "69/12/31    00:00:00",           { 000000,  00, 00, 00,    31,  11, 2069, 2,   364,   {-28800, 0 }}},

    /* 31-Dec-1999 */
    { "31 Dec 1999 00:00:00",           { 000000,  00, 00, 00,    31,  11, 1999, 5,   364,   {-28800, 0 }}},
    { "12/31/99    00:00:00",           { 000000,  00, 00, 00,    31,  11, 1999, 5,   364,   {-28800, 0 }}},
    { "12/31/1999  00:00:00",           { 000000,  00, 00, 00,    31,  11, 1999, 5,   364,   {-28800, 0 }}},
    { "12-31-99    00:00:00",           { 000000,  00, 00, 00,    31,  11, 1999, 5,   364,   {-28800, 0 }}},
    { "12-31-1999  00:00:00",           { 000000,  00, 00, 00,    31,  11, 1999, 5,   364,   {-28800, 0 }}},
    { "99-12-31    00:00:00",           { 000000,  00, 00, 00,    31,  11, 1999, 5,   364,   {-28800, 0 }}},
    { "99/12/31    00:00:00",           { 000000,  00, 00, 00,    31,  11, 1999, 5,   364,   {-28800, 0 }}},

    /* 01-Jan-2000 */
    { "01 Jan 2000 00:00:00",           { 000000,  00, 00, 00,     1,   0, 2000, 6,     0,   {-28800, 0 }}},
    { "1/1/00      00:00:00",           { 000000,  00, 00, 00,     1,   0, 2000, 6,     0,   {-28800, 0 }}},
    { "1/1/2000    00:00:00",           { 000000,  00, 00, 00,     1,   0, 2000, 6,     0,   {-28800, 0 }}},
    { "1-1-00      00:00:00",           { 000000,  00, 00, 00,     1,   0, 2000, 6,     0,   {-28800, 0 }}},
    { "1-1-2000    00:00:00",           { 000000,  00, 00, 00,     1,   0, 2000, 6,     0,   {-28800, 0 }}},
    { "01-01-00    00:00:00",           { 000000,  00, 00, 00,     1,   0, 2000, 6,     0,   {-28800, 0 }}},
    { "Saturday 01-01-2000  00:00:00",  { 000000,  00, 00, 00,     1,   0, 2000, 6,     0,   {-28800, 0 }}},

    /* 29-Feb-2000 */
    { "29 Feb 2000 00:00:00",           { 000000,  00, 00, 00,    29,   1, 2000, 2,    59,   {-28800, 0 }}},
    { "2/29/00     00:00:00",           { 000000,  00, 00, 00,    29,   1, 2000, 2,    59,   {-28800, 0 }}},
    { "2/29/2000   00:00:00",           { 000000,  00, 00, 00,    29,   1, 2000, 2,    59,   {-28800, 0 }}},
    { "2-29-00     00:00:00",           { 000000,  00, 00, 00,    29,   1, 2000, 2,    59,   {-28800, 0 }}},
    { "2-29-2000   00:00:00",           { 000000,  00, 00, 00,    29,   1, 2000, 2,    59,   {-28800, 0 }}},
    { "02-29-00    00:00:00",           { 000000,  00, 00, 00,    29,   1, 2000, 2,    59,   {-28800, 0 }}},
    { "02-29-2000  00:00:00",           { 000000,  00, 00, 00,    29,   1, 2000, 2,    59,   {-28800, 0 }}},

    /* 01-Mar-2000 */
    { "01 Mar 2000 00:00:00",           { 000000,  00, 00, 00,     1,   2, 2000, 3,    60,   {-28800, 0 }}},
    { "3/1/00      00:00:00",           { 000000,  00, 00, 00,     1,   2, 2000, 3,    60,   {-28800, 0 }}},
    { "3/1/2000    00:00:00",           { 000000,  00, 00, 00,     1,   2, 2000, 3,    60,   {-28800, 0 }}},
    { "3-1-00      00:00:00",           { 000000,  00, 00, 00,     1,   2, 2000, 3,    60,   {-28800, 0 }}},
    { "03-01-00    00:00:00",           { 000000,  00, 00, 00,     1,   2, 2000, 3,    60,   {-28800, 0 }}},
    { "03-01-2000  00:00:00",           { 000000,  00, 00, 00,     1,   2, 2000, 3,    60,   {-28800, 0 }}},

    /* 01-Mar-2001 */
    { "01 Mar 2001 00:00:00",           { 000000,  00, 00, 00,     1,   2, 2001, 4,    59,   {-28800, 0 }}},
    { "3/1/01      00:00:00",           { 000000,  00, 00, 00,     1,   2, 2001, 4,    59,   {-28800, 0 }}},
    { "3/1/2001    00:00:00",           { 000000,  00, 00, 00,     1,   2, 2001, 4,    59,   {-28800, 0 }}},
    { "3-1-01      00:00:00",           { 000000,  00, 00, 00,     1,   2, 2001, 4,    59,   {-28800, 0 }}},
    { "3-1-2001    00:00:00",           { 000000,  00, 00, 00,     1,   2, 2001, 4,    59,   {-28800, 0 }}},
    { "03-01-01    00:00:00",           { 000000,  00, 00, 00,     1,   2, 2001, 4,    59,   {-28800, 0 }}},
    { "03-01-2001  00:00:00",           { 000000,  00, 00, 00,     1,   2, 2001, 4,    59,   {-28800, 0 }}},

    /* 29-Feb-2004 */
    { "29 Feb 2004 00:00:00",           { 000000,  00, 00, 00,    29,   1, 2004, 0,    59,   {-28800, 0 }}},
    { "2/29/04     00:00:00",           { 000000,  00, 00, 00,    29,   1, 2004, 0,    59,   {-28800, 0 }}},
    { "2/29/2004   00:00:00",           { 000000,  00, 00, 00,    29,   1, 2004, 0,    59,   {-28800, 0 }}},
    { "2-29-04     00:00:00",           { 000000,  00, 00, 00,    29,   1, 2004, 0,    59,   {-28800, 0 }}},
    { "2-29-2004   00:00:00",           { 000000,  00, 00, 00,    29,   1, 2004, 0,    59,   {-28800, 0 }}},

    /* 01-Mar-2004 */
    { "01 Mar 2004 00:00:00",           { 000000,  00, 00, 00,     1,   2, 2004, 1,    60,   {-28800, 0 }}},
    { "3/1/04      00:00:00",           { 000000,  00, 00, 00,     1,   2, 2004, 1,    60,   {-28800, 0 }}},
    { "3/1/2004    00:00:00",           { 000000,  00, 00, 00,     1,   2, 2004, 1,    60,   {-28800, 0 }}},
    { "3-1-04      00:00:00",           { 000000,  00, 00, 00,     1,   2, 2004, 1,    60,   {-28800, 0 }}},
    { "3-1-2004    00:00:00",           { 000000,  00, 00, 00,     1,   2, 2004, 1,    60,   {-28800, 0 }}},
    { "03-01-04    00:00:00",           { 000000,  00, 00, 00,     1,   2, 2004, 1,    60,   {-28800, 0 }}},
    { "03-01-2004  00:00:00",           { 000000,  00, 00, 00,     1,   2, 2004, 1,    60,   {-28800, 0 }}},

    /* 01-Mar-2005 */
    { "01 Mar 2005 00:00:00",           { 000000,  00, 00, 00,     1,   2, 2005, 2,    59,   {-28800, 0 }}},
    { "3/1/05      00:00:00",           { 000000,  00, 00, 00,     1,   2, 2005, 2,    59,   {-28800, 0 }}},
    { "3/1/2005    00:00:00",           { 000000,  00, 00, 00,     1,   2, 2005, 2,    59,   {-28800, 0 }}},
    { "3-1-05      00:00:00",           { 000000,  00, 00, 00,     1,   2, 2005, 2,    59,   {-28800, 0 }}},
    { "3-1-2005    00:00:00",           { 000000,  00, 00, 00,     1,   2, 2005, 2,    59,   {-28800, 0 }}},
    { "03-01-05    00:00:00",           { 000000,  00, 00, 00,     1,   2, 2005, 2,    59,   {-28800, 0 }}},
    { "03-01-2005  00:00:00",           { 000000,  00, 00, 00,     1,   2, 2005, 2,    59,   {-28800, 0 }}},

    /* 09-Sep-1999. Interesting because of its use as an eof marker? */
    { "09 Sep 1999 00:00:00",           { 000000,  00, 00, 00,     9,   8, 1999, 4,   251,   {-28800, 3600 }}},
    { "9/9/99      00:00:00",           { 000000,  00, 00, 00,     9,   8, 1999, 4,   251,   {-28800, 3600 }}},
    { "9/9/1999    00:00:00",           { 000000,  00, 00, 00,     9,   8, 1999, 4,   251,   {-28800, 3600 }}},
    { "9-9-99      00:00:00",           { 000000,  00, 00, 00,     9,   8, 1999, 4,   251,   {-28800, 3600 }}},
    { "9-9-1999    00:00:00",           { 000000,  00, 00, 00,     9,   8, 1999, 4,   251,   {-28800, 3600 }}},
    { "09-09-99    00:00:00",           { 000000,  00, 00, 00,     9,   8, 1999, 4,   251,   {-28800, 3600 }}},
    { "09-09-1999  00:00:00",           { 000000,  00, 00, 00,     9,   8, 1999, 4,   251,   {-28800, 3600 }}},
    { "99-09-09    00:00:00",           { 000000,  00, 00, 00,     9,   8, 1999, 4,   251,   {-28800, 3600 }}},
    
    /* last element. string must be null */
    { NULL }        
}; /* end array of ParseTest */

/*
** TestParseTime() -- Test PR_ParseTimeString() for y2k compliance
**
** TestParseTime() loops thru the array parseArray. For each element in
** the array, he calls PR_ParseTimeString() with sDate as the conversion
** argument. The result (ct) is then converted to a PRExplodedTime structure
** and compared with the exploded time value (parseArray[n].et) in the
** array element; if equal, the element passes the test.
**
** The array parseArray[] contains entries that are interesting to the
** y2k problem.
**
**
*/
static PRStatus TestParseTime( void )
{
    ParseTest       *ptp = parseArray;
    PRTime          ct;
    PRExplodedTime  cet;
    char            *sp = ptp->sDate;
    PRStatus        rc;
    PRStatus        rv = PR_SUCCESS;

    while ( sp != NULL)
    {
        rc = PR_ParseTimeString( sp, PR_FALSE, &ct );
        if ( PR_FAILURE == rc )
        {
            printf("TestParseTime(): PR_ParseTimeString() failed to convert: %s\n", sp );
            rv = PR_FAILURE;
            failed_already = 1;
        }
        else
        {
            PR_ExplodeTime( ct, PR_LocalTimeParameters , &cet );

            if ( !ExplodedTimeIsEqual( &cet, &ptp->et ))
            {
                printf("TestParseTime(): Exploded time compare failed: %s\n", sp );
                if ( debug_mode )
                {
                    PrintExplodedTime( &cet );
                    printf("\n");
                    PrintExplodedTime( &ptp->et );
                    printf("\n");
                }
                
                rv = PR_FAILURE;
                failed_already = 1;
            }
        }
                
        /* point to next element in array, keep going */
        ptp++;
        sp = ptp->sDate;
    } /* end while() */

    return( rv );
} /* end TestParseTime() */

int main(int argc, char** argv)
{
	/* The command line argument: -d is used to determine if the test is being run
	in debug mode. The regress tool requires only one line output:PASS or FAIL.
	All of the printfs associated with this test has been handled with a if (debug_mode)
	test.
	Usage: test_name -d
	*/
	PLOptStatus os;
	PLOptState *opt;
    
    PR_STDIO_INIT();
	opt = PL_CreateOptState(argc, argv, "d");
	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
		if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'd':  /* debug mode */
			debug_mode = PR_TRUE;
            break;
         default:
            break;
        }
    }
	PL_DestroyOptState(opt);

 /* main test */
	
    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    lm = PR_NewLogModule("test");

#ifdef XP_MAC
	/* Set up the console */
	InitializeSIOUX(true);
	debug_mode = PR_TRUE;
#endif

    if ( PR_FAILURE == TestExplodeImplodeTime())
    {
        PR_LOG( lm, PR_LOG_ERROR,
            ("TestExplodeImplodeTime() failed"));
    }
    else
    	printf("Test 1: Calendar Time Test passed\n");

    if ( PR_FAILURE == TestNormalizeTime())
    {
        PR_LOG( lm, PR_LOG_ERROR,
            ("TestNormalizeTime() failed"));
    }
    else
    	printf("Test 2: Normalize Time Test passed\n");

    if ( PR_FAILURE == TestParseTime())
    {
        PR_LOG( lm, PR_LOG_ERROR,
            ("TestParseTime() failed"));
    }
    else
    	printf("Test 3: Parse Time Test passed\n");

#ifdef XP_MAC
	if (1)
	{
		char dummyChar;
		
		printf("Press return to exit\n\n");
		scanf("%c", &dummyChar);
	}
#endif

	if (failed_already) 
	    return 1;
	else 
	    return 0;
} /* end main() y2k.c */

