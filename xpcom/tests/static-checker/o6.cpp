typedef int PRUint32;
typedef int PRInt32;

typedef PRUint32 nsresult;

nsresult foo(__attribute__((user("outparam"))) int *a) {
  if (a) {
    *a = 1;
  }
  return 0;
}
