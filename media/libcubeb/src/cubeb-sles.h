#ifndef _CUBEB_SLES_H_
#define _CUBEB_SLES_H_
#include <OpenSLESProvider.h>
#include <SLES/OpenSLES.h>

static SLresult cubeb_get_sles_engine(
  SLObjectItf             *pEngine,
  SLuint32                numOptions,
  const SLEngineOption    *pEngineOptions,
  SLuint32                numInterfaces,
  const SLInterfaceID     *pInterfaceIds,
  const SLboolean         * pInterfaceRequired) {
  return mozilla_get_sles_engine(pEngine, numOptions, pEngineOptions);
}

static void cubeb_destroy_sles_engine(SLObjectItf *self) {
 mozilla_destroy_sles_engine(self);
}

#endif
