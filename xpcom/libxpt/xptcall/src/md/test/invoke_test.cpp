
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
