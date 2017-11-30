/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSCertificateDB.h"

#include "AppTrustDomain.h"
#include "CryptoTask.h"
#include "NSSCertDBTrustDomain.h"
#include "ScopedNSSTypes.h"
#include "SharedCertVerifier.h"
#include "certdb.h"
#include "cms.h"
#include "mozilla/Base64.h"
#include "mozilla/Casting.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsDependentString.h"
#include "nsHashKeys.h"
#include "nsIDirectoryEnumerator.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsIInputStream.h"
#include "nsIStringEnumerator.h"
#include "nsIZipReader.h"
#include "nsNSSCertificate.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"
#include "nsString.h"
#include "nsTHashtable.h"
#include "pkix/pkix.h"
#include "pkix/pkixnss.h"
#include "plstr.h"
#include "secmime.h"


using namespace mozilla::pkix;
using namespace mozilla;
using namespace mozilla::psm;

extern mozilla::LazyLogModule gPIPNSSLog;

namespace {

// A convenient way to pair the bytes of a digest with the algorithm that
// purportedly produced those bytes. Only SHA-1 and SHA-256 are supported.
struct DigestWithAlgorithm
{
  nsresult ValidateLength() const
  {
    size_t hashLen;
    switch (mAlgorithm) {
      case SEC_OID_SHA256:
        hashLen = SHA256_LENGTH;
        break;
      case SEC_OID_SHA1:
        hashLen = SHA1_LENGTH;
        break;
      default:
        MOZ_ASSERT_UNREACHABLE(
          "unsupported hash type in DigestWithAlgorithm::ValidateLength");
        return NS_ERROR_FAILURE;
    }
    if (mDigest.Length() != hashLen) {
      return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
    }
    return NS_OK;
  }

