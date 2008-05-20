typedef int PRBool;
typedef int PRUint32;
typedef int PRInt32;

typedef PRUint32 nsresult;
typedef short PRUnichar;

#define NS_OUTPARAM __attribute__((user("NS_outparam")))

PRBool baz(int *p NS_OUTPARAM);

PRBool bar(int *p NS_OUTPARAM) {
  return baz(p);
}

nsresult foo(int *p) {
  if (bar(p)) {
    return 0;
  } else {
    return 1;
  }
}
