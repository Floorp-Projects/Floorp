typedef int PRUint32;
typedef int PRInt32;

typedef PRUint32 nsresult;
typedef short PRUnichar;

nsresult foo(int *p __attribute__((user("NS_inparam")))) {
  return 0;
}
