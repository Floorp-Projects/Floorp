#if defined (__SSE__)
  #include <immintrin.h>
  #if defined (__clang__)
    #include <avxintrin.h>
    #include <avx2intrin.h>
    #include <fmaintrin.h>
  #endif
#endif
