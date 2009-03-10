#include <iostream>
#include <string>

using namespace std;

int main(int argc, char* argv[]) {

  string test = "mozilla";

  if (test.compare(argv[1]) != 0)
      return -1;
  
  return 0;
} 
