typedef int PRUint32;
typedef int PRInt32;
typedef PRUint32 nsresult;

typedef nsresult (*xpcomFunc)(PRInt32 * __attribute__((user("NS_outparam"))) a,
                              PRInt32 ** __attribute__((user("NS_outparam"))) b);

struct A {
  virtual nsresult TestMethod(PRInt32 *a __attribute__((user("NS_outparam"))),
                              PRInt32 **b __attribute__((user("NS_outparam"))));

  struct FuncTable {
    xpcomFunc mFunc;
  } *mTable;
};

nsresult A::TestMethod(PRInt32 *a, PRInt32 **b)
{
  return mTable->mFunc(a, b);
}
