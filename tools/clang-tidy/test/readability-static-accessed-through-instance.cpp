struct C {
  static int x;
};

int C::x = 0;

// Expressions with side effects
C &f(int, int, int, int);
void g() {
  f(1, 2, 3, 4).x;
}
