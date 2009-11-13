// LzmaBenchCon.h

#ifndef __LZMABENCHCON_H
#define __LZMABENCHCON_H

#include <stdio.h>
#include "../../../Common/Types.h"
#ifdef EXTERNAL_LZMA
#include "../../UI/Common/LoadCodecs.h"
#endif
HRESULT LzmaBenchCon(
  #ifdef EXTERNAL_LZMA
  CCodecs *codecs,
  #endif
  FILE *f, UInt32 numIterations, UInt32 numThreads, UInt32 dictionary);

HRESULT CrcBenchCon(FILE *f, UInt32 numIterations, UInt32 numThreads, UInt32 dictionary);

#endif

