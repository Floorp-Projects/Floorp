/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "Http2Compression.h"
#include "Http2HuffmanIncoming.h"
#include "Http2HuffmanOutgoing.h"

extern PRThread *gSocketThread;

namespace mozilla {
namespace net {

static nsDeque *gStaticHeaders = nullptr;

void
Http2CompressionCleanup()
{
  // this happens after the socket thread has been destroyed
  delete gStaticHeaders;
  gStaticHeaders = nullptr;
}

static void
AddStaticElement(const nsCString &name, const nsCString &value)
{
  nvPair *pair = new nvPair(name, value);
  gStaticHeaders->Push(pair);
}

static void
AddStaticElement(const nsCString &name)
{
  AddStaticElement(name, EmptyCString());
}

static void
InitializeStaticHeaders()
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  if (!gStaticHeaders) {
    gStaticHeaders = new nsDeque();
    AddStaticElement(NS_LITERAL_CSTRING(":authority"));
    AddStaticElement(NS_LITERAL_CSTRING(":method"), NS_LITERAL_CSTRING("GET"));
    AddStaticElement(NS_LITERAL_CSTRING(":method"), NS_LITERAL_CSTRING("POST"));
    AddStaticElement(NS_LITERAL_CSTRING(":path"), NS_LITERAL_CSTRING("/"));
    AddStaticElement(NS_LITERAL_CSTRING(":path"), NS_LITERAL_CSTRING("/index.html"));
    AddStaticElement(NS_LITERAL_CSTRING(":scheme"), NS_LITERAL_CSTRING("http"));
    AddStaticElement(NS_LITERAL_CSTRING(":scheme"), NS_LITERAL_CSTRING("https"));
    AddStaticElement(NS_LITERAL_CSTRING(":status"), NS_LITERAL_CSTRING("200"));
    AddStaticElement(NS_LITERAL_CSTRING(":status"), NS_LITERAL_CSTRING("500"));
    AddStaticElement(NS_LITERAL_CSTRING(":status"), NS_LITERAL_CSTRING("404"));
    AddStaticElement(NS_LITERAL_CSTRING(":status"), NS_LITERAL_CSTRING("403"));
    AddStaticElement(NS_LITERAL_CSTRING(":status"), NS_LITERAL_CSTRING("400"));
    AddStaticElement(NS_LITERAL_CSTRING(":status"), NS_LITERAL_CSTRING("401"));
    AddStaticElement(NS_LITERAL_CSTRING("accept-charset"));
    AddStaticElement(NS_LITERAL_CSTRING("accept-encoding"));
    AddStaticElement(NS_LITERAL_CSTRING("accept-language"));
    AddStaticElement(NS_LITERAL_CSTRING("accept-ranges"));
    AddStaticElement(NS_LITERAL_CSTRING("accept"));
    AddStaticElement(NS_LITERAL_CSTRING("access-control-allow-origin"));
    AddStaticElement(NS_LITERAL_CSTRING("age"));
    AddStaticElement(NS_LITERAL_CSTRING("allow"));
    AddStaticElement(NS_LITERAL_CSTRING("authorization"));
    AddStaticElement(NS_LITERAL_CSTRING("cache-control"));
    AddStaticElement(NS_LITERAL_CSTRING("content-disposition"));
    AddStaticElement(NS_LITERAL_CSTRING("content-encoding"));
    AddStaticElement(NS_LITERAL_CSTRING("content-language"));
    AddStaticElement(NS_LITERAL_CSTRING("content-length"));
    AddStaticElement(NS_LITERAL_CSTRING("content-location"));
    AddStaticElement(NS_LITERAL_CSTRING("content-range"));
    AddStaticElement(NS_LITERAL_CSTRING("content-type"));
    AddStaticElement(NS_LITERAL_CSTRING("cookie"));
    AddStaticElement(NS_LITERAL_CSTRING("date"));
    AddStaticElement(NS_LITERAL_CSTRING("etag"));
    AddStaticElement(NS_LITERAL_CSTRING("expect"));
    AddStaticElement(NS_LITERAL_CSTRING("expires"));
    AddStaticElement(NS_LITERAL_CSTRING("from"));
    AddStaticElement(NS_LITERAL_CSTRING("if-match"));
    AddStaticElement(NS_LITERAL_CSTRING("if-modified-since"));
    AddStaticElement(NS_LITERAL_CSTRING("if-none-match"));
    AddStaticElement(NS_LITERAL_CSTRING("if-range"));
    AddStaticElement(NS_LITERAL_CSTRING("if-unmodified-since"));
    AddStaticElement(NS_LITERAL_CSTRING("last-modified"));
    AddStaticElement(NS_LITERAL_CSTRING("link"));
    AddStaticElement(NS_LITERAL_CSTRING("location"));
    AddStaticElement(NS_LITERAL_CSTRING("max-forwards"));
    AddStaticElement(NS_LITERAL_CSTRING("proxy-authenticate"));
    AddStaticElement(NS_LITERAL_CSTRING("proxy-authorization"));
    AddStaticElement(NS_LITERAL_CSTRING("range"));
    AddStaticElement(NS_LITERAL_CSTRING("referer"));
    AddStaticElement(NS_LITERAL_CSTRING("refresh"));
    AddStaticElement(NS_LITERAL_CSTRING("retry-after"));
    AddStaticElement(NS_LITERAL_CSTRING("server"));
    AddStaticElement(NS_LITERAL_CSTRING("set-cookie"));
    AddStaticElement(NS_LITERAL_CSTRING("strict-transport-security"));
    AddStaticElement(NS_LITERAL_CSTRING("transfer-encoding"));
    AddStaticElement(NS_LITERAL_CSTRING("user-agent"));
    AddStaticElement(NS_LITERAL_CSTRING("vary"));
    AddStaticElement(NS_LITERAL_CSTRING("via"));
    AddStaticElement(NS_LITERAL_CSTRING("www-authenticate"));
  }
}

nvFIFO::nvFIFO()
  : mByteCount(0)
  , mTable()
{
  InitializeStaticHeaders();
}

nvFIFO::~nvFIFO()
{
  Clear();
}

void
nvFIFO::AddElement(const nsCString &name, const nsCString &value)
{
  mByteCount += name.Length() + value.Length() + 32;
  nvPair *pair = new nvPair(name, value);
  mTable.PushFront(pair);
}

void
nvFIFO::AddElement(const nsCString &name)
{
  AddElement(name, EmptyCString());
}

void
nvFIFO::RemoveElement()
{
  nvPair *pair = static_cast<nvPair *>(mTable.Pop());
  if (pair) {
    mByteCount -= pair->Size();
    delete pair;
  }
}

uint32_t
nvFIFO::ByteCount() const
{
  return mByteCount;
}

uint32_t
nvFIFO::Length() const
{
  return mTable.GetSize() + gStaticHeaders->GetSize();
}

uint32_t
nvFIFO::VariableLength() const
{
  return mTable.GetSize();
}

void
nvFIFO::Clear()
{
  mByteCount = 0;
  while (mTable.GetSize())
    delete static_cast<nvPair *>(mTable.Pop());
}

const nvPair *
nvFIFO::operator[] (int32_t index) const
{
  if (index >= (mTable.GetSize() + gStaticHeaders->GetSize())) {
    MOZ_ASSERT(false);
    NS_WARNING("nvFIFO Table Out of Range");
    return nullptr;
  }
  if (index >= mTable.GetSize()) {
    return static_cast<nvPair *>(gStaticHeaders->ObjectAt(index - mTable.GetSize()));
  }
  return static_cast<nvPair *>(mTable.ObjectAt(index));
}

Http2BaseCompressor::Http2BaseCompressor()
  : mOutput(nullptr)
  , mMaxBuffer(kDefaultMaxBuffer)
{
}

void
Http2BaseCompressor::ClearHeaderTable()
{
  uint32_t dynamicCount = mHeaderTable.VariableLength();
  mHeaderTable.Clear();

  for (int32_t i = mReferenceSet.Length() - 1; i >= 0; --i) {
    if (mReferenceSet[i] < dynamicCount) {
      mReferenceSet.RemoveElementAt(i);
    } else {
      mReferenceSet[i] -= dynamicCount;
    }
  }

  for (int32_t i = mAlternateReferenceSet.Length() - 1; i >= 0; --i) {
    if (mAlternateReferenceSet[i] < dynamicCount) {
      mAlternateReferenceSet.RemoveElementAt(i);
    } else {
      mAlternateReferenceSet[i] -= dynamicCount;
    }
  }
}

void
Http2BaseCompressor::UpdateReferenceSet(int32_t delta)
{
  if (!delta)
    return;

  uint32_t headerTableSize = mHeaderTable.VariableLength();
  uint32_t oldHeaderTableSize = headerTableSize + delta;

  for (int32_t i = mReferenceSet.Length() - 1; i >= 0; --i) {
    uint32_t indexRef = mReferenceSet[i];
    if (indexRef >= headerTableSize) {
      if (indexRef < oldHeaderTableSize) {
        // This one got dropped
        LOG3(("HTTP base compressor reference to index %u removed.\n",
              indexRef));
        mReferenceSet.RemoveElementAt(i);
      } else {
        // This pointed to the static table, need to adjust
        uint32_t newRef = indexRef - delta;
        LOG3(("HTTP base compressor reference to index %u changed to %d (%s)\n",
              mReferenceSet[i], newRef, mHeaderTable[newRef]->mName.get()));
        mReferenceSet[i] = newRef;
      }
    }
  }

  for (int32_t i = mAlternateReferenceSet.Length() - 1; i >= 0; --i) {
    uint32_t indexRef = mAlternateReferenceSet[i];
    if (indexRef >= headerTableSize) {
      if (indexRef < oldHeaderTableSize) {
        // This one got dropped
        LOG3(("HTTP base compressor new reference to index %u removed.\n",
              indexRef));
        mAlternateReferenceSet.RemoveElementAt(i);
      } else {
        // This pointed to the static table, need to adjust
        uint32_t newRef = indexRef - delta;
        LOG3(("HTTP base compressor new reference to index %u changed to %d (%s)\n",
              mAlternateReferenceSet[i], newRef, mHeaderTable[newRef]->mName.get()));
        mAlternateReferenceSet[i] = newRef;
      }
    }
  }
}

void
Http2BaseCompressor::IncrementReferenceSetIndices()
{
  for (int32_t i = mReferenceSet.Length() - 1; i >= 0; --i) {
    mReferenceSet[i] = mReferenceSet[i] + 1;
  }

  for (int32_t i = mAlternateReferenceSet.Length() - 1; i >= 0; --i) {
    mAlternateReferenceSet[i] = mAlternateReferenceSet[i] + 1;
  }
}

nsresult
Http2Decompressor::DecodeHeaderBlock(const uint8_t *data, uint32_t datalen,
                                     nsACString &output)
{
  mAlternateReferenceSet.Clear();
  mOffset = 0;
  mData = data;
  mDataLen = datalen;
  mOutput = &output;
  mOutput->Truncate();
  mHeaderStatus.Truncate();
  mHeaderHost.Truncate();
  mHeaderScheme.Truncate();
  mHeaderPath.Truncate();
  mHeaderMethod.Truncate();

  nsresult rv = NS_OK;
  while (NS_SUCCEEDED(rv) && (mOffset < datalen)) {
    if (mData[mOffset] & 0x80) {
      rv = DoIndexed();
    } else if (mData[mOffset] & 0x40) {
      rv = DoLiteralWithoutIndex();
    } else {
      rv = DoLiteralWithIncremental();
    }
  }

  // after processing the input the decompressor comapres the alternate
  // set to the inherited reference set and generates headers for
  // anything implicit in reference - alternate.

  uint32_t setLen = mReferenceSet.Length();
  for (uint32_t index = 0; index < setLen; ++index) {
    if (!mAlternateReferenceSet.Contains(mReferenceSet[index])) {
      LOG3(("HTTP decompressor carryover in reference set with index %u %s %s\n",
            mReferenceSet[index],
            mHeaderTable[mReferenceSet[index]]->mName.get(),
            mHeaderTable[mReferenceSet[index]]->mValue.get()));
      OutputHeader(mReferenceSet[index]);
    }
  }

  mAlternateReferenceSet.Clear();
  return rv;
}

nsresult
Http2Decompressor::DecodeInteger(uint32_t prefixLen, uint32_t &accum)
{
  accum = 0;

  if (prefixLen) {
    uint32_t mask = (1 << prefixLen) - 1;

    accum = mData[mOffset] & mask;
    ++mOffset;

    if (accum != mask) {
      // the simple case for small values
      return NS_OK;
    }
  }

  uint32_t factor = 1; // 128 ^ 0

  // we need a series of bytes. The high bit signifies if we need another one.
  // The first one is a a factor of 128 ^ 0, the next 128 ^1, the next 128 ^2, ..

  if (mOffset >= mDataLen) {
    NS_WARNING("Ran out of data to decode integer");
    return NS_ERROR_ILLEGAL_VALUE;
  }
  bool chainBit = mData[mOffset] & 0x80;
  accum += (mData[mOffset] & 0x7f) * factor;

  ++mOffset;
  factor = factor * 128;

  while (chainBit) {
    // really big offsets are just trawling for overflows
    if (accum >= 0x800000) {
      NS_WARNING("Decoding integer >= 0x800000");
      return NS_ERROR_ILLEGAL_VALUE;
    }

    if (mOffset >= mDataLen) {
      NS_WARNING("Ran out of data to decode integer");
      return NS_ERROR_ILLEGAL_VALUE;
    }
    chainBit = mData[mOffset] & 0x80;
    accum += (mData[mOffset] & 0x7f) * factor;
    ++mOffset;
    factor = factor * 128;
  }
  return NS_OK;
}

nsresult
Http2Decompressor::OutputHeader(const nsACString &name, const nsACString &value)
{
    // exclusions
  if (name.Equals(NS_LITERAL_CSTRING("connection")) ||
      name.Equals(NS_LITERAL_CSTRING("host")) ||
      name.Equals(NS_LITERAL_CSTRING("keep-alive")) ||
      name.Equals(NS_LITERAL_CSTRING("proxy-connection")) ||
      name.Equals(NS_LITERAL_CSTRING("te")) ||
      name.Equals(NS_LITERAL_CSTRING("transfer-encoding")) ||
      name.Equals(NS_LITERAL_CSTRING("upgrade")) ||
      name.Equals(("accept-encoding"))) {
    nsCString toLog(name);
    LOG3(("HTTP Decompressor illegal response header found : %s",
          toLog.get()));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  // Look for upper case characters in the name.
  for (const char *cPtr = name.BeginReading();
       cPtr && cPtr < name.EndReading();
       ++cPtr) {
    if (*cPtr <= 'Z' && *cPtr >= 'A') {
      nsCString toLog(name);
      LOG3(("HTTP Decompressor upper case response header found. [%s]\n",
            toLog.get()));
      return NS_ERROR_ILLEGAL_VALUE;
    }
  }

  // Look for CR OR LF in value - could be smuggling Sec 10.3
  // can map to space safely
  for (const char *cPtr = value.BeginReading();
       cPtr && cPtr < value.EndReading();
       ++cPtr) {
    if (*cPtr == '\r' || *cPtr== '\n') {
      char *wPtr = const_cast<char *>(cPtr);
      *wPtr = ' ';
    }
  }

  // Status comes first
  if (name.Equals(NS_LITERAL_CSTRING(":status"))) {
    nsAutoCString status(NS_LITERAL_CSTRING("HTTP/1.1 "));
    status.Append(value);
    status.Append(NS_LITERAL_CSTRING("\r\n"));
    mOutput->Insert(status, 0);
    mHeaderStatus = value;
  } else if (name.Equals(NS_LITERAL_CSTRING(":authority"))) {
    mHeaderHost = value;
  } else if (name.Equals(NS_LITERAL_CSTRING(":scheme"))) {
    mHeaderScheme = value;
  } else if (name.Equals(NS_LITERAL_CSTRING(":path"))) {
    mHeaderPath = value;
  } else if (name.Equals(NS_LITERAL_CSTRING(":method"))) {
    mHeaderMethod = value;
  }

  // http/2 transport level headers shouldn't be gatewayed into http/1
  if(*(name.BeginReading()) == ':') {
    LOG3(("HTTP Decompressor not gatewaying %s into http/1",
          name.BeginReading()));
    return NS_OK;
  }

  mOutput->Append(name);
  mOutput->Append(NS_LITERAL_CSTRING(": "));
  mOutput->Append(value);
  mOutput->Append(NS_LITERAL_CSTRING("\r\n"));
  return NS_OK;
}

nsresult
Http2Decompressor::OutputHeader(uint32_t index)
{
  // bounds check
  if (mHeaderTable.Length() <= index)
    return NS_ERROR_ILLEGAL_VALUE;

  return OutputHeader(mHeaderTable[index]->mName,
                      mHeaderTable[index]->mValue);
}

nsresult
Http2Decompressor::CopyHeaderString(uint32_t index, nsACString &name)
{
  // bounds check
  if (mHeaderTable.Length() <= index)
    return NS_ERROR_ILLEGAL_VALUE;

  name = mHeaderTable[index]->mName;
  return NS_OK;
}

nsresult
Http2Decompressor::CopyStringFromInput(uint32_t bytes, nsACString &val)
{
  if (mOffset + bytes > mDataLen)
    return NS_ERROR_ILLEGAL_VALUE;

  val.Assign(reinterpret_cast<const char *>(mData) + mOffset, bytes);
  mOffset += bytes;
  return NS_OK;
}

nsresult
Http2Decompressor::DecodeFinalHuffmanCharacter(HuffmanIncomingTable *table,
                                               uint8_t &c, uint8_t &bitsLeft)
{
  uint8_t idxLen = table->mPrefixLen;
  uint8_t mask = (1 << bitsLeft) - 1;
  uint8_t idx = mData[mOffset - 1] & mask;
  if (idxLen < bitsLeft) {
    mask &= ~((1 << (bitsLeft - idxLen)) - 1);
    idx &= mask;
    idx >>= (bitsLeft - idxLen);
    idx &= ((1 << idxLen) - 1);
  } else {
    idx <<= (idxLen - bitsLeft);
  }
  // Don't update bitsLeft yet, because we need to check that value against the
  // number of bits used by our encoding later on. We'll update when we are sure
  // how many bits we've actually used.

  if (table->mEntries[idx].mPtr) {
    // Can't chain to another table when we're all out of bits in the encoding
    LOG3(("DecodeFinalHuffmanCharacter trying to chain when we're out of bits"));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (bitsLeft < table->mEntries[idx].mPrefixLen) {
    // We don't have enough bits to actually make a match, this is some sort of
    // invalid coding
    LOG3(("DecodeFinalHuffmanCharacter does't have enough bits to match"));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  // This is a character!
  if (table->mEntries[idx].mValue == 256) {
    // EOS
    LOG3(("DecodeFinalHuffmanCharacter actually decoded an EOS"));
    return NS_ERROR_ILLEGAL_VALUE;
  }
  c = static_cast<uint8_t>(table->mEntries[idx].mValue & 0xFF);
  bitsLeft -= table->mEntries[idx].mPrefixLen;

  return NS_OK;
}

nsresult
Http2Decompressor::DecodeHuffmanCharacter(HuffmanIncomingTable *table,
                                          uint8_t &c, uint32_t &bytesConsumed,
                                          uint8_t &bitsLeft)
{
  uint8_t idxLen = table->mPrefixLen;
  uint8_t idx;
  uint8_t mask;

  if (idxLen < bitsLeft) {
    // Only need to consume part of the rest of the previous byte
    mask = (1 << bitsLeft) - 1;
    bitsLeft -= idxLen;
    mask &= ~((1 << bitsLeft) - 1);
    idx = (mData[mOffset - 1] & mask) >> bitsLeft;
    idx &= ((1 << idxLen) - 1);
  } else if (bitsLeft) {
    // Need to consume all of the rest of the previous byte, and possibly some
    // of the current byte
    mask = (1 << bitsLeft) - 1;
    idxLen -= bitsLeft;
    idx = (mData[mOffset - 1] & mask) << idxLen;
    bitsLeft = 0;
    if (idxLen) {
      // Need to consume some of the current byte
      bitsLeft = 8 - idxLen;
      mask = ~((1 << bitsLeft) - 1);
      uint8_t lastBits = (mData[mOffset] & mask) >> bitsLeft;
      idx |= (lastBits & ((1 << idxLen) - 1));
      bytesConsumed++;
      mOffset++;
    }
  } else {
    // byte-aligned already
    mask = (1 << 8) - 1;
    bitsLeft = 8 - idxLen;
    mask &= ~((1 << bitsLeft) - 1);
    idx = (mData[mOffset] & mask) >> bitsLeft;
    idx &= ((1 << idxLen) - 1);
    bytesConsumed++;
    mOffset++;
  }

  if (table->mEntries[idx].mPtr) {
    if (bytesConsumed >= mDataLen) {
      if (!bitsLeft || (bytesConsumed > mDataLen)) {
        // No info left in input to try to consume, we're done
        LOG3(("DecodeHuffmanCharacter all out of bits to consume, can't chain"));
        return NS_ERROR_ILLEGAL_VALUE;
      }

      // We might get lucky here!
      return DecodeFinalHuffmanCharacter(table->mEntries[idx].mPtr, c,
                                         bitsLeft);
    }

    // We're sorry, Mario, but your princess is in another castle
    return DecodeHuffmanCharacter(table->mEntries[idx].mPtr, c, bytesConsumed,
                                  bitsLeft);
  }

  if (table->mEntries[idx].mValue == 256) {
    LOG3(("DecodeHuffmanCharacter found an actual EOS"));
    return NS_ERROR_ILLEGAL_VALUE;
  }
  c = static_cast<uint8_t>(table->mEntries[idx].mValue & 0xFF);

  // Need to adjust bitsLeft (and possibly other values) because we may not have
  // consumed all of the bits that the table requires for indexing.
  bitsLeft += (table->mPrefixLen - table->mEntries[idx].mPrefixLen);
  if (bitsLeft >= 8) {
    mOffset--;
    bytesConsumed--;
    bitsLeft -= 8;
  }
  MOZ_ASSERT(bitsLeft < 8);

  return NS_OK;
}

nsresult
Http2Decompressor::CopyHuffmanStringFromInput(uint32_t bytes, nsACString &val)
{
  if (mOffset + bytes > mDataLen) {
    LOG3(("CopyHuffmanStringFromInput not enough data"));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  uint32_t bytesRead = 0;
  uint8_t bitsLeft = 0;
  nsAutoCString buf;
  nsresult rv;
  uint8_t c;

  while (bytesRead < bytes) {
    uint32_t bytesConsumed = 0;
    rv = DecodeHuffmanCharacter(&HuffmanIncomingRoot, c, bytesConsumed,
                                bitsLeft);
    if (NS_FAILED(rv)) {
      LOG3(("CopyHuffmanStringFromInput failed to decode a character"));
      return rv;
    }

    bytesRead += bytesConsumed;
    buf.Append(c);
  }

  if (bytesRead > bytes) {
    LOG3(("CopyHuffmanStringFromInput read more bytes than was allowed!"));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (bitsLeft) {
    // The shortest valid code is 4 bits, so we know there can be at most one
    // character left that our loop didn't decode. Check to see if that's the
    // case, and if so, add it to our output.
    rv = DecodeFinalHuffmanCharacter(&HuffmanIncomingRoot, c, bitsLeft);
    if (NS_SUCCEEDED(rv)) {
      buf.Append(c);
    }
  }

  if (bitsLeft) {
    // Any bits left at this point must belong to the EOS symbol, so make sure
    // they make sense (ie, are all ones)
    uint8_t mask = (1 << bitsLeft) - 1;
    uint8_t bits = mData[mOffset - 1] & mask;
    if (bits != mask) {
      LOG3(("CopyHuffmanStringFromInput ran out of data but found possible "
            "non-EOS symbol"));
      return NS_ERROR_ILLEGAL_VALUE;
    }
  }

  val = buf;
  LOG3(("CopyHuffmanStringFromInput decoded a full string!"));
  return NS_OK;
}

void
Http2Decompressor::MakeRoom(uint32_t amount)
{
  // make room in the header table
  uint32_t removedCount = 0;
  while (mHeaderTable.VariableLength() && ((mHeaderTable.ByteCount() + amount) > mMaxBuffer)) {
    uint32_t index = mHeaderTable.VariableLength() - 1;
    mHeaderTable.RemoveElement();
    ++removedCount;
    LOG3(("HTTP decompressor header table index %u removed for size.\n",
          index));
  }

  // adjust references to header table
  UpdateReferenceSet(removedCount);
}

nsresult
Http2Decompressor::DoIndexed()
{
  // this starts with a 1 bit pattern
  MOZ_ASSERT(mData[mOffset] & 0x80);

  // Indexed entries toggle the reference set
  // This is a 7 bit prefix

  uint32_t index;
  nsresult rv = DecodeInteger(7, index);
  if (NS_FAILED(rv))
    return rv;

  LOG3(("HTTP decompressor indexed entry %u\n", index));

  // Toggle this in the reference set..
  // if its not currently in the reference set then add it and
  // also emit it. If it is currently in the reference set then just
  // remove it from there.
  if (mReferenceSet.RemoveElement(index)) {
    mAlternateReferenceSet.RemoveElement(index);
    return NS_OK;
  }

  rv = OutputHeader(index);
  if (index >= mHeaderTable.VariableLength()) {
    const nvPair *pair = mHeaderTable[index];
    uint32_t room = pair->Size();

    if (room > mMaxBuffer) {
      ClearHeaderTable();
      LOG3(("HTTP decompressor index not referenced due to size %u %s\n",
            room, pair->mName.get()));
      return rv;
    }

    MakeRoom(room);
    mHeaderTable.AddElement(pair->mName, pair->mValue);
    IncrementReferenceSetIndices();
    index = 0;
  }

  mReferenceSet.AppendElement(index);
  mAlternateReferenceSet.AppendElement(index);
  return rv;
}

nsresult
Http2Decompressor::DoLiteralInternal(nsACString &name, nsACString &value)
{
  // guts of doliteralwithoutindex and doliteralwithincremental
  MOZ_ASSERT(((mData[mOffset] & 0xC0) == 0x40) ||  // withoutindex
             ((mData[mOffset] & 0xC0) == 0x00));   // withincremental

  // first let's get the name
  uint32_t index;
  nsresult rv = DecodeInteger(6, index);
  if (NS_FAILED(rv))
    return rv;

  bool isHuffmanEncoded;

  if (!index) {
    // name is embedded as a literal
    uint32_t nameLen;
    isHuffmanEncoded = mData[mOffset] & (1 << 7);
    rv = DecodeInteger(7, nameLen);
    if (NS_SUCCEEDED(rv)) {
      if (isHuffmanEncoded) {
        rv = CopyHuffmanStringFromInput(nameLen, name);
      } else {
        rv = CopyStringFromInput(nameLen, name);
      }
    }
  } else {
    // name is from headertable
    rv = CopyHeaderString(index - 1, name);
  }
  if (NS_FAILED(rv))
    return rv;

  // now the value
  uint32_t valueLen;
  isHuffmanEncoded = mData[mOffset] & (1 << 7);
  rv = DecodeInteger(7, valueLen);
  if (NS_SUCCEEDED(rv)) {
    if (isHuffmanEncoded) {
      rv = CopyHuffmanStringFromInput(valueLen, value);
    } else {
      rv = CopyStringFromInput(valueLen, value);
    }
  }
  if (NS_FAILED(rv))
    return rv;
  return NS_OK;
}

nsresult
Http2Decompressor::DoLiteralWithoutIndex()
{
  // this starts with 01 bit pattern
  MOZ_ASSERT((mData[mOffset] & 0xC0) == 0x40);

  // This is not indexed so there is no adjustment to the
  // persistent reference set
  nsAutoCString name, value;
  nsresult rv = DoLiteralInternal(name, value);

  LOG3(("HTTP decompressor literal without index %s %s\n",
        name.get(), value.get()));

  // Output the header now because we don't keep void
  // indicies in the reference set
  if (NS_SUCCEEDED(rv))
    rv = OutputHeader(name, value);
  return rv;
}

nsresult
Http2Decompressor::DoLiteralWithIncremental()
{
  // this starts with 00 bit pattern
  MOZ_ASSERT((mData[mOffset] & 0xC0) == 0x00);

  nsAutoCString name, value;
  nsresult rv = DoLiteralInternal(name, value);
  if (NS_SUCCEEDED(rv))
    rv = OutputHeader(name, value);
  if (NS_FAILED(rv))
    return rv;

  uint32_t room = nvPair(name, value).Size();
  if (room > mMaxBuffer) {
    ClearHeaderTable();
    LOG3(("HTTP decompressor literal with index not referenced due to size %u %s\n",
          room, name.get()));
    return NS_OK;
  }

  MakeRoom(room);

  // Incremental Indexing implicitly adds a row to the header table.
  // It also adds the new row to the Reference Set
  mHeaderTable.AddElement(name, value);
  IncrementReferenceSetIndices();
  mReferenceSet.AppendElement(0);
  mAlternateReferenceSet.AppendElement(0);

  LOG3(("HTTP decompressor literal with index 0 %s %s\n",
        name.get(), value.get()));

  return NS_OK;
}

/////////////////////////////////////////////////////////////////

nsresult
Http2Compressor::EncodeHeaderBlock(const nsCString &nvInput,
                                   const nsACString &method, const nsACString &path,
                                   const nsACString &host, const nsACString &scheme,
                                   nsACString &output)
{
  mAlternateReferenceSet.Clear();
  mImpliedReferenceSet.Clear();
  mOutput = &output;
  output.SetCapacity(1024);
  output.Truncate();
  mParsedContentLength = -1;

  // colon headers first
  ProcessHeader(nvPair(NS_LITERAL_CSTRING(":method"), method));
  ProcessHeader(nvPair(NS_LITERAL_CSTRING(":path"), path));
  ProcessHeader(nvPair(NS_LITERAL_CSTRING(":authority"), host));
  ProcessHeader(nvPair(NS_LITERAL_CSTRING(":scheme"), scheme));

  // now the non colon headers
  const char *beginBuffer = nvInput.BeginReading();

  int32_t crlfIndex = nvInput.Find("\r\n");
  while (true) {
    int32_t startIndex = crlfIndex + 2;

    crlfIndex = nvInput.Find("\r\n", false, startIndex);
    if (crlfIndex == -1)
      break;

    int32_t colonIndex = nvInput.Find(":", false, startIndex,
                                      crlfIndex - startIndex);
    if (colonIndex == -1)
      break;

    nsDependentCSubstring name = Substring(beginBuffer + startIndex,
                                           beginBuffer + colonIndex);
    // all header names are lower case in http/2
    ToLowerCase(name);

    // exclusions
    if (name.Equals("connection") ||
        name.Equals("host") ||
        name.Equals("keep-alive") ||
        name.Equals("proxy-connection") ||
        name.Equals("te") ||
        name.Equals("transfer-encoding") ||
        name.Equals("upgrade") ||
        name.Equals("accept-encoding")) {
      continue;
    }

    // colon headers are for http/2 and this is http/1 input, so that
    // is probably a smuggling attack of some kind
    if(*(name.BeginReading()) == ':') {
      continue;
    }

    int32_t valueIndex = colonIndex + 1;

    // if we have Expect: *100-continue,*" redact the 100-continue
    // as we don't have a good mechanism for clients to make use of it
    // anyhow
    if (name.Equals("expect")) {
      const char *continueHeader =
        nsHttp::FindToken(beginBuffer + valueIndex, "100-continue",
                          HTTP_HEADER_VALUE_SEPS);
      if (continueHeader) {
        char *writableVal = const_cast<char *>(continueHeader);
        memset(writableVal, 0, 12);
        writableVal += 12;
        // this will terminate safely because CRLF EOL has been confirmed
        while ((*writableVal == ' ') || (*writableVal == '\t') ||
               (*writableVal == ',')) {
          *writableVal = ' ';
          ++writableVal;
        }
      }
    }

    while (valueIndex < crlfIndex && beginBuffer[valueIndex] == ' ')
      ++valueIndex;

    nsDependentCSubstring value = Substring(beginBuffer + valueIndex,
                                            beginBuffer + crlfIndex);

    if (name.Equals("content-length")) {
      int64_t len;
      nsCString tmp(value);
      if (nsHttp::ParseInt64(tmp.get(), nullptr, &len))
        mParsedContentLength = len;
    }

    ProcessHeader(nvPair(name, value));
  }

  // iterate mreference set and if !alternate.contains(old[i])
  // toggle off
  uint32_t setLen = mReferenceSet.Length();
  for (uint32_t index = 0; index < setLen; ++index) {
    if (!mAlternateReferenceSet.Contains(mReferenceSet[index])) {
      DoOutput(kToggleOff, mHeaderTable[mReferenceSet[index]],
               mReferenceSet[index]);
    }
  }

  mReferenceSet = mAlternateReferenceSet;
  mAlternateReferenceSet.Clear();
  mImpliedReferenceSet.Clear();
  mOutput = nullptr;
  return NS_OK;
}

void
Http2Compressor::DoOutput(Http2Compressor::outputCode code,
                          const class nvPair *pair, uint32_t index)
{
  // start Byte needs to be calculated from the offset after
  // the opcode has been written out in case the output stream
  // buffer gets resized/relocated
  uint32_t offset = mOutput->Length();
  uint8_t *startByte;

  switch (code) {
  case kPlainLiteral:
    LOG3(("HTTP compressor %p noindex literal with name reference %u %s: %s\n",
          this, index, pair->mName.get(), pair->mValue.get()));

    EncodeInteger(6, index); // 01 2 bit prefix
    startByte = reinterpret_cast<unsigned char *>(mOutput->BeginWriting()) + offset;
    *startByte = (*startByte & 0x3f) | 0x40;

    if (!index) {
      HuffmanAppend(pair->mName);
    }

    HuffmanAppend(pair->mValue);
    break;

  case kIndexedLiteral:
    LOG3(("HTTP compressor %p literal with name reference %u %s: %s\n",
          this, index, pair->mName.get(), pair->mValue.get()));

    EncodeInteger(6, index); // 00 2 bit prefix
    startByte = reinterpret_cast<unsigned char *>(mOutput->BeginWriting()) + offset;
    *startByte = *startByte & 0x3f;

    if (!index) {
      HuffmanAppend(pair->mName);
    }

    HuffmanAppend(pair->mValue);
    break;

  case kToggleOff:
  case kToggleOn:
    LOG3(("HTTP compressor %p toggle %s index %u %s\n",
          this, (code == kToggleOff) ? "off" : "on",
          index, pair->mName.get()));
    EncodeInteger(7, index);
    startByte = reinterpret_cast<unsigned char *>(mOutput->BeginWriting()) + offset;
    *startByte = *startByte | 0x80; // 1 1 bit prefix
    break;

  case kNop:
    LOG3(("HTTP compressor %p implied in reference set index %u %s\n",
          this, index, pair->mName.get()));
    break;
  }
}

// writes the encoded integer onto the output
void
Http2Compressor::EncodeInteger(uint32_t prefixLen, uint32_t val)
{
  uint32_t mask = (1 << prefixLen) - 1;
  uint8_t tmp;

  if (val < mask) {
    // 1 byte encoding!
    tmp = val;
    mOutput->Append(reinterpret_cast<char *>(&tmp), 1);
    return;
  }

  if (mask) {
    val -= mask;
    tmp = mask;
    mOutput->Append(reinterpret_cast<char *>(&tmp), 1);
  }

  uint32_t q, r;
  do {
    q = val / 128;
    r = val % 128;
    tmp = r;
    if (q)
      tmp |= 0x80; // chain bit
    val = q;
    mOutput->Append(reinterpret_cast<char *>(&tmp), 1);
  } while (q);
}

void
Http2Compressor::ClearHeaderTable()
{
  uint32_t dynamicCount = mHeaderTable.VariableLength();

  Http2BaseCompressor::ClearHeaderTable();

  for (int32_t i = mImpliedReferenceSet.Length() - 1; i >= 0; --i) {
    if (mImpliedReferenceSet[i] < dynamicCount) {
      mImpliedReferenceSet.RemoveElementAt(i);
    } else {
      mImpliedReferenceSet[i] -= dynamicCount;
    }
  }
}


void
Http2Compressor::UpdateReferenceSet(int32_t delta)
{
  if (!delta)
    return;

  Http2BaseCompressor::UpdateReferenceSet(delta);

  uint32_t headerTableSize = mHeaderTable.VariableLength();
  uint32_t oldHeaderTableSize = headerTableSize + delta;

  for (int32_t i = mImpliedReferenceSet.Length() - 1; i >= 0; --i) {
    uint32_t indexRef = mImpliedReferenceSet[i];
    if (indexRef >= headerTableSize) {
      if (indexRef < oldHeaderTableSize) {
        // This one got dropped
        LOG3(("HTTP compressor implied reference to index %u removed.\n",
              indexRef));
        mImpliedReferenceSet.RemoveElementAt(i);
      } else {
        // This pointed to the static table, need to adjust
        uint32_t newRef = indexRef - delta;
        LOG3(("HTTP compressor implied reference to index %u changed to %d (%s)\n",
              mImpliedReferenceSet[i], newRef, mHeaderTable[newRef]->mName.get()));
        mImpliedReferenceSet[i] = newRef;
      }
    }
  }
}

void
Http2Compressor::IncrementReferenceSetIndices()
{
  Http2BaseCompressor::IncrementReferenceSetIndices();

  for (int32_t i = mImpliedReferenceSet.Length() - 1; i >= 0; --i) {
    mImpliedReferenceSet[i] = mImpliedReferenceSet[i] + 1;
  }
}

void
Http2Compressor::MakeRoom(uint32_t amount)
{
  // make room in the header table
  uint32_t removedCount = 0;
  while (mHeaderTable.VariableLength() && ((mHeaderTable.ByteCount() + amount) > mMaxBuffer)) {

    // if there is a reference to removedCount (~0) in the implied reference set we need,
    // to toggle it off/on so that the implied reference is not lost when the
    // table is trimmed
    uint32_t index = mHeaderTable.VariableLength() - 1;
    if (mImpliedReferenceSet.Contains(index) ) {
      LOG3(("HTTP compressor header table index %u %s about to be "
            "removed for size but has an implied reference. Will Toggle.\n",
            index, mHeaderTable[index]->mName.get()));

      DoOutput(kToggleOff, mHeaderTable[index], index);
      DoOutput(kToggleOn, mHeaderTable[index], index);
    }

    LOG3(("HTTP compressor header table index %u %s removed for size.\n",
          index, mHeaderTable[index]->mName.get()));
    mHeaderTable.RemoveElement();
    ++removedCount;
  }

  // adjust references to header table
  UpdateReferenceSet(removedCount);
}

void
Http2Compressor::HuffmanAppend(const nsCString &value)
{
  nsAutoCString buf;
  uint8_t bitsLeft = 8;
  uint32_t length = value.Length();
  uint32_t offset;
  uint8_t *startByte;

  for (uint32_t i = 0; i < length; ++i) {
    uint8_t idx = static_cast<uint8_t>(value[i]);
    uint8_t huffLength = HuffmanOutgoing[idx].mLength;
    uint32_t huffValue = HuffmanOutgoing[idx].mValue;

    if (bitsLeft < 8) {
      // Fill in the least significant <bitsLeft> bits of the previous byte
      // first
      uint32_t val;
      if (huffLength >= bitsLeft) {
        val = huffValue & ~((1 << (huffLength - bitsLeft)) - 1);
        val >>= (huffLength - bitsLeft);
      } else {
        val = huffValue << (bitsLeft - huffLength);
      }
      val &= ((1 << bitsLeft) - 1);
      offset = buf.Length() - 1;
      startByte = reinterpret_cast<unsigned char *>(buf.BeginWriting()) + offset;
      *startByte = *startByte | static_cast<uint8_t>(val & 0xFF);
      if (huffLength >= bitsLeft) {
        huffLength -= bitsLeft;
        bitsLeft = 8;
      } else {
        bitsLeft -= huffLength;
        huffLength = 0;
      }
    }

    while (huffLength > 8) {
      uint32_t mask = ~((1 << (huffLength - 8)) - 1);
      uint8_t val = ((huffValue & mask) >> (huffLength - 8)) & 0xFF;
      buf.Append(reinterpret_cast<char *>(&val), 1);
      huffLength -= 8;
    }

    if (huffLength) {
      // Fill in the most significant <huffLength> bits of the next byte
      bitsLeft = 8 - huffLength;
      uint8_t val = (huffValue & ((1 << huffLength) - 1)) << bitsLeft;
      buf.Append(reinterpret_cast<char *>(&val), 1);
    }
  }

  if (bitsLeft != 8) {
    // Pad the last <bitsLeft> bits with ones, which corresponds to the EOS
    // encoding
    uint8_t val = (1 << bitsLeft) - 1;
    offset = buf.Length() - 1;
    startByte = reinterpret_cast<unsigned char *>(buf.BeginWriting()) + offset;
    *startByte = *startByte | val;
  }

  // Now we know how long our encoded string is, we can fill in our length
  uint32_t bufLength = buf.Length();
  offset = mOutput->Length();
  EncodeInteger(7, bufLength);
  startByte = reinterpret_cast<unsigned char *>(mOutput->BeginWriting()) + offset;
  *startByte = *startByte | 0x80;

  // Finally, we can add our REAL data!
  mOutput->Append(buf);
}

void
Http2Compressor::ProcessHeader(const nvPair inputPair)
{
  uint32_t newSize = inputPair.Size();
  uint32_t headerTableSize = mHeaderTable.Length();
  uint32_t matchedIndex;
  uint32_t nameReference = 0;
  bool match = false;

  for (uint32_t index = 0; index < headerTableSize; ++index) {
    if (mHeaderTable[index]->mName.Equals(inputPair.mName)) {
      nameReference = index + 1;
      if (mHeaderTable[index]->mValue.Equals(inputPair.mValue)) {
        match = true;
        matchedIndex = index;
        break;
      }
    }
  }

  // We need to emit a new literal
  if (!match) {
    if ((newSize > (mMaxBuffer / 2)) || (mMaxBuffer < 128)) {
      DoOutput(kPlainLiteral, &inputPair, nameReference);
      return;
    }

    // make sure to makeroom() first so that any implied items
    // get preserved.
    MakeRoom(newSize);
    DoOutput(kIndexedLiteral, &inputPair, nameReference);

    mHeaderTable.AddElement(inputPair.mName, inputPair.mValue);
    IncrementReferenceSetIndices();
    LOG3(("HTTP compressor %p new literal placed at index 0\n",
          this));
    mAlternateReferenceSet.AppendElement(0);
    return;
  }

  // It is in the reference set. just check to see if it is
  // a duplicate for output purposes
  if (mReferenceSet.Contains(matchedIndex)) {
    if (mAlternateReferenceSet.Contains(matchedIndex)) {
      DoOutput(kToggleOff, &inputPair, matchedIndex);
      DoOutput(kToggleOn, &inputPair, matchedIndex);
    } else {
      DoOutput(kNop, &inputPair, matchedIndex);
      if (!mImpliedReferenceSet.Contains(matchedIndex))
        mImpliedReferenceSet.AppendElement(matchedIndex);
      mAlternateReferenceSet.AppendElement(matchedIndex);
    }
    return;
  }

  // emit an index to add to reference set
  DoOutput(kToggleOn, &inputPair, matchedIndex);
  if (matchedIndex >= mHeaderTable.VariableLength()) {
    MakeRoom(newSize);
    mHeaderTable.AddElement(inputPair.mName, inputPair.mValue);
    IncrementReferenceSetIndices();
    mAlternateReferenceSet.AppendElement(0);
  } else {
    mAlternateReferenceSet.AppendElement(matchedIndex);
  }
  return;
}

void
Http2Compressor::SetMaxBufferSize(uint32_t maxBufferSize)
{
  uint32_t removedCount = 0;

  LOG3(("Http2Compressor::SetMaxBufferSize %u called", maxBufferSize));

  while (mHeaderTable.VariableLength() && (mHeaderTable.ByteCount() > maxBufferSize)) {
    mHeaderTable.RemoveElement();
    ++removedCount;
  }
  UpdateReferenceSet(removedCount);

  mMaxBuffer = maxBufferSize;
}

} // namespace mozilla::net
} // namespace mozilla
