/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include <stdio.h>

typedef unsigned nsresult;
typedef unsigned PRUint32;
typedef unsigned nsXPCVariant;


#if defined(WIN32)
#define NS_IMETHOD virtual nsresult __stdcall
#define NS_IMETHODIMP nsresult __stdcall
#else
#define NS_IMETHOD virtual nsresult
#define NS_IMETHODIMP nsresult
#endif


class base{
public:
  NS_IMETHOD ignored() = 0;
};

class foo : public base {
public:
  NS_IMETHOD callme1(int i, int j) = 0;
  NS_IMETHOD callme2(int i, int j) = 0;
  NS_IMETHOD callme3(int i, int j) = 0;
};

class bar : public foo{
public:
  NS_IMETHOD ignored();
  NS_IMETHOD callme1(int i, int j);
  NS_IMETHOD callme2(int i, int j);
  NS_IMETHOD callme3(int i, int j);
};

/*
class baz : public base {
public:
  NS_IMETHOD ignored();
  NS_IMETHOD callme1();
  NS_IMETHOD callme2();
  NS_IMETHOD callme3();
  void setfoo(foo* f) {other = f;}

  foo* other;
};
NS_IMETHODIMP baz::ignored(){return 0;}
*/

NS_IMETHODIMP bar::ignored(){return 0;}

NS_IMETHODIMP bar::callme1(int i, int j)
{
  printf("called bar::callme1 with: %d %d\n", i, j);
  return 5;
}

NS_IMETHODIMP bar::callme2(int i, int j)
{
  printf("called bar::callme2 with: %d %d\n", i, j);
  return 5;
}

NS_IMETHODIMP bar::callme3(int i, int j)
{
  printf("called bar::callme3 with: %d %d\n", i, j);
  return 5;
}

void docall(foo* f, int i, int j){
  f->callme1(i, j); 
}

/***************************************************************************/
#if defined(WIN32)

static PRUint32 __stdcall
invoke_count_words(PRUint32 paramCount, nsXPCVariant* s)
{
    return paramCount;
}    

static void __stdcall
invoke_copy_to_stack(PRUint32* d, PRUint32 paramCount, nsXPCVariant* s)
{
    for(PRUint32 i = 0; i < paramCount; i++, d++, s++)
    {
        *((PRUint32*)d) = *((PRUint32*)s);
    }
}

static nsresult __stdcall
DoInvoke(void* that, PRUint32 index,
         PRUint32 paramCount, nsXPCVariant* params)
{
    __asm {
        push    params
        push    paramCount
        call    invoke_count_words  // stdcall, result in eax
        shl     eax,2               // *= 4
        sub     esp,eax             // make space for params
        mov     edx,esp
        push    params
        push    paramCount
        push    edx
        call    invoke_copy_to_stack // stdcall
        mov     ecx,that            // instance in ecx
        push    ecx                 // push this
        mov     edx,[ecx]           // vtable in edx
        mov     eax,index
        shl     eax,2               // *= 4
        add     edx,eax
        call    [edx]               // stdcall, i.e. callee cleans up stack.
    }
}

#else
/***************************************************************************/
// just Linux_x86 now. Add other later...

static PRUint32 
invoke_count_words(PRUint32 paramCount, nsXPCVariant* s)
{
    return paramCount;
}    

static void 
invoke_copy_to_stack(PRUint32* d, PRUint32 paramCount, nsXPCVariant* s)
{
    for(PRUint32 i = 0; i < paramCount; i++, d++, s++)
    {
        *((PRUint32*)d) = *((PRUint32*)s);
    }
}

static nsresult
DoInvoke(void* that, PRUint32 index,
         PRUint32 paramCount, nsXPCVariant* params)
{
    PRUint32 result;
    void* fn_count = invoke_count_words;
    void* fn_copy = invoke_copy_to_stack;

 __asm__ __volatile__(
    "pushl %4\n\t"
    "pushl %3\n\t"
    "movl  %5, %%eax\n\t"
    "call  *%%eax\n\t"       /* count words */
    "addl  $0x8, %%esp\n\t"
    "shl   $2, %%eax\n\t"    /* *= 4 */
    "subl  %%eax, %%esp\n\t" /* make room for params */
    "movl  %%esp, %%edx\n\t"
    "pushl %4\n\t"
    "pushl %3\n\t"
    "pushl %%edx\n\t"
    "movl  %6, %%eax\n\t"
    "call  *%%eax\n\t"       /* copy params */
    "addl  $0xc, %%esp\n\t"
    "movl  %1, %%ecx\n\t"
    "pushl %%ecx\n\t"
    "movl  (%%ecx), %%edx\n\t"
    "movl  %2, %%eax\n\t"   /* function index */
    "shl   $2, %%eax\n\t"   /* *= 4 */
    "addl  $8, %%eax\n\t"   /* += 8 */
    "addl  %%eax, %%edx\n\t"
    "call  *(%%edx)\n\t"    /* safe to not cleanup esp */
    "movl  %%eax, %0"
    : "=g" (result)         /* %0 */
    : "g" (that),           /* %1 */
      "g" (index),          /* %2 */
      "g" (paramCount),     /* %3 */
      "g" (params),         /* %4 */
      "g" (fn_count),       /* %5 */
      "g" (fn_copy)         /* %6 */
    : "ax", "cx", "dx", "memory" 
    );
  
  return result;
}    

#endif
/***************************************************************************/

int main()
{
  nsXPCVariant params1[2] = {1,2};
  nsXPCVariant params2[2] = {2,4};
  nsXPCVariant params3[2] = {3,6};

  foo* a = new bar();

//  printf("calling via C++...\n");
//  docall(a, 12, 24);

  printf("calling via ASM...\n");
  DoInvoke(a, 1, 2, params1);
  DoInvoke(a, 2, 2, params2);
  DoInvoke(a, 3, 2, params3);

  return 0;
}