  nsAutoCString mDigest;
  SECOidTag mAlgorithm;
};

// The digest must have a lifetime greater than or equal to the returned string.
inline nsDependentCSubstring
DigestToDependentString(const Digest& digest)
{
  return nsDependentCSubstring(
    BitwiseCast<char*, unsigned char*>(digest.get().data),
    digest.get().len);
}

// Reads a maximum of 1MB from a stream into the supplied buffer.
// The reason for the 1MB limit is because this function is used to read
// signature-related files and we want to avoid OOM. The uncompressed length of
// an entry can be hundreds of times larger than the compressed version,
// especially if someone has specifically crafted the entry to cause OOM or to
// consume massive amounts of disk space.
//
// @param stream  The input stream to read from.
// @param buf     The buffer that we read the stream into, which must have
//                already been allocated.
nsresult
ReadStream(const nsCOMPtr<nsIInputStream>& stream, /*out*/ SECItem& buf)
{
  // The size returned by Available() might be inaccurate so we need
  // to check that Available() matches up with the actual length of
  // the file.
  uint64_t length;
  nsresult rv = stream->Available(&length);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Cap the maximum accepted size of signature-related files at 1MB (which is
  // still crazily huge) to avoid OOM. The uncompressed length of an entry can be
  // hundreds of times larger than the compressed version, especially if
  // someone has speifically crafted the entry to cause OOM or to consume
  // massive amounts of disk space.
  static const uint32_t MAX_LENGTH = 1024 * 1024;
  if (length > MAX_LENGTH) {
    return NS_ERROR_FILE_TOO_BIG;
  }

  // With bug 164695 in mind we +1 to leave room for null-terminating
  // the buffer.
  SECITEM_AllocItem(buf, static_cast<uint32_t>(length + 1));

  // buf.len == length + 1. We attempt to read length + 1 bytes
  // instead of length, so that we can check whether the metadata for
  // the entry is incorrect.
  uint32_t bytesRead;
  rv = stream->Read(BitwiseCast<char*, unsigned char*>(buf.data), buf.len,
                    &bytesRead);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (bytesRead != length) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  buf.data[buf.len - 1] = 0; // null-terminate

  return NS_OK;
}

// Finds exactly one (signature metadata) JAR entry that matches the given
// search pattern, and then loads it. Fails if there are no matches or if
// there is more than one match. If bufDigest is not null then on success
// bufDigest will contain the digeset of the entry using the given digest
// algorithm.
nsresult
FindAndLoadOneEntry(nsIZipReader* zip,
                    const nsACString& searchPattern,
                    /*out*/ nsACString& filename,
                    /*out*/ SECItem& buf,
                    /*optional, in*/ SECOidTag digestAlgorithm = SEC_OID_SHA1,
                    /*optional, out*/ Digest* bufDigest = nullptr)
{
  nsCOMPtr<nsIUTF8StringEnumerator> files;
  nsresult rv = zip->FindEntries(searchPattern, getter_AddRefs(files));
  if (NS_FAILED(rv) || !files) {
    return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
  }

  bool more;
  rv = files->HasMore(&more);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!more) {
    return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
  }

  rv = files->GetNext(filename);
  NS_ENSURE_SUCCESS(rv, rv);

  // Check if there is more than one match, if so then error!
  rv = files->HasMore(&more);
  NS_ENSURE_SUCCESS(rv, rv);
  if (more) {
    return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
  }

  nsCOMPtr<nsIInputStream> stream;
  rv = zip->GetInputStream(filename, getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ReadStream(stream, buf);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_SIGNED_JAR_ENTRY_INVALID;
  }

  if (bufDigest) {
    rv = bufDigest->DigestBuf(digestAlgorithm, buf.data, buf.len - 1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

// Verify the digest of an entry. We avoid loading the entire entry into memory
// at once, which would require memory in proportion to the size of the largest
// entry. Instead, we require only a small, fixed amount of memory.
//
// @param stream  an input stream from a JAR entry or file depending on whether
//                it is from a signed archive or unpacked into a directory
// @param digestFromManifest The digest that we're supposed to check the file's
//                           contents against, from the manifest
// @param buf A scratch buffer that we use for doing the I/O, which must have
//            already been allocated. The size of this buffer is the unit
//            size of our I/O.
nsresult
VerifyStreamContentDigest(nsIInputStream* stream,
                          const DigestWithAlgorithm& digestFromManifest,
                          SECItem& buf)
{
  MOZ_ASSERT(buf.len > 0);
  nsresult rv = digestFromManifest.ValidateLength();
  if (NS_FAILED(rv)) {
    return rv;
  }

  uint64_t len64;
  rv = stream->Available(&len64);
  NS_ENSURE_SUCCESS(rv, rv);
  if (len64 > UINT32_MAX) {
    return NS_ERROR_SIGNED_JAR_ENTRY_TOO_LARGE;
  }

  UniquePK11Context digestContext(
    PK11_CreateDigestContext(digestFromManifest.mAlgorithm));
  if (!digestContext) {
    return mozilla::psm::GetXPCOMFromNSSError(PR_GetError());
  }

  rv = MapSECStatus(PK11_DigestBegin(digestContext.get()));
  NS_ENSURE_SUCCESS(rv, rv);

  uint64_t totalBytesRead = 0;
  for (;;) {
    uint32_t bytesRead;
    rv = stream->Read(BitwiseCast<char*, unsigned char*>(buf.data), buf.len,
                      &bytesRead);
    NS_ENSURE_SUCCESS(rv, rv);

    if (bytesRead == 0) {
      break; // EOF
    }

    totalBytesRead += bytesRead;
    if (totalBytesRead >= UINT32_MAX) {
      return NS_ERROR_SIGNED_JAR_ENTRY_TOO_LARGE;
    }

    rv = MapSECStatus(PK11_DigestOp(digestContext.get(), buf.data, bytesRead));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (totalBytesRead != len64) {
    // The metadata we used for Available() doesn't match the actual size of
    // the entry.
    return NS_ERROR_SIGNED_JAR_ENTRY_INVALID;
  }

  // Verify that the digests match.
  Digest digest;
  rv = digest.End(digestFromManifest.mAlgorithm, digestContext);
  NS_ENSURE_SUCCESS(rv, rv);

  nsDependentCSubstring digestStr(DigestToDependentString(digest));
  if (!digestStr.Equals(digestFromManifest.mDigest)) {
    return NS_ERROR_SIGNED_JAR_MODIFIED_ENTRY;
  }

  return NS_OK;
}

nsresult
VerifyEntryContentDigest(nsIZipReader* zip, const nsACString& aFilename,
                         const DigestWithAlgorithm& digestFromManifest,
                         SECItem& buf)
{
  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = zip->GetInputStream(aFilename, getter_AddRefs(stream));
  if (NS_FAILED(rv)) {
    return NS_ERROR_SIGNED_JAR_ENTRY_MISSING;
  }

  return VerifyStreamContentDigest(stream, digestFromManifest, buf);
}

// On input, nextLineStart is the start of the current line. On output,
// nextLineStart is the start of the next line.
nsresult
ReadLine(/*in/out*/ const char* & nextLineStart, /*out*/ nsCString & line,
         bool allowContinuations = true)
{
  line.Truncate();
  size_t previousLength = 0;
  size_t currentLength = 0;
  for (;;) {
    const char* eol = PL_strpbrk(nextLineStart, "\r\n");

    if (!eol) { // Reached end of file before newline
      eol = nextLineStart + strlen(nextLineStart);
    }

    previousLength = currentLength;
    line.Append(nextLineStart, eol - nextLineStart);
    currentLength = line.Length();

    // The spec says "No line may be longer than 72 bytes (not characters)"
    // in its UTF8-encoded form.
    static const size_t lineLimit = 72;
    if (currentLength - previousLength > lineLimit) {
      return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
    }

    // The spec says: "Implementations should support 65535-byte
    // (not character) header values..."
    if (currentLength > 65535) {
      return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
    }

    if (*eol == '\r') {
      ++eol;
    }
    if (*eol == '\n') {
      ++eol;
    }

    nextLineStart = eol;

    if (*eol != ' ') {
      // not a continuation
      return NS_OK;
    }

    // continuation
    if (!allowContinuations) {
      return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
    }

    ++nextLineStart; // skip space and keep appending
  }
}

// The header strings are defined in the JAR specification.
#define JAR_MF_SEARCH_STRING "(M|/M)ETA-INF/(M|m)(ANIFEST|anifest).(MF|mf)$"
#define JAR_SF_SEARCH_STRING "(M|/M)ETA-INF/*.(SF|sf)$"
#define JAR_RSA_SEARCH_STRING "(M|/M)ETA-INF/*.(RSA|rsa)$"
#define JAR_META_DIR "META-INF"
#define JAR_MF_HEADER "Manifest-Version: 1.0"
#define JAR_SF_HEADER "Signature-Version: 1.0"

nsresult
ParseAttribute(const nsAutoCString & curLine,
               /*out*/ nsAutoCString & attrName,
               /*out*/ nsAutoCString & attrValue)
{
  // Find the colon that separates the name from the value.
  int32_t colonPos = curLine.FindChar(':');
  if (colonPos == kNotFound) {
    return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
  }

  // set attrName to the name, skipping spaces between the name and colon
  int32_t nameEnd = colonPos;
  for (;;) {
    if (nameEnd == 0) {
      return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID; // colon with no name
    }
    if (curLine[nameEnd - 1] != ' ')
      break;
    --nameEnd;
  }
  curLine.Left(attrName, nameEnd);

  // Set attrValue to the value, skipping spaces between the colon and the
  // value. The value may be empty.
  int32_t valueStart = colonPos + 1;
  int32_t curLineLength = curLine.Length();
  while (valueStart != curLineLength && curLine[valueStart] == ' ') {
    ++valueStart;
  }
  curLine.Right(attrValue, curLineLength - valueStart);

  return NS_OK;
}

// Parses the version line of the MF or SF header.
nsresult
CheckManifestVersion(const char* & nextLineStart,
                     const nsACString & expectedHeader)
{
  // The JAR spec says: "Manifest-Version and Signature-Version must be first,
  // and in exactly that case (so that they can be recognized easily as magic
  // strings)."
  nsAutoCString curLine;
  nsresult rv = ReadLine(nextLineStart, curLine, false);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!curLine.Equals(expectedHeader)) {
    return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
  }
  return NS_OK;
}

// Parses a signature file (SF) based on the JDK 8 JAR Specification.
//
// The SF file must contain a SHA*-Digest-Manifest attribute in the main
// section (where the * is either 1 or 256, depending on the given digest
// algorithm). All other sections are ignored. This means that this will NOT
// parse old-style signature files that have separate digests per entry.
// The JDK8 x-Digest-Manifest variant is better because:
//
//   (1) It allows us to follow the principle that we should minimize the
//       processing of data that we do before we verify its signature. In
//       particular, with the x-Digest-Manifest style, we can verify the digest
//       of MANIFEST.MF before we parse it, which prevents malicious JARs
//       exploiting our MANIFEST.MF parser.
//   (2) It is more time-efficient and space-efficient to have one
//       x-Digest-Manifest instead of multiple x-Digest values.
//
// filebuf must be null-terminated. On output, mfDigest will contain the
// decoded value of the appropriate SHA*-DigestManifest, if found.
nsresult
ParseSF(const char* filebuf, SECOidTag digestAlgorithm,
        /*out*/ nsAutoCString& mfDigest)
{
  const char* digestNameToFind = nullptr;
  switch (digestAlgorithm) {
    case SEC_OID_SHA256:
      digestNameToFind = "sha256-digest-manifest";
      break;
    case SEC_OID_SHA1:
      digestNameToFind = "sha1-digest-manifest";
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("bad argument to ParseSF");
      return NS_ERROR_FAILURE;
  }

  const char* nextLineStart = filebuf;
  nsresult rv = CheckManifestVersion(nextLineStart,
                                     NS_LITERAL_CSTRING(JAR_SF_HEADER));
  if (NS_FAILED(rv)) {
    return rv;
  }

  for (;;) {
    nsAutoCString curLine;
    rv = ReadLine(nextLineStart, curLine);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (curLine.Length() == 0) {
      // End of main section (blank line or end-of-file). We didn't find the
      // SHA*-Digest-Manifest we were looking for.
      return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
    }

    nsAutoCString attrName;
    nsAutoCString attrValue;
    rv = ParseAttribute(curLine, attrName, attrValue);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (attrName.EqualsIgnoreCase(digestNameToFind)) {
      rv = Base64Decode(attrValue, mfDigest);
      if (NS_FAILED(rv)) {
        return rv;
      }

      // There could be multiple SHA*-Digest-Manifest attributes, which
      // would be an error, but it's better to just skip any erroneous
      // duplicate entries rather than trying to detect them, because:
      //
      //   (1) It's simpler, and simpler generally means more secure
      //   (2) An attacker can't make us accept a JAR we would otherwise
      //       reject just by adding additional SHA*-Digest-Manifest
      //       attributes.
      return NS_OK;
    }

    // ignore unrecognized attributes
  }

  MOZ_ASSERT_UNREACHABLE("somehow exited loop in ParseSF without returning");
  return NS_ERROR_FAILURE;
}

// Parses MANIFEST.MF. The filenames of all entries will be returned in
// mfItems. buf must be a pre-allocated scratch buffer that is used for doing
// I/O. Each file's contents are verified against the entry in the manifest with
// the digest algorithm that matches the given one. This algorithm comes from
// the signature file. If the signature file has a SHA-256 digest, then SHA-256
// entries must be present in the manifest file. If the signature file only has
// a SHA-1 digest, then only SHA-1 digests will be used in the manifest file.
nsresult
ParseMF(const char* filebuf, nsIZipReader* zip, SECOidTag digestAlgorithm,
        /*out*/ nsTHashtable<nsCStringHashKey>& mfItems, ScopedAutoSECItem& buf)
{
  const char* digestNameToFind = nullptr;
  switch (digestAlgorithm) {
    case SEC_OID_SHA256:
      digestNameToFind = "sha256-digest";
      break;
    case SEC_OID_SHA1:
      digestNameToFind = "sha1-digest";
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("bad argument to ParseMF");
      return NS_ERROR_FAILURE;
  }

  const char* nextLineStart = filebuf;
  nsresult rv = CheckManifestVersion(nextLineStart,
                                     NS_LITERAL_CSTRING(JAR_MF_HEADER));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Skip the rest of the header section, which ends with a blank line.
  {
    nsAutoCString line;
    do {
      rv = ReadLine(nextLineStart, line);
      if (NS_FAILED(rv)) {
        return rv;
      }
    } while (line.Length() > 0);

    // Manifest containing no file entries is OK, though useless.
    if (*nextLineStart == '\0') {
      return NS_OK;
    }
  }

  nsAutoCString curItemName;
  nsAutoCString digest;

  for (;;) {
    nsAutoCString curLine;
    rv = ReadLine(nextLineStart, curLine);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (curLine.Length() == 0) {
      // end of section (blank line or end-of-file)

      if (curItemName.Length() == 0) {
        // '...Each section must start with an attribute with the name as
        // "Name",...', so every section must have a Name attribute.
        return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
      }

      if (digest.IsEmpty()) {
        // We require every entry to have a digest, since we require every
        // entry to be signed and we don't allow duplicate entries.
        return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
      }

      if (mfItems.Contains(curItemName)) {
        // Duplicate entry
        return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
      }

      // Verify that the entry's content digest matches the digest from this
      // MF section.
      DigestWithAlgorithm digestWithAlgorithm = { digest, digestAlgorithm };
      rv = VerifyEntryContentDigest(zip, curItemName, digestWithAlgorithm, buf);
      if (NS_FAILED(rv)) {
        return rv;
      }

      mfItems.PutEntry(curItemName);

      if (*nextLineStart == '\0') {
        // end-of-file
        break;
      }

      // reset so we know we haven't encountered either of these for the next
      // item yet.
      curItemName.Truncate();
      digest.Truncate();

      continue; // skip the rest of the loop below
    }

    nsAutoCString attrName;
    nsAutoCString attrValue;
    rv = ParseAttribute(curLine, attrName, attrValue);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Lines to look for:

    // (1) Digest:
    if (attrName.EqualsIgnoreCase(digestNameToFind)) {
      if (!digest.IsEmpty()) { // multiple SHA* digests in section
        return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
      }

      rv = Base64Decode(attrValue, digest);
      if (NS_FAILED(rv)) {
        return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
      }

      continue;
    }

    // (2) Name: associates this manifest section with a file in the jar.
    if (attrName.LowerCaseEqualsLiteral("name")) {
      if (MOZ_UNLIKELY(curItemName.Length() > 0)) // multiple names in section
        return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;

      if (MOZ_UNLIKELY(attrValue.Length() == 0))
        return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;

      curItemName = attrValue;

      continue;
    }

    // (3) Magic: the only other must-understand attribute
    if (attrName.LowerCaseEqualsLiteral("magic")) {
      // We don't understand any magic, so we can't verify an entry that
      // requires magic. Since we require every entry to have a valid
      // signature, we have no choice but to reject the entry.
      return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
    }

    // unrecognized attributes must be ignored
  }

  return NS_OK;
}

nsresult
VerifyCertificate(CERTCertificate* signerCert, AppTrustedRoot trustedRoot,
                  /*out*/ UniqueCERTCertList& builtChain)
{
  if (NS_WARN_IF(!signerCert)) {
    return NS_ERROR_INVALID_ARG;
  }
  // TODO: pinArg is null.
  AppTrustDomain trustDomain(builtChain, nullptr);
  nsresult rv = trustDomain.SetTrustedRoot(trustedRoot);
  if (NS_FAILED(rv)) {
    return rv;
  }
  Input certDER;
  mozilla::pkix::Result result = certDER.Init(signerCert->derCert.data,
                                              signerCert->derCert.len);
  if (result != Success) {
    return mozilla::psm::GetXPCOMFromNSSError(MapResultToPRErrorCode(result));
  }

  result = BuildCertChain(trustDomain, certDER, Now(),
                          EndEntityOrCA::MustBeEndEntity,
                          KeyUsage::digitalSignature,
                          KeyPurposeId::id_kp_codeSigning,
                          CertPolicyId::anyPolicy,
                          nullptr /*stapledOCSPResponse*/);
  if (result == mozilla::pkix::Result::ERROR_EXPIRED_CERTIFICATE) {
    // For code-signing you normally need trusted 3rd-party timestamps to
    // handle expiration properly. The signer could always mess with their
    // system clock so you can't trust the certificate was un-expired when
    // the signing took place. The choice is either to ignore expiration
    // or to enforce expiration at time of use. The latter leads to the
    // user-hostile result that perfectly good code stops working.
    //
    // Our package format doesn't support timestamps (nor do we have a
    // trusted 3rd party timestamper), but since we sign all of our apps and
    // add-ons ourselves we can trust ourselves not to mess with the clock
    // on the signing systems. We also have a revocation mechanism if we
    // need it. It's OK to ignore cert expiration under these conditions.
    //
    // This is an invalid approach if
    //  * we issue certs to let others sign their own packages
    //  * mozilla::pkix returns "expired" when there are "worse" problems
    //    with the certificate or chain.
    // (see bug 1267318)
    result = Success;
  }
  if (result != Success) {
    return mozilla::psm::GetXPCOMFromNSSError(MapResultToPRErrorCode(result));
  }

  return NS_OK;
}

// Given a SECOidTag representing a digest algorithm (either SEC_OID_SHA1 or
// SEC_OID_SHA256), returns the first signerInfo in the given signedData that
// purports to have been created using that digest algorithm, or nullptr if
// there is none.
// The returned signerInfo is owned by signedData, so the caller must ensure
// that the lifetime of the signerInfo is contained by the lifetime of the
// signedData.
NSSCMSSignerInfo*
GetSignerInfoForDigestAlgorithm(NSSCMSSignedData* signedData,
                                SECOidTag digestAlgorithm)
{
  MOZ_ASSERT(digestAlgorithm == SEC_OID_SHA1 ||
             digestAlgorithm == SEC_OID_SHA256);
  if (digestAlgorithm != SEC_OID_SHA1 && digestAlgorithm != SEC_OID_SHA256) {
    return nullptr;
  }

  int numSigners = NSS_CMSSignedData_SignerInfoCount(signedData);
  if (numSigners < 1) {
    return nullptr;
  }
  for (int i = 0; i < numSigners; i++) {
    NSSCMSSignerInfo* signerInfo =
      NSS_CMSSignedData_GetSignerInfo(signedData, i);
    // NSS_CMSSignerInfo_GetDigestAlgTag isn't exported from NSS.
    SECOidData* digestAlgOID = SECOID_FindOID(&signerInfo->digestAlg.algorithm);
    if (!digestAlgOID) {
      continue;
    }
    if (digestAlgorithm == digestAlgOID->offset) {
      return signerInfo;
    }
  }
  return nullptr;
}

nsresult
VerifySignature(AppTrustedRoot trustedRoot, const SECItem& buffer,
                const SECItem& detachedSHA1Digest,
                const SECItem& detachedSHA256Digest,
                /*out*/ SECOidTag& digestAlgorithm,
                /*out*/ UniqueCERTCertList& builtChain)
{
  // Currently, this function is only called within the CalculateResult() method
  // of CryptoTasks. As such, NSS should not be shut down at this point and the
  // CryptoTask implementation should already hold a nsNSSShutDownPreventionLock.

  if (NS_WARN_IF(!buffer.data || buffer.len == 0 || !detachedSHA1Digest.data ||
                 detachedSHA1Digest.len == 0 || !detachedSHA256Digest.data ||
                 detachedSHA256Digest.len == 0)) {
    return NS_ERROR_INVALID_ARG;
  }

  UniqueNSSCMSMessage
    cmsMsg(NSS_CMSMessage_CreateFromDER(const_cast<SECItem*>(&buffer), nullptr,
                                        nullptr, nullptr, nullptr, nullptr,
                                        nullptr));
  if (!cmsMsg) {
    return NS_ERROR_CMS_VERIFY_NOT_SIGNED;
  }

  if (!NSS_CMSMessage_IsSigned(cmsMsg.get())) {
    return NS_ERROR_CMS_VERIFY_NOT_SIGNED;
  }

  NSSCMSContentInfo* cinfo = NSS_CMSMessage_ContentLevel(cmsMsg.get(), 0);
  if (!cinfo) {
    return NS_ERROR_CMS_VERIFY_NO_CONTENT_INFO;
  }

  // We're expecting this to be a PKCS#7 signedData content info.
  if (NSS_CMSContentInfo_GetContentTypeTag(cinfo)
        != SEC_OID_PKCS7_SIGNED_DATA) {
    return NS_ERROR_CMS_VERIFY_NO_CONTENT_INFO;
  }

  // signedData is non-owning
  NSSCMSSignedData* signedData =
    static_cast<NSSCMSSignedData*>(NSS_CMSContentInfo_GetContent(cinfo));
  if (!signedData) {
    return NS_ERROR_CMS_VERIFY_NO_CONTENT_INFO;
  }

  // Parse the certificates into CERTCertificate objects held in memory so
  // verifyCertificate will be able to find them during path building.
  UniqueCERTCertList certs(CERT_NewCertList());
  if (!certs) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (signedData->rawCerts) {
    for (size_t i = 0; signedData->rawCerts[i]; ++i) {
      UniqueCERTCertificate
        cert(CERT_NewTempCertificate(CERT_GetDefaultCertDB(),
                                     signedData->rawCerts[i], nullptr, false,
                                     true));
      // Skip certificates that fail to parse
      if (!cert) {
        continue;
      }

      if (CERT_AddCertToListTail(certs.get(), cert.get()) != SECSuccess) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      Unused << cert.release(); // Ownership transferred to the cert list.
    }
  }

  NSSCMSSignerInfo* signerInfo =
    GetSignerInfoForDigestAlgorithm(signedData, SEC_OID_SHA256);
  const SECItem* detachedDigest = &detachedSHA256Digest;
  digestAlgorithm = SEC_OID_SHA256;
  if (!signerInfo) {
    signerInfo = GetSignerInfoForDigestAlgorithm(signedData, SEC_OID_SHA1);
    if (!signerInfo) {
      return NS_ERROR_CMS_VERIFY_NOT_SIGNED;
    }
    detachedDigest = &detachedSHA1Digest;
    digestAlgorithm = SEC_OID_SHA1;
  }

  // Get the end-entity certificate.
  CERTCertificate* signerCert =
    NSS_CMSSignerInfo_GetSigningCertificate(signerInfo,
                                            CERT_GetDefaultCertDB());
  if (!signerCert) {
    return NS_ERROR_CMS_VERIFY_ERROR_PROCESSING;
  }

  nsresult rv = VerifyCertificate(signerCert, trustedRoot, builtChain);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Ensure that the PKCS#7 data OID is present as the PKCS#9 contentType.
  const char* pkcs7DataOidString = "1.2.840.113549.1.7.1";
  ScopedAutoSECItem pkcs7DataOid;
  if (SEC_StringToOID(nullptr, &pkcs7DataOid, pkcs7DataOidString, 0)
        != SECSuccess) {
    return NS_ERROR_CMS_VERIFY_ERROR_PROCESSING;
  }

  return MapSECStatus(
    NSS_CMSSignerInfo_Verify(signerInfo, const_cast<SECItem*>(detachedDigest),
                             &pkcs7DataOid));
}

// This corresponds to the preference "security.signed_app_signatures.policy".
enum class SignaturePolicy {
  PKCS7WithSHA1OrSHA256 = 0,
  PKCS7WithSHA256 = 1,
};

nsresult
OpenSignedAppFile(AppTrustedRoot aTrustedRoot, nsIFile* aJarFile,
                  SignaturePolicy aPolicy,
                  /*out, optional */ nsIZipReader** aZipReader,
                  /*out, optional */ nsIX509Cert** aSignerCert)
{
  NS_ENSURE_ARG_POINTER(aJarFile);

  if (aZipReader) {
    *aZipReader = nullptr;
  }

  if (aSignerCert) {
    *aSignerCert = nullptr;
  }

  nsresult rv;

  static NS_DEFINE_CID(kZipReaderCID, NS_ZIPREADER_CID);
  nsCOMPtr<nsIZipReader> zip = do_CreateInstance(kZipReaderCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = zip->Open(aJarFile);
  NS_ENSURE_SUCCESS(rv, rv);

  // Signature (RSA) file
  nsAutoCString sigFilename;
  ScopedAutoSECItem sigBuffer;
  rv = FindAndLoadOneEntry(zip, nsLiteralCString(JAR_RSA_SEARCH_STRING),
                           sigFilename, sigBuffer);
  if (NS_FAILED(rv)) {
    return NS_ERROR_SIGNED_JAR_NOT_SIGNED;
  }

  // Signature (SF) file
  nsAutoCString sfFilename;
  ScopedAutoSECItem sfBuffer;
  rv = FindAndLoadOneEntry(zip, NS_LITERAL_CSTRING(JAR_SF_SEARCH_STRING),
                           sfFilename, sfBuffer);
  if (NS_FAILED(rv)) {
    return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
  }

  // Calculate both the SHA-1 and SHA-256 hashes of the signature file - we
  // don't know what algorithm the PKCS#7 signature used.
  Digest sfCalculatedSHA1Digest;
  rv = sfCalculatedSHA1Digest.DigestBuf(SEC_OID_SHA1, sfBuffer.data,
                                        sfBuffer.len - 1);
  if (NS_FAILED(rv)) {
    return rv;
  }
  Digest sfCalculatedSHA256Digest;
  rv = sfCalculatedSHA256Digest.DigestBuf(SEC_OID_SHA256, sfBuffer.data,
                                          sfBuffer.len - 1);
  if (NS_FAILED(rv)) {
    return rv;
  }

  sigBuffer.type = siBuffer;
  UniqueCERTCertList builtChain;
  SECOidTag digestToUse;
  rv = VerifySignature(aTrustedRoot, sigBuffer, sfCalculatedSHA1Digest.get(),
                       sfCalculatedSHA256Digest.get(), digestToUse, builtChain);
  if (NS_FAILED(rv)) {
    return rv;
  }

  switch (aPolicy) {
    case SignaturePolicy::PKCS7WithSHA256:
      if (digestToUse != SEC_OID_SHA256) {
        return NS_ERROR_SIGNED_JAR_WRONG_SIGNATURE;
      }
      break;
    case SignaturePolicy::PKCS7WithSHA1OrSHA256:
      break;
  }

  nsAutoCString mfDigest;
  rv = ParseSF(BitwiseCast<char*, unsigned char*>(sfBuffer.data), digestToUse,
               mfDigest);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Manifest (MF) file
  nsAutoCString mfFilename;
  ScopedAutoSECItem manifestBuffer;
  Digest mfCalculatedDigest;
  rv = FindAndLoadOneEntry(zip, NS_LITERAL_CSTRING(JAR_MF_SEARCH_STRING),
                           mfFilename, manifestBuffer, digestToUse,
                           &mfCalculatedDigest);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsDependentCSubstring calculatedDigest(
    DigestToDependentString(mfCalculatedDigest));
  if (!mfDigest.Equals(calculatedDigest)) {
    return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
  }

  // Allocate the I/O buffer only once per JAR, instead of once per entry, in
  // order to minimize malloc/free calls and in order to avoid fragmenting
  // memory.
  ScopedAutoSECItem buf(128 * 1024);

  nsTHashtable<nsCStringHashKey> items;

  rv = ParseMF(BitwiseCast<char*, unsigned char*>(manifestBuffer.data), zip,
               digestToUse, items, buf);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Verify every entry in the file.
  nsCOMPtr<nsIUTF8StringEnumerator> entries;
  rv = zip->FindEntries(EmptyCString(), getter_AddRefs(entries));
  if (NS_SUCCEEDED(rv) && !entries) {
    rv = NS_ERROR_UNEXPECTED;
  }
  if (NS_FAILED(rv)) {
    return rv;
  }

  for (;;) {
    bool hasMore;
    rv = entries->HasMore(&hasMore);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!hasMore) {
      break;
    }

    nsAutoCString entryFilename;
    rv = entries->GetNext(entryFilename);
    NS_ENSURE_SUCCESS(rv, rv);

    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("Verifying digests for %s",
           entryFilename.get()));

    // The files that comprise the signature mechanism are not covered by the
    // signature.
    //
    // XXX: This is OK for a single signature, but doesn't work for
    // multiple signatures, because the metadata for the other signatures
    // is not signed either.
    if (entryFilename == mfFilename ||
        entryFilename == sfFilename ||
        entryFilename == sigFilename) {
      continue;
    }

    if (entryFilename.Length() == 0) {
      return NS_ERROR_SIGNED_JAR_ENTRY_INVALID;
    }

    // Entries with names that end in "/" are directory entries, which are not
    // signed.
    //
    // XXX: As long as we don't unpack the JAR into the filesystem, the "/"
    // entries are harmless. But, it is not clear what the security
    // implications of directory entries are if/when we were to unpackage the
    // JAR into the filesystem.
    if (entryFilename[entryFilename.Length() - 1] == '/') {
      continue;
    }

    nsCStringHashKey * item = items.GetEntry(entryFilename);
    if (!item) {
      return NS_ERROR_SIGNED_JAR_UNSIGNED_ENTRY;
    }

    // Remove the item so we can check for leftover items later
    items.RemoveEntry(item);
  }

  // We verified that every entry that we require to be signed is signed. But,
  // were there any missing entries--that is, entries that are mentioned in the
  // manifest but missing from the archive?
  if (items.Count() != 0) {
    return NS_ERROR_SIGNED_JAR_ENTRY_MISSING;
  }

  // Return the reader to the caller if they want it
  if (aZipReader) {
    zip.forget(aZipReader);
  }

  // Return the signer's certificate to the reader if they want it.
  // XXX: We should return an nsIX509CertList with the whole validated chain.
  if (aSignerCert) {
    CERTCertListNode* signerCertNode = CERT_LIST_HEAD(builtChain);
    if (!signerCertNode || CERT_LIST_END(signerCertNode, builtChain) ||
        !signerCertNode->cert) {
      return NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsIX509Cert> signerCert =
      nsNSSCertificate::Create(signerCertNode->cert);
    NS_ENSURE_TRUE(signerCert, NS_ERROR_OUT_OF_MEMORY);
    signerCert.forget(aSignerCert);
  }

  return NS_OK;
}

class OpenSignedAppFileTask final : public CryptoTask
{
public:
  OpenSignedAppFileTask(AppTrustedRoot aTrustedRoot, nsIFile* aJarFile,
                        SignaturePolicy aPolicy,
                        nsIOpenSignedAppFileCallback* aCallback)
    : mTrustedRoot(aTrustedRoot)
    , mJarFile(aJarFile)
    , mPolicy(aPolicy)
    , mCallback(new nsMainThreadPtrHolder<nsIOpenSignedAppFileCallback>(
        "OpenSignedAppFileTask::mCallback", aCallback))
  {
  }

private:
  virtual nsresult CalculateResult() override
  {
    return OpenSignedAppFile(mTrustedRoot, mJarFile, mPolicy,
                             getter_AddRefs(mZipReader),
                             getter_AddRefs(mSignerCert));
  }

  // nsNSSCertificate implements nsNSSShutdownObject, so there's nothing that
  // needs to be released
  virtual void ReleaseNSSResources() override { }

  virtual void CallCallback(nsresult rv) override
  {
    (void) mCallback->OpenSignedAppFileFinished(rv, mZipReader, mSignerCert);
  }

  const AppTrustedRoot mTrustedRoot;
  const nsCOMPtr<nsIFile> mJarFile;
  const SignaturePolicy mPolicy;
  nsMainThreadPtrHandle<nsIOpenSignedAppFileCallback> mCallback;
  nsCOMPtr<nsIZipReader> mZipReader; // out
  nsCOMPtr<nsIX509Cert> mSignerCert; // out
};

static const SignaturePolicy sDefaultSignaturePolicy =
  SignaturePolicy::PKCS7WithSHA1OrSHA256;

} // unnamed namespace

NS_IMETHODIMP
nsNSSCertificateDB::OpenSignedAppFileAsync(
  AppTrustedRoot aTrustedRoot, nsIFile* aJarFile,
  nsIOpenSignedAppFileCallback* aCallback)
{
  NS_ENSURE_ARG_POINTER(aJarFile);
  NS_ENSURE_ARG_POINTER(aCallback);
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }
  SignaturePolicy policy =
    static_cast<SignaturePolicy>(
      Preferences::GetInt("security.signed_app_signatures.policy",
                          static_cast<int32_t>(sDefaultSignaturePolicy)));
  switch (policy) {
    case SignaturePolicy::PKCS7WithSHA1OrSHA256:
      break;
    case SignaturePolicy::PKCS7WithSHA256:
      break;
    default:
      policy = sDefaultSignaturePolicy;
      break;
  }
  RefPtr<OpenSignedAppFileTask> task(new OpenSignedAppFileTask(aTrustedRoot,
                                                               aJarFile,
                                                               policy,
                                                               aCallback));
  return task->Dispatch("SignedJAR");
}

NS_IMETHODIMP
nsNSSCertificateDB::VerifySignedDirectoryAsync(AppTrustedRoot, nsIFile*,
  nsIVerifySignedDirectoryCallback* aCallback)
{
  NS_ENSURE_ARG_POINTER(aCallback);
  return aCallback->VerifySignedDirectoryFinished(
    NS_ERROR_SIGNED_JAR_NOT_SIGNED, nullptr);
}
