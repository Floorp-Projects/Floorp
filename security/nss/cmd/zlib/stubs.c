/*
  !!! WARNING: If you add files here, be sure to:
	1. add them to the Metrowerks project
	2. edit Makefile and makefile.win to add a dependency on the .c file
*/


/* XXX Mac */
#ifndef XP_MAC
#include "_stubs/java_util_zip_Adler32.c"
#include "_stubs/java_util_zip_CRC32.c"
#include "_stubs/java_util_zip_Deflater.c"
#include "_stubs/java_util_zip_Inflater.c"
PR_PUBLIC_API(void) _java_zlib_init(void) { }
#endif


