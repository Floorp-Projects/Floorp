/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Invoke tests xptcall. */

#include <stdio.h>
#include "xptcall.h"
#include "prlong.h"
#include "prinrval.h"
#include "nsMemory.h"

// Allows us to mark unused functions as known-unused
#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED
#endif

// forward declration
static void DoMultipleInheritenceTest();
static void DoMultipleInheritenceTest2();
static void UNUSED DoSpeedTest();

// {AAC1FB90-E099-11d2-984E-006008962422}
#define INVOKETESTTARGET_IID \
{ 0xaac1fb90, 0xe099, 0x11d2, \
  { 0x98, 0x4e, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22 } }


class InvokeTestTargetInterface : public nsISupports
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(INVOKETESTTARGET_IID)
    NS_IMETHOD AddTwoInts(int32_t p1, int32_t p2, int32_t* retval) = 0;
    NS_IMETHOD MultTwoInts(int32_t p1, int32_t p2, int32_t* retval) = 0;
    NS_IMETHOD AddTwoLLs(int64_t p1, int64_t p2, int64_t* retval) = 0;
    NS_IMETHOD MultTwoLLs(int64_t p1, int64_t p2, int64_t* retval) = 0;

    NS_IMETHOD AddManyInts(int32_t p1, int32_t p2, int32_t p3, int32_t p4,
                           int32_t p5, int32_t p6, int32_t p7, int32_t p8,
                           int32_t p9, int32_t p10, int32_t* retval) = 0;

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

    NS_IMETHOD AddMixedInts(int64_t p1, int32_t p2, int64_t p3, int32_t p4,
                            int32_t p5, int64_t p6, int32_t p7, int32_t p8,
                            int64_t p9, int32_t p10, int64_t* retval) = 0;

    NS_IMETHOD AddMixedInts2(int32_t p1, int64_t p2, int32_t p3, int64_t p4,
                             int64_t p5, int32_t p6, int64_t p7, int64_t p8,
                             int32_t p9, int64_t p10, int64_t* retval) = 0;

    NS_IMETHOD AddMixedFloats(float p1, float p2, double p3, double p4,
                              float p5, float p6, double p7, double p8,
                              float p9, double p10, float p11,
                              double *retval) = 0;

    NS_IMETHOD PassTwoStrings(const char* s1, const char* s2, char** retval) = 0;

    NS_IMETHOD AddMixedInts3(int64_t p1, int64_t p2, int32_t p3, int64_t p4,
                             int32_t p5, int32_t p6, int64_t p7, int64_t p8,
                             int32_t p9, int64_t p10, int64_t* retval) = 0;
    NS_IMETHOD ShouldFail(int32_t p) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(InvokeTestTargetInterface, INVOKETESTTARGET_IID)

class InvokeTestTarget : public InvokeTestTargetInterface
{
public:
    NS_DECL_ISUPPORTS
    NS_IMETHOD AddTwoInts(int32_t p1, int32_t p2, int32_t* retval);
    NS_IMETHOD MultTwoInts(int32_t p1, int32_t p2, int32_t* retval);
    NS_IMETHOD AddTwoLLs(int64_t p1, int64_t p2, int64_t* retval);
    NS_IMETHOD MultTwoLLs(int64_t p1, int64_t p2, int64_t* retval);

    NS_IMETHOD AddManyInts(int32_t p1, int32_t p2, int32_t p3, int32_t p4,
                           int32_t p5, int32_t p6, int32_t p7, int32_t p8,
                           int32_t p9, int32_t p10, int32_t* retval);

    NS_IMETHOD AddTwoFloats(float p1, float p2, float* retval);

    NS_IMETHOD AddManyDoubles(double p1, double p2, double p3, double p4,
                              double p5, double p6, double p7, double p8,
                              double p9, double p10, double* retval);

    NS_IMETHOD AddManyFloats(float p1, float p2, float p3, float p4,
                             float p5, float p6, float p7, float p8,
                             float p9, float p10, float* retval);

    NS_IMETHOD AddMixedInts(int64_t p1, int32_t p2, int64_t p3, int32_t p4,
			    int32_t p5, int64_t p6, int32_t p7, int32_t p8,
			    int64_t p9, int32_t p10, int64_t* retval);

    NS_IMETHOD AddMixedInts2(int32_t p1, int64_t p2, int32_t p3, int64_t p4,
			     int64_t p5, int32_t p6, int64_t p7, int64_t p8,
			     int32_t p9, int64_t p10, int64_t* retval);

