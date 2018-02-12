// Crypto/RarAes.h

#ifndef __CRYPTO_RAR_AES_H
#define __CRYPTO_RAR_AES_H

#include "../../../C/Aes.h"

#include "../../Common/MyBuffer.h"

#include "../IPassword.h"

#include "MyAes.h"

namespace NCrypto {
namespace NRar3 {

const unsigned kAesKeySize = 16;

class CDecoder:
  public CAesCbcDecoder
  // public ICompressSetDecoderProperties2,
  // public ICryptoSetPassword
{
  Byte _salt[8];
  bool _thereIsSalt;
  bool _needCalc;
  // bool _rar350Mode;
  
  CByteBuffer _password;
  
  Byte _key[kAesKeySize];
  Byte _iv[AES_BLOCK_SIZE];

  void CalcKey();
public:
  /*
  MY_UNKNOWN_IMP1(
    ICryptoSetPassword
    // ICompressSetDecoderProperties2
  */
  STDMETHOD(Init)();
  
  void SetPassword(const Byte *data, unsigned size);
  HRESULT SetDecoderProperties2(const Byte *data, UInt32 size);

  CDecoder();
  // void SetRar350Mode(bool rar350Mode) { _rar350Mode = rar350Mode; }
};

}}

#endif
