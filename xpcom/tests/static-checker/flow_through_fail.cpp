static void MUST_FLOW_THROUGH(const char *label) {
}

int test(int x, int y) {
  MUST_FLOW_THROUGH("out");
  
  if (!x) {
    x = y;
    goto out;
  }

  return y;
 out:
  x--;
  return x;
}
