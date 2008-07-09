typedef int PRInt32;
typedef int nsresult;

void
OutFunc(PRInt32 *out __attribute__((user("NS_outparam"))))
{
  *out = 0;
}

nsresult TestFunc(PRInt32 *a __attribute__((user("NS_outparam"))))
{
  OutFunc(a);
  return 0;
}
