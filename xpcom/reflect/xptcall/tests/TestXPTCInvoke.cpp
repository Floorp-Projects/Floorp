/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Chris Seawood <cls@seawood.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Invoke tests xptcall. */

#include <stdio.h>
#include "xptcall.h"
#include "prlong.h"

// forward declration
static void DoMultipleInheritenceTest();
static void DoMultipleInheritenceTest2();

// {AAC1FB90-E099-11d2-984E-006008962422}
#define INVOKETESTTARGET_IID \
{ 0xaac1fb90, 0xe099, 0x11d2, \
  { 0x98, 0x4e, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22 } }


class InvokeTestTargetInterface : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(INVOKETESTTARGET_IID)
    NS_IMETHOD AddTwoInts(PRInt32 p1, PRInt32 p2, PRInt32* retval) = 0;
    NS_IMETHOD MultTwoInts(PRInt32 p1, PRInt32 p2, PRInt32* retval) = 0;
    NS_IMETHOD AddTwoLLs(PRInt64 p1, PRInt64 p2, PRInt64* retval) = 0;
    NS_IMETHOD MultTwoLLs(PRInt64 p1, PRInt64 p2, PRInt64* retval) = 0;

    NS_IMETHOD AddManyInts(PRInt32 p1, PRInt32 p2, PRInt32 p3, PRInt32 p4,
                           PRInt32 p5, PRInt32 p6, PRInt32 p7, PRInt32 p8,
                           PRInt32 p9, PRInt32 p10, PRInt32* retval) = 0;

    NS_IMETHOD AddTwoFloats(float p1, float p2, float* retval) = 0;

    NS_IMETHOD AddManyDoubles(double p1, double p2, double p3, double p4,
                              double p5, double p6, double p7, double p8,
                              double p9, double p10, double* retval) = 0;

    NS_IMETHOD AddManyFloats(float p1, float p2, float p3, float p4,
                             float p5, float p6, float p7, float p8,
                             float p9, float p10, float* retval) = 0;

    NS_IMETHOD AddManyManyFloats(float p1, float p2, float p3, float p4,
                                 float p5, float p6, float p7, float p8,
                                 float p9, float p10, float p11, float p12, 
                                 float p13, float p14, float p15, float p16, 
                                 float p17, float p18, float p19, float p20, 
                                 float *retval) = 0;

    NS_IMETHOD PassTwoStrings(const char* s1, const char* s2, char** retval) = 0;

};

class InvokeTestTarget : public InvokeTestTargetInterface
{
public:
    NS_DECL_ISUPPORTS
    NS_IMETHOD AddTwoInts(PRInt32 p1, PRInt32 p2, PRInt32* retval);
    NS_IMETHOD MultTwoInts(PRInt32 p1, PRInt32 p2, PRInt32* retval);
    NS_IMETHOD AddTwoLLs(PRInt64 p1, PRInt64 p2, PRInt64* retval);
    NS_IMETHOD MultTwoLLs(PRInt64 p1, PRInt64 p2, PRInt64* retval);

    NS_IMETHOD AddManyInts(PRInt32 p1, PRInt32 p2, PRInt32 p3, PRInt32 p4,
                           PRInt32 p5, PRInt32 p6, PRInt32 p7, PRInt32 p8,
                           PRInt32 p9, PRInt32 p10, PRInt32* retval);

    NS_IMETHOD AddTwoFloats(float p1, float p2, float* retval);

    NS_IMETHOD AddManyDoubles(double p1, double p2, double p3, double p4,
                              double p5, double p6, double p7, double p8,
                              double p9, double p10, double* retval);

    NS_IMETHOD AddManyFloats(float p1, float p2, float p3, float p4,
                             float p5, float p6, float p7, float p8,
                             float p9, float p10, float* retval);

    NS_IMETHOD AddManyManyFloats(float p1, float p2, float p3, float p4,
                                 float p5, float p6, float p7, float p8,
                                 float p9, float p10, float p11, float p12, 
                                 float p13, float p14, float p15, float p16, 
                                 float p17, float p18, float p19, float p20, 
                                 float *retval);

    NS_IMETHOD PassTwoStrings(const char* s1, const char* s2, char** retval);

