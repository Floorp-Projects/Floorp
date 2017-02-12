struct foo {
  __attribute__((__vector_size__(1 * sizeof(long long)))) long long mMember;
};
