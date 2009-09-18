class A
{
  static int m();
};

class B : A
{
  __attribute__((user("NS_override"))) static int m();
};
