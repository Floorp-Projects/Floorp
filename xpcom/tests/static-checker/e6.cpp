typedef int PRUint32;
typedef int PRInt32;

typedef PRUint32 nsresult;

class A {
};

nsresult bar(__attribute__((user("outparam"))) void **a);

nsresult foo(__attribute__((user("outparam"))) A **a) {
  nsresult rv = bar((void **) a);
  return 1;
}
