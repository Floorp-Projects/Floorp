//* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Effective-TLD Service
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pamela Greene <pamg.bugs@gmail.com> (original author)
 *   Daniel Witte <dwitte@stanford.edu>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// This service reads a file of rules describing TLD-like domain names.  For a
// complete description of the expected file format and parsing rules, see
// http://wiki.mozilla.org/Gecko:Effective_TLD_Service

#include "nsEffectiveTLDService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsFileStreams.h"
#include "nsIFile.h"
#include "nsIIDNService.h"
#include "nsNetUtil.h"
#include "prnetdb.h"

// The file name of the list of TLD-like names.  A file with this name in the
// system "res" directory will always be used.  In addition, if a file with
// the same name is present in the user's profile directory, its contents will
// also be used, as though those rules were appended to the system file.
#define EFF_TLD_FILENAME NS_LITERAL_CSTRING("effective_tld_names.dat")

NS_IMPL_ISUPPORTS1(nsEffectiveTLDService, nsIEffectiveTLDService)

// ----------------------------------------------------------------------

#define PL_ARENA_CONST_ALIGN_MASK 3
#include "plarena.h"

static PLArenaPool *gArena = nsnull;

#define ARENA_SIZE 512

// equivalent to strdup() - does no error checking,
// we're assuming we're only called with a valid pointer
static char *
ArenaStrDup(const char* str, PLArenaPool* aArena)
{
  void *mem;
  PRUint32 size = strlen(str) + 1;
  PL_ARENA_ALLOCATE(mem, aArena, size);
  if (mem)
    memcpy(mem, str, size);
  return static_cast<char*>(mem);
}

nsDomainEntry::nsDomainEntry(const char *aDomain)
 : mDomain(ArenaStrDup(aDomain, gArena))
 , mIsNormal(PR_FALSE)
 , mIsException(PR_FALSE)
 , mIsWild(PR_FALSE)
{
}

// ----------------------------------------------------------------------

