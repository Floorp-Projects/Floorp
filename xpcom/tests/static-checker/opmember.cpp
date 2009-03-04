/**
 * Make sure treehydra/outparams don't choke on pointer-to-members.
 */

typedef int PRUint32;
typedef PRUint32 nsresult;

class A
{
  int i;
  int j;
};

nsresult
TestMethod(int A::* member,
           __attribute__((user("outparam"))) int *out) {
  *out = 1;
  return 0;
}

           
