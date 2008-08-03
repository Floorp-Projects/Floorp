typedef int PRUint32;
typedef int PRInt32;

typedef PRUint32 nsresult;

nsresult foo(__attribute__((user("inoutparam"))) int *a) {
  *a = 9;
  return 1;
}
