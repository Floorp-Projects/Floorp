// https://clang-analyzer.llvm.org/available_checks.html

void use(int *p);

void test_use_parameter_after_delete(int *p)
{
  delete p;
  use(p); // warning: use after free
}

class SomeClass {
public:
  void f();
};

void test_use_local_after_delete()
{
  SomeClass *c = new SomeClass;
  delete c;
  c->f(); // warning: use after free
}

// XXX clang documentation says this should cause a warning but it doesn't!
void test_delete_alloca()
{
  int *p = (int *)__builtin_alloca(sizeof(int));
  delete p; // NO warning: deleting memory allocated by alloca
}

void test_double_free()
{
  int *p = new int;
  delete p;
  delete p; // warning: attempt to free released
}

void test_delete_local()
{
  int i;
  delete &i; // warning: delete address of local
}

// XXX clang documentation says this should cause a warning but it doesn't!
void test_delete_offset()
{
  int *p = new int[1];
  delete[] (++p);
    // NO warning: argument to 'delete[]' is offset by 4 bytes
    // from the start of memory allocated by 'new[]'
}
