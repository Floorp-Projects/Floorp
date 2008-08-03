#include <stdio.h>
#include "prenv.h"

int main(int argc, char* argv[])
{
  if (argc != 2)
    return 1;

  const char* value = PR_GetEnv("WRITE_ARGUMENT_FILE");

  if (!value)
    return 2;

  FILE* outfile = fopen(value, "w");
  if (!outfile)
    return 3;

  // We only need to write out the first argument (no newline).
  fputs(argv[argc -1], outfile);

  fclose(outfile);

  return 0;
}
