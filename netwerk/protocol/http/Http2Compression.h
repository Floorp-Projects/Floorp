/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_Http2Compression_Internal_h
#define mozilla_net_Http2Compression_Internal_h

// HPACK - RFC 7541
// https://www.rfc-editor.org/rfc/rfc7541.txt

#include "mozilla/Attributes.h"
#include "nsDeque.h"
#include "nsString.h"
#include "nsIMemoryReporter.h"

namespace mozilla {
namespace net {

struct HuffmanIncomingTable;

void Http2CompressionCleanup();

class nvPair
{
public:
nvPair(const nsACString &name, const nsACString &value)
  : mName(name)
  , mValue(value)
  { }

  uint32_t Size() const { return mName.Length() + mValue.Length() + 32; }
  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

  nsCString mName;
  nsCString mValue;
};

class nvFIFO
{
public:
  nvFIFO();
  ~nvFIFO();
  void AddElement(const nsCString &name, const nsCString &value);
  void AddElement(const nsCString &name);
  void RemoveElement();
  uint32_t ByteCount() const;
  uint32_t Length() const;
  uint32_t VariableLength() const;
  uint32_t StaticLength() const;
  void Clear();
  const nvPair *operator[] (int32_t index) const;

private:
  uint32_t mByteCount;
  nsDeque  mTable;
};

class HpackDynamicTableReporter;

class Http2BaseCompressor
{
public:
  Http2BaseCompressor();
  virtual ~Http2BaseCompressor();
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

protected:
  const static uint32_t kDefaultMaxBuffer = 4096;

  virtual void ClearHeaderTable();
  virtual void MakeRoom(uint32_t amount, const char *direction);
  virtual void DumpState();

  nsACString *mOutput;
  nvFIFO mHeaderTable;

  uint32_t mMaxBuffer;

private:
  nsRefPtr<HpackDynamicTableReporter> mDynamicReporter;
};

class Http2Compressor;

class Http2Decompressor final : public Http2BaseCompressor
{
public:
  Http2Decompressor() { };
  virtual ~Http2Decompressor() { } ;

  // NS_OK: Produces the working set of HTTP/1 formatted headers
  nsresult DecodeHeaderBlock(const uint8_t *data, uint32_t datalen,
                             nsACString &output, bool isPush);

  void GetStatus(nsACString &hdr) { hdr = mHeaderStatus; }
  void GetHost(nsACString &hdr) { hdr = mHeaderHost; }
  void GetScheme(nsACString &hdr) { hdr = mHeaderScheme; }
  void GetPath(nsACString &hdr) { hdr = mHeaderPath; }
  void GetMethod(nsACString &hdr) { hdr = mHeaderMethod; }
  void SetCompressor(Http2Compressor *compressor) { mCompressor = compressor; }

private:
  nsresult DoIndexed();
  nsresult DoLiteralWithoutIndex();
  nsresult DoLiteralWithIncremental();
  nsresult DoLiteralInternal(nsACString &, nsACString &, uint32_t);
  nsresult DoLiteralNeverIndexed();
  nsresult DoContextUpdate();

  nsresult DecodeInteger(uint32_t prefixLen, uint32_t &result);
  nsresult OutputHeader(uint32_t index);
  nsresult OutputHeader(const nsACString &name, const nsACString &value);

  nsresult CopyHeaderString(uint32_t index, nsACString &name);
  nsresult CopyStringFromInput(uint32_t index, nsACString &val);
  uint8_t ExtractByte(uint8_t bitsLeft, uint32_t &bytesConsumed);
  nsresult CopyHuffmanStringFromInput(uint32_t index, nsACString &val);
  nsresult DecodeHuffmanCharacter(HuffmanIncomingTable *table, uint8_t &c,
                                  uint32_t &bytesConsumed, uint8_t &bitsLeft);
  nsresult DecodeFinalHuffmanCharacter(HuffmanIncomingTable *table, uint8_t &c,
                                       uint8_t &bitsLeft);

  Http2Compressor *mCompressor;

  nsCString mHeaderStatus;
  nsCString mHeaderHost;
  nsCString mHeaderScheme;
  nsCString mHeaderPath;
  nsCString mHeaderMethod;

  // state variables when DecodeBlock() is on the stack
  uint32_t mOffset;
  const uint8_t *mData;
  uint32_t mDataLen;
  bool mSeenNonColonHeader;
  bool mIsPush;
};


class Http2Compressor final : public Http2BaseCompressor
{
public:
  Http2Compressor() : mParsedContentLength(-1),
                      mMaxBufferSetting(kDefaultMaxBuffer),
                      mBufferSizeChangeWaiting(false),
                      mLowestBufferSizeWaiting(0)
  { };
  virtual ~Http2Compressor() { }

  // HTTP/1 formatted header block as input - HTTP/2 formatted
  // header block as output
  nsresult EncodeHeaderBlock(const nsCString &nvInput,
                             const nsACString &method, const nsACString &path,
                             const nsACString &host, const nsACString &scheme,
                             bool connectForm, nsACString &output);

  int64_t GetParsedContentLength() { return mParsedContentLength; } // -1 on not found

  void SetMaxBufferSize(uint32_t maxBufferSize);
  nsresult SetMaxBufferSizeInternal(uint32_t maxBufferSize);

private:
  enum outputCode {
    kNeverIndexedLiteral,
    kPlainLiteral,
    kIndexedLiteral,
    kIndex
  };

  void DoOutput(Http2Compressor::outputCode code,
                const class nvPair *pair, uint32_t index);
  void EncodeInteger(uint32_t prefixLen, uint32_t val);
  void ProcessHeader(const nvPair inputPair, bool noLocalIndex,
                     bool neverIndex);
  void HuffmanAppend(const nsCString &value);
  void EncodeTableSizeChange(uint32_t newMaxSize);

  int64_t mParsedContentLength;
  uint32_t mMaxBufferSetting;
  bool mBufferSizeChangeWaiting;
  uint32_t mLowestBufferSizeWaiting;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_Http2Compression_Internal_h
