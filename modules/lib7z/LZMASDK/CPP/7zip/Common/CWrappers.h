// CWrappers.h

#ifndef __C_WRAPPERS_H
#define __C_WRAPPERS_H

#include "../../../C/Types.h"

#include "../ICoder.h"
#include "../../Common/MyCom.h"

struct CCompressProgressWrap
{
  ICompressProgress p;
  ICompressProgressInfo *Progress;
  HRESULT Res;
  CCompressProgressWrap(ICompressProgressInfo *progress);
};

struct CSeqInStreamWrap
{
  ISeqInStream p;
  ISequentialInStream *Stream;
  HRESULT Res;
  CSeqInStreamWrap(ISequentialInStream *stream);
};

struct CSeekInStreamWrap
{
  ISeekInStream p;
  IInStream *Stream;
  HRESULT Res;
  CSeekInStreamWrap(IInStream *stream);
};

struct CSeqOutStreamWrap
{
  ISeqOutStream p;
  ISequentialOutStream *Stream;
  HRESULT Res;
  UInt64 Processed;
  CSeqOutStreamWrap(ISequentialOutStream *stream);
};

HRESULT SResToHRESULT(SRes res);

#endif
