/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
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
