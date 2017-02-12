union UnionWithDtor {
  ~UnionWithDtor();
  int mFoo;
  void* mBar;
};
