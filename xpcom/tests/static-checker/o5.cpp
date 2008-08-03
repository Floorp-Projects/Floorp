typedef int PRUint32;
typedef int PRInt32;

typedef PRUint32 nsresult;

nsresult bar(__attribute__((user("outparam"))) int *a);

nsresult foo(__attribute__((user("outparam"))) int *a) {
  int rv = bar(a);
  return rv;
}
