/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CTObjectsExtractor.h"

#include "hasht.h"
#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "mozilla/Move.h"
#include "mozilla/PodOperations.h"
#include "mozilla/RangedPtr.h"
#include "mozilla/Vector.h"
#include "pkix/pkixnss.h"
#include "pkixutil.h"

namespace mozilla { namespace ct {

using namespace mozilla::pkix;

// Holds a non-owning pointer to a byte buffer and allows writing chunks of data
// to the buffer, placing the later chunks after the earlier ones
// in a stream-like fashion.
// Note that writing to Output always succeeds. If the internal buffer
// overflows, an error flag is turned on and you won't be able to retrieve
// the final data.
class Output
{
public:
  Output(uint8_t* buffer, size_t length)
    : begin(buffer)
    , end(buffer + length)
    , current(buffer, begin, end)
    , overflowed(false)
  {
  }

  template <size_t N>
  explicit Output(uint8_t (&buffer)[N])
    : Output(buffer, N)
  {
  }

  void Write(Input data)
  {
    Write(data.UnsafeGetData(), data.GetLength());
  }

  void Write(uint8_t b)
  {
    Write(&b, 1);
  }

  bool IsOverflowed() const { return overflowed; }

  Result GetInput(/*out*/ Input& input) const
  {
    if (overflowed) {
      return Result::FATAL_ERROR_INVALID_STATE;
    }
    size_t length = AssertedCast<size_t>(current.get() - begin);
    return input.Init(begin, length);
  }

private:
  uint8_t* begin;
  uint8_t* end;
  RangedPtr<uint8_t> current;
  bool overflowed;

  Output(const Output&) = delete;
  void operator=(const Output&) = delete;

  void Write(const uint8_t* data, size_t length)
  {
    size_t available = AssertedCast<size_t>(end - current.get());
    if (available < length) {
      overflowed = true;
    }
    if (overflowed) {
      return;
    }
    PodCopy(current.get(), data, length);
    current += length;
  }
};

// For reference:
//
// Certificate  ::=  SEQUENCE  {
//      tbsCertificate       TBSCertificate,
//      signatureAlgorithm   AlgorithmIdentifier,
//      signatureValue       BIT STRING  }
//
// TBSCertificate  ::=  SEQUENCE  {
//      version         [0]  EXPLICIT Version DEFAULT v1,
//      serialNumber         CertificateSerialNumber,
//      signature            AlgorithmIdentifier,
//      issuer               Name,
//      validity             Validity,
//      subject              Name,
//      subjectPublicKeyInfo SubjectPublicKeyInfo,
//      issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL,
//                           -- If present, version MUST be v2 or v3
//      subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL,
//                           -- If present, version MUST be v2 or v3
//      extensions      [3]  EXPLICIT Extensions OPTIONAL
//                           -- If present, version MUST be v3
//      }

// python DottedOIDToCode.py id-embeddedSctList 1.3.6.1.4.1.11129.2.4.2
// See Section 3.3 of RFC 6962.
static const uint8_t EMBEDDED_SCT_LIST_OID[] = {
  0x2b, 0x06, 0x01, 0x04, 0x01, 0xd6, 0x79, 0x02, 0x04, 0x02
};
// Maximum length of DER TLV header
static const size_t MAX_TLV_HEADER_LENGTH = 4;
// DER tag of the "extensions [3]" field from TBSCertificate
static const uint8_t EXTENSIONS_CONTEXT_TAG =
  der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 3;

// Given a leaf certificate, extracts the DER-encoded TBSCertificate component
// of the corresponding Precertificate.
// Basically, the extractor needs to remove the embedded SCTs extension
// from the certificate and return its TBSCertificate. We do it in an ad hoc
// manner by breaking the source DER into several parts and then joining
// the right parts, taking care to update the relevant TLV headers.
// See WriteOutput for more details on the parts involved.
class PrecertTBSExtractor
{
public:
  // |buffer| is the buffer to be used for writing the output. Since the
  // required buffer size is not generally known in advance, it's best
  // to use at least the size of the input certificate DER.
  PrecertTBSExtractor(Input der, uint8_t* buffer, size_t bufferLength)
    : mDER(der)
    , mOutput(buffer, bufferLength)
  {
  }

