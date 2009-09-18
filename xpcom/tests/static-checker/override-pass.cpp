class A
{
  int a(int, char*);
  void c(char);
};

class B : A
{
  __attribute__((user("NS_override"))) int a(int, char*);
};

class C : B
{
  __attribute__((user("NS_override"))) void c(char);
};
