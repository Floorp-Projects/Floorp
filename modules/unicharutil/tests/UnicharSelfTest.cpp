/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
#include <iostream.h>
#include "nsISupports.h"
#include "nsRepository.h"
#include "nsICaseConversion.h"
#include "nsUnicharUtilCIID.h"

NS_DEFINE_CID(kUnicharUtilCID, NS_UNICHARUTIL_CID);
NS_DEFINE_IID(kCaseConversionIID, NS_ICASECONVERSION_IID);

#define UNICHARUTIL_DLL_NAME "unicharutilDebug.shlib"

#define TESTLEN 29
#define T2LEN TESTLEN
#define T3LEN TESTLEN
#define T4LEN TESTLEN

// test data for ToUpper 
static PRUnichar t2data  [T2LEN+1] = {
  0x0031 ,  //  0
  0x0019 ,  //  1
  0x0043 ,  //  2
  0x0067 ,  //  3
  0x00C8 ,  //  4
  0x00E9 ,  //  5
  0x0147 ,  //  6
  0x01C4 ,  //  7
  0x01C6 ,  //  8
  0x01C5 ,  //  9
  0x03C0 ,  // 10
  0x03B2 ,  // 11
  0x0438 ,  // 12
  0x04A5 ,  // 13
  0x05D0 ,  // 14
  0x0A20 ,  // 15
  0x30B0 ,  // 16
  0x5185 ,  // 17
  0xC021 ,  // 18
  0xFF48 ,  // 19
  0x01C7 ,  // 20
  0x01C8 ,  // 21
  0x01C9 ,  // 22
  0x01CA ,  // 23
  0x01CB ,  // 24
  0x01CC ,  // 25
  0x01F1 ,  // 26
  0x01F2 ,  // 27
  0x01F3 ,  // 28
  0x00  
};
// expected result for ToUpper 
static PRUnichar t2result[T2LEN+1] =  {
  0x0031 ,  //  0
  0x0019 ,  //  1
  0x0043 ,  //  2
  0x0047 ,  //  3
  0x00C8 ,  //  4
  0x00C9 ,  //  5
  0x0147 ,  //  6
  0x01C4 ,  //  7
  0x01C4 ,  //  8
  0x01C4 ,  //  9
  0x03A0 ,  // 10
  0x0392 ,  // 11
  0x0418 ,  // 12
  0x04A4 ,  // 13
  0x05D0 ,  // 14
  0x0A20 ,  // 15
  0x30B0 ,  // 16
  0x5185 ,  // 17
  0xC021 ,  // 18
  0xFF28 ,  // 19
  0x01C7 ,  // 20
  0x01C7 ,  // 21
  0x01C7 ,  // 22
  0x01CA ,  // 23
  0x01CA ,  // 24
  0x01CA ,  // 25
  0x01F1 ,  // 26
  0x01F1 ,  // 27
  0x01F1 ,  // 28
  0x00  
};
// test data for ToLower 
static PRUnichar t3data  [T3LEN+1] =  {
  0x0031 ,  //  0
  0x0019 ,  //  1
  0x0043 ,  //  2
  0x0067 ,  //  3
  0x00C8 ,  //  4
  0x00E9 ,  //  5
  0x0147 ,  //  6
  0x01C4 ,  //  7
  0x01C6 ,  //  8
  0x01C5 ,  //  9
  0x03A0 ,  // 10
  0x0392 ,  // 11
  0x0418 ,  // 12
  0x04A4 ,  // 13
  0x05D0 ,  // 14
  0x0A20 ,  // 15
  0x30B0 ,  // 16
  0x5187 ,  // 17
  0xC023 ,  // 18
  0xFF28 ,  // 19
  0x01C7 ,  // 20
  0x01C8 ,  // 21
  0x01C9 ,  // 22
  0x01CA ,  // 23
  0x01CB ,  // 24
  0x01CC ,  // 25
  0x01F1 ,  // 26
  0x01F2 ,  // 27
  0x01F3 ,  // 28
  0x00  
};
// expected result for ToLower 
static PRUnichar t3result[T3LEN+1] =  {
  0x0031 ,  //  0
  0x0019 ,  //  1
  0x0063 ,  //  2
  0x0067 ,  //  3
  0x00E8 ,  //  4
  0x00E9 ,  //  5
  0x0148 ,  //  6
  0x01C6 ,  //  7
  0x01C6 ,  //  8
  0x01C6 ,  //  9
  0x03C0 ,  // 10
  0x03B2 ,  // 11
  0x0438 ,  // 12
  0x04A5 ,  // 13
  0x05D0 ,  // 14
  0x0A20 ,  // 15
  0x30B0 ,  // 16
  0x5187 ,  // 17
  0xC023 ,  // 18
  0xFF48 ,  // 19
  0x01C9 ,  // 20
  0x01C9 ,  // 21
  0x01C9 ,  // 22
  0x01CC ,  // 23
  0x01CC ,  // 24
  0x01CC ,  // 25
  0x01F3 ,  // 26
  0x01F3 ,  // 27
  0x01F3 ,  // 28
  0x00  
};
// test data for ToTitle 
static PRUnichar t4data  [T4LEN+1] =  {
  0x0031 ,  //  0
  0x0019 ,  //  1
  0x0043 ,  //  2
  0x0067 ,  //  3
  0x00C8 ,  //  4
  0x00E9 ,  //  5
  0x0147 ,  //  6
  0x01C4 ,  //  7
  0x01C6 ,  //  8
  0x01C5 ,  //  9
  0x03C0 ,  // 10
  0x03B2 ,  // 11
  0x0438 ,  // 12
  0x04A5 ,  // 13
  0x05D0 ,  // 14
  0x0A20 ,  // 15
  0x30B0 ,  // 16
  0x5189 ,  // 17
  0xC013 ,  // 18
  0xFF52 ,  // 19
  0x01C7 ,  // 20
  0x01C8 ,  // 21
  0x01C9 ,  // 22
  0x01CA ,  // 23
  0x01CB ,  // 24
  0x01CC ,  // 25
  0x01F1 ,  // 26
  0x01F2 ,  // 27
  0x01F3 ,  // 28
  0x00  
};
// expected result for ToTitle 
static PRUnichar t4result[T4LEN+1] =  {
  0x0031 ,  //  0
  0x0019 ,  //  1
  0x0043 ,  //  2
  0x0047 ,  //  3
  0x00C8 ,  //  4
  0x00C9 ,  //  5
  0x0147 ,  //  6
  0x01C5 ,  //  7
  0x01C5 ,  //  8
  0x01C5 ,  //  9
  0x03A0 ,  // 10
  0x0392 ,  // 11
  0x0418 ,  // 12
  0x04A4 ,  // 13
  0x05D0 ,  // 14
  0x0A20 ,  // 15
  0x30B0 ,  // 16
  0x5189 ,  // 17
  0xC013 ,  // 18
  0xFF32 ,  // 19
  0x01C8 ,  // 20
  0x01C8 ,  // 21
  0x01C8 ,  // 22
  0x01CB ,  // 23
  0x01CB ,  // 24
  0x01CB ,  // 25
  0x01F2 ,  // 26
  0x01F2 ,  // 27
  0x01F2 ,  // 28
  0x00  
};

