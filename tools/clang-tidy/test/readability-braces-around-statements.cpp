
void do_something(const char *) {}

bool cond(const char *) {
  return false;
}

void test() {
if (cond("if0") /*comment*/) do_something("same-line");
}

void foo() {
if (1) while (2) if (3) for (;;) do ; while(false) /**/;/**/
}
