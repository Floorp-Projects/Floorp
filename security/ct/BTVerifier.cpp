/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BTVerifier.h"

#include <stdint.h>

#include "CTUtils.h"
#include "SignedCertificateTimestamp.h"
#include "hasht.h"
#include "mozpkix/pkixnss.h"
#include "mozpkix/pkixutil.h"

namespace mozilla {
namespace ct {

using namespace mozilla::pkix;

typedef mozilla::pkix::Result Result;

// Common prefix lengths
static const size_t kLogIdPrefixLengthBytes = 1;
static const size_t kBTTreeSizeLength = 8;
static const size_t kNodeHashPrefixLengthBytes = 1;

// Members of a SignedTreeHeadDataV2 struct
static const size_t kSTHTimestampLength = 8;
static const size_t kSTHExtensionsLengthBytes = 2;
static const size_t kSTHSignatureLengthBytes = 2;

// Members of a Inclusion Proof struct
static const size_t kLeafIndexLength = 8;
static const size_t kInclusionPathLengthBytes = 2;

static Result GetDigestAlgorithmLengthAndIdentifier(
    DigestAlgorithm digestAlgorithm,
    /* out */ size_t& digestAlgorithmLength,
    /* out */ SECOidTag& digestAlgorithmId) {
  switch (digestAlgorithm) {
    case DigestAlgorithm::sha512:
      digestAlgorithmLength = SHA512_LENGTH;
      digestAlgorithmId = SEC_OID_SHA512;
      return Success;
    case DigestAlgorithm::sha256:
      digestAlgorithmLength = SHA256_LENGTH;
      digestAlgorithmId = SEC_OID_SHA256;
      return Success;
    default:
      return pkix::Result::FATAL_ERROR_INVALID_ARGS;
  }
}

Result DecodeAndVerifySignedTreeHead(
    Input signerSubjectPublicKeyInfo, DigestAlgorithm digestAlgorithm,
    der::PublicKeyAlgorithm publicKeyAlgorithm, Input signedTreeHeadInput,
    /* out */ SignedTreeHeadDataV2& signedTreeHead) {
  SignedTreeHeadDataV2 result;
  Reader reader(signedTreeHeadInput);

  Input logId;
  Result rv = ReadVariableBytes<kLogIdPrefixLengthBytes>(reader, logId);
  if (rv != Success) {
    return rv;
  }
  InputToBuffer(logId, result.logId);

  // This is the beginning of the data covered by the signature.
  Reader::Mark signedDataMark = reader.GetMark();

  rv = ReadUint<kSTHTimestampLength>(reader, result.timestamp);
  if (rv != Success) {
    return rv;
  }

  rv = ReadUint<kBTTreeSizeLength>(reader, result.treeSize);
  if (rv != Success) {
    return rv;
  }

  Input hash;
  rv = ReadVariableBytes<kNodeHashPrefixLengthBytes>(reader, hash);
  if (rv != Success) {
    return rv;
  }
  InputToBuffer(hash, result.rootHash);

  // We ignore any extensions, but we have to read them.
  Input extensionsInput;
  rv = ReadVariableBytes<kSTHExtensionsLengthBytes>(reader, extensionsInput);
  if (rv != Success) {
    return rv;
  }

  Input signedDataInput;
  rv = reader.GetInput(signedDataMark, signedDataInput);
  if (rv != Success) {
    return rv;
  }

  Input signatureInput;
  rv = ReadVariableBytes<kSTHSignatureLengthBytes>(reader, signatureInput);
  if (rv != Success) {
    return rv;
  }

  switch (publicKeyAlgorithm) {
    case der::PublicKeyAlgorithm::ECDSA:
      rv = VerifyECDSASignedDataNSS(signedDataInput, digestAlgorithm,
                                    signatureInput, signerSubjectPublicKeyInfo,
                                    nullptr);
      break;
    case der::PublicKeyAlgorithm::RSA_PKCS1:
    default:
      return Result::FATAL_ERROR_INVALID_ARGS;
  }
  if (rv != Success) {
    return rv;
  }

  if (!reader.AtEnd()) {
    return pkix::Result::ERROR_BAD_DER;
  }

  signedTreeHead = std::move(result);
  return Success;
}

Result DecodeInclusionProof(Input input, InclusionProofDataV2& output) {
  InclusionProofDataV2 result;
  Reader reader(input);

  Input logId;
  Result rv = ReadVariableBytes<kLogIdPrefixLengthBytes>(reader, logId);
  if (rv != Success) {
    return rv;
  }

  rv = ReadUint<kBTTreeSizeLength>(reader, result.treeSize);
  if (rv != Success) {
    return rv;
  }

  if (result.treeSize < 1) {
    return pkix::Result::ERROR_BAD_DER;
  }

  rv = ReadUint<kLeafIndexLength>(reader, result.leafIndex);
  if (rv != Success) {
    return rv;
  }

  if (result.leafIndex >= result.treeSize) {
    return pkix::Result::ERROR_BAD_DER;
  }

  Input pathInput;
  rv = ReadVariableBytes<kInclusionPathLengthBytes>(reader, pathInput);
  if (rv != Success) {
    return rv;
  }

  if (pathInput.GetLength() < 1) {
    return pkix::Result::ERROR_BAD_DER;
  }

  Reader pathReader(pathInput);
  std::vector<Buffer> inclusionPath;

  while (!pathReader.AtEnd()) {
    Input hash;
    rv = ReadVariableBytes<kNodeHashPrefixLengthBytes>(pathReader, hash);
    if (rv != Success) {
      return rv;
    }

    Buffer hashBuffer;
    InputToBuffer(hash, hashBuffer);

    inclusionPath.push_back(std::move(hashBuffer));
  }

  if (!reader.AtEnd()) {
    return pkix::Result::ERROR_BAD_DER;
  }

  InputToBuffer(logId, result.logId);

  result.inclusionPath = std::move(inclusionPath);

  output = std::move(result);
  return Success;
}

static Result CommonFinishDigest(UniquePK11Context& context,
                                 size_t digestAlgorithmLength,
                                 /* out */ Buffer& outputBuffer) {
  uint32_t outLen = 0;
  outputBuffer.assign(digestAlgorithmLength, 0);
  if (PK11_DigestFinal(context.get(), outputBuffer.data(), &outLen,
                       digestAlgorithmLength) != SECSuccess) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  if (outLen != digestAlgorithmLength) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  return Success;
}

static Result LeafHash(Input leafEntry, size_t digestAlgorithmLength,
                       SECOidTag digestAlgorithmId,
                       /* out */ Buffer& calculatedHash) {
  UniquePK11Context context(PK11_CreateDigestContext(digestAlgorithmId));
  if (!context) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  const unsigned char zero = 0;
  if (PK11_DigestOp(context.get(), &zero, 1u) != SECSuccess) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  SECItem leafEntryItem = UnsafeMapInputToSECItem(leafEntry);
  if (PK11_DigestOp(context.get(), leafEntryItem.data, leafEntryItem.len) !=
      SECSuccess) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  return CommonFinishDigest(context, digestAlgorithmLength, calculatedHash);
}

static Result NodeHash(const Buffer& left, const Buffer& right,
                       size_t digestAlgorithmLength,
                       SECOidTag digestAlgorithmId,
                       /* out */ Buffer& calculatedHash) {
  UniquePK11Context context(PK11_CreateDigestContext(digestAlgorithmId));
  if (!context) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  const unsigned char one = 1;
  if (PK11_DigestOp(context.get(), &one, 1u) != SECSuccess) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  if (PK11_DigestOp(context.get(), left.data(), left.size()) != SECSuccess) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  if (PK11_DigestOp(context.get(), right.data(), right.size()) != SECSuccess) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  return CommonFinishDigest(context, digestAlgorithmLength, calculatedHash);
}

// This algorithm is specified by:
// https://tools.ietf.org/html/draft-ietf-trans-rfc6962-bis-28#section-2.1.3.2
Result VerifyInclusionProof(const InclusionProofDataV2& proof, Input leafEntry,
                            Input expectedRootHash,
                            DigestAlgorithm digestAlgorithm) {
  if (proof.treeSize == 0) {
    return pkix::Result::ERROR_BAD_SIGNATURE;
  }
  size_t digestAlgorithmLength;
  SECOidTag digestAlgorithmId;
  Result rv = GetDigestAlgorithmLengthAndIdentifier(
      digestAlgorithm, digestAlgorithmLength, digestAlgorithmId);
  if (rv != Success) {
    return rv;
  }
  if (proof.leafIndex >= proof.treeSize) {
    return pkix::Result::ERROR_BAD_SIGNATURE;
  }
  if (expectedRootHash.GetLength() != digestAlgorithmLength) {
    return pkix::Result::ERROR_BAD_SIGNATURE;
  }
  uint64_t leafIndex = proof.leafIndex;
  uint64_t lastNodeIndex = proof.treeSize - 1;
  Buffer calculatedHash;
  rv = LeafHash(leafEntry, digestAlgorithmLength, digestAlgorithmId,
                calculatedHash);
  if (rv != Success) {
    return rv;
  }
  for (const auto& hash : proof.inclusionPath) {
    if (lastNodeIndex == 0) {
      return pkix::Result::ERROR_BAD_SIGNATURE;
    }
    if (leafIndex % 2 == 1 || leafIndex == lastNodeIndex) {
      rv = NodeHash(hash, calculatedHash, digestAlgorithmLength,
                    digestAlgorithmId, calculatedHash);
      if (rv != Success) {
        return rv;
      }
      if (leafIndex % 2 == 0) {
        while (leafIndex % 2 == 0 && lastNodeIndex > 0) {
          leafIndex >>= 1;
          lastNodeIndex >>= 1;
        }
      }
    } else {
      rv = NodeHash(calculatedHash, hash, digestAlgorithmLength,
                    digestAlgorithmId, calculatedHash);
      if (rv != Success) {
        return rv;
      }
    }
    leafIndex >>= 1;
    lastNodeIndex >>= 1;
  }
  if (lastNodeIndex != 0) {
    return pkix::Result::ERROR_BAD_SIGNATURE;
  }
  assert(calculatedHash.size() == digestAlgorithmLength);
  if (calculatedHash.size() != digestAlgorithmLength) {
    return pkix::Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  if (memcmp(calculatedHash.data(), expectedRootHash.UnsafeGetData(),
             digestAlgorithmLength) != 0) {
    return pkix::Result::ERROR_BAD_SIGNATURE;
  }
  return Success;
}

}  // namespace ct
}  // namespace mozilla
