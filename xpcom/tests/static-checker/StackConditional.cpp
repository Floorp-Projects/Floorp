#define NS_STACK_CLASS __attribute__((user("NS_stack")))

struct NS_STACK_CLASS A
{
  int i;
};

void
GetIt(int i)
{
  A *a;

  if (i)
    a = new A();
}
