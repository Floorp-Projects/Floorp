typedef int PRUint32;
typedef int PRInt32;

typedef PRUint32 nsresult;
typedef short PRUnichar;

nsresult bar(PRUnichar **a, int q);

nsresult foo(PRUnichar **a) {
  return bar(a, 0);
}
