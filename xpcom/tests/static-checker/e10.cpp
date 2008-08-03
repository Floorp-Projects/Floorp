typedef int PRUint32;
typedef int PRInt32;

typedef PRUint32 nsresult;
typedef short PRUnichar;

class nsACString;

nsresult bar(nsACString &a);
nsresult baz();

nsresult foo(nsACString &a) {
  bar(a);
  return baz();
}
