typedef int PRUint32;
typedef int PRInt32;

typedef PRUint32 nsresult;

nsresult bar(int *a);

nsresult foo(__attribute__((user("outparam"))) int *a) {
  bar(a);
  return 0;
}
