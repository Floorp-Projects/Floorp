typedef int PRUint32;
typedef int PRInt32;

typedef PRUint32 nsresult;

nsresult foo(nsresult x, __attribute__((user("outparam"))) int *a) {
  if (x == 0) {
    *a = 1;
  }
  return x;
}