    NS_IMETHOD AddMixedFloats(float p1, float p2, double p3, double p4,
                              float p5, float p6, double p7, double p8,
                              float p9, double p10, float p11,
                              double *retval);

    NS_IMETHOD AddManyManyFloats(float p1, float p2, float p3, float p4,
                                 float p5, float p6, float p7, float p8,
                                 float p9, float p10, float p11, float p12, 
                                 float p13, float p14, float p15, float p16, 
                                 float p17, float p18, float p19, float p20, 
                                 float *retval);

    NS_IMETHOD PassTwoStrings(const char* s1, const char* s2, char** retval);

    InvokeTestTarget();

    NS_IMETHOD AddMixedInts3(int64_t p1, int64_t p2, int32_t p3, int64_t p4,
                             int32_t p5, int32_t p6, int64_t p7, int64_t p8,
                             int32_t p9, int64_t p10, int64_t* retval);
    NS_IMETHOD ShouldFail(int32_t p);
};

NS_IMPL_ISUPPORTS1(InvokeTestTarget, InvokeTestTargetInterface)

InvokeTestTarget::InvokeTestTarget()
{
    NS_ADDREF_THIS();
}

NS_IMETHODIMP
InvokeTestTarget::ShouldFail(int32_t p) {
    return NS_ERROR_NULL_POINTER;
}
NS_IMETHODIMP
InvokeTestTarget::AddTwoInts(int32_t p1, int32_t p2, int32_t* retval)
{
    *retval = p1 + p2;
    return NS_OK;
}

NS_IMETHODIMP
InvokeTestTarget::MultTwoInts(int32_t p1, int32_t p2, int32_t* retval)
{
    *retval = p1 * p2;
    return NS_OK;
}

NS_IMETHODIMP
InvokeTestTarget::AddTwoLLs(int64_t p1, int64_t p2, int64_t* retval)
{
    *retval = p1 + p2;
    return NS_OK;
}

NS_IMETHODIMP
InvokeTestTarget::MultTwoLLs(int64_t p1, int64_t p2, int64_t* retval)
{
    *retval = p1 * p2;
    return NS_OK;
}

