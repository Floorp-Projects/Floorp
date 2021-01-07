void f() {
  int * pointer = nullptr, value = 42, * const const_ptr = &value;
  // This declaration will be diagnosed and transformed into:
  // int * pointer = nullptr;
  // int value = 42;
  // int * const const_ptr = &value;
}
