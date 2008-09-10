static void MUST_FLOW_THROUGH(const char *label) {
}

int test(int x, int y) {
  if (x == 3)
    return 0;

  if(x)
    MUST_FLOW_THROUGH("out");
  
  if (x) {
    x = y;
    goto out;
  }

  return y;
 out:
  x--;
  return x;
}