    InvokeTestTarget();
};

NS_IMPL_ISUPPORTS1(InvokeTestTarget, InvokeTestTargetInterface)

InvokeTestTarget::InvokeTestTarget()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

NS_IMETHODIMP
InvokeTestTarget::AddTwoInts(PRInt32 p1, PRInt32 p2, PRInt32* retval)
{
    *retval = p1 + p2;
    return NS_OK;
}

NS_IMETHODIMP
InvokeTestTarget::MultTwoInts(PRInt32 p1, PRInt32 p2, PRInt32* retval)
{
    *retval = p1 * p2;
    return NS_OK;
}

NS_IMETHODIMP
InvokeTestTarget::AddTwoLLs(PRInt64 p1, PRInt64 p2, PRInt64* retval)
{
    LL_ADD(*retval, p1, p2);
    return NS_OK;
}

NS_IMETHODIMP
InvokeTestTarget::MultTwoLLs(PRInt64 p1, PRInt64 p2, PRInt64* retval)
{
    LL_MUL(*retval, p1, p2);
    return NS_OK;
}

NS_IMETHODIMP
InvokeTestTarget::AddManyInts(PRInt32 p1, PRInt32 p2, PRInt32 p3, PRInt32 p4,
                              PRInt32 p5, PRInt32 p6, PRInt32 p7, PRInt32 p8,
                              PRInt32 p9, PRInt32 p10, PRInt32* retval)
{
#ifdef DEBUG_TESTINVOKE
    printf("%d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n", 
           p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
#endif
    *retval = p1 + p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9 + p10;
    return NS_OK;
}

NS_IMETHODIMP
InvokeTestTarget::AddTwoFloats(float p1, float p2, float *retval)
{
#ifdef DEBUG_TESTINVOKE
    printf("%f, %f\n", p1, p2);
#endif
    *retval = p1 + p2;
    return NS_OK;
}

NS_IMETHODIMP
InvokeTestTarget::AddManyDoubles(double p1, double p2, double p3, double p4,
                                 double p5, double p6, double p7, double p8,
                                 double p9, double p10, double* retval)
{
#ifdef DEBUG_TESTINVOKE
    printf("%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf\n", 
           p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
#endif
    *retval = p1 + p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9 + p10;
    return NS_OK;
}

NS_IMETHODIMP
InvokeTestTarget::AddManyFloats(float p1, float p2, float p3, float p4,
                                float p5, float p6, float p7, float p8,
                                float p9, float p10, float* retval)
{
#ifdef DEBUG_TESTINVOKE
    printf("%f, %f, %f, %f, %f, %f, %f, %f, %f, %f\n", 
           p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
#endif
    *retval = p1 + p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9 + p10;
    return NS_OK;
}

NS_IMETHODIMP
InvokeTestTarget::AddManyManyFloats(float p1, float p2, float p3, float p4,
                                    float p5, float p6, float p7, float p8,
                                    float p9, float p10, float p11, float p12, 
                                    float p13, float p14, float p15, float p16, 
                                    float p17, float p18, float p19, float p20,
                                    float *retval)
{
#ifdef DEBUG_TESTINVOKE
    printf("%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, "
           "%f, %f, %f, %f, %f, %f, %f, %f, %f, %f\n", 
           p1, p2, p3, p4, p5, p6, p7, p8, p9, p10,
           p11, p12, p13, p14, p15, p16, p17, p18, p19, p20);
#endif
    *retval = p1 + p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9 + p10 +
        p11 + p12 + p13 + p14 + p15 + p16 + p17 + p18 + p19 + p20;
    return NS_OK;
}

NS_IMETHODIMP
InvokeTestTarget::PassTwoStrings(const char* s1, const char* s2, char** retval)
{
    char *ret = "milk";
    printf("\t%s %s", s1, s2);
    *retval = ret;
    return NS_OK;
}

int main()
{
    InvokeTestTarget *test = new InvokeTestTarget();

    /* here we make the global 'check for alloc failure' checker happy */
    if(!test)
        return 1;

    PRInt32 out, tmp32 = 0;
    PRInt64 out64;
    printf("calling direct:\n");
    if(NS_SUCCEEDED(test->AddTwoInts(1,1,&out)))
        printf("\t1 + 1 = %d\n", out);
    else
        printf("\tFAILED");
    PRInt64 one, two;
    LL_I2L(one, 1);
    LL_I2L(two, 2);
    if(NS_SUCCEEDED(test->AddTwoLLs(one,one,&out64)))
    {
        LL_L2I(tmp32, out64);
        printf("\t1L + 1L = %d\n", (int)tmp32);
    }
    else
        printf("\tFAILED");
    if(NS_SUCCEEDED(test->MultTwoInts(2,2,&out)))
        printf("\t2 * 2 = %d\n", out);
    else
        printf("\tFAILED");
    if(NS_SUCCEEDED(test->MultTwoLLs(two,two,&out64)))
    {
        LL_L2I(tmp32, out64);
        printf("\t2L * 2L = %d\n", (int)tmp32);
    }
    else
        printf("\tFAILED");

    double outD;
    float outF;
    PRInt32 outI;
    char *outS;

    if(NS_SUCCEEDED(test->AddManyInts(1,2,3,4,5,6,7,8,9,10,&outI)))
        printf("\t1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 = %d\n", outI);
    else
        printf("\tFAILED");

    if(NS_SUCCEEDED(test->AddTwoFloats(1,2,&outF)))
        printf("\t1 + 2 = %ff\n", (double)outF);
    else
        printf("\tFAILED");

    if(NS_SUCCEEDED(test->AddManyDoubles(1,2,3,4,5,6,7,8,9,10,&outD)))
        printf("\t1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 = %f\n", outD);
    else
        printf("\tFAILED");

    if(NS_SUCCEEDED(test->AddManyFloats(1,2,3,4,5,6,7,8,9,10,&outF)))
        printf("\t1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 = %ff\n", (double)outF);
    else
        printf("\tFAILED");

    if(NS_SUCCEEDED(test->AddManyManyFloats(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,&outF)))
        printf("\t1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 + 11 + 12 + 13 + 14 +1 15 + 16 + 17 + 18 + 19 + 20 = %ff\n", (double)outF);
    else
        printf("\tFAILED");

    if(NS_SUCCEEDED(test->PassTwoStrings("moo","cow",&outS)))
        printf(" = %s\n", outS);
    else
        printf("\tFAILED");



    printf("calling via invoke:\n");

    nsXPTCVariant var[21];

    var[0].val.i32 = 1;
    var[0].type = nsXPTType::T_I32;
    var[0].flags = 0;

    var[1].val.i32 = 1;
    var[1].type = nsXPTType::T_I32;
    var[1].flags = 0;

    var[2].val.i32 = 0;
    var[2].type = nsXPTType::T_I32;
    var[2].flags = nsXPTCVariant::PTR_IS_DATA;
    var[2].ptr = &var[2].val.i32;

    if(NS_SUCCEEDED(XPTC_InvokeByIndex(test, 3, 3, var)))
        printf("\t1 + 1 = %d\n", var[2].val.i32);
    else
        printf("\tFAILED");

    LL_I2L(var[0].val.i64, 1);
    var[0].type = nsXPTType::T_I64;
    var[0].flags = 0;

    LL_I2L(var[1].val.i64, 1);
    var[1].type = nsXPTType::T_I64;
    var[1].flags = 0;

    LL_I2L(var[2].val.i64, 0);
    var[2].type = nsXPTType::T_I64;
    var[2].flags = nsXPTCVariant::PTR_IS_DATA;
    var[2].ptr = &var[2].val.i64;

    if(NS_SUCCEEDED(XPTC_InvokeByIndex(test, 5, 3, var)))
        printf("\t1L + 1L = %d\n", (int)var[2].val.i64);
    else
        printf("\tFAILED");

    var[0].val.i32 = 2;
    var[0].type = nsXPTType::T_I32;
    var[0].flags = 0;

    var[1].val.i32 = 2;
    var[1].type = nsXPTType::T_I32;
    var[1].flags = 0;

    var[2].val.i32 = 0;
    var[2].type = nsXPTType::T_I32;
    var[2].flags = nsXPTCVariant::PTR_IS_DATA;
    var[2].ptr = &var[2].val.i32;

    if(NS_SUCCEEDED(XPTC_InvokeByIndex(test, 4, 3, var)))
        printf("\t2 * 2 = %d\n", var[2].val.i32);
    else
        printf("\tFAILED");

    LL_I2L(var[0].val.i64,2);
    var[0].type = nsXPTType::T_I64;
    var[0].flags = 0;

    LL_I2L(var[1].val.i64,2);
    var[1].type = nsXPTType::T_I64;
    var[1].flags = 0;

    LL_I2L(var[2].val.i64,0);
    var[2].type = nsXPTType::T_I64;
    var[2].flags = nsXPTCVariant::PTR_IS_DATA;
    var[2].ptr = &var[2].val.i64;

     if(NS_SUCCEEDED(XPTC_InvokeByIndex(test, 6, 3, var)))
         printf("\t2L * 2L = %d\n", (int)var[2].val.i64);
    else
        printf("\tFAILED");

    var[0].val.i32 = 1;
    var[0].type = nsXPTType::T_I32;
    var[0].flags = 0;

    var[1].val.i32 = 2;
    var[1].type = nsXPTType::T_I32;
    var[1].flags = 0;

    var[2].val.i32 = 3;
    var[2].type = nsXPTType::T_I32;
    var[2].flags = 0;

    var[3].val.i32 = 4;
    var[3].type = nsXPTType::T_I32;
    var[3].flags = 0;

    var[4].val.i32 = 5;
    var[4].type = nsXPTType::T_I32;
    var[4].flags = 0;

    var[5].val.i32 = 6;
    var[5].type = nsXPTType::T_I32;
    var[5].flags = 0;

    var[6].val.i32 = 7;
    var[6].type = nsXPTType::T_I32;
    var[6].flags = 0;

    var[7].val.i32 = 8;
    var[7].type = nsXPTType::T_I32;
    var[7].flags = 0;

    var[8].val.i32 = 9;
    var[8].type = nsXPTType::T_I32;
    var[8].flags = 0;

    var[9].val.i32 = 10;
    var[9].type = nsXPTType::T_I32;
    var[9].flags = 0;

    var[10].val.i32 = 0;
    var[10].type = nsXPTType::T_I32;
    var[10].flags = nsXPTCVariant::PTR_IS_DATA;
    var[10].ptr = &var[10].val.i32;

    if(NS_SUCCEEDED(XPTC_InvokeByIndex(test, 7, 11, var)))
        printf("\t1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 = %d\n",
                var[10].val.i32);

    var[0].val.f = 1.0f;
    var[0].type = nsXPTType::T_FLOAT;
    var[0].flags = 0;

    var[1].val.f = 2.0f;
    var[1].type = nsXPTType::T_FLOAT;
    var[1].flags = 0;

    var[2].val.f = 0.0f;
    var[2].type = nsXPTType::T_FLOAT;
    var[2].flags = nsXPTCVariant::PTR_IS_DATA;
    var[2].ptr = &var[2].val.f;

    if(NS_SUCCEEDED(XPTC_InvokeByIndex(test, 8, 3, var)))
        printf("\t1 + 2 = %ff\n",
                (double) var[2].val.f);


    var[0].val.d = 1.0;
    var[0].type = nsXPTType::T_DOUBLE;
    var[0].flags = 0;

    var[1].val.d = 2.0;
    var[1].type = nsXPTType::T_DOUBLE;
    var[1].flags = 0;

    var[2].val.d = 3.0;
    var[2].type = nsXPTType::T_DOUBLE;
    var[2].flags = 0;

    var[3].val.d = 4.0;
    var[3].type = nsXPTType::T_DOUBLE;
    var[3].flags = 0;

    var[4].val.d = 5.0;
    var[4].type = nsXPTType::T_DOUBLE;
    var[4].flags = 0;

    var[5].val.d = 6.0;
    var[5].type = nsXPTType::T_DOUBLE;
    var[5].flags = 0;

    var[6].val.d = 7.0;
    var[6].type = nsXPTType::T_DOUBLE;
    var[6].flags = 0;

    var[7].val.d = 8.0;
    var[7].type = nsXPTType::T_DOUBLE;
    var[7].flags = 0;

    var[8].val.d = 9.0;
    var[8].type = nsXPTType::T_DOUBLE;
    var[8].flags = 0;

    var[9].val.d = 10.0;
    var[9].type = nsXPTType::T_DOUBLE;
    var[9].flags = 0;

    var[10].val.d = 0.0;
    var[10].type = nsXPTType::T_DOUBLE;
    var[10].flags = nsXPTCVariant::PTR_IS_DATA;
    var[10].ptr = &var[10].val.d;

    if(NS_SUCCEEDED(XPTC_InvokeByIndex(test, 9, 11, var)))
        printf("\t1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 = %f\n",
                var[10].val.d);
    else
        printf("\tFAILED");

    var[0].val.f = 1.0f;
    var[0].type = nsXPTType::T_FLOAT;
    var[0].flags = 0;

    var[1].val.f = 2.0f;
    var[1].type = nsXPTType::T_FLOAT;
    var[1].flags = 0;

    var[2].val.f = 3.0f;
    var[2].type = nsXPTType::T_FLOAT;
    var[2].flags = 0;

    var[3].val.f = 4.0f;
    var[3].type = nsXPTType::T_FLOAT;
    var[3].flags = 0;

    var[4].val.f = 5.0f;
    var[4].type = nsXPTType::T_FLOAT;
    var[4].flags = 0;

    var[5].val.f = 6.0f;
    var[5].type = nsXPTType::T_FLOAT;
    var[5].flags = 0;

    var[6].val.f = 7.0f;
    var[6].type = nsXPTType::T_FLOAT;
    var[6].flags = 0;

    var[7].val.f = 8.0f;
    var[7].type = nsXPTType::T_FLOAT;
    var[7].flags = 0;

    var[8].val.f = 9.0f;
    var[8].type = nsXPTType::T_FLOAT;
    var[8].flags = 0;

    var[9].val.f = 10.0f;
    var[9].type = nsXPTType::T_FLOAT;
    var[9].flags = 0;

    var[10].val.f = 0.0f;
    var[10].type = nsXPTType::T_FLOAT;
    var[10].flags = nsXPTCVariant::PTR_IS_DATA;
    var[10].ptr = &var[10].val.f;

    if(NS_SUCCEEDED(XPTC_InvokeByIndex(test, 10, 11, var)))
        printf("\t1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 = %ff\n",
                (double) var[10].val.f);

    var[0].val.f = 1.0f;
    var[0].type = nsXPTType::T_FLOAT;
    var[0].flags = 0;

    var[1].val.f = 2.0f;
    var[1].type = nsXPTType::T_FLOAT;
    var[1].flags = 0;

    var[2].val.f = 3.0f;
    var[2].type = nsXPTType::T_FLOAT;
    var[2].flags = 0;

    var[3].val.f = 4.0f;
    var[3].type = nsXPTType::T_FLOAT;
    var[3].flags = 0;

    var[4].val.f = 5.0f;
    var[4].type = nsXPTType::T_FLOAT;
    var[4].flags = 0;

    var[5].val.f = 6.0f;
    var[5].type = nsXPTType::T_FLOAT;
    var[5].flags = 0;

    var[6].val.f = 7.0f;
    var[6].type = nsXPTType::T_FLOAT;
    var[6].flags = 0;

    var[7].val.f = 8.0f;
    var[7].type = nsXPTType::T_FLOAT;
    var[7].flags = 0;

    var[8].val.f = 9.0f;
    var[8].type = nsXPTType::T_FLOAT;
    var[8].flags = 0;

    var[9].val.f = 10.0f;
    var[9].type = nsXPTType::T_FLOAT;
    var[9].flags = 0;

    var[10].val.f = 11.0f;
    var[10].type = nsXPTType::T_FLOAT;
    var[10].flags = 0;

    var[11].val.f = 12.0f;
    var[11].type = nsXPTType::T_FLOAT;
    var[11].flags = 0;

    var[12].val.f = 13.0f;
    var[12].type = nsXPTType::T_FLOAT;
    var[12].flags = 0;

    var[13].val.f = 14.0f;
    var[13].type = nsXPTType::T_FLOAT;
    var[13].flags = 0;

    var[14].val.f = 15.0f;
    var[14].type = nsXPTType::T_FLOAT;
    var[14].flags = 0;

    var[15].val.f = 16.0f;
    var[15].type = nsXPTType::T_FLOAT;
    var[15].flags = 0;

    var[16].val.f = 17.0f;
    var[16].type = nsXPTType::T_FLOAT;
    var[16].flags = 0;

    var[17].val.f = 18.0f;
    var[17].type = nsXPTType::T_FLOAT;
    var[17].flags = 0;

    var[18].val.f = 19.0f;
    var[18].type = nsXPTType::T_FLOAT;
    var[18].flags = 0;

    var[19].val.f = 20.0f;
    var[19].type = nsXPTType::T_FLOAT;
    var[19].flags = 0;

    var[20].val.f = 0.0f;
    var[20].type = nsXPTType::T_FLOAT;
    var[20].flags = nsXPTCVariant::PTR_IS_DATA;
    var[20].ptr = &var[20].val.f;

    if(NS_SUCCEEDED(XPTC_InvokeByIndex(test, 11, 21, var)))
        printf("\t1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 + 11 + 12 + 13 + 14 + 15 + 16 + 17 + 18 + 19 + 20 = %ff\n",
                (double) var[20].val.f);

    var[0].val.p = (void*)"moo";
    var[0].type = nsXPTType::T_CHAR_STR;
    var[0].flags = 0;

    var[1].val.p = (void*)"cow";
    var[1].type = nsXPTType::T_CHAR_STR;
    var[1].flags = 0;
    
    var[2].val.p = 0;
    var[2].type = nsXPTType::T_CHAR_STR;
    var[2].flags = nsXPTCVariant::PTR_IS_DATA;
    var[2].ptr = &var[2].val.p;
    
    if(NS_SUCCEEDED(XPTC_InvokeByIndex(test, 12, 3, var)))
        printf(" = %s\n", var[2].val.p);
    else
        printf("\tFAILED");


    DoMultipleInheritenceTest();
    DoMultipleInheritenceTest2();

    return 0;
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

// {491C65A0-3317-11d3-9885-006008962422}
#define FOO_IID \
{ 0x491c65a0, 0x3317, 0x11d3, \
    { 0x98, 0x85, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22 } }

// {491C65A1-3317-11d3-9885-006008962422}
#define BAR_IID \
{ 0x491c65a1, 0x3317, 0x11d3, \
    { 0x98, 0x85, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22 } }

/***************************/

class nsIFoo : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(FOO_IID)
    NS_IMETHOD FooMethod1(PRInt32 i) = 0;
    NS_IMETHOD FooMethod2(PRInt32 i) = 0;
};

class nsIBar : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(BAR_IID)
    NS_IMETHOD BarMethod1(PRInt32 i) = 0;
    NS_IMETHOD BarMethod2(PRInt32 i) = 0;
};

/***************************/

class FooImpl : public nsIFoo
{
public:
    NS_IMETHOD FooMethod1(PRInt32 i);
    NS_IMETHOD FooMethod2(PRInt32 i);

    FooImpl();
    virtual ~FooImpl();

    virtual char* ImplName() = 0;

    int SomeData1;
    int SomeData2;
    char* Name;
};

class BarImpl : public nsIBar
{
public:
    NS_IMETHOD BarMethod1(PRInt32 i);
    NS_IMETHOD BarMethod2(PRInt32 i);

    BarImpl();
    virtual ~BarImpl();

    virtual char * ImplName() = 0;

    int SomeData1;
    int SomeData2;
    char* Name;
};

/***************************/

FooImpl::FooImpl() : Name("FooImpl")
{
}
FooImpl::~FooImpl()
{
}

NS_IMETHODIMP FooImpl::FooMethod1(PRInt32 i)
{
    printf("\tFooImpl::FooMethod1 called with i == %d, %s part of a %s\n", 
           i, Name, ImplName());
    return NS_OK;
}

NS_IMETHODIMP FooImpl::FooMethod2(PRInt32 i)
{
    printf("\tFooImpl::FooMethod2 called with i == %d, %s part of a %s\n", 
           i, Name, ImplName());
    return NS_OK;
}

/***************************/

BarImpl::BarImpl() : Name("BarImpl")
{
}
BarImpl::~BarImpl()
{
}

NS_IMETHODIMP BarImpl::BarMethod1(PRInt32 i)
{
    printf("\tBarImpl::BarMethod1 called with i == %d, %s part of a %s\n", 
           i, Name, ImplName());
    return NS_OK;
}

NS_IMETHODIMP BarImpl::BarMethod2(PRInt32 i)
{
    printf("\tBarImpl::BarMethod2 called with i == %d, %s part of a %s\n", 
           i, Name, ImplName());
    return NS_OK;
}

/***************************/

class FooBarImpl : public FooImpl, public BarImpl
{
public:
    NS_DECL_ISUPPORTS

    char* ImplName();

    FooBarImpl();
    virtual ~FooBarImpl();
    char* MyName;
};

FooBarImpl::FooBarImpl() : MyName("FooBarImpl")
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

FooBarImpl::~FooBarImpl()
{
}

char* FooBarImpl::ImplName()
{
    return MyName;
}

NS_IMETHODIMP
FooBarImpl::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  *aInstancePtr = NULL;


  if (aIID.Equals(NS_GET_IID(nsIFoo))) {
    *aInstancePtr = (void*) NS_STATIC_CAST(nsIFoo*,this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIBar))) {
    *aInstancePtr = (void*) NS_STATIC_CAST(nsIBar*,this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = (void*) NS_STATIC_CAST(nsISupports*,
                                           NS_STATIC_CAST(nsIFoo*,this));
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(FooBarImpl)
NS_IMPL_RELEASE(FooBarImpl)


static void DoMultipleInheritenceTest()
{
    FooBarImpl* impl = new FooBarImpl();
    if(!impl)
        return;

    nsIFoo* foo;
    nsIBar* bar;

    nsXPTCVariant var[1];

    printf("\n");
    if(NS_SUCCEEDED(impl->QueryInterface(NS_GET_IID(nsIFoo), (void**)&foo)) &&
       NS_SUCCEEDED(impl->QueryInterface(NS_GET_IID(nsIBar), (void**)&bar)))
    {
        printf("impl == %p\n", impl);
        printf("foo  == %p\n", foo);
        printf("bar  == %p\n", bar);

        printf("Calling Foo...\n");
        printf("direct calls:\n");
        foo->FooMethod1(1);
        foo->FooMethod2(2);

        printf("invoke calls:\n");
        var[0].val.i32 = 1;
        var[0].type = nsXPTType::T_I32;
        var[0].flags = 0;
        XPTC_InvokeByIndex(foo, 3, 1, var);

        var[0].val.i32 = 2;
        var[0].type = nsXPTType::T_I32;
        var[0].flags = 0;
        XPTC_InvokeByIndex(foo, 4, 1, var);

        printf("\n");

        printf("Calling Bar...\n");
        printf("direct calls:\n");
        bar->BarMethod1(1);
        bar->BarMethod2(2);

        printf("invoke calls:\n");
        var[0].val.i32 = 1;
        var[0].type = nsXPTType::T_I32;
        var[0].flags = 0;
        XPTC_InvokeByIndex(bar, 3, 1, var);

        var[0].val.i32 = 2;
        var[0].type = nsXPTType::T_I32;
        var[0].flags = 0;
        XPTC_InvokeByIndex(bar, 4, 1, var);

        printf("\n");

        NS_RELEASE(foo);
        NS_RELEASE(bar);
    }
    NS_RELEASE(impl);
}
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/* This is a variation on the theme submitted by duncan@be.com (Duncan Wilcox).
*  He was seeing the other test work and this test not work. They should both
*  Work on any given platform
*/

class nsIFoo2 : public nsISupports
{
public:
    NS_IMETHOD FooMethod1(PRInt32 i) = 0;
    NS_IMETHOD FooMethod2(PRInt32 i) = 0;
};

class nsIBar2 : public nsISupports
{
public:
    NS_IMETHOD BarMethod1(PRInt32 i) = 0;
    NS_IMETHOD BarMethod2(PRInt32 i) = 0;
};

class FooBarImpl2 : public nsIFoo2, public nsIBar2
{
public:
    // Foo interface
    NS_IMETHOD FooMethod1(PRInt32 i);
    NS_IMETHOD FooMethod2(PRInt32 i);

    // Bar interface
    NS_IMETHOD BarMethod1(PRInt32 i);
    NS_IMETHOD BarMethod2(PRInt32 i);

    NS_DECL_ISUPPORTS

    FooBarImpl2();
    virtual ~FooBarImpl2();
        PRInt32 value;
};

FooBarImpl2::FooBarImpl2() : value(0x12345678)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

FooBarImpl2::~FooBarImpl2()
{
}

NS_IMETHODIMP FooBarImpl2::FooMethod1(PRInt32 i)
{
    printf("\tFooBarImpl2::FooMethod1 called with i == %d, local value = %x\n", 
           i, value);
    return NS_OK;
}

NS_IMETHODIMP FooBarImpl2::FooMethod2(PRInt32 i)
{
    printf("\tFooBarImpl2::FooMethod2 called with i == %d, local value = %x\n", 
           i, value);
    return NS_OK;
}

NS_IMETHODIMP FooBarImpl2::BarMethod1(PRInt32 i)
{
    printf("\tFooBarImpl2::BarMethod1 called with i == %d, local value = %x\n", 
           i, value);
    return NS_OK;
}

NS_IMETHODIMP FooBarImpl2::BarMethod2(PRInt32 i)
{
    printf("\tFooBarImpl2::BarMethod2 called with i == %d, local value = %x\n", 
           i, value);
    return NS_OK;
}

NS_IMETHODIMP
FooBarImpl2::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  *aInstancePtr = NULL;


  if (aIID.Equals(NS_GET_IID(nsIFoo))) {
    *aInstancePtr = (void*) NS_STATIC_CAST(nsIFoo2*,this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIBar))) {
    *aInstancePtr = (void*) NS_STATIC_CAST(nsIBar2*,this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = (void*) NS_STATIC_CAST(nsISupports*,
                                           NS_STATIC_CAST(nsIFoo2*,this));
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(FooBarImpl2)
NS_IMPL_RELEASE(FooBarImpl2)

static void DoMultipleInheritenceTest2()
{
    FooBarImpl2* impl = new FooBarImpl2();
    if(!impl)
        return;

    nsIFoo2* foo;
    nsIBar2* bar;

    nsXPTCVariant var[1];

    printf("\n");
    if(NS_SUCCEEDED(impl->QueryInterface(NS_GET_IID(nsIFoo), (void**)&foo)) &&
       NS_SUCCEEDED(impl->QueryInterface(NS_GET_IID(nsIBar), (void**)&bar)))
    {
        printf("impl == %p\n", impl);
        printf("foo  == %p\n", foo);
        printf("bar  == %p\n", bar);

        printf("Calling Foo...\n");
        printf("direct calls:\n");
        foo->FooMethod1(1);
        foo->FooMethod2(2);

        printf("invoke calls:\n");
        var[0].val.i32 = 1;
        var[0].type = nsXPTType::T_I32;
        var[0].flags = 0;
        XPTC_InvokeByIndex(foo, 3, 1, var);

        var[0].val.i32 = 2;
        var[0].type = nsXPTType::T_I32;
        var[0].flags = 0;
        XPTC_InvokeByIndex(foo, 4, 1, var);

        printf("\n");

        printf("Calling Bar...\n");
        printf("direct calls:\n");
        bar->BarMethod1(1);
        bar->BarMethod2(2);

        printf("invoke calls:\n");
        var[0].val.i32 = 1;
        var[0].type = nsXPTType::T_I32;
        var[0].flags = 0;
        XPTC_InvokeByIndex(bar, 3, 1, var);

        var[0].val.i32 = 2;
        var[0].type = nsXPTType::T_I32;
        var[0].flags = 0;
        XPTC_InvokeByIndex(bar, 4, 1, var);

        printf("\n");

        NS_RELEASE(foo);
        NS_RELEASE(bar);
    }
    NS_RELEASE(impl);
}


