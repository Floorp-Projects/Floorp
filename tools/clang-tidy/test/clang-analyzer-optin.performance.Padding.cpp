struct OverlyAlignedChar {
  char c1;
  int x;
  char c2;
  char c __attribute__((aligned(4096)));
};
