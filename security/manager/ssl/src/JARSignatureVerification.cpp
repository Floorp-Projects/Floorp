/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG 1
#endif

#include "nsNSSCertificateDB.h"

#include "mozilla/RefPtr.h"
#include "CryptoTask.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsIStringEnumerator.h"
#include "nsIZipReader.h"
#include "nsNSSCertificate.h"
#include "nsString.h"
#include "nsTHashtable.h"
#include "ScopedNSSTypes.h"

#include "base64.h"
#include "secmime.h"
#include "plstr.h"
#include "prlog.h"

using namespace mozilla;

#ifdef MOZ_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

namespace {

// Finds exactly one (signature metadata) entry that matches the given
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

  // The size returned by Available() might be inaccurate so we need to check
  // that Available() matches up with the actual length of the file.
  uint64_t len64;
  rv = stream->Available(&len64);
  NS_ENSURE_SUCCESS(rv, rv);


  // Cap the maximum accepted size of signature-related files at 1MB (which is
  // still crazily huge) to avoid OOM. The uncompressed length of an entry can be
  // hundreds of times larger than the compressed version, especially if
  // someone has speifically crafted the entry to cause OOM or to consume
  // massive amounts of disk space.
  //
  // Also, keep in mind bug 164695 and that we must leave room for
  // null-terminating the buffer.
  static const uint32_t MAX_LENGTH = 1024 * 1024;
  MOZ_STATIC_ASSERT(MAX_LENGTH < UINT32_MAX, "MAX_LENGTH < UINT32_MAX");
  NS_ENSURE_TRUE(len64 < MAX_LENGTH, NS_ERROR_FILE_CORRUPTED);
  NS_ENSURE_TRUE(len64 < UINT32_MAX, NS_ERROR_FILE_CORRUPTED); // bug 164695
  SECITEM_AllocItem(buf, static_cast<uint32_t>(len64 + 1));

  // buf.len == len64 + 1. We attempt to read len64 + 1 bytes instead of len64,
  // so that we can check whether the metadata in the ZIP for the entry is
  // incorrect.
  uint32_t bytesRead;
  rv = stream->Read(char_ptr_cast(buf.data), buf.len, &bytesRead);
  NS_ENSURE_SUCCESS(rv, rv);
  if (bytesRead != len64) {
    return NS_ERROR_SIGNED_JAR_ENTRY_INVALID;
  }

