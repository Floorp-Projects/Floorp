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
#include "mozilla/Telemetry.h"

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
  size_t StaticLength() const;
  void Clear();
  const nvPair *operator[] (size_t index) const;

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
  nsresult SetInitialMaxBufferSize(uint32_t maxBufferSize);

protected:
  const static uint32_t kDefaultMaxBuffer = 4096;

  virtual void ClearHeaderTable();
  virtual void MakeRoom(uint32_t amount, const char *direction);
  virtual void DumpState();
  virtual void SetMaxBufferSizeInternal(uint32_t maxBufferSize);

  nsACString *mOutput;
  nvFIFO mHeaderTable;

  uint32_t mMaxBuffer;
  uint32_t mMaxBufferSetting;
  bool mSetInitialMaxBufferSizeAllowed;

  uint32_t mPeakSize;
  uint32_t mPeakCount;
  MOZ_INIT_OUTSIDE_CTOR
  Telemetry::HistogramID mPeakSizeID;
  MOZ_INIT_OUTSIDE_CTOR
  Telemetry::HistogramID mPeakCountID;

private:
  RefPtr<HpackDynamicTableReporter> mDynamicReporter;
};

class Http2Compressor;

class Http2Decompressor final : public Http2BaseCompressor
{
public:
  Http2Decompressor()
  {
    mPeakSizeID = Telemetry::HPACK_PEAK_SIZE_DECOMPRESSOR;
    mPeakCountID = Telemetry::HPACK_PEAK_COUNT_DECOMPRESSOR;
  };
  virtual ~Http2Decompressor() { } ;

  // NS_OK: Produces the working set of HTTP/1 formatted headers
  MOZ_MUST_USE nsresult DecodeHeaderBlock(const uint8_t *data,
                                          uint32_t datalen, nsACString &output,
                                          bool isPush);

  void GetStatus(nsACString &hdr) { hdr = mHeaderStatus; }
  void GetHost(nsACString &hdr) { hdr = mHeaderHost; }
  void GetScheme(nsACString &hdr) { hdr = mHeaderScheme; }
  void GetPath(nsACString &hdr) { hdr = mHeaderPath; }
  void GetMethod(nsACString &hdr) { hdr = mHeaderMethod; }

private:
  MOZ_MUST_USE nsresult DoIndexed();
  MOZ_MUST_USE nsresult DoLiteralWithoutIndex();
  MOZ_MUST_USE nsresult DoLiteralWithIncremental();
  MOZ_MUST_USE nsresult DoLiteralInternal(nsACString &, nsACString &, uint32_t);
  MOZ_MUST_USE nsresult DoLiteralNeverIndexed();
  MOZ_MUST_USE nsresult DoContextUpdate();

  MOZ_MUST_USE nsresult DecodeInteger(uint32_t prefixLen, uint32_t &result);
  MOZ_MUST_USE nsresult OutputHeader(uint32_t index);
  MOZ_MUST_USE nsresult OutputHeader(const nsACString &name, const nsACString &value);

  MOZ_MUST_USE nsresult CopyHeaderString(uint32_t index, nsACString &name);
  MOZ_MUST_USE nsresult CopyStringFromInput(uint32_t index, nsACString &val);
  uint8_t ExtractByte(uint8_t bitsLeft, uint32_t &bytesConsumed);
  MOZ_MUST_USE nsresult CopyHuffmanStringFromInput(uint32_t index, nsACString &val);
  MOZ_MUST_USE nsresult
  DecodeHuffmanCharacter(const HuffmanIncomingTable *table, uint8_t &c,
                         uint32_t &bytesConsumed, uint8_t &bitsLeft);
  MOZ_MUST_USE nsresult
  DecodeFinalHuffmanCharacter(const HuffmanIncomingTable *table, uint8_t &c,
                              uint8_t &bitsLeft);

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
                      mBufferSizeChangeWaiting(false),
                      mLowestBufferSizeWaiting(0)
  {
    mPeakSizeID = Telemetry::HPACK_PEAK_SIZE_COMPRESSOR;
    mPeakCountID = Telemetry::HPACK_PEAK_COUNT_COMPRESSOR;
  };
  virtual ~Http2Compressor() { }

  // HTTP/1 formatted header block as input - HTTP/2 formatted
  // header block as output
  MOZ_MUST_USE nsresult EncodeHeaderBlock(const nsCString &nvInput,
                                          const nsACString &method,
                                          const nsACString &path,
                                          const nsACString &host,
                                          const nsACString &scheme,
                                          bool connectForm,
                                          nsACString &output);

  int64_t GetParsedContentLength() { return mParsedContentLength; } // -1 on not found

  void SetMaxBufferSize(uint32_t maxBufferSize);

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
  bool mBufferSizeChangeWaiting;
  uint32_t mLowestBufferSizeWaiting;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_Http2Compression_Internal_h
