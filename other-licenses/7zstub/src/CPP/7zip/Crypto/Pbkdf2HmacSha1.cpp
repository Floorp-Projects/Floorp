// Pbkdf2HmacSha1.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "HmacSha1.h"

namespace NCrypto {
namespace NSha1 {

void Pbkdf2Hmac(const Byte *pwd, size_t pwdSize,
    const Byte *salt, size_t saltSize,
    UInt32 numIterations,
    Byte *key, size_t keySize)
{
  CHmac baseCtx;
  baseCtx.SetKey(pwd, pwdSize);
  
  for (UInt32 i = 1; keySize != 0; i++)
  {
    CHmac ctx = baseCtx;
    ctx.Update(salt, saltSize);
  
    Byte u[kDigestSize];
    SetBe32(u, i);
    
    ctx.Update(u, 4);
    ctx.Final(u, kDigestSize);

    const unsigned curSize = (keySize < kDigestSize) ? (unsigned)keySize : kDigestSize;
    unsigned s;
    for (s = 0; s < curSize; s++)
      key[s] = u[s];
    
    for (UInt32 j = numIterations; j > 1; j--)
    {
      ctx = baseCtx;
      ctx.Update(u, kDigestSize);
      ctx.Final(u, kDigestSize);
      for (s = 0; s < curSize; s++)
        key[s] ^= u[s];
    }

    key += curSize;
    keySize -= curSize;
  }
}

void Pbkdf2Hmac32(const Byte *pwd, size_t pwdSize,
    const UInt32 *salt, size_t saltSize,
    UInt32 numIterations,
    UInt32 *key, size_t keySize)
{
  CHmac32 baseCtx;
  baseCtx.SetKey(pwd, pwdSize);
  
  for (UInt32 i = 1; keySize != 0; i++)
  {
    CHmac32 ctx = baseCtx;
    ctx.Update(salt, saltSize);
    
    UInt32 u[kNumDigestWords];
    u[0] = i;
    
    ctx.Update(u, 1);
    ctx.Final(u, kNumDigestWords);

    // Speed-optimized code start
    ctx = baseCtx;
    ctx.GetLoopXorDigest(u, numIterations - 1);
    // Speed-optimized code end
    
    const unsigned curSize = (keySize < kNumDigestWords) ? (unsigned)keySize : kNumDigestWords;
    unsigned s;
    for (s = 0; s < curSize; s++)
      key[s] = u[s];
    
    /*
    // Default code start
    for (UInt32 j = numIterations; j > 1; j--)
    {
      ctx = baseCtx;
      ctx.Update(u, kNumDigestWords);
      ctx.Final(u, kNumDigestWords);
      for (s = 0; s < curSize; s++)
        key[s] ^= u[s];
    }
    // Default code end
    */

    key += curSize;
    keySize -= curSize;
  }
}

}}