NS_IMETHODIMP
InvokeTestTarget::AddManyInts(int32_t p1, int32_t p2, int32_t p3, int32_t p4,
                              int32_t p5, int32_t p6, int32_t p7, int32_t p8,
                              int32_t p9, int32_t p10, int32_t* retval)
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
InvokeTestTarget::AddMixedFloats(float p1, float p2, double p3, double p4,
                                 float p5, float p6, double p7, double p8,
                                 float p9, double p10, float p11,
                                 double *retval)
{
#ifdef DEBUG_TESTINVOKE
    printf("%f, %f, %lf, %lf, %f, %f, %lf, %lf, %f, %lf, %f\n", 
           p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);
#endif
    *retval = p1 + p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9 + p10 + p11;
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
InvokeTestTarget::AddMixedInts(int64_t p1, int32_t p2, int64_t p3, int32_t p4,
			       int32_t p5, int64_t p6, int32_t p7, int32_t p8,
			       int64_t p9, int32_t p10, int64_t* retval)
{
    *retval = p1 + p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9 + p10;
    return NS_OK;
}

NS_IMETHODIMP
InvokeTestTarget::AddMixedInts2(int32_t p1, int64_t p2, int32_t p3, int64_t p4,
				int64_t p5, int32_t p6, int64_t p7, int64_t p8,
				int32_t p9, int64_t p10, int64_t* retval)
{
    *retval = p1 + p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9 + p10;
    return NS_OK;
}

NS_IMETHODIMP
InvokeTestTarget::AddMixedInts3(int64_t p1, int64_t p2, int32_t p3, int64_t p4,
                             int32_t p5, int32_t p6, int64_t p7, int64_t p8,
                             int32_t p9, int64_t p10, int64_t* retval)
{
	printf("P1 : %lld\n", p1);
	printf("P2 : %lld\n", p2);
	printf("P3 : %d\n",   p3);
	printf("P4 : %lld\n", p4);
	printf("P5 : %d\n",   p5);
	printf("P6 : %d\n",   p6);
	printf("P7 : %lld\n", p7);
	printf("P8 : %lld\n", p8);
	printf("P9 : %d\n",   p9);
	printf("P10: %lld\n", p10);
	printf("ret: %p\n",   static_cast<void*>(retval));
    *retval = p1 + p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9 + p10;
    return NS_OK;
}

NS_IMETHODIMP
InvokeTestTarget::PassTwoStrings(const char* s1, const char* s2, char** retval)
{
    const char milk[] = "milk";
    char *ret = (char*)nsMemory::Alloc(sizeof(milk));
    if (!ret)
      return NS_ERROR_OUT_OF_MEMORY;
    strncpy(ret, milk, sizeof(milk));
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

    int32_t out, tmp32 = 0;
    int64_t out64;
    nsresult failed_rv;
    printf("calling direct:\n");
    if(NS_SUCCEEDED(test->AddTwoInts(1,1,&out)))
        printf("\t1 + 1 = %d\n", out);
    else
        printf("\tFAILED");
    int64_t one = 1, two = 2;
    if(NS_SUCCEEDED(test->AddTwoLLs(one,one,&out64)))
    {
        tmp32 = (int)out64;
        printf("\t1L + 1L = %d\n", tmp32);
    }
    else
        printf("\tFAILED");
    if(NS_SUCCEEDED(test->MultTwoInts(2,2,&out)))
        printf("\t2 * 2 = %d\n", out);
    else
        printf("\tFAILED");
    if(NS_SUCCEEDED(test->MultTwoLLs(two,two,&out64)))
    {
        tmp32 = (int)out64;
        printf("\t2L * 2L = %d\n", tmp32);
    }
    else
        printf("\tFAILED");

    double outD;
    float outF;
    int32_t outI;
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

    if(NS_SUCCEEDED(test->AddMixedInts(1,2,3,4,5,6,7,8,9,10,&out64)))
     {
         tmp32 = (int)out64;
         printf("\t1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 = %d\n", tmp32);
     }
     else
         printf("\tFAILED");
 
     if(NS_SUCCEEDED(test->AddMixedInts2(1,2,3,4,5,6,7,8,9,10,&out64)))
     {
         tmp32 = (int)out64;
         printf("\t1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 = %d\n", tmp32);
     }
     else
         printf("\tFAILED");

     if(NS_SUCCEEDED(test->AddMixedInts3(3,5,7,11,13,17,19,23,29,31,&out64)))
     {
         tmp32 = (int)out64;
         printf("\t3 + 5 + 7 + 11 + 13 + 17 + 19 + 23 + 29 + 31 = %d\n", tmp32);
     }
     else
         printf("\tFAILED");

     if(NS_SUCCEEDED(test->AddMixedFloats(1,2,3,4,5,6,7,8,9,10,11,&outD)))
         printf("\t1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 + 11 = %f\n", (double)outD);
     else
         printf("\tFAILED");

     if (NS_SUCCEEDED(test->PassTwoStrings("moo","cow",&outS))) {
       printf(" = %s\n", outS);
        nsMemory::Free(outS);
      } else
        printf("\tFAILED");

     failed_rv = test->ShouldFail(5);
     printf("should fail %s, returned %x\n", failed_rv == NS_ERROR_NULL_POINTER ? "failed" :"passed", failed_rv);
    
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

    if(NS_SUCCEEDED(NS_InvokeByIndex(test, 3, 3, var)))
        printf("\t1 + 1 = %d\n", var[2].val.i32);
    else
        printf("\tFAILED");

    var[0].val.i64 = 1;
    var[0].type = nsXPTType::T_I64;
    var[0].flags = 0;

    var[1].val.i64 = 1;
    var[1].type = nsXPTType::T_I64;
    var[1].flags = 0;

    var[2].val.i64 = 0;
    var[2].type = nsXPTType::T_I64;
    var[2].flags = nsXPTCVariant::PTR_IS_DATA;
    var[2].ptr = &var[2].val.i64;

    if(NS_SUCCEEDED(NS_InvokeByIndex(test, 5, 3, var)))
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

    if(NS_SUCCEEDED(NS_InvokeByIndex(test, 4, 3, var)))
        printf("\t2 * 2 = %d\n", var[2].val.i32);
    else
        printf("\tFAILED");

    var[0].val.i64 = 2;
    var[0].type = nsXPTType::T_I64;
    var[0].flags = 0;

    var[1].val.i64 = 2;
    var[1].type = nsXPTType::T_I64;
    var[1].flags = 0;

    var[2].val.i64 = 0;
    var[2].type = nsXPTType::T_I64;
    var[2].flags = nsXPTCVariant::PTR_IS_DATA;
    var[2].ptr = &var[2].val.i64;

    if(NS_SUCCEEDED(NS_InvokeByIndex(test, 6, 3, var)))
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

    if(NS_SUCCEEDED(NS_InvokeByIndex(test, 7, 11, var)))
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

    if(NS_SUCCEEDED(NS_InvokeByIndex(test, 8, 3, var)))
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

    if(NS_SUCCEEDED(NS_InvokeByIndex(test, 9, 11, var)))
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

    if(NS_SUCCEEDED(NS_InvokeByIndex(test, 10, 11, var)))
        printf("\t1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 = %ff\n",
                (double) var[10].val.f);
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

    if(NS_SUCCEEDED(NS_InvokeByIndex(test, 11, 21, var)))
        printf("\t1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 + 11 + 12 + 13 + 14 + 15 + 16 + 17 + 18 + 19 + 20 = %ff\n",
                (double) var[20].val.f);

    var[0].val.i64 = 1;
    var[0].type = nsXPTType::T_I64;
    var[0].flags = 0;

    var[1].val.i32 = 2;
    var[1].type = nsXPTType::T_I32;
    var[1].flags = 0;

    var[2].val.i64 = 3;
    var[2].type = nsXPTType::T_I64;
    var[2].flags = 0;

    var[3].val.i32 = 4;
    var[3].type = nsXPTType::T_I32;
    var[3].flags = 0;

    var[4].val.i32 = 5;
    var[4].type = nsXPTType::T_I32;
    var[4].flags = 0;

    var[5].val.i64 = 6;
    var[5].type = nsXPTType::T_I64;
    var[5].flags = 0;

    var[6].val.i32 = 7;
    var[6].type = nsXPTType::T_I32;
    var[6].flags = 0;

    var[7].val.i32 = 8;
    var[7].type = nsXPTType::T_I32;
    var[7].flags = 0;

    var[8].val.i64 = 9;
    var[8].type = nsXPTType::T_I64;
    var[8].flags = 0;

    var[9].val.i32 = 10;
    var[9].type = nsXPTType::T_I32;
    var[9].flags = 0;

    var[10].val.i64 = 0;
    var[10].type = nsXPTType::T_I64;
    var[10].flags = nsXPTCVariant::PTR_IS_DATA;
    var[10].ptr = &var[10].val.i64;

    if(NS_SUCCEEDED(NS_InvokeByIndex(test, 12, 11, var)))
        printf("\t1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 = %d\n",
	       (int)var[10].val.i64);
    else
        printf("\tFAILED");

    var[0].val.i32 = 1;
    var[0].type = nsXPTType::T_I32;
    var[0].flags = 0;

    var[1].val.i64 = 2;
    var[1].type = nsXPTType::T_I64;
    var[1].flags = 0;

    var[2].val.i32 = 3;
    var[2].type = nsXPTType::T_I32;
    var[2].flags = 0;

    var[3].val.i64 = 4;
    var[3].type = nsXPTType::T_I64;
    var[3].flags = 0;

    var[4].val.i64 = 5;
    var[4].type = nsXPTType::T_I64;
    var[4].flags = 0;

    var[5].val.i32 = 6;
    var[5].type = nsXPTType::T_I32;
    var[5].flags = 0;

    var[6].val.i64 = 7;
    var[6].type = nsXPTType::T_I64;
    var[6].flags = 0;

    var[7].val.i64 = 8;
    var[7].type = nsXPTType::T_I64;
    var[7].flags = 0;

    var[8].val.i32 = 9;
    var[8].type = nsXPTType::T_I32;
    var[8].flags = 0;

    var[9].val.i64 = 10;
    var[9].type = nsXPTType::T_I64;
    var[9].flags = 0;

    var[10].val.i64 = 0;
    var[10].type = nsXPTType::T_I64;
    var[10].flags = nsXPTCVariant::PTR_IS_DATA;
    var[10].ptr = &var[10].val.i64;

    if(NS_SUCCEEDED(NS_InvokeByIndex(test, 13, 11, var)))
        printf("\t1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 = %d\n",
	       (int)var[10].val.i64);
    else
        printf("\tFAILED");

    var[0].val.f = 1.0f;
    var[0].type = nsXPTType::T_FLOAT;
    var[0].flags = 0;

    var[1].val.f = 2.0f;
    var[1].type = nsXPTType::T_FLOAT;
    var[1].flags = 0;

    var[2].val.d = 3.0;
    var[2].type = nsXPTType::T_DOUBLE;
    var[2].flags = 0;

    var[3].val.d = 4.0;
    var[3].type = nsXPTType::T_DOUBLE;
    var[3].flags = 0;

    var[4].val.f = 5.0f;
    var[4].type = nsXPTType::T_FLOAT;
    var[4].flags = 0;

    var[5].val.f = 6.0f;
    var[5].type = nsXPTType::T_FLOAT;
    var[5].flags = 0;

    var[6].val.d = 7.0;
    var[6].type = nsXPTType::T_DOUBLE;
    var[6].flags = 0;

    var[7].val.d = 8.0;
    var[7].type = nsXPTType::T_DOUBLE;
    var[7].flags = 0;

    var[8].val.f = 9.0f;
    var[8].type = nsXPTType::T_FLOAT;
    var[8].flags = 0;

    var[9].val.d = 10.0;
    var[9].type = nsXPTType::T_DOUBLE;
    var[9].flags = 0;

    var[10].val.f = 11.0f;
    var[10].type = nsXPTType::T_FLOAT;
    var[10].flags = 0;

    var[11].val.d = 0.0;
    var[11].type = nsXPTType::T_DOUBLE;
    var[11].flags = nsXPTCVariant::PTR_IS_DATA;
    var[11].ptr = &var[11].val.d;

    if(NS_SUCCEEDED(NS_InvokeByIndex(test, 14, 12, var)))
        printf("\t1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 + 11 = %f\n",
                var[11].val.d);
    else
        printf("\tFAILED");

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
    
    if(NS_SUCCEEDED(NS_InvokeByIndex(test, 15, 3, var)))
        printf(" = %s\n", static_cast<char*>(var[2].val.p));
    else
        printf("\tFAILED");

    var[0].val.i32 = 5;
    var[0].type = nsXPTType::T_I32;
    var[0].flags = 0;

    failed_rv = NS_InvokeByIndex(test, 17, 1, var);
    printf("should fail %s, returned %x\n", failed_rv == NS_ERROR_NULL_POINTER ? "failed" :"passed", failed_rv);

    var[0].val.i64 = 3;
    var[0].type = nsXPTType::T_I64;
    var[0].flags = 0;

    var[1].val.i64 = 5;
    var[1].type = nsXPTType::T_I64;
    var[1].flags = 0;

    var[2].val.i32 = 7;
    var[2].type = nsXPTType::T_I32;
    var[2].flags = 0;

    var[3].val.i64 = 11;
    var[3].type = nsXPTType::T_I64;
    var[3].flags = 0;

    var[4].val.i32 = 13;
    var[4].type = nsXPTType::T_I32;
    var[4].flags = 0;

    var[5].val.i32 = 17;
    var[5].type = nsXPTType::T_I32;
    var[5].flags = 0;

    var[6].val.i64 = 19;
    var[6].type = nsXPTType::T_I64;
    var[6].flags = 0;

    var[7].val.i64 = 23;
    var[7].type = nsXPTType::T_I64;
    var[7].flags = 0;

    var[8].val.i32 = 29;
    var[8].type = nsXPTType::T_I32;
    var[8].flags = 0;

    var[9].val.i64 = 31;
    var[9].type = nsXPTType::T_I64;
    var[9].flags = 0;

    var[10].val.i64 = 0;
    var[10].type = nsXPTType::T_I64;
    var[10].flags = nsXPTCVariant::PTR_IS_DATA;
    var[10].ptr = &var[10].val.i64;

    if(NS_SUCCEEDED(NS_InvokeByIndex(test, 16, 11, var)))
        printf("\t3 + 5 + 7 + 11 + 13 + 17 + 19 + 23 + 29+ 31 = %d\n",
	       (int)var[10].val.i64);
    else
        printf("\tFAILED");

    DoMultipleInheritenceTest();
    DoMultipleInheritenceTest2();
    // Disabled by default - takes too much time on slow machines
    //DoSpeedTest();

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
    NS_DECLARE_STATIC_IID_ACCESSOR(FOO_IID)
    NS_IMETHOD FooMethod1(int32_t i) = 0;
    NS_IMETHOD FooMethod2(int32_t i) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIFoo, FOO_IID)

class nsIBar : public nsISupports
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(BAR_IID)
    NS_IMETHOD BarMethod1(int32_t i) = 0;
    NS_IMETHOD BarMethod2(int32_t i) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIBar, BAR_IID)

