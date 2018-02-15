// HmacSha256.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "HmacSha256.h"

namespace NCrypto {
namespace NSha256 {

static const unsigned kBlockSize = 64;

void CHmac::SetKey(const Byte *key, size_t keySize)
{
  Byte temp[kBlockSize];
  size_t i;
  
  for (i = 0; i < kBlockSize; i++)
    temp[i] = 0;
  
  if (keySize > kBlockSize)
  {
    Sha256_Init(&_sha);
    Sha256_Update(&_sha, key, keySize);
    Sha256_Final(&_sha, temp);
  }
  else
    for (i = 0; i < keySize; i++)
      temp[i] = key[i];
  
  for (i = 0; i < kBlockSize; i++)
    temp[i] ^= 0x36;
  
  Sha256_Init(&_sha);
  Sha256_Update(&_sha, temp, kBlockSize);
  
  for (i = 0; i < kBlockSize; i++)
    temp[i] ^= 0x36 ^ 0x5C;

  Sha256_Init(&_sha2);
  Sha256_Update(&_sha2, temp, kBlockSize);
}

void CHmac::Final(Byte *mac)
{
  Sha256_Final(&_sha, mac);
  Sha256_Update(&_sha2, mac, SHA256_DIGEST_SIZE);
  Sha256_Final(&_sha2, mac);
}

/*
void CHmac::Final(Byte *mac, size_t macSize)
{
  Byte digest[SHA256_DIGEST_SIZE];
  Final(digest);
  for (size_t i = 0; i < macSize; i++)
    mac[i] = digest[i];
}
*/

}}
