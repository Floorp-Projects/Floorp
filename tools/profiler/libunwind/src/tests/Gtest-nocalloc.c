/* libunwind - a platform-independent unwind library
   Copyright (C) 2011 Google, Inc
	Contributed by Paul Pluzhnikov <ppluzhnikov@google.com>

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <execinfo.h>  /* for backtrace  */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <pthread.h>
#include <libunwind.h>

#define panic(args...)				\
	{ fprintf (stderr, args); exit (-1); }

int verbose;
int num_mallocs;
int num_callocs;
int in_unwind;

void *
calloc(size_t n, size_t s)
{
  static void * (*func)();

#ifdef __GLIBC__
  /* In glibc, dlsym() calls calloc. Calling dlsym(RTLD_NEXT, "calloc") here
     causes infinite recursion.  Instead, we simply use it by its other
     name.  */
  extern void *__libc_calloc();
  func = &__libc_calloc;
#else
  if(!func)
    func = (void *(*)()) dlsym(RTLD_NEXT, "calloc");
#endif

  if (in_unwind) {
    num_callocs++;
    return NULL;
  } else {
    return func(n, s);
  }
}

void *
malloc(size_t s)
{
  static void * (*func)();

  if(!func)
    func = (void *(*)()) dlsym(RTLD_NEXT, "malloc");

  if (in_unwind) {
    num_mallocs++;
    return NULL;
  } else {
    return func(s);
  }
}

static void
do_backtrace (void)
{
  const int num_levels = 100;
  void *pc[num_levels];

  in_unwind = 1;
  backtrace(pc, num_levels);
  in_unwind = 0;
}

void
foo3 ()
{
  do_backtrace ();
}

void
foo2 ()
{
  foo3 ();
}

void
foo1 (void)
{
  foo2 ();
  return NULL;
}

int
main (int argc, char **argv)
{
  int i, num_errors;

  /* Create (and leak) 100 TSDs, then call backtrace()
     and check that it doesn't call malloc()/calloc().  */
  for (i = 0; i < 100; ++i) {
    pthread_key_t key;
    if (pthread_key_create (&key, NULL))
      panic ("FAILURE: unable to create key %d\n", i);
  }
  foo1 ();
  num_errors = num_mallocs + num_callocs;
  if (num_errors > 0)
    {
      fprintf (stderr,
	       "FAILURE: detected %d error%s (malloc: %d, calloc: %d)\n",
	       num_errors, num_errors > 1 ? "s" : "",
	       num_mallocs, num_callocs);
      exit (-1);
    }
  return 0;
}
