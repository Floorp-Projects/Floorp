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
#include "mozilla/Telemetry.h"

namespace mozilla {
namespace net {

struct HuffmanIncomingTable;

void Http2CompressionCleanup();

class nvPair {
 public:
  nvPair(const nsACString& name, const nsACString& value)
      : mName(name), mValue(value) {}

  uint32_t Size() const { return mName.Length() + mValue.Length() + 32; }
  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

  nsCString mName;
  nsCString mValue;
};

class nvFIFO {
 public:
  nvFIFO();
  ~nvFIFO();
  void AddElement(const nsCString& name, const nsCString& value);
  void AddElement(const nsCString& name);
  void RemoveElement();
  uint32_t ByteCount() const;
  uint32_t Length() const;
  uint32_t VariableLength() const;
  size_t StaticLength() const;
  void Clear();
  const nvPair* operator[](size_t index) const;

 private:
  uint32_t mByteCount{0};
  nsDeque<nvPair> mTable;
};

class HpackDynamicTableReporter;

class Http2BaseCompressor {
 public:
  Http2BaseCompressor();
  virtual ~Http2BaseCompressor();
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
  nsresult SetInitialMaxBufferSize(uint32_t maxBufferSize);
  void SetDumpTables(bool dumpTables);

 protected:
  const static uint32_t kDefaultMaxBuffer = 4096;

  virtual void ClearHeaderTable();
  virtual void MakeRoom(uint32_t amount, const char* direction);
  virtual void DumpState(const char*);
  virtual void SetMaxBufferSizeInternal(uint32_t maxBufferSize);

  nsACString* mOutput{nullptr};
  nvFIFO mHeaderTable;

  uint32_t mMaxBuffer{kDefaultMaxBuffer};
  uint32_t mMaxBufferSetting{kDefaultMaxBuffer};
  bool mSetInitialMaxBufferSizeAllowed{true};

  uint32_t mPeakSize{0};
  uint32_t mPeakCount{0};
  MOZ_INIT_OUTSIDE_CTOR
  Telemetry::HistogramID mPeakSizeID;
  MOZ_INIT_OUTSIDE_CTOR
  Telemetry::HistogramID mPeakCountID;

  bool mDumpTables{false};

 private:
  RefPtr<HpackDynamicTableReporter> mDynamicReporter;
};

class Http2Compressor;

class Http2Decompressor final : public Http2BaseCompressor {
 public:
  Http2Decompressor() {
    mPeakSizeID = Telemetry::HPACK_PEAK_SIZE_DECOMPRESSOR;
    mPeakCountID = Telemetry::HPACK_PEAK_COUNT_DECOMPRESSOR;
  };
  virtual ~Http2Decompressor() = default;

  // NS_OK: Produces the working set of HTTP/1 formatted headers
  [[nodiscard]] nsresult DecodeHeaderBlock(const uint8_t* data,
                                           uint32_t datalen, nsACString& output,
                                           bool isPush);

  void GetStatus(nsACString& hdr) { hdr = mHeaderStatus; }
  void GetHost(nsACString& hdr) { hdr = mHeaderHost; }
  void GetScheme(nsACString& hdr) { hdr = mHeaderScheme; }
  void GetPath(nsACString& hdr) { hdr = mHeaderPath; }
  void GetMethod(nsACString& hdr) { hdr = mHeaderMethod; }

 private:
  [[nodiscard]] nsresult DoIndexed();
  [[nodiscard]] nsresult DoLiteralWithoutIndex();
  [[nodiscard]] nsresult DoLiteralWithIncremental();
  [[nodiscard]] nsresult DoLiteralInternal(nsACString&, nsACString&, uint32_t);
  [[nodiscard]] nsresult DoLiteralNeverIndexed();
  [[nodiscard]] nsresult DoContextUpdate();

  [[nodiscard]] nsresult DecodeInteger(uint32_t prefixLen, uint32_t& accum);
  [[nodiscard]] nsresult OutputHeader(uint32_t index);
  [[nodiscard]] nsresult OutputHeader(const nsACString& name,
                                      const nsACString& value);

  [[nodiscard]] nsresult CopyHeaderString(uint32_t index, nsACString& name);
  [[nodiscard]] nsresult CopyStringFromInput(uint32_t bytes, nsACString& val);
  uint8_t ExtractByte(uint8_t bitsLeft, uint32_t& bytesConsumed);
  [[nodiscard]] nsresult CopyHuffmanStringFromInput(uint32_t bytes,
                                                    nsACString& val);
  [[nodiscard]] nsresult DecodeHuffmanCharacter(
      const HuffmanIncomingTable* table, uint8_t& c, uint32_t& bytesConsumed,
      uint8_t& bitsLeft);
  [[nodiscard]] nsresult DecodeFinalHuffmanCharacter(
      const HuffmanIncomingTable* table, uint8_t& c, uint8_t& bitsLeft);

  nsCString mHeaderStatus;
  nsCString mHeaderHost;
  nsCString mHeaderScheme;
  nsCString mHeaderPath;
  nsCString mHeaderMethod;

  // state variables when DecodeBlock() is on the stack
  uint32_t mOffset{0};
  const uint8_t* mData{nullptr};
  uint32_t mDataLen{0};
  bool mSeenNonColonHeader{false};
  bool mIsPush{false};
};

class Http2Compressor final : public Http2BaseCompressor {
 public:
  Http2Compressor() {
    mPeakSizeID = Telemetry::HPACK_PEAK_SIZE_COMPRESSOR;
    mPeakCountID = Telemetry::HPACK_PEAK_COUNT_COMPRESSOR;
  };
  virtual ~Http2Compressor() = default;

  // HTTP/1 formatted header block as input - HTTP/2 formatted
  // header block as output
  [[nodiscard]] nsresult EncodeHeaderBlock(
      const nsCString& nvInput, const nsACString& method,
      const nsACString& path, const nsACString& host, const nsACString& scheme,
      const nsACString& protocol, bool simpleConnectForm, nsACString& output);

  int64_t GetParsedContentLength() {
    return mParsedContentLength;
  }  // -1 on not found

  void SetMaxBufferSize(uint32_t maxBufferSize);

 private:
  enum outputCode {
    kNeverIndexedLiteral,
    kPlainLiteral,
    kIndexedLiteral,
    kIndex
  };

  void DoOutput(Http2Compressor::outputCode code, const class nvPair* pair,
                uint32_t index);
  void EncodeInteger(uint32_t prefixLen, uint32_t val);
  void ProcessHeader(const nvPair inputPair, bool noLocalIndex,
                     bool neverIndex);
  void HuffmanAppend(const nsCString& value);
  void EncodeTableSizeChange(uint32_t newMaxSize);

  int64_t mParsedContentLength{-1};
  bool mBufferSizeChangeWaiting{false};
  uint32_t mLowestBufferSizeWaiting{0};
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_Http2Compression_Internal_h
