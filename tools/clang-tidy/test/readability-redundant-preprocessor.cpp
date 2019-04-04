#ifndef FOO
#ifdef FOO // inner ifdef is considered redundant
void f();
#endif
#endif