#include "structures.h"

int add(int x, int y) { return x + y; }
void f_bind() {
    auto clj = std::bind(add, 2, 2);
}
