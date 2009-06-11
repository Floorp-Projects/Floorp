#include <string.h>

int main(int argc, char* argv[]) {
  if (argc != 7)
      return -1;

  if (strcmp("mozilla", argv[1]) != 0)
      return 1;
  if (strcmp("firefox", argv[2]) != 0)
      return 2;
  if (strcmp("thunderbird", argv[3]) != 0)
      return 3;
  if (strcmp("seamonkey", argv[4]) != 0)
      return 4;
  if (strcmp("foo", argv[5]) != 0)
      return 5;
  if (strcmp("bar", argv[6]) != 0)
      return 6;
  
  return 0;
} 