/***************************/

class FooImpl : public nsIFoo
{
public:
    NS_IMETHOD FooMethod1(int32_t i);
    NS_IMETHOD FooMethod2(int32_t i);

    FooImpl();

protected:
    ~FooImpl() {}

public:
    virtual const char* ImplName() = 0;

    int SomeData1;
    int SomeData2;
    const char* Name;
};

class BarImpl : public nsIBar
{
public:
    NS_IMETHOD BarMethod1(int32_t i);
    NS_IMETHOD BarMethod2(int32_t i);

    BarImpl();

protected:
    ~BarImpl() {}

public:
    virtual const char * ImplName() = 0;

    int SomeData1;
    int SomeData2;
    const char* Name;
};

/***************************/

FooImpl::FooImpl() : Name("FooImpl")
{
}

NS_IMETHODIMP FooImpl::FooMethod1(int32_t i)
{
    printf("\tFooImpl::FooMethod1 called with i == %d, %s part of a %s\n", 
           i, Name, ImplName());
    return NS_OK;
}

NS_IMETHODIMP FooImpl::FooMethod2(int32_t i)
{
    printf("\tFooImpl::FooMethod2 called with i == %d, %s part of a %s\n", 
           i, Name, ImplName());
    return NS_OK;
}

/***************************/

