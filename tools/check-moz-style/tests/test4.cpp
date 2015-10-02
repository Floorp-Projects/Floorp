class ShouldUseExplicit {
  // runtime/explicit
  ShouldUseExplicit(int i);
};

// readability/function
int foo(int) {
}

int main() {
  int i = 0;

  // readability/control_flow
  // XXX This doesn't trigger it. It needs to be fixed.
  if (i) {
    return;
  } else {
    i++;
  }

  // whitespace/parens
  if(i){}

  // readability/casting
  void* bad = (void*)i;

  // readability/comparison_to_zero
  if (i == true) {}
  if (i == false) {}
  if (i != true) {}
  if (i != false) {}
  if (i == NULL) {}
  if (i != NULL) {}
  if (i == nullptr) {}
  if (i != nullptr) {}
  if (i) {}
  if (!i) {}

  return 0;
}
