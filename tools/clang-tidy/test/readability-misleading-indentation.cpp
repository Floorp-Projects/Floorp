void f()
{
}

void foo() {
  if (1)
    if (0)
      f();
  else
    f();
}

void foo2() {
  constexpr bool a = true;
  if constexpr (a) {
    f();
  } else {
    f();
  }
}