BarImpl::BarImpl() : Name("BarImpl")
{
}

NS_IMETHODIMP BarImpl::BarMethod1(int32_t i)
{
    printf("\tBarImpl::BarMethod1 called with i == %d, %s part of a %s\n", 
           i, Name, ImplName());
    return NS_OK;
}

NS_IMETHODIMP BarImpl::BarMethod2(int32_t i)
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

    const char* ImplName();

    FooBarImpl();

private:
    ~FooBarImpl() {}

public:
    const char* MyName;
};

FooBarImpl::FooBarImpl() : MyName("FooBarImpl")
{
    NS_ADDREF_THIS();
}

const char* FooBarImpl::ImplName()
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
    *aInstancePtr = (void*) static_cast<nsIFoo*>(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIBar))) {
    *aInstancePtr = (void*) static_cast<nsIBar*>(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = (void*) static_cast<nsISupports*>
                                       (static_cast<nsIFoo*>(this));
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
        printf("impl == %p\n", static_cast<void*>(impl));
        printf("foo  == %p\n", static_cast<void*>(foo));
        printf("bar  == %p\n", static_cast<void*>(bar));

        printf("Calling Foo...\n");
        printf("direct calls:\n");
        foo->FooMethod1(1);
        foo->FooMethod2(2);

        printf("invoke calls:\n");
        var[0].val.i32 = 1;
        var[0].type = nsXPTType::T_I32;
        var[0].flags = 0;
        NS_InvokeByIndex(foo, 3, 1, var);

        var[0].val.i32 = 2;
        var[0].type = nsXPTType::T_I32;
        var[0].flags = 0;
        NS_InvokeByIndex(foo, 4, 1, var);

        printf("\n");

        printf("Calling Bar...\n");
        printf("direct calls:\n");
        bar->BarMethod1(1);
        bar->BarMethod2(2);

        printf("invoke calls:\n");
        var[0].val.i32 = 1;
        var[0].type = nsXPTType::T_I32;
        var[0].flags = 0;
        NS_InvokeByIndex(bar, 3, 1, var);

        var[0].val.i32 = 2;
        var[0].type = nsXPTType::T_I32;
        var[0].flags = 0;
        NS_InvokeByIndex(bar, 4, 1, var);

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
    NS_IMETHOD FooMethod1(int32_t i) = 0;
    NS_IMETHOD FooMethod2(int32_t i) = 0;
};

class nsIBar2 : public nsISupports
{
public:
    NS_IMETHOD BarMethod1(int32_t i) = 0;
    NS_IMETHOD BarMethod2(int32_t i) = 0;
};

class FooBarImpl2 : public nsIFoo2, public nsIBar2
{
public:
    // Foo interface
    NS_IMETHOD FooMethod1(int32_t i);
    NS_IMETHOD FooMethod2(int32_t i);

    // Bar interface
    NS_IMETHOD BarMethod1(int32_t i);
    NS_IMETHOD BarMethod2(int32_t i);

    NS_DECL_ISUPPORTS

    FooBarImpl2();

private:
    ~FooBarImpl2() {}

public:
    int32_t value;
};

FooBarImpl2::FooBarImpl2() : value(0x12345678)
{
    NS_ADDREF_THIS();
}

NS_IMETHODIMP FooBarImpl2::FooMethod1(int32_t i)
{
    printf("\tFooBarImpl2::FooMethod1 called with i == %d, local value = %x\n", 
           i, value);
    return NS_OK;
}

NS_IMETHODIMP FooBarImpl2::FooMethod2(int32_t i)
{
    printf("\tFooBarImpl2::FooMethod2 called with i == %d, local value = %x\n", 
           i, value);
    return NS_OK;
}

NS_IMETHODIMP FooBarImpl2::BarMethod1(int32_t i)
{
    printf("\tFooBarImpl2::BarMethod1 called with i == %d, local value = %x\n", 
           i, value);
    return NS_OK;
}

NS_IMETHODIMP FooBarImpl2::BarMethod2(int32_t i)
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
    *aInstancePtr = (void*) static_cast<nsIFoo2*>(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIBar))) {
    *aInstancePtr = (void*) static_cast<nsIBar2*>(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = (void*) static_cast<nsISupports*>
                                       (static_cast<nsIFoo2*>(this));
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
        printf("impl == %p\n", static_cast<void*>(impl));
        printf("foo  == %p\n", static_cast<void*>(foo));
        printf("bar  == %p\n", static_cast<void*>(bar));

        printf("Calling Foo...\n");
        printf("direct calls:\n");
        foo->FooMethod1(1);
        foo->FooMethod2(2);

        printf("invoke calls:\n");
        var[0].val.i32 = 1;
        var[0].type = nsXPTType::T_I32;
        var[0].flags = 0;
        NS_InvokeByIndex(foo, 3, 1, var);

        var[0].val.i32 = 2;
        var[0].type = nsXPTType::T_I32;
        var[0].flags = 0;
        NS_InvokeByIndex(foo, 4, 1, var);

        printf("\n");

        printf("Calling Bar...\n");
        printf("direct calls:\n");
        bar->BarMethod1(1);
        bar->BarMethod2(2);

        printf("invoke calls:\n");
        var[0].val.i32 = 1;
        var[0].type = nsXPTType::T_I32;
        var[0].flags = 0;
        NS_InvokeByIndex(bar, 3, 1, var);

        var[0].val.i32 = 2;
        var[0].type = nsXPTType::T_I32;
        var[0].flags = 0;
        NS_InvokeByIndex(bar, 4, 1, var);

        printf("\n");

        NS_RELEASE(foo);
        NS_RELEASE(bar);
    }
    NS_RELEASE(impl);
}

