typedef int *MyPtr;
MyPtr getPtr();

void foo() {
  auto TdNakedPtr = getPtr();
}