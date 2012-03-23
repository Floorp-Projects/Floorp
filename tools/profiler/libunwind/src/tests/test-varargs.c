#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern int backtrace (void **, int);

static void
b (void)
{
  void *v[20];
  int i, n;

  n = backtrace(v, 20);
  for (i = 0; i < n; ++i)
    printf ("[%d] %p\n", i, v[i]);
}

static void
c (void)
{
    b ();
}

static void
a (int d, ...)
{
  switch (d)
    {
    case 5:
      a (4, 2,4);
      break;
    case 4:
      a (3, 1,3,5);
      break;
    case 3:
      a (2, 11, 13, 17, 23);
      break;
    case 2:
      a (1);
      break;
    case 1:
      c ();
    }
}

int
main (int argc, char **argv)
{
  a (5, 3, 4, 5, 6);
  return 0;
}
