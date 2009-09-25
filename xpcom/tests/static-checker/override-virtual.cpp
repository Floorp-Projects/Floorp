class A
{
  int m();
};

class B : A
{
  __attribute__((user("NS_override"))) virtual int m();
};