  buf.data[buf.len - 1] = 0; // null-terminate

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
// @param digestFromManifest The digest that we're supposed to check the file's
//                           contents against, from the manifest
// @param buf A scratch buffer that we use for doing the I/O, which must have
//            already been allocated. The size of this buffer is the unit
//            size of our I/O.
nsresult
VerifyEntryContentDigest(nsIZipReader * zip, const nsACString & aFilename,
                         const SECItem & digestFromManifest, SECItem & buf)
{
  MOZ_ASSERT(buf.len > 0);
  if (digestFromManifest.len != SHA1_LENGTH)
    return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;

  nsresult rv;

  nsCOMPtr<nsIInputStream> stream;
  rv = zip->GetInputStream(aFilename, getter_AddRefs(stream));
  if (NS_FAILED(rv)) {
    return NS_ERROR_SIGNED_JAR_ENTRY_MISSING;
  }

  uint64_t len64;
  rv = stream->Available(&len64);
  NS_ENSURE_SUCCESS(rv, rv);
  if (len64 > UINT32_MAX) {
    return NS_ERROR_SIGNED_JAR_ENTRY_TOO_LARGE;
  }

  ScopedPK11Context digestContext(PK11_CreateDigestContext(SEC_OID_SHA1));
  if (!digestContext) {
    return PRErrorCode_to_nsresult(PR_GetError());
  }

  rv = MapSECStatus(PK11_DigestBegin(digestContext));
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

    rv = MapSECStatus(PK11_DigestOp(digestContext, buf.data, bytesRead));
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

// On input, nextLineStart is the start of the current line. On output,
// nextLineStart is the start of the next line.
nsresult
ReadLine(/*in/out*/ const char* & nextLineStart, /*out*/ nsCString & line,
         bool allowContinuations = true)
{
  line.Truncate();
  for (;;) {
    const char* eol = PL_strpbrk(nextLineStart, "\r\n");

    if (!eol) { // Reached end of file before newline
      eol = nextLineStart + strlen(nextLineStart);
    }

    line.Append(nextLineStart, eol - nextLineStart);

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
#define JAR_MF_HEADER (const char*)"Manifest-Version: 1.0"
#define JAR_SF_HEADER (const char*)"Signature-Version: 1.0"

nsresult
ParseAttribute(const nsAutoCString & curLine,
               /*out*/ nsAutoCString & attrName,
               /*out*/ nsAutoCString & attrValue)
{
  nsAutoCString::size_type len = curLine.Length();
  if (len > 72) {
    // The spec says "No line may be longer than 72 bytes (not characters)"
    // in its UTF8-encoded form. This check also ensures that len < INT32_MAX,
    // which is required below.
    return NS_ERROR_SIGNED_JAR_MANIFEST_INVALID;
  }

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
  rv = CheckManifestVersion(nextLineStart, nsLiteralCString(JAR_SF_HEADER));
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

  rv = CheckManifestVersion(nextLineStart, nsLiteralCString(JAR_MF_HEADER));
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

// Callback functions for decoder. For now, use empty/default functions.
void
ContentCallback(void *arg, const char *buf, unsigned long len)
{
}
PK11SymKey *
GetDecryptKeyCallback(void *, SECAlgorithmID *)
{
  return nullptr;
}
PRBool
DecryptionAllowedCallback(SECAlgorithmID *algid, PK11SymKey *bulkkey)
{
  return false;
}
void *
GetPasswordKeyCallback(void *arg, void *handle)
{
  return nullptr;
}

NS_IMETHODIMP
OpenSignedJARFile(nsIFile * aJarFile,
                  /*out, optional */ nsIZipReader ** aZipReader,
                  /*out, optional */ nsIX509Cert3 ** aSignerCert)
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

  sigBuffer.type = siBuffer;
  ScopedSEC_PKCS7ContentInfo p7_info(SEC_PKCS7DecodeItem(&sigBuffer,
                                        ContentCallback, nullptr,
                                        GetPasswordKeyCallback, nullptr,
                                        GetDecryptKeyCallback, nullptr,
                                        DecryptionAllowedCallback));
  if (!p7_info) {
    PRErrorCode error = PR_GetError();
    const char * errorName = PR_ErrorToName(error);
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Failed to decode PKCS#7 item: %s",
           errorName));
    return PRErrorCode_to_nsresult(error);
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

  // Verify that the signature file is a valid signature of the SF file
  if (!SEC_PKCS7VerifyDetachedSignatureAtTime(p7_info, certUsageObjectSigner,
                                              &sfCalculatedDigest.get(),
                                              HASH_AlgSHA1, false, PR_Now())) {
    PRErrorCode error = PR_GetError();
    const char * errorName = PR_ErrorToName(error);
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Failed to verify detached signature: %s",
           errorName));
    rv = PRErrorCode_to_nsresult(error);
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
  items.Init();

  rv = ParseMF(char_ptr_cast(manifestBuffer.data), zip, items, buf);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Verify every entry in the file.
  nsCOMPtr<nsIUTF8StringEnumerator> entries;
  rv = zip->FindEntries(NS_LITERAL_CSTRING(""), getter_AddRefs(entries));
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

    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Verifying digests for %s",
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
    items.RemoveEntry(entryFilename);
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
  // XXX: We should return an nsIX509CertList with the whole validated chain,
  //      but we can't do that until we switch to libpkix.
  if (aSignerCert) {
    CERTCertificate *rawSignerCert
      = p7_info->content.signedData->signerInfos[0]->cert;
    NS_ENSURE_TRUE(rawSignerCert, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIX509Cert3> signerCert = nsNSSCertificate::Create(rawSignerCert);
    NS_ENSURE_TRUE(signerCert, NS_ERROR_OUT_OF_MEMORY);

    signerCert.forget(aSignerCert);
  }

  return NS_OK;
}

class OpenSignedJARFileTask MOZ_FINAL : public CryptoTask
{
public:
  OpenSignedJARFileTask(nsIFile * aJarFile,
                        nsIOpenSignedJARFileCallback * aCallback)
    : mJarFile(aJarFile)
    , mCallback(aCallback)
  {
  }

private:
  virtual nsresult CalculateResult() MOZ_OVERRIDE
  {
    return OpenSignedJARFile(mJarFile, getter_AddRefs(mZipReader),
                             getter_AddRefs(mSignerCert));
  }

  // nsNSSCertificate implements nsNSSShutdownObject, so there's nothing that
  // needs to be released
  virtual void ReleaseNSSResources() { }

  virtual void CallCallback(nsresult rv)
  {
    (void) mCallback->OpenSignedJARFileFinished(rv, mZipReader, mSignerCert);
  }

  const nsCOMPtr<nsIFile> mJarFile;
  const nsCOMPtr<nsIOpenSignedJARFileCallback> mCallback;
  nsCOMPtr<nsIZipReader> mZipReader; // out
  nsCOMPtr<nsIX509Cert3> mSignerCert; // out
};

} // unnamed namespace

NS_IMETHODIMP
nsNSSCertificateDB::OpenSignedJARFileAsync(
  nsIFile * aJarFile, nsIOpenSignedJARFileCallback * aCallback)
{
  NS_ENSURE_ARG_POINTER(aJarFile);
  NS_ENSURE_ARG_POINTER(aCallback);
  RefPtr<OpenSignedJARFileTask> task(new OpenSignedJARFileTask(aJarFile,
                                                               aCallback));
  return task->Dispatch("SignedJAR");
}