  // Performs the extraction.
  Result Init()
  {
    Reader tbsReader;
    Result rv = GetTBSCertificate(tbsReader);
    if (rv != Success) {
      return rv;
    }

    rv = ExtractTLVsBeforeExtensions(tbsReader);
    if (rv != Success) {
      return rv;
    }

    rv = ExtractOptionalExtensionsExceptSCTs(tbsReader);
    if (rv != Success) {
      return rv;
    }

    return WriteOutput();
  }

  // Use to retrieve the result after a successful call to Init.
  // The returned Input points to the buffer supplied in the constructor.
  Input GetPrecertTBS()
  {
    return mPrecertTBS;
  }

private:
  Result GetTBSCertificate(Reader& tbsReader)
  {
    Reader certificateReader;
    Result rv = der::ExpectTagAndGetValueAtEnd(mDER, der::SEQUENCE,
                                               certificateReader);
    if (rv != Success) {
      return rv;
    }
    return ExpectTagAndGetValue(certificateReader, der::SEQUENCE, tbsReader);
  }

  Result ExtractTLVsBeforeExtensions(Reader& tbsReader)
  {
    Reader::Mark tbsBegin = tbsReader.GetMark();
    while (!tbsReader.AtEnd()) {
      if (tbsReader.Peek(EXTENSIONS_CONTEXT_TAG)) {
        break;
      }
      uint8_t tag;
      Input tagValue;
      Result rv = der::ReadTagAndGetValue(tbsReader, tag, tagValue);
      if (rv != Success) {
        return rv;
      }
    }
    return tbsReader.GetInput(tbsBegin, mTLVsBeforeExtensions);
  }

  Result ExtractOptionalExtensionsExceptSCTs(Reader& tbsReader)
  {
    if (!tbsReader.Peek(EXTENSIONS_CONTEXT_TAG)) {
      return Success;
    }

    Reader extensionsContextReader;
    Result rv = der::ExpectTagAndGetValueAtEnd(tbsReader,
                                               EXTENSIONS_CONTEXT_TAG,
                                               extensionsContextReader);
    if (rv != Success) {
      return rv;
    }

    Reader extensionsReader;
    rv = der::ExpectTagAndGetValueAtEnd(extensionsContextReader, der::SEQUENCE,
                                        extensionsReader);
    if (rv != Success) {
      return rv;
    }

    while (!extensionsReader.AtEnd()) {
      Reader::Mark extensionTLVBegin = extensionsReader.GetMark();
      Reader extension;
      rv = der::ExpectTagAndGetValue(extensionsReader, der::SEQUENCE,
                                     extension);
      if (rv != Success) {
        return rv;
      }
      Reader extensionID;
      rv = der::ExpectTagAndGetValue(extension, der::OIDTag, extensionID);
      if (rv != Success) {
        return rv;
      }
      if (!extensionID.MatchRest(EMBEDDED_SCT_LIST_OID)) {
        Input extensionTLV;
        rv = extensionsReader.GetInput(extensionTLVBegin, extensionTLV);
        if (rv != Success) {
          return rv;
        }
        if (!mExtensionTLVs.append(std::move(extensionTLV))) {
          return Result::FATAL_ERROR_NO_MEMORY;
        }
      }
    }
    return Success;
  }

