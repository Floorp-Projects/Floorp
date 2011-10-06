typedef int bool;
typedef int PRUint32;
typedef int PRInt32;

typedef PRUint32 nsresult;
typedef short PRUnichar;

void bar(int *p __attribute__((user("NS_outparam")))) {
}
