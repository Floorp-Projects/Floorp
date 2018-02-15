// Crypto/Sha1.h

#ifndef __CRYPTO_SHA1_H
#define __CRYPTO_SHA1_H

#include "../../../C/Sha1.h"

namespace NCrypto {
namespace NSha1 {

const unsigned kNumBlockWords = SHA1_NUM_BLOCK_WORDS;
const unsigned kNumDigestWords = SHA1_NUM_DIGEST_WORDS;

const unsigned kBlockSize = SHA1_BLOCK_SIZE;
const unsigned kDigestSize = SHA1_DIGEST_SIZE;

class CContextBase
{
protected:
  CSha1 _s;
 
public:
  void Init() throw() { Sha1_Init(&_s); }
  void GetBlockDigest(const UInt32 *blockData, UInt32 *destDigest) throw() { Sha1_GetBlockDigest(&_s, blockData, destDigest); }
};

class CContext: public CContextBase
{
public:
  void Update(const Byte *data, size_t size) throw() { Sha1_Update(&_s, data, size); }
  void UpdateRar(Byte *data, size_t size /* , bool rar350Mode */) throw() { Sha1_Update_Rar(&_s, data, size /* , rar350Mode ? 1 : 0 */); }
  void Final(Byte *digest) throw() { Sha1_Final(&_s, digest); }
};

class CContext32: public CContextBase
{
public:
  void Update(const UInt32 *data, size_t size) throw() { Sha1_32_Update(&_s, data, size); }
  void Final(UInt32 *digest) throw() { Sha1_32_Final(&_s, digest); }
  
  /* PrepareBlock can be used only when size <= 13. size in Words
     _buffer must be empty (_count & 0xF) == 0) */
  void PrepareBlock(UInt32 *block, unsigned size) const throw()
  {
    Sha1_32_PrepareBlock(&_s, block, size);
  }
};

}}

#endif
