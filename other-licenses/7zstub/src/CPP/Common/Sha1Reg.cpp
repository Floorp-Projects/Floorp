// Sha1Reg.cpp

#include "StdAfx.h"

#include "../../C/Sha1.h"

#include "../Common/MyCom.h"

#include "../7zip/Common/RegisterCodec.h"

class CSha1Hasher:
  public IHasher,
  public CMyUnknownImp
{
  CSha1 _sha;
  Byte mtDummy[1 << 7];
  
public:
  CSha1Hasher() { Sha1_Init(&_sha); }

  MY_UNKNOWN_IMP1(IHasher)
  INTERFACE_IHasher(;)
};

STDMETHODIMP_(void) CSha1Hasher::Init() throw()
{
  Sha1_Init(&_sha);
}

STDMETHODIMP_(void) CSha1Hasher::Update(const void *data, UInt32 size) throw()
{
  Sha1_Update(&_sha, (const Byte *)data, size);
}

STDMETHODIMP_(void) CSha1Hasher::Final(Byte *digest) throw()
{
  Sha1_Final(&_sha, digest);
}

REGISTER_HASHER(CSha1Hasher, 0x201, "SHA1", SHA1_DIGEST_SIZE)
