
#include "FuzzingInterface.h"

#define LLVMFuzzerInitialize Dav1dFuzzerInitialize
#define LLVMFuzzerTestOneInput Dav1dFuzzerTestOneInput

extern "C" {
#include "tests/libfuzzer/dav1d_fuzzer.c"
}

#undef LLVMFuzzerInitialize
#undef LLVMFuzzerTestOneInput

MOZ_FUZZING_INTERFACE_RAW(Dav1dFuzzerInitialize, Dav1dFuzzerTestOneInput,
                          Dav1dDecode);