static void DoSpeedTest()
{
    InvokeTestTarget *test = new InvokeTestTarget();

    nsXPTCVariant var[3];

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

    int32_t in1 = 1;
    int32_t in2 = 1;
    int32_t out;

    // Crank this number down if your platform is slow :)
    static const int count = 100000000;
    int i;
    PRIntervalTime start;
    PRIntervalTime interval_direct;
    PRIntervalTime interval_invoke;

    printf("Speed test...\n\n");
    printf("Doing %d direct call iterations...\n", count); 
    start = PR_IntervalNow();
    for(i = count; i; i--)
        (void)test->AddTwoInts(in1, in2, &out);
    interval_direct = PR_IntervalNow() - start;

    printf("Doing %d invoked call iterations...\n", count); 
    start = PR_IntervalNow();
    for(i = count; i; i--)
        (void)NS_InvokeByIndex(test, 3, 3, var);
    interval_invoke = PR_IntervalNow() - start;

    printf(" direct took %0.2f seconds\n", 
            (double)interval_direct/(double)PR_TicksPerSecond());
    printf(" invoke took %0.2f seconds\n", 
            (double)interval_invoke/(double)PR_TicksPerSecond());
    printf(" So, invoke overhead was ~ %0.2f seconds (~ %0.0f%%)\n", 
            (double)(interval_invoke-interval_direct)/(double)PR_TicksPerSecond(),
            (double)(interval_invoke-interval_direct)/(double)interval_invoke*100);
}        
