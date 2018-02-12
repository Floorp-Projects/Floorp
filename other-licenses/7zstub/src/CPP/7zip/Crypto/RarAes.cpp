// Crypto/RarAes.cpp

#include "StdAfx.h"

#include "RarAes.h"
#include "Sha1Cls.h"

namespace NCrypto {
namespace NRar3 {

CDecoder::CDecoder():
    CAesCbcDecoder(kAesKeySize),
    _thereIsSalt(false),
    _needCalc(true)
    // _rar350Mode(false)
{
  for (unsigned i = 0; i < sizeof(_salt); i++)
    _salt[i] = 0;
}

HRESULT CDecoder::SetDecoderProperties2(const Byte *data, UInt32 size)
{
  bool prev = _thereIsSalt;
  _thereIsSalt = false;
  if (size == 0)
  {
    if (!_needCalc && prev)
      _needCalc = true;
    return S_OK;
  }
  if (size < 8)
    return E_INVALIDARG;
  _thereIsSalt = true;
  bool same = false;
  if (_thereIsSalt == prev)
  {
    same = true;
    if (_thereIsSalt)
    {
      for (unsigned i = 0; i < sizeof(_salt); i++)
        if (_salt[i] != data[i])
        {
          same = false;
          break;
        }
    }
  }
  for (unsigned i = 0; i < sizeof(_salt); i++)
    _salt[i] = data[i];
  if (!_needCalc && !same)
    _needCalc = true;
  return S_OK;
}

static const unsigned kPasswordLen_Bytes_MAX = 127 * 2;

void CDecoder::SetPassword(const Byte *data, unsigned size)
{
  if (size > kPasswordLen_Bytes_MAX)
    size = kPasswordLen_Bytes_MAX;
  bool same = false;
  if (size == _password.Size())
  {
    same = true;
    for (UInt32 i = 0; i < size; i++)
      if (data[i] != _password[i])
      {
        same = false;
        break;
      }
  }
  if (!_needCalc && !same)
    _needCalc = true;
  _password.CopyFrom(data, (size_t)size);
}

STDMETHODIMP CDecoder::Init()
{
  CalcKey();
  RINOK(SetKey(_key, kAesKeySize));
  RINOK(SetInitVector(_iv, AES_BLOCK_SIZE));
  return CAesCbcCoder::Init();
}

void CDecoder::CalcKey()
{
  if (!_needCalc)
    return;

  const unsigned kSaltSize = 8;
  
  Byte buf[kPasswordLen_Bytes_MAX + kSaltSize];
  
  if (_password.Size() != 0)
    memcpy(buf, _password, _password.Size());
  
  size_t rawSize = _password.Size();
  
  if (_thereIsSalt)
  {
    memcpy(buf + rawSize, _salt, kSaltSize);
    rawSize += kSaltSize;
  }
  
  NSha1::CContext sha;
  sha.Init();
  
  Byte digest[NSha1::kDigestSize];
  // rar reverts hash for sha.
  const UInt32 kNumRounds = ((UInt32)1 << 18);
  UInt32 i;
  for (i = 0; i < kNumRounds; i++)
  {
    sha.UpdateRar(buf, rawSize /* , _rar350Mode */);
    Byte pswNum[3] = { (Byte)i, (Byte)(i >> 8), (Byte)(i >> 16) };
    sha.UpdateRar(pswNum, 3 /* , _rar350Mode */);
    if (i % (kNumRounds / 16) == 0)
    {
      NSha1::CContext shaTemp = sha;
      shaTemp.Final(digest);
      _iv[i / (kNumRounds / 16)] = (Byte)digest[4 * 4 + 3];
    }
  }
  
  sha.Final(digest);
  for (i = 0; i < 4; i++)
    for (unsigned j = 0; j < 4; j++)
      _key[i * 4 + j] = (digest[i * 4 + 3 - j]);
    
  _needCalc = false;
}

}}
