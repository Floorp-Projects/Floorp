typedef int PRUint32;
typedef int PRInt32;

typedef PRUint32 nsresult;

inline int NS_FAILED(nsresult _nsresult) {
  return _nsresult & 0x80000000;
}

inline int NS_SUCCEEDED(nsresult _nsresult) {
    return !(_nsresult & 0x80000000);
}

int SomeFunc(nsresult *rv);

nsresult foo(__attribute__((user("NS_outparam"))) int *a) {
  nsresult rv;
  int i = SomeFunc(&rv);
  if (NS_FAILED(rv))
    return rv;
  
  *a = i;
  return 0;
}
