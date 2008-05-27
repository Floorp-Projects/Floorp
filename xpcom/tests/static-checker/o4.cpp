typedef int PRUint32;
typedef int PRInt32;

typedef PRUint32 nsresult;

nsresult bar(__attribute__((user("outparam"))) int q);

inline int NS_FAILED(nsresult _nsresult) {
  return _nsresult & 0x80000000;
}

inline int NS_SUCCEEDED(nsresult _nsresult) {
  return !(_nsresult & 0x80000000);
}

nsresult foo(__attribute__((user("outparam"))) int *a) {
  nsresult rv = bar(4);
  if (NS_FAILED(rv)) return rv;
  *a = 1;
  return 0;
}
