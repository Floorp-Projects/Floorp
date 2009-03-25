#include <iostream>
#include <string>

using namespace std;

int main(int argc, char* argv[]) {
  if (argc != 2)
      return -1;

  string test = "mozilla";

  if (test.compare(argv[1]) != 0)
      return -1;
  
  return 0;
} 
