typedef int PRUint32;
typedef int PRInt32;

typedef PRUint32 nsresult;

nsresult bar(char **a, int q);

nsresult foo(char **a) {
  bar(a, 0);
  return 0;
}
