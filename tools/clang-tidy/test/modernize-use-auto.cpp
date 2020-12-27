#include <vector>

void func() {
  int val = 42;
  std::vector<int> my_container;
  for (std::vector<int>::iterator I = my_container.begin(),
                                  E = my_container.end();
       I != E;
       ++I) {
  }
}