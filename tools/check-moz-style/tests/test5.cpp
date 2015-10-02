// License bogus

// runtime/virtual
class ShouldHaveVirtualDes {
  virtual foo();
};

int main() {
  // runtime/memset
  memset(blah, sizeof(blah), 0);

  // runtime/rtti
  dynamic_cast<Derived*>(obj);

  // runtime/sizeof
  int varname = 0;
  int mySize = sizeof(int);

  // runtime/threadsafe_fn
  getpwuid();
  strtok();

  return 0;
}
