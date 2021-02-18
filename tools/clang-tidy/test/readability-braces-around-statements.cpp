
void do_something(const char *) {}

bool cond(const char *) {
  return false;
}

void test() {
if (cond("if0") /*comment*/) do_something("same-line");

if (cond("if1") /*comment*/)
  do_something("next-line");

  if (!1) return;
  if (!2) { return; }

  if (!3)
    return;

  if (!4) {
    return;
  }
}

void foo() {
if (1) while (2) if (3) for (;;) do ; while(false) /**/;/**/
}

void f() {}

void foo2() {
  constexpr bool a = true;
  if constexpr (a) {
    f();
  } else {
    f();
  }
}
