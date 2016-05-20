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
#include "base64.h"
#include "certdb.h"
#include "mozilla/Casting.h"
#include "mozilla/Logging.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsDataSignatureVerifier.h"
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
#include "nssb64.h"
#include "pkix/pkix.h"
#include "pkix/pkixnss.h"
#include "plstr.h"
#include "secmime.h"


using namespace mozilla::pkix;
using namespace mozilla;
using namespace mozilla::psm;

extern mozilla::LazyLogModule gPIPNSSLog;

namespace {

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
  rv = stream->Read(char_ptr_cast(buf.data), buf.len, &bytesRead);
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
// search pattern, and then load it. Fails if there are no matches or if
// there is more than one match. If bugDigest is not null then on success
// bufDigest will contain the SHA-1 digeset of the entry.
nsresult
FindAndLoadOneEntry(nsIZipReader * zip,
                    const nsACString & searchPattern,
                    /*out*/ nsACString & filename,
                    /*out*/ SECItem & buf,
                    /*optional, out*/ Digest * bufDigest)
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
    rv = bufDigest->DigestBuf(SEC_OID_SHA1, buf.data, buf.len - 1);
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
                          const SECItem& digestFromManifest, SECItem& buf)
{
  MOZ_ASSERT(buf.len > 0);
  if (digestFromManifest.len != SHA1_LENGTH)
    return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;

  nsresult rv;
  uint64_t len64;
  rv = stream->Available(&len64);
  NS_ENSURE_SUCCESS(rv, rv);
  if (len64 > UINT32_MAX) {
    return NS_ERROR_SIGNED_JAR_ENTRY_TOO_LARGE;
  }

  UniquePK11Context digestContext(PK11_CreateDigestContext(SEC_OID_SHA1));
  if (!digestContext) {
    return mozilla::psm::GetXPCOMFromNSSError(PR_GetError());
  }

  rv = MapSECStatus(PK11_DigestBegin(digestContext.get()));
  NS_ENSURE_SUCCESS(rv, rv);

  uint64_t totalBytesRead = 0;
  for (;;) {
    uint32_t bytesRead;
    rv = stream->Read(char_ptr_cast(buf.data), buf.len, &bytesRead);
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
  rv = digest.End(SEC_OID_SHA1, digestContext);
  NS_ENSURE_SUCCESS(rv, rv);

  if (SECITEM_CompareItem(&digestFromManifest, &digest.get()) != SECEqual) {
    return NS_ERROR_SIGNED_JAR_MODIFIED_ENTRY;
  }

  return NS_OK;
}

nsresult
VerifyEntryContentDigest(nsIZipReader* zip, const nsACString& aFilename,
                         const SECItem& digestFromManifest, SECItem& buf)
{
  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = zip->GetInputStream(aFilename, getter_AddRefs(stream));
  if (NS_FAILED(rv)) {
    return NS_ERROR_SIGNED_JAR_ENTRY_MISSING;
  }

  return VerifyStreamContentDigest(stream, digestFromManifest, buf);
}

// @oaram aDir       directory containing the unpacked signed archive
// @param aFilename  path of the target file relative to aDir
// @param digestFromManifest The digest that we're supposed to check the file's
//                           contents against, from the manifest
// @param buf A scratch buffer that we use for doing the I/O
nsresult
VerifyFileContentDigest(nsIFile* aDir, const nsAString& aFilename,
                        const SECItem& digestFromManifest, SECItem& buf)
{
  // Find the file corresponding to the manifest path
  nsCOMPtr<nsIFile> file;
  nsresult rv = aDir->Clone(getter_AddRefs(file));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // We don't know how to handle JARs with signed directory entries.
  // It's technically possible in the manifest but makes no sense on disk.
  // Inside an archive we just ignore them, but here we have to treat it
  // as an error because the signed bytes never got unpacked.
  int32_t pos = 0;
  int32_t slash;
  int32_t namelen = aFilename.Length();
  if (namelen == 0 || aFilename[namelen - 1] == '/') {
    return NS_ERROR_SIGNED_JAR_ENTRY_INVALID;
  }

  // Append path segments one by one
  do {
    slash = aFilename.FindChar('/', pos);
    int32_t segend = (slash == kNotFound) ? namelen : slash;
    rv = file->Append(Substring(aFilename, pos, (segend - pos)));
    if (NS_FAILED(rv)) {
      return NS_ERROR_SIGNED_JAR_ENTRY_INVALID;
    }
    pos = slash + 1;
  }  while (pos < namelen && slash != kNotFound);

  bool exists;
  rv = file->Exists(&exists);
  if (NS_FAILED(rv) || !exists) {
    return NS_ERROR_SIGNED_JAR_ENTRY_MISSING;
  }

  bool isDir;
  rv = file->IsDirectory(&isDir);
  if (NS_FAILED(rv) || isDir) {
    // We only support signed files, not directory entries
    return NS_ERROR_SIGNED_JAR_ENTRY_INVALID;
  }

  // Open an input stream for that file and verify it.
  nsCOMPtr<nsIInputStream> stream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(stream), file, -1, -1,
                                  nsIFileInputStream::CLOSE_ON_EOF);
  if (NS_FAILED(rv) || !stream) {
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

// Parses a signature file (SF) as defined in the JDK 8 JAR Specification.
//
// The SF file *must* contain exactly one SHA1-Digest-Manifest attribute in
// the main section. All other sections are ignored. This means that this will
// NOT parse old-style signature files that have separate digests per entry.
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
// In order to get benefit (1), we do NOT implement the fallback to the older
// mechanism as the spec requires/suggests. Also, for simplity's sake, we only
// support exactly one SHA1-Digest-Manifest attribute, and no other
// algorithms.
//
// filebuf must be null-terminated. On output, mfDigest will contain the
// decoded value of SHA1-Digest-Manifest.
nsresult
ParseSF(const char* filebuf, /*out*/ SECItem & mfDigest)
{
  nsresult rv;

  const char* nextLineStart = filebuf;
  rv = CheckManifestVersion(nextLineStart, NS_LITERAL_CSTRING(JAR_SF_HEADER));
  if (NS_FAILED(rv))
    return rv;

  // Find SHA1-Digest-Manifest
  for (;;) {
    nsAutoCString curLine;
    rv = ReadLine(nextLineStart, curLine);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (curLine.Length() == 0) {
      // End of main section (blank line or end-of-file), and no
      // SHA1-Digest-Manifest found.
      return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
    }

    nsAutoCString attrName;
    nsAutoCString attrValue;
    rv = ParseAttribute(curLine, attrName, attrValue);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (attrName.LowerCaseEqualsLiteral("sha1-digest-manifest")) {
      rv = MapSECStatus(ATOB_ConvertAsciiToItem(&mfDigest, attrValue.get()));
      if (NS_FAILED(rv)) {
        return rv;
      }

      // There could be multiple SHA1-Digest-Manifest attributes, which
      // would be an error, but it's better to just skip any erroneous
      // duplicate entries rather than trying to detect them, because:
      //
      //   (1) It's simpler, and simpler generally means more secure
      //   (2) An attacker can't make us accept a JAR we would otherwise
      //       reject just by adding additional SHA1-Digest-Manifest
      //       attributes.
      break;
    }

    // ignore unrecognized attributes
  }

  return NS_OK;
}

// Parses MANIFEST.MF. The filenames of all entries will be returned in
// mfItems. buf must be a pre-allocated scratch buffer that is used for doing
// I/O.
nsresult
ParseMF(const char* filebuf, nsIZipReader * zip,
        /*out*/ nsTHashtable<nsCStringHashKey> & mfItems,
        ScopedAutoSECItem & buf)
{
  nsresult rv;

  const char* nextLineStart = filebuf;

  rv = CheckManifestVersion(nextLineStart, NS_LITERAL_CSTRING(JAR_MF_HEADER));
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
  ScopedAutoSECItem digest;

  for (;;) {
    nsAutoCString curLine;
    rv = ReadLine(nextLineStart, curLine);
    NS_ENSURE_SUCCESS(rv, rv);

    if (curLine.Length() == 0) {
      // end of section (blank line or end-of-file)

      if (curItemName.Length() == 0) {
        // '...Each section must start with an attribute with the name as
        // "Name",...', so every section must have a Name attribute.
        return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
      }

      if (digest.len == 0) {
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
      rv = VerifyEntryContentDigest(zip, curItemName, digest, buf);
      if (NS_FAILED(rv))
        return rv;

      mfItems.PutEntry(curItemName);

      if (*nextLineStart == '\0') // end-of-file
        break;

      // reset so we know we haven't encountered either of these for the next
      // item yet.
      curItemName.Truncate();
      digest.reset();

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
    if (attrName.LowerCaseEqualsLiteral("sha1-digest"))
    {
      if (digest.len > 0) // multiple SHA1 digests in section
        return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;

      rv = MapSECStatus(ATOB_ConvertAsciiToItem(&digest, attrValue.get()));
      if (NS_FAILED(rv))
        return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;

      continue;
    }

    // (2) Name: associates this manifest section with a file in the jar.
    if (attrName.LowerCaseEqualsLiteral("name"))
    {
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

struct VerifyCertificateContext {
  AppTrustedRoot trustedRoot;
  UniqueCERTCertList& builtChain;
};

nsresult
VerifyCertificate(CERTCertificate* signerCert, void* voidContext, void* pinArg)
{
  // TODO: null pinArg is tolerated.
  if (NS_WARN_IF(!signerCert) || NS_WARN_IF(!voidContext)) {
    return NS_ERROR_INVALID_ARG;
  }
  const VerifyCertificateContext& context =
    *static_cast<const VerifyCertificateContext*>(voidContext);

  AppTrustDomain trustDomain(context.builtChain, pinArg);
  if (trustDomain.SetTrustedRoot(context.trustedRoot) != SECSuccess) {
    return MapSECStatus(SECFailure);
  }
  Input certDER;
  Result rv = certDER.Init(signerCert->derCert.data, signerCert->derCert.len);
  if (rv != Success) {
    return mozilla::psm::GetXPCOMFromNSSError(MapResultToPRErrorCode(rv));
  }

  rv = BuildCertChain(trustDomain, certDER, Now(),
                      EndEntityOrCA::MustBeEndEntity,
                      KeyUsage::digitalSignature,
                      KeyPurposeId::id_kp_codeSigning,
                      CertPolicyId::anyPolicy,
                      nullptr/*stapledOCSPResponse*/);
  if (rv == Result::ERROR_EXPIRED_CERTIFICATE) {
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
    rv = Success;
  }
  if (rv != Success) {
    return mozilla::psm::GetXPCOMFromNSSError(MapResultToPRErrorCode(rv));
  }

  return NS_OK;
}

nsresult
VerifySignature(AppTrustedRoot trustedRoot, const SECItem& buffer,
                const SECItem& detachedDigest,
                /*out*/ UniqueCERTCertList& builtChain)
{
  // Currently, this function is only called within the CalculateResult() method
  // of CryptoTasks. As such, NSS should not be shut down at this point and the
  // CryptoTask implementation should already hold a nsNSSShutDownPreventionLock.
  // We acquire a nsNSSShutDownPreventionLock here solely to prove we did to
  // VerifyCMSDetachedSignatureIncludingCertificate().
  nsNSSShutDownPreventionLock locker;
  VerifyCertificateContext context = { trustedRoot, builtChain };
  // XXX: missing pinArg
  return VerifyCMSDetachedSignatureIncludingCertificate(buffer, detachedDigest,
                                                        VerifyCertificate,
                                                        &context, nullptr,
                                                        locker);
}

NS_IMETHODIMP
OpenSignedAppFile(AppTrustedRoot aTrustedRoot, nsIFile* aJarFile,
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
                           sigFilename, sigBuffer, nullptr);
  if (NS_FAILED(rv)) {
    return NS_ERROR_SIGNED_JAR_NOT_SIGNED;
  }

  // Signature (SF) file
  nsAutoCString sfFilename;
  ScopedAutoSECItem sfBuffer;
  Digest sfCalculatedDigest;
  rv = FindAndLoadOneEntry(zip, NS_LITERAL_CSTRING(JAR_SF_SEARCH_STRING),
                           sfFilename, sfBuffer, &sfCalculatedDigest);
  if (NS_FAILED(rv)) {
    return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
  }

  sigBuffer.type = siBuffer;
  UniqueCERTCertList builtChain;
  rv = VerifySignature(aTrustedRoot, sigBuffer, sfCalculatedDigest.get(),
                       builtChain);
  if (NS_FAILED(rv)) {
    return rv;
  }

  ScopedAutoSECItem mfDigest;
  rv = ParseSF(char_ptr_cast(sfBuffer.data), mfDigest);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Manifest (MF) file
  nsAutoCString mfFilename;
  ScopedAutoSECItem manifestBuffer;
  Digest mfCalculatedDigest;
  rv = FindAndLoadOneEntry(zip, NS_LITERAL_CSTRING(JAR_MF_SEARCH_STRING),
                           mfFilename, manifestBuffer, &mfCalculatedDigest);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (SECITEM_CompareItem(&mfDigest, &mfCalculatedDigest.get()) != SECEqual) {
    return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
  }

  // Allocate the I/O buffer only once per JAR, instead of once per entry, in
  // order to minimize malloc/free calls and in order to avoid fragmenting
  // memory.
  ScopedAutoSECItem buf(128 * 1024);

  nsTHashtable<nsCStringHashKey> items;

  rv = ParseMF(char_ptr_cast(manifestBuffer.data), zip, items, buf);
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
    MOZ_ASSERT(CERT_LIST_HEAD(builtChain));
    nsCOMPtr<nsIX509Cert> signerCert =
      nsNSSCertificate::Create(CERT_LIST_HEAD(builtChain)->cert);
    NS_ENSURE_TRUE(signerCert, NS_ERROR_OUT_OF_MEMORY);
    signerCert.forget(aSignerCert);
  }

  return NS_OK;
}

nsresult
VerifySignedManifest(AppTrustedRoot aTrustedRoot,
                     nsIInputStream* aManifestStream,
                     nsIInputStream* aSignatureStream,
                     /*out, optional */ nsIX509Cert** aSignerCert)
{
  NS_ENSURE_ARG(aManifestStream);
  NS_ENSURE_ARG(aSignatureStream);

  if (aSignerCert) {
    *aSignerCert = nullptr;
  }

  // Load signature file in buffer
  ScopedAutoSECItem signatureBuffer;
  nsresult rv = ReadStream(aSignatureStream, signatureBuffer);
  if (NS_FAILED(rv)) {
    return rv;
  }
  signatureBuffer.type = siBuffer;

  // Load manifest file in buffer
  ScopedAutoSECItem manifestBuffer;
  rv = ReadStream(aManifestStream, manifestBuffer);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Calculate SHA1 digest of the manifest buffer
  Digest manifestCalculatedDigest;
  rv = manifestCalculatedDigest.DigestBuf(SEC_OID_SHA1,
                                          manifestBuffer.data,
                                          manifestBuffer.len - 1); // buffer is null terminated
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Get base64 encoded string from manifest buffer digest
  UniquePORTString
    base64EncDigest(NSSBase64_EncodeItem(nullptr, nullptr, 0,
                      const_cast<SECItem*>(&manifestCalculatedDigest.get())));
  if (NS_WARN_IF(!base64EncDigest)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Calculate SHA1 digest of the base64 encoded string
  Digest doubleDigest;
  rv = doubleDigest.DigestBuf(SEC_OID_SHA1,
                              BitwiseCast<uint8_t*, char*>(base64EncDigest.get()),
                              strlen(base64EncDigest.get()));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Verify the manifest signature (signed digest of the base64 encoded string)
  UniqueCERTCertList builtChain;
  rv = VerifySignature(aTrustedRoot, signatureBuffer,
                       doubleDigest.get(), builtChain);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Return the signer's certificate to the reader if they want it.
  if (aSignerCert) {
    MOZ_ASSERT(CERT_LIST_HEAD(builtChain));
    nsCOMPtr<nsIX509Cert> signerCert =
      nsNSSCertificate::Create(CERT_LIST_HEAD(builtChain)->cert);
    if (NS_WARN_IF(!signerCert)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    signerCert.forget(aSignerCert);
  }

  return NS_OK;
}

class OpenSignedAppFileTask final : public CryptoTask
{
public:
  OpenSignedAppFileTask(AppTrustedRoot aTrustedRoot, nsIFile* aJarFile,
                        nsIOpenSignedAppFileCallback* aCallback)
    : mTrustedRoot(aTrustedRoot)
    , mJarFile(aJarFile)
    , mCallback(new nsMainThreadPtrHolder<nsIOpenSignedAppFileCallback>(aCallback))
  {
  }

private:
  virtual nsresult CalculateResult() override
  {
    return OpenSignedAppFile(mTrustedRoot, mJarFile,
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
  nsMainThreadPtrHandle<nsIOpenSignedAppFileCallback> mCallback;
  nsCOMPtr<nsIZipReader> mZipReader; // out
  nsCOMPtr<nsIX509Cert> mSignerCert; // out
};

class VerifySignedmanifestTask final : public CryptoTask
{
public:
  VerifySignedmanifestTask(AppTrustedRoot aTrustedRoot,
                           nsIInputStream* aManifestStream,
                           nsIInputStream* aSignatureStream,
                           nsIVerifySignedManifestCallback* aCallback)
    : mTrustedRoot(aTrustedRoot)
    , mManifestStream(aManifestStream)
    , mSignatureStream(aSignatureStream)
    , mCallback(
      new nsMainThreadPtrHolder<nsIVerifySignedManifestCallback>(aCallback))
  {
  }

private:
  virtual nsresult CalculateResult() override
  {
    return VerifySignedManifest(mTrustedRoot, mManifestStream,
                                mSignatureStream, getter_AddRefs(mSignerCert));
  }

  // nsNSSCertificate implements nsNSSShutdownObject, so there's nothing that
  // needs to be released
  virtual void ReleaseNSSResources() override { }

  virtual void CallCallback(nsresult rv) override
  {
    (void) mCallback->VerifySignedManifestFinished(rv, mSignerCert);
  }

  const AppTrustedRoot mTrustedRoot;
  const nsCOMPtr<nsIInputStream> mManifestStream;
  const nsCOMPtr<nsIInputStream> mSignatureStream;
  nsMainThreadPtrHandle<nsIVerifySignedManifestCallback> mCallback;
  nsCOMPtr<nsIX509Cert> mSignerCert; // out
};

} // unnamed namespace

NS_IMETHODIMP
nsNSSCertificateDB::OpenSignedAppFileAsync(
  AppTrustedRoot aTrustedRoot, nsIFile* aJarFile,
  nsIOpenSignedAppFileCallback* aCallback)
{
  NS_ENSURE_ARG_POINTER(aJarFile);
  NS_ENSURE_ARG_POINTER(aCallback);
  RefPtr<OpenSignedAppFileTask> task(new OpenSignedAppFileTask(aTrustedRoot,
                                                               aJarFile,
                                                               aCallback));
  return task->Dispatch("SignedJAR");
}

NS_IMETHODIMP
nsNSSCertificateDB::VerifySignedManifestAsync(
  AppTrustedRoot aTrustedRoot, nsIInputStream* aManifestStream,
  nsIInputStream* aSignatureStream, nsIVerifySignedManifestCallback* aCallback)
{
  NS_ENSURE_ARG_POINTER(aManifestStream);
  NS_ENSURE_ARG_POINTER(aSignatureStream);
  NS_ENSURE_ARG_POINTER(aCallback);

  RefPtr<VerifySignedmanifestTask> task(
    new VerifySignedmanifestTask(aTrustedRoot, aManifestStream,
                                 aSignatureStream, aCallback));
  return task->Dispatch("SignedManifest");
}


//
// Signature verification for archives unpacked into a file structure
//

// Finds the "*.rsa" signature file in the META-INF directory and returns
// the name. It is an error if there are none or more than one .rsa file
nsresult
FindSignatureFilename(nsIFile* aMetaDir,
                      /*out*/ nsAString& aFilename)
{
  nsCOMPtr<nsISimpleEnumerator> entries;
  nsresult rv = aMetaDir->GetDirectoryEntries(getter_AddRefs(entries));
  nsCOMPtr<nsIDirectoryEnumerator> files = do_QueryInterface(entries);
  if (NS_FAILED(rv) || !files) {
    return NS_ERROR_SIGNED_JAR_NOT_SIGNED;
  }

  bool found = false;
  nsCOMPtr<nsIFile> file;
  rv = files->GetNextFile(getter_AddRefs(file));

  while (NS_SUCCEEDED(rv) && file) {
    nsAutoString leafname;
    rv = file->GetLeafName(leafname);
    if (NS_SUCCEEDED(rv)) {
      if (StringEndsWith(leafname, NS_LITERAL_STRING(".rsa"))) {
        if (!found) {
          found = true;
          aFilename = leafname;
        } else {
          // second signature file is an error
          rv = NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
          break;
        }
      }
      rv = files->GetNextFile(getter_AddRefs(file));
    }
  }

  if (!found) {
    rv = NS_ERROR_SIGNED_JAR_NOT_SIGNED;
  }

  files->Close();
  return rv;
}

// Loads the signature metadata file that matches the given filename in
// the passed-in Meta-inf directory. If bufDigest is not null then on
// success bufDigest will contain the SHA-1 digest of the entry.
nsresult
LoadOneMetafile(nsIFile* aMetaDir,
                const nsAString& aFilename,
                /*out*/ SECItem& aBuf,
                /*optional, out*/ Digest* aBufDigest)
{
  nsCOMPtr<nsIFile> metafile;
  nsresult rv = aMetaDir->Clone(getter_AddRefs(metafile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = metafile->Append(aFilename);
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;
  rv = metafile->Exists(&exists);
  if (NS_FAILED(rv) || !exists) {
    // we can call a missing .rsa file "unsigned" but FindSignatureFilename()
    // already found one: missing other metadata files means a broken signature.
    return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
  }

  nsCOMPtr<nsIInputStream> stream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(stream), metafile);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ReadStream(stream, aBuf);
  stream->Close();
  NS_ENSURE_SUCCESS(rv, rv);

  if (aBufDigest) {
    rv = aBufDigest->DigestBuf(SEC_OID_SHA1, aBuf.data, aBuf.len - 1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

// Parses MANIFEST.MF and verifies the contents of the unpacked files
// listed in the manifest.
// The filenames of all entries will be returned in aMfItems. aBuf must
// be a pre-allocated scratch buffer that is used for doing I/O.
nsresult
ParseMFUnpacked(const char* aFilebuf, nsIFile* aDir,
                /*out*/ nsTHashtable<nsStringHashKey>& aMfItems,
                ScopedAutoSECItem& aBuf)
{
  nsresult rv;

  const char* nextLineStart = aFilebuf;

  rv = CheckManifestVersion(nextLineStart, NS_LITERAL_CSTRING(JAR_MF_HEADER));
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

  nsAutoString curItemName;
  ScopedAutoSECItem digest;

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

      if (digest.len == 0) {
        // We require every entry to have a digest, since we require every
        // entry to be signed and we don't allow duplicate entries.
        return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
      }

      if (aMfItems.Contains(curItemName)) {
        // Duplicate entry
        return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
      }

      // Verify that the file's content digest matches the digest from this
      // MF section.
      rv = VerifyFileContentDigest(aDir, curItemName, digest, aBuf);
      if (NS_FAILED(rv)) {
        return rv;
      }

      aMfItems.PutEntry(curItemName);

      if (*nextLineStart == '\0') {
        // end-of-file
        break;
      }

      // reset so we know we haven't encountered either of these for the next
      // item yet.
      curItemName.Truncate();
      digest.reset();

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
    if (attrName.LowerCaseEqualsLiteral("sha1-digest")) {
      if (digest.len > 0) {
        // multiple SHA1 digests in section
        return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
      }

      rv = MapSECStatus(ATOB_ConvertAsciiToItem(&digest, attrValue.get()));
      if (NS_FAILED(rv)) {
        return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
      }

      continue;
    }

    // (2) Name: associates this manifest section with a file in the jar.
    if (attrName.LowerCaseEqualsLiteral("name")) {
      if (MOZ_UNLIKELY(curItemName.Length() > 0)) {
        // multiple names in section
        return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
      }

      if (MOZ_UNLIKELY(attrValue.Length() == 0)) {
        return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
      }

      curItemName = NS_ConvertUTF8toUTF16(attrValue);

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

// recursively check a directory tree for files not in the list of
// verified files we found in the manifest. For each file we find
// Check it against the files found in the manifest. If the file wasn't
// in the manifest then it's unsigned and we can stop looking. Otherwise
// remove it from the collection so we can check leftovers later.
//
// @param aDir   Directory to check
// @param aPath  Relative path to that directory (to check against aItems)
// @param aItems All the files found
// @param *Filename  signature files that won't be in the manifest
nsresult
CheckDirForUnsignedFiles(nsIFile* aDir,
                         const nsString& aPath,
                         /* in/out */ nsTHashtable<nsStringHashKey>& aItems,
                         const nsAString& sigFilename,
                         const nsAString& sfFilename,
                         const nsAString& mfFilename)
{
  nsCOMPtr<nsISimpleEnumerator> entries;
  nsresult rv = aDir->GetDirectoryEntries(getter_AddRefs(entries));
  nsCOMPtr<nsIDirectoryEnumerator> files = do_QueryInterface(entries);
  if (NS_FAILED(rv) || !files) {
    return NS_ERROR_SIGNED_JAR_ENTRY_MISSING;
  }

  bool inMeta = StringBeginsWith(aPath, NS_LITERAL_STRING(JAR_META_DIR));

  while (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIFile> file;
    rv = files->GetNextFile(getter_AddRefs(file));
    if (NS_FAILED(rv) || !file) {
      break;
    }

    nsAutoString leafname;
    rv = file->GetLeafName(leafname);
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsAutoString curName(aPath + leafname);

    bool isDir;
    rv = file->IsDirectory(&isDir);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // if it's a directory we need to recurse
    if (isDir) {
      curName.Append(NS_LITERAL_STRING("/"));
      rv = CheckDirForUnsignedFiles(file, curName, aItems,
                                    sigFilename, sfFilename, mfFilename);
    } else {
      // The files that comprise the signature mechanism are not covered by the
      // signature.
      //
      // XXX: This is OK for a single signature, but doesn't work for
      // multiple signatures because the metadata for the other signatures
      // is not signed either.
      if (inMeta && ( leafname == sigFilename ||
                      leafname == sfFilename ||
                      leafname == mfFilename )) {
        continue;
      }

      // make sure the current file was found in the manifest
      nsStringHashKey* item = aItems.GetEntry(curName);
      if (!item) {
        return NS_ERROR_SIGNED_JAR_UNSIGNED_ENTRY;
      }

      // Remove the item so we can check for leftover items later
      aItems.RemoveEntry(item);
    }
  }
  files->Close();
  return rv;
}

/*
 * Verify the signature of a directory structure as if it were a
 * signed JAR file (used for unpacked JARs)
 */
nsresult
VerifySignedDirectory(AppTrustedRoot aTrustedRoot,
                      nsIFile* aDirectory,
                      /*out, optional */ nsIX509Cert** aSignerCert)
{
  NS_ENSURE_ARG_POINTER(aDirectory);

  if (aSignerCert) {
    *aSignerCert = nullptr;
  }

  // Make sure there's a META-INF directory

  nsCOMPtr<nsIFile> metaDir;
  nsresult rv = aDirectory->Clone(getter_AddRefs(metaDir));
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = metaDir->Append(NS_LITERAL_STRING(JAR_META_DIR));
  if (NS_FAILED(rv)) {
    return rv;
  }

  bool exists;
  rv = metaDir->Exists(&exists);
  if (NS_FAILED(rv) || !exists) {
    return NS_ERROR_SIGNED_JAR_NOT_SIGNED;
  }
  bool isDirectory;
  rv = metaDir->IsDirectory(&isDirectory);
  if (NS_FAILED(rv) || !isDirectory) {
    return NS_ERROR_SIGNED_JAR_NOT_SIGNED;
  }

  // Find and load the Signature (RSA) file

  nsAutoString sigFilename;
  rv = FindSignatureFilename(metaDir, sigFilename);
  if (NS_FAILED(rv)) {
    return rv;
  }

  ScopedAutoSECItem sigBuffer;
  rv = LoadOneMetafile(metaDir, sigFilename, sigBuffer, nullptr);
  if (NS_FAILED(rv)) {
    return NS_ERROR_SIGNED_JAR_NOT_SIGNED;
  }

  // Load the signature (SF) file and verify the signature.
  // The .sf and .rsa files must have the same name apart from the extension.

  nsAutoString sfFilename(Substring(sigFilename, 0, sigFilename.Length() - 3)
                          + NS_LITERAL_STRING("sf"));

  ScopedAutoSECItem sfBuffer;
  Digest sfCalculatedDigest;
  rv = LoadOneMetafile(metaDir, sfFilename, sfBuffer, &sfCalculatedDigest);
  if (NS_FAILED(rv)) {
    return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
  }

  sigBuffer.type = siBuffer;
  UniqueCERTCertList builtChain;
  rv = VerifySignature(aTrustedRoot, sigBuffer, sfCalculatedDigest.get(),
                       builtChain);
  if (NS_FAILED(rv)) {
    return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
  }

  // Get the expected manifest hash from the signed .sf file

  ScopedAutoSECItem mfDigest;
  rv = ParseSF(char_ptr_cast(sfBuffer.data), mfDigest);
  if (NS_FAILED(rv)) {
    return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
  }

  // Load manifest (MF) file and verify signature

  nsAutoString mfFilename(NS_LITERAL_STRING("manifest.mf"));
  ScopedAutoSECItem manifestBuffer;
  Digest mfCalculatedDigest;
  rv = LoadOneMetafile(metaDir, mfFilename, manifestBuffer, &mfCalculatedDigest);
  if (NS_FAILED(rv)) {
    return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
  }

  if (SECITEM_CompareItem(&mfDigest, &mfCalculatedDigest.get()) != SECEqual) {
    return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
  }

  // Parse manifest and verify signed hash of all listed files

  // Allocate the I/O buffer only once per JAR, instead of once per entry, in
  // order to minimize malloc/free calls and in order to avoid fragmenting
  // memory.
  ScopedAutoSECItem buf(128 * 1024);

  nsTHashtable<nsStringHashKey> items;
  rv = ParseMFUnpacked(char_ptr_cast(manifestBuffer.data),
                       aDirectory, items, buf);
  if (NS_FAILED(rv)){
    return rv;
  }

  // We've checked that everything listed in the manifest exists and is signed
  // correctly. Now check on disk for extra (unsigned) files.
  // Deletes found entries from items as it goes.
  rv = CheckDirForUnsignedFiles(aDirectory, EmptyString(), items,
                                sigFilename, sfFilename, mfFilename);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // We verified that every entry that we require to be signed is signed. But,
  // were there any missing entries--that is, entries that are mentioned in the
  // manifest but missing from the directory tree? (There shouldn't be given
  // ParseMFUnpacked() checking them all, but it's a cheap sanity check.)
  if (items.Count() != 0) {
    return NS_ERROR_SIGNED_JAR_ENTRY_MISSING;
  }

  // Return the signer's certificate to the reader if they want it.
  // XXX: We should return an nsIX509CertList with the whole validated chain.
  if (aSignerCert) {
    MOZ_ASSERT(CERT_LIST_HEAD(builtChain));
    nsCOMPtr<nsIX509Cert> signerCert =
      nsNSSCertificate::Create(CERT_LIST_HEAD(builtChain)->cert);
    NS_ENSURE_TRUE(signerCert, NS_ERROR_OUT_OF_MEMORY);
    signerCert.forget(aSignerCert);
  }

  return NS_OK;
}

class VerifySignedDirectoryTask final : public CryptoTask
{
public:
  VerifySignedDirectoryTask(AppTrustedRoot aTrustedRoot, nsIFile* aUnpackedJar,
                            nsIVerifySignedDirectoryCallback* aCallback)
    : mTrustedRoot(aTrustedRoot)
    , mDirectory(aUnpackedJar)
    , mCallback(new nsMainThreadPtrHolder<nsIVerifySignedDirectoryCallback>(aCallback))
  {
  }

private:
  virtual nsresult CalculateResult() override
  {
    return VerifySignedDirectory(mTrustedRoot,
                                 mDirectory,
                                 getter_AddRefs(mSignerCert));
  }

  // This class doesn't directly hold NSS resources so there's nothing that
  // needs to be released
  virtual void ReleaseNSSResources() override { }

  virtual void CallCallback(nsresult rv) override
  {
    (void) mCallback->VerifySignedDirectoryFinished(rv, mSignerCert);
  }

  const AppTrustedRoot mTrustedRoot;
  const nsCOMPtr<nsIFile> mDirectory;
  nsMainThreadPtrHandle<nsIVerifySignedDirectoryCallback> mCallback;
  nsCOMPtr<nsIX509Cert> mSignerCert; // out
};

NS_IMETHODIMP
nsNSSCertificateDB::VerifySignedDirectoryAsync(
  AppTrustedRoot aTrustedRoot, nsIFile* aUnpackedJar,
  nsIVerifySignedDirectoryCallback* aCallback)
{
  NS_ENSURE_ARG_POINTER(aUnpackedJar);
  NS_ENSURE_ARG_POINTER(aCallback);
  RefPtr<VerifySignedDirectoryTask> task(new VerifySignedDirectoryTask(aTrustedRoot,
                                                                       aUnpackedJar,
                                                                       aCallback));
  return task->Dispatch("UnpackedJar");
}