nsresult
nsEffectiveTLDService::Init()
{
  if (!mHash.Init())
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv;
  mIDNService = do_GetService(NS_IDNSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  return LoadEffectiveTLDFiles();
}

nsEffectiveTLDService::nsEffectiveTLDService()
{
}

nsEffectiveTLDService::~nsEffectiveTLDService()
{
  if (gArena) {
    PL_FinishArenaPool(gArena);
    delete gArena;
  }
  gArena = nsnull;
}

// External function for dealing with URI's correctly.
// Pulls out the host portion from an nsIURI, and calls through to
// GetPublicSuffixFromHost().
NS_IMETHODIMP
nsEffectiveTLDService::GetPublicSuffix(nsIURI     *aURI,
                                       nsACString &aPublicSuffix)
{
  NS_ENSURE_ARG_POINTER(aURI);

  nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(aURI);
  NS_ENSURE_ARG_POINTER(innerURI);

  nsCAutoString host;
  nsresult rv = innerURI->GetAsciiHost(host);
  if (NS_FAILED(rv)) return rv;

  return GetBaseDomainInternal(host, 0, aPublicSuffix);
}

// External function for dealing with URI's correctly.
// Pulls out the host portion from an nsIURI, and calls through to
// GetBaseDomainFromHost().
NS_IMETHODIMP
nsEffectiveTLDService::GetBaseDomain(nsIURI     *aURI,
                                     PRUint32    aAdditionalParts,
                                     nsACString &aBaseDomain)
{
  NS_ENSURE_ARG_POINTER(aURI);

  nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(aURI);
  NS_ENSURE_ARG_POINTER(innerURI);

  nsCAutoString host;
  nsresult rv = innerURI->GetAsciiHost(host);
  if (NS_FAILED(rv)) return rv;

  return GetBaseDomainInternal(host, aAdditionalParts + 1, aBaseDomain);
}

// External function for dealing with a host string directly: finds the public
// suffix (e.g. co.uk) for the given hostname. See GetBaseDomainInternal().
NS_IMETHODIMP
nsEffectiveTLDService::GetPublicSuffixFromHost(const nsACString &aHostname,
                                               nsACString       &aPublicSuffix)
{
  // Create a mutable copy of the hostname and normalize it to ACE.
  // This will fail if the hostname includes invalid characters.
  nsCAutoString normHostname(aHostname);
  nsresult rv = NormalizeHostname(normHostname);
  if (NS_FAILED(rv)) return rv;

  return GetBaseDomainInternal(normHostname, 0, aPublicSuffix);
}

// External function for dealing with a host string directly: finds the base
// domain (e.g. www.co.uk) for the given hostname and number of subdomain parts
// requested. See GetBaseDomainInternal().
NS_IMETHODIMP
nsEffectiveTLDService::GetBaseDomainFromHost(const nsACString &aHostname,
                                             PRUint32          aAdditionalParts,
                                             nsACString       &aBaseDomain)
{
  // Create a mutable copy of the hostname and normalize it to ACE.
  // This will fail if the hostname includes invalid characters.
  nsCAutoString normHostname(aHostname);
  nsresult rv = NormalizeHostname(normHostname);
  if (NS_FAILED(rv)) return rv;

  return GetBaseDomainInternal(normHostname, aAdditionalParts + 1, aBaseDomain);
}

// Finds the base domain for a host, with requested number of additional parts.
// This will fail, generating an error, if the host is an IPv4/IPv6 address,
// if more subdomain parts are requested than are available, or if the hostname
// includes characters that are not valid in a URL. Normalization is performed
// on the host string and the result will be in UTF8.
nsresult
nsEffectiveTLDService::GetBaseDomainInternal(nsCString  &aHostname,
                                             PRUint32    aAdditionalParts,
                                             nsACString &aBaseDomain)
{
  if (aHostname.IsEmpty())
    return NS_ERROR_INVALID_ARG;

  // chomp any trailing dot, and keep track of it for later
  PRBool trailingDot = aHostname.Last() == '.';
  if (trailingDot)
    aHostname.Truncate(aHostname.Length() - 1);

  // Check if we're dealing with an IPv4/IPv6 hostname, and return
  PRNetAddr addr;
  PRStatus result = PR_StringToNetAddr(aHostname.get(), &addr);
  if (result == PR_SUCCESS)
    return NS_ERROR_HOST_IS_IP_ADDRESS;

  // walk up the domain tree, most specific to least specific,
  // looking for matches at each level. note that a given level may
  // have multiple attributes (e.g. IsWild() and IsNormal()).
  const char *prevDomain = nsnull;
  const char *currDomain = aHostname.get();
  const char *nextDot = strchr(currDomain, '.');
  const char *end = currDomain + aHostname.Length();
  const char *eTLD = currDomain;
  while (1) {
    nsDomainEntry *entry = mHash.GetEntry(currDomain);
    if (entry) {
      if (entry->IsWild() && prevDomain) {
        // wildcard rules imply an eTLD one level inferior to the match.
        eTLD = prevDomain;
        break;

      } else if (entry->IsNormal() || !nextDot) {
        // specific match, or we've hit the top domain level
        eTLD = currDomain;
        break;

      } else if (entry->IsException()) {
        // exception rules imply an eTLD one level superior to the match.
        eTLD = nextDot + 1;
        break;
      }
    }

    if (!nextDot) {
      // we've hit the top domain level; use it by default.
      eTLD = currDomain;
      break;
    }

    prevDomain = currDomain;
    currDomain = nextDot + 1;
    nextDot = strchr(currDomain, '.');
  }

  // count off the number of requested domains.
  const char *begin = aHostname.get();
  const char *iter = eTLD;
  while (1) {
    if (iter == begin)
      break;

    if (*(--iter) == '.' && aAdditionalParts-- == 0) {
      ++iter;
      ++aAdditionalParts;
      break;
    }
  }

  if (aAdditionalParts != 0)
    return NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS;

  aBaseDomain = Substring(iter, end);
  // add on the trailing dot, if applicable
  if (trailingDot)
    aBaseDomain.Append('.');

  return NS_OK;
}

// Normalizes characters of hostname. ASCII/ACE names are lower-cased,
// and UTF8 names are normalized per RFC 3454 and converted to ACE.
nsresult
nsEffectiveTLDService::NormalizeHostname(nsCString &aHostname)
{
  if (IsASCII(aHostname)) {
    ToLowerCase(aHostname);
    return NS_OK;
  }

  return mIDNService->ConvertUTF8toACE(aHostname, aHostname);
}

// Adds the given domain name rule to the effective-TLD hash.
// CAUTION: As a side effect, the domain name rule will be normalized.
// see NormalizeHostname().
nsresult
nsEffectiveTLDService::AddEffectiveTLDEntry(nsCString &aDomainName)
{
  // lazily init the arena pool
  if (!gArena) {
    gArena = new PLArenaPool;
    NS_ENSURE_TRUE(gArena, NS_ERROR_OUT_OF_MEMORY);
    PL_INIT_ARENA_POOL(gArena, "eTLDArena", ARENA_SIZE);
  }

  PRBool isException = PR_FALSE, isWild = PR_FALSE;

  // Is this node an exception?
  if (aDomainName.First() == '!') {
    isException = PR_TRUE;
    aDomainName.Cut(0, 1);

  // ... or wild?
  } else if (StringBeginsWith(aDomainName, NS_LITERAL_CSTRING("*."))) {
    isWild = PR_TRUE;
    aDomainName.Cut(0, 2);

    NS_ASSERTION(!StringBeginsWith(aDomainName, NS_LITERAL_CSTRING("*.")),
                 "only one wildcard level supported!");
  }

  // Normalize the domain name.
  nsresult rv = NormalizeHostname(aDomainName);
  NS_ENSURE_SUCCESS(rv, rv);

  nsDomainEntry *entry = mHash.PutEntry(aDomainName.get());
  NS_ENSURE_TRUE(entry, NS_ERROR_FAILURE);

  // check for arena string alloc failure
  if (!entry->GetKey()) {
    mHash.RawRemoveEntry(entry);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // add the new flags, without stomping existing ones
  entry->IsWild() |= isWild;
  entry->IsException() |= isException;
  // note: isWild also implies isNormal (e.g. *.co.nz also implies the co.nz eTLD)
  entry->IsNormal() |= isWild || !isException;

  return NS_OK;
}

// Locates the effective-TLD file.  If aUseProfile is true, uses the file from
// the user's profile directory; otherwise uses the one from the system "res"
// directory.  Places nsnull in foundFile if the desired file was not found.
nsresult
LocateEffectiveTLDFile(nsCOMPtr<nsIFile>& foundFile, PRBool aUseProfile)
{
  foundFile = nsnull;

  nsCOMPtr<nsIFile> effTLDFile = nsnull;
  nsresult rv = NS_OK;
  PRBool exists = PR_FALSE;
  if (aUseProfile) {
    // Look for the file in the user's profile directory.
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                getter_AddRefs(effTLDFile));
    // We allow a nonfatal error so that this component can be tested in an
    // xpcshell with no profile present.
    if (NS_FAILED(rv))
      return rv;
  }
  else {
    // Look for the file in the application "res" directory.
    rv = NS_GetSpecialDirectory(NS_OS_CURRENT_PROCESS_DIR,
                                getter_AddRefs(effTLDFile));
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = effTLDFile->AppendNative(NS_LITERAL_CSTRING("res"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = effTLDFile->AppendNative(EFF_TLD_FILENAME);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = effTLDFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists)
    foundFile = effTLDFile;

  return rv;
}

void
TruncateAtWhitespace(nsCString &aString)
{
  // Searching for a space or tab one byte at a time is fine since UTF-8 is a
  // superset of 7-bit ASCII.
  nsASingleFragmentCString::const_char_iterator begin, iter, end;
  aString.BeginReading(begin);
  aString.EndReading(end);

  for (iter = begin; iter != end; ++iter) {
    if (*iter == ' ' || *iter == '\t') {
      aString.Truncate(iter - begin);
      break;
    }
  }
}

// Loads the contents of the given effective-TLD file, building the tree as it
// goes.
nsresult
nsEffectiveTLDService::LoadOneEffectiveTLDFile(nsCOMPtr<nsIFile>& effTLDFile)
{
  // Open the file as an input stream.
  nsCOMPtr<nsIInputStream> fileStream;
  nsresult rv = NS_NewLocalFileInputStream(getter_AddRefs(fileStream),
                                           effTLDFile,
                                           0x01, // read-only mode
                                           -1,   // all permissions
                                           nsIFileInputStream::CLOSE_ON_EOF);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILineInputStream> lineStream = do_QueryInterface(fileStream, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString lineData;
  PRBool isMore;
  NS_NAMED_LITERAL_CSTRING(kCommentMarker, "//");
  while (NS_SUCCEEDED(lineStream->ReadLine(lineData, &isMore)) && isMore) {
    if (StringBeginsWith(lineData, kCommentMarker))
      continue;

    TruncateAtWhitespace(lineData);
    if (!lineData.IsEmpty()) {
      rv = AddEffectiveTLDEntry(lineData);
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Error adding effective TLD to list");
    }
  }

  return NS_OK;
}

// Loads the contents of the system and user effective-TLD files.
nsresult
nsEffectiveTLDService::LoadEffectiveTLDFiles()
{
  nsCOMPtr<nsIFile> effTLDFile;
  nsresult rv = LocateEffectiveTLDFile(effTLDFile, PR_FALSE);

  // If we didn't find any system effective-TLD file, warn but keep trying.  We
  // can struggle along using the base TLDs.
  if (NS_FAILED(rv) || nsnull == effTLDFile) {
    NS_WARNING("No effective-TLD file found in system res directory");
  }
  else {
    rv = LoadOneEffectiveTLDFile(effTLDFile);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = LocateEffectiveTLDFile(effTLDFile, PR_TRUE);

  // Since the profile copy isn't strictly needed, ignore any errors trying to
  // find or read it, in order to allow testing using xpcshell.
  if (NS_FAILED(rv) || nsnull == effTLDFile)
    return NS_OK;

  return LoadOneEffectiveTLDFile(effTLDFile);
}
