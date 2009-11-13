// MethodProps.h

#ifndef __7Z_METHOD_PROPS_H
#define __7Z_METHOD_PROPS_H

#include "../../Common/MyVector.h"

#include "../../Windows/PropVariant.h"

#include "MethodId.h"

struct CProp
{
  PROPID Id;
  NWindows::NCOM::CPropVariant Value;
};

struct CMethod
{
  CMethodId Id;
  CObjectVector<CProp> Props;
};

struct CMethodsMode
{
  CObjectVector<CMethod> Methods;
  #ifdef COMPRESS_MT
  UInt32 NumThreads;
  #endif

  CMethodsMode()
      #ifdef COMPRESS_MT
      : NumThreads(1)
      #endif
  {}
  bool IsEmpty() const { return Methods.IsEmpty() ; }
};

HRESULT SetMethodProperties(const CMethod &method, const UInt64 *inSizeForReduce, IUnknown *coder);

#endif
