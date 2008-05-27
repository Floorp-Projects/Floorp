typedef int PRUint32;
typedef int PRInt32;

typedef PRUint32 nsresult;

int x;

nsresult foo(__attribute__((user("outparam"))) int *a) {
  if (x) {
    *a = 1;
    return 0;
  } else {
    return 1;
  }
}
