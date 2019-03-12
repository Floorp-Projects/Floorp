
#include "FuzzingInterface.h"

namespace dav1dfuzz {
#include "tests/libfuzzer/dav1d_fuzzer.c"
}

MOZ_FUZZING_INTERFACE_RAW(nullptr, dav1dfuzz::LLVMFuzzerTestOneInput,
                          Dav1dDecode);