  Result WriteOutput()
  {
    // What should be written here:
    //
    // TBSCertificate ::= SEQUENCE (TLV with header |tbsHeader|)
    //   dump of |mTLVsBeforeExtensions|
    //   extensions [3] OPTIONAL (TLV with header |extensionsContextHeader|)
    //     SEQUENCE (TLV with with header |extensionsHeader|)
    //       dump of |mExtensionTLVs|

    Result rv;
    if (mExtensionTLVs.length() > 0) {
      uint8_t tbsHeaderBuffer[MAX_TLV_HEADER_LENGTH];
      uint8_t extensionsContextHeaderBuffer[MAX_TLV_HEADER_LENGTH];
      uint8_t extensionsHeaderBuffer[MAX_TLV_HEADER_LENGTH];

      Input tbsHeader;
      Input extensionsContextHeader;
      Input extensionsHeader;

      // Count the total size of the extensions. Note that since
      // the extensions data is contained within mDER (an Input),
      // their combined length won't overflow Input::size_type.
      Input::size_type extensionsValueLength = 0;
      for (auto& extensionTLV : mExtensionTLVs) {
        extensionsValueLength += extensionTLV.GetLength();
      }

      rv = MakeTLVHeader(der::SEQUENCE, extensionsValueLength,
                         extensionsHeaderBuffer, extensionsHeader);
      if (rv != Success) {
        return rv;
      }
      Input::size_type extensionsContextLength =
        AssertedCast<Input::size_type>(extensionsHeader.GetLength() +
                                       extensionsValueLength);
      rv = MakeTLVHeader(EXTENSIONS_CONTEXT_TAG,
                         extensionsContextLength,
                         extensionsContextHeaderBuffer,
                         extensionsContextHeader);
      if (rv != Success) {
        return rv;
      }
      Input::size_type tbsLength =
        AssertedCast<Input::size_type>(mTLVsBeforeExtensions.GetLength() +
                                       extensionsContextHeader.GetLength() +
                                       extensionsHeader.GetLength() +
                                       extensionsValueLength);
      rv = MakeTLVHeader(der::SEQUENCE, tbsLength, tbsHeaderBuffer, tbsHeader);
      if (rv != Success) {
        return rv;
      }

      mOutput.Write(tbsHeader);
      mOutput.Write(mTLVsBeforeExtensions);
      mOutput.Write(extensionsContextHeader);
      mOutput.Write(extensionsHeader);
      for (auto& extensionTLV : mExtensionTLVs) {
        mOutput.Write(extensionTLV);
      }
    } else {
      uint8_t tbsHeaderBuffer[MAX_TLV_HEADER_LENGTH];
      Input tbsHeader;
      rv = MakeTLVHeader(der::SEQUENCE, mTLVsBeforeExtensions.GetLength(),
                         tbsHeaderBuffer, tbsHeader);
      if (rv != Success) {
        return rv;
      }
      mOutput.Write(tbsHeader);
      mOutput.Write(mTLVsBeforeExtensions);
    }

    return mOutput.GetInput(mPrecertTBS);
  }

  Result MakeTLVHeader(uint8_t tag, size_t length,
                       uint8_t (&buffer)[MAX_TLV_HEADER_LENGTH],
                       /*out*/ Input& header)
  {
    Output output(buffer);
    output.Write(tag);
    if (length < 128) {
      output.Write(AssertedCast<uint8_t>(length));
    } else if (length < 256) {
      output.Write(0x81u);
      output.Write(AssertedCast<uint8_t>(length));
    } else if (length < 65536) {
      output.Write(0x82u);
      output.Write(AssertedCast<uint8_t>(length / 256));
      output.Write(AssertedCast<uint8_t>(length % 256));
    } else {
      return Result::FATAL_ERROR_INVALID_ARGS;
    }
    return output.GetInput(header);
  }

  Input mDER;
  Input mTLVsBeforeExtensions;
  Vector<Input, 16> mExtensionTLVs;
  Output mOutput;
  Input mPrecertTBS;
};

Result
GetPrecertLogEntry(Input leafCertificate, Input issuerSubjectPublicKeyInfo,
                   LogEntry& output)
{
  MOZ_ASSERT(leafCertificate.GetLength() > 0);
  MOZ_ASSERT(issuerSubjectPublicKeyInfo.GetLength() > 0);
  output.Reset();

  Buffer precertTBSBuffer;
  if (!precertTBSBuffer.resize(leafCertificate.GetLength())) {
    return Result::FATAL_ERROR_NO_MEMORY;
  }

  PrecertTBSExtractor extractor(leafCertificate,
                                precertTBSBuffer.begin(),
                                precertTBSBuffer.length());
  Result rv = extractor.Init();
  if (rv != Success) {
    return rv;
  }
  Input precertTBS(extractor.GetPrecertTBS());
  MOZ_ASSERT(precertTBS.UnsafeGetData() == precertTBSBuffer.begin());
  precertTBSBuffer.shrinkTo(precertTBS.GetLength());

  output.type = LogEntry::Type::Precert;
  output.tbsCertificate = std::move(precertTBSBuffer);

  if (!output.issuerKeyHash.resizeUninitialized(SHA256_LENGTH)) {
    return Result::FATAL_ERROR_NO_MEMORY;
  }
  return DigestBufNSS(issuerSubjectPublicKeyInfo, DigestAlgorithm::sha256,
                      output.issuerKeyHash.begin(),
                      output.issuerKeyHash.length());
}

Result
GetX509LogEntry(Input leafCertificate, LogEntry& output)
{
  MOZ_ASSERT(leafCertificate.GetLength() > 0);
  output.Reset();
  output.type = LogEntry::Type::X509;
  return InputToBuffer(leafCertificate, output.leafCertificate);
}

} } // namespace mozilla::ct