void TestCaseConversion()
{
   cout << "==============================\n";
   cout << "Start nsICaseConversion Test \n";
   cout << "==============================\n";
   nsICaseConversion *t = NULL;
   nsresult res;
   res = nsRepository::CreateInstance(kUnicharUtilCID,
                                NULL,
                                kCaseConversionIID,
                                (void**) &t);
           
   cout << "Test 1 - CreateInstance():\n";
   if(( NS_OK != res) || ( t == NULL ) ) {
     cout << "\t1st CreateInstance failed\n";
   } else {
     t->Release();
   }

   res = nsRepository::CreateInstance(kUnicharUtilCID,
                                NULL,
                                kCaseConversionIID,
                                (void**) &t);
           
   if(( NS_OK != res) || ( t == NULL ) ) {
     cout << "\t2nd CreateInstance failed\n";
   } else {
     int i;
     PRUnichar ch;
     PRUnichar buf[256];
     nsresult res;

    cout << "Test 2 - ToUpper(PRUnichar, PRUnichar*):\n";
    for(i=0;i < T2LEN ; i++)
    {
         res = t->ToUpper(t2data[i], &ch);
         if(NS_OK != res) {
            cout << "\tFailed!! return value != NS_OK\n";
            break;
         }
         if(ch != t2result[i]) 
            cout << "\tFailed!! result unexpected " << i << "\n";
     }


    cout << "Test 3 - ToLower(PRUnichar, PRUnichar*):\n";
    for(i=0;i < T3LEN; i++)
    {
         res = t->ToLower(t3data[i], &ch);
         if(NS_OK != res) {
            cout << "\tFailed!! return value != NS_OK\n";
            break;
         }
         if(ch != t3result[i]) 
            cout << "\tFailed!! result unexpected " << i << "\n";
     }


    cout << "Test 4 - ToTitle(PRUnichar, PRUnichar*):\n";
    for(i=0;i < T4LEN; i++)
    {
         res = t->ToTitle(t4data[i], &ch);
         if(NS_OK != res) {
            cout << "\tFailed!! return value != NS_OK\n";
            break;
         }
         if(ch != t4result[i]) 
            cout << "\tFailed!! result unexpected " << i << "\n";
     }


    cout << "Test 5 - ToUpper(PRUnichar*, PRUnichar*, PRUint32):\n";
    res = t->ToUpper(t2data, buf, T2LEN);
    if(NS_OK != res) {
       cout << "\tFailed!! return value != NS_OK\n";
    } else {
       for(i = 0; i < T2LEN; i++)
       {
          if(buf[i] != t2result[i])
          {
            cout << "\tFailed!! result unexpected " << i << "\n";
            break;
          }
       }
    }

    cout << "Test 6 - ToLower(PRUnichar*, PRUnichar*, PRUint32):\n";
    res = t->ToLower(t3data, buf, T3LEN);
    if(NS_OK != res) {
       cout << "\tFailed!! return value != NS_OK\n";
    } else {
       for(i = 0; i < T3LEN; i++)
       {
          if(buf[i] != t3result[i])
          {
            cout << "\tFailed!! result unexpected " << i << "\n";
            break;
          }
       }
    }

     cout << "Test 7 - ToTitle(PRUnichar*, PRUnichar*, PRUint32):\n";
     cout << "!!! To Be Implemented !!!\n";

     t->Release();
   }
   cout << "==============================\n";
   cout << "Finish nsICaseConversion Test \n";
   cout << "==============================\n";

}

void RegisterFactories()
{
   nsresult res;
   res = nsRepository::RegisterFactory(kUnicharUtilCID,
                                 UNICHARUTIL_DLL_NAME,
                                 PR_FALSE,
                                 PR_TRUE);
   if(NS_OK != res)
     cout << "RegisterFactory failed\n";
}
 
int main(int argc, char** argv) {
   
#ifndef USE_NSREG
   RegisterFactories();
#endif

   // --------------------------------------------

   TestCaseConversion();

   // --------------------------------------------
   cout << "Finish All The Test Cases\n";
   nsresult res;
   res = nsRepository::FreeLibraries();

   if(NS_OK != res)
      cout << "nsRepository failed\n";
   else
      cout << "nsRepository FreeLibraries Done\n";
   return 0;
}
