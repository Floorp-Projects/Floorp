typedef int PRUint32;
typedef int PRInt32;

typedef PRUint32 nsresult;

nsresult foo(__attribute__((user("NS_inoutparam"))) int *a) {
  return 0;
}
