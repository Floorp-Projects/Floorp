class A
{
  int m(int);
};

class B : A
{
  __attribute__((user("NS_override"))) int m(void*);
};
