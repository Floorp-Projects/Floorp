#define ZIP(p, er) p = er;
#include "test.h"
#include "case/include/file.h"
#include "case/file.h"
#ifdef FOO
#else
#endif
#pragma warn
#error
#if FOO && BAR
#else if 0
#if defined(CUPID)
#endif

static int number = 1;
const char[] regexp = "/this/";
volatile char* regexpi = "/InSensitive/i";
/* simple C style comment */
(void) 1;
(void)3;
const char constant = '\3';
char single_quoted_string = 's';
char* double_quoted_string = "doubly quoted string";
char** array = {"foo", "bar"};
int* sparse_array = {1, , };

typedef int * foo;
struct X {
  int y, z;
  union w {
    char p;
    double u;
  };
  long:20 t:
};

typedef X XX;

void foo(void* argument, int* argument2, char &argument3,

bool argument4) {
  if (condition) {
    expression;
  } else if (!condition2) {
    expression2;
    expression3;
  } else {
    if (other && conditions) {
      !expression;
    }
  }
}

int exception() {
  try {
    throw 1;
  } catch (e) {
    do_something;
  } finally {
    return something_else;
  }
  return not_reached;
}

char **array3 = new char*[4];
array3[0] = "a";
array3[1] = "big";
array3[2] = "bird";
array3[3] = "can't" " fly";

int* reserved_words() {
  {
    for (int i = 0; i < 2; ++i);
    do { nothing; } while (false);
    if (!true || !!'' && " " | 3 & 7);
    while (false | true & false) nothing;
    int a[5];
    a[3] = 0;
    A qq;
    qq.re = RegExp("2");
  }
  return(NULL);
}

void not_reserved_words() {
  try();
  throw();
  catch();
  finally();
  returnThis();
  var();
  constThis();
  new();
  voidThis();
  ifThis();
  elseThis();
  elsif;
}
