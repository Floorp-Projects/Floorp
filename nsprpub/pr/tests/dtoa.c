/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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

/******************************************************************************
 *
 * This file contains a test program for the function conversion functions
 * for double precision code:
 * PR_strtod
 * PR_dtoa
 * PR_cnvtf
 *
 * This file was ns/nspr/tests/dtoa.c, created by rrj on 1996/06/22.
 *
 *****************************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <locale.h>
#include "prprf.h"
#include "prdtoa.h"

static int failed_already = 0;

int main( int argc, char* argv[] )
{
    double num;
    double num1;
    double zero = 0.0;
    char   cnvt[50];
    
    num = 1e24;
    num1 = PR_strtod("1e24",NULL);
    if(num1 != num){
	fprintf(stderr,"Failed to convert numeric value %s\n","1e24");
        failed_already = 1;
    }

    PR_cnvtf(cnvt,sizeof(cnvt),20,num);
    if(strcmp("1e+24",cnvt) != 0){
	fprintf(stderr,"Failed to convert numeric value %lf %s\n",num,cnvt);
        failed_already = 1;
    }

    num = 0.001e7;
    num1 = PR_strtod("0.001e7",NULL);
    if(num1 != num){
	fprintf(stderr,"Failed to convert numeric value %s\n","0.001e7");
        failed_already = 1;
    }
    PR_cnvtf(cnvt,sizeof(cnvt),20,num);
    if(strcmp("10000",cnvt) != 0){
	fprintf(stderr,"Failed to convert numeric value %lf %s\n",num,cnvt);
        failed_already = 1;
    }

    num = 0.0000000000000753;
    num1 = PR_strtod("0.0000000000000753",NULL);
    if(num1 != num){
	fprintf(stderr,"Failed to convert numeric value %s\n",
		"0.0000000000000753");
        failed_already = 1;
    }
    PR_cnvtf(cnvt,sizeof(cnvt),20,num);
    if(strcmp("7.53e-14",cnvt) != 0){
	fprintf(stderr,"Failed to convert numeric value %lf %s\n",num,cnvt);
        failed_already = 1;
    }

    num = 1.867e73;
    num1 = PR_strtod("1.867e73",NULL);
    if(num1 != num){
	fprintf(stderr,"Failed to convert numeric value %s\n","1.867e73");
        failed_already = 1;
    }
    PR_cnvtf(cnvt,sizeof(cnvt),20,num);
    if(strcmp("1.867e+73",cnvt) != 0){
	fprintf(stderr,"Failed to convert numeric value %lf %s\n",num,cnvt);
        failed_already = 1;
    }


    num = -1.867e73;
    num1 = PR_strtod("-1.867e73",NULL);
    if(num1 != num){
	fprintf(stderr,"Failed to convert numeric value %s\n","-1.867e73");
        failed_already = 1;
    }
    PR_cnvtf(cnvt,sizeof(cnvt),20,num);
    if(strcmp("-1.867e+73",cnvt) != 0){
	fprintf(stderr,"Failed to convert numeric value %lf %s\n",num,cnvt);
        failed_already = 1;
    }

    num = -1.867e-73;
    num1 = PR_strtod("-1.867e-73",NULL);
    if(num1 != num){
	fprintf(stderr,"Failed to convert numeric value %s\n","-1.867e-73");
        failed_already = 1;
    }

    PR_cnvtf(cnvt,sizeof(cnvt),20,num);
    if(strcmp("-1.867e-73",cnvt) != 0){
	fprintf(stderr,"Failed to convert numeric value %lf %s\n",num,cnvt);
        failed_already = 1;
    }

    /* Testing for infinity */
    num = 1.0 / zero;
    num1 = PR_strtod("1.867e765",NULL);
    if(num1 != num){
	fprintf(stderr,"Failed to convert numeric value %s\n","1.867e765");
        failed_already = 1;
    }

    PR_cnvtf(cnvt,sizeof(cnvt),20,num);
    if(strcmp("Infinity",cnvt) != 0){
	fprintf(stderr,"Failed to convert numeric value %lf %s\n",num,cnvt);
        failed_already = 1;
    }

    num = -1.0 / zero;
    num1 = PR_strtod("-1.867e765",NULL);
    if(num1 != num){
	fprintf(stderr,"Failed to convert numeric value %s\n","-1.867e765");
        failed_already = 1;
    }

    PR_cnvtf(cnvt,sizeof(cnvt),20,num);
    if(strcmp("-Infinity",cnvt) != 0){
	fprintf(stderr,"Failed to convert numeric value %lf %s\n",num,cnvt);
        failed_already = 1;
    }

    /* Testing for NaN. PR_strtod can't parse "NaN" and "Infinity" */
    num = zero / zero;

    PR_cnvtf(cnvt,sizeof(cnvt),20,num);
    if(strcmp("NaN",cnvt) != 0){
	fprintf(stderr,"Failed to convert numeric value %lf %s\n",num,cnvt);
        failed_already = 1;
    }

    num = - zero / zero;
    PR_cnvtf(cnvt,sizeof(cnvt),20,num);
    if(strcmp("NaN",cnvt) != 0){
	fprintf(stderr,"Failed to convert numeric value %lf %s\n",num,cnvt);
        failed_already = 1;
    }

    num = 1.0000000001e21;
    num1 = PR_strtod("1.0000000001e21",NULL);
    if(num1 != num){
	fprintf(stderr,"Failed to convert numeric value %s\n",
		"1.0000000001e21");
        failed_already = 1;
    }

    PR_cnvtf(cnvt,sizeof(cnvt),20,num);
    if(strcmp("1.0000000001e+21",cnvt) != 0){
	fprintf(stderr,"Failed to convert numeric value %lf %s\n",num,cnvt);
        failed_already = 1;
    }


    num = -1.0000000001e-21;
    num1 = PR_strtod("-1.0000000001e-21",NULL);
    if(num1 != num){
	fprintf(stderr,"Failed to convert numeric value %s\n",
		"-1.0000000001e-21");
        failed_already = 1;
    }
    PR_cnvtf(cnvt,sizeof(cnvt),20,num);
    if(strcmp("-1.0000000001e-21",cnvt) != 0){
	fprintf(stderr,"Failed to convert numeric value %lf %s\n",num,cnvt);
        failed_already = 1;
    }
    if (failed_already) {
        printf("FAILED\n");
    } else {
        printf("PASSED\n");
    }
    return failed_already;
}
