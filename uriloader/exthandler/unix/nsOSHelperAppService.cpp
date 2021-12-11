/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <sys/types.h>
#include <sys/stat.h>

#include "nsOSHelperAppService.h"
#include "nsMIMEInfoUnix.h"
#ifdef MOZ_WIDGET_GTK
#  include "nsGNOMERegistry.h"
#  ifdef MOZ_BUILD_APP_IS_BROWSER
#    include "nsIToolkitShellService.h"
#    include "nsIGNOMEShellService.h"
#  endif
#endif
#include "nsISupports.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsIFileStreams.h"
#include "nsILineInputStream.h"
#include "nsIFile.h"
#include "nsIProcess.h"
#include "nsNetCID.h"
#include "nsXPCOM.h"
#include "nsComponentManagerUtils.h"
#include "nsCRT.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsXULAppAPI.h"
#include "ContentHandlerService.h"
#include "prenv.h"  // for PR_GetEnv()
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_browser.h"
#include "nsMimeTypes.h"

using namespace mozilla;

#define LOG(args) MOZ_LOG(mLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(mLog, mozilla::LogLevel::Debug)

static nsresult FindSemicolon(nsAString::const_iterator& aSemicolon_iter,
                              const nsAString::const_iterator& aEnd_iter);
static nsresult ParseMIMEType(const nsAString::const_iterator& aStart_iter,
                              nsAString::const_iterator& aMajorTypeStart,
                              nsAString::const_iterator& aMajorTypeEnd,
                              nsAString::const_iterator& aMinorTypeStart,
                              nsAString::const_iterator& aMinorTypeEnd,
                              const nsAString::const_iterator& aEnd_iter);

inline bool IsNetscapeFormat(const nsACString& aBuffer);

nsOSHelperAppService::~nsOSHelperAppService() {}

/*
 * Take a command with all the mailcap escapes in it and unescape it
 * Ideally this needs the mime type, mime type options, and location of the
 * temporary file, but this last can't be got from here
 */
// static
nsresult nsOSHelperAppService::UnescapeCommand(const nsAString& aEscapedCommand,
                                               const nsAString& aMajorType,
                                               const nsAString& aMinorType,
                                               nsACString& aUnEscapedCommand) {
  LOG(("-- UnescapeCommand"));
  LOG(("Command to escape: '%s'\n",
       NS_LossyConvertUTF16toASCII(aEscapedCommand).get()));
  //  XXX This function will need to get the mime type and various stuff like
  //  that being passed in to work properly

  LOG(
      ("UnescapeCommand really needs some work -- it should actually do some "
       "unescaping\n"));

  CopyUTF16toUTF8(aEscapedCommand, aUnEscapedCommand);
  LOG(("Escaped command: '%s'\n", PromiseFlatCString(aUnEscapedCommand).get()));
  return NS_OK;
}

/* Put aSemicolon_iter at the first non-escaped semicolon after
 * aStart_iter but before aEnd_iter
 */

static nsresult FindSemicolon(nsAString::const_iterator& aSemicolon_iter,
                              const nsAString::const_iterator& aEnd_iter) {
  bool semicolonFound = false;
  while (aSemicolon_iter != aEnd_iter && !semicolonFound) {
    switch (*aSemicolon_iter) {
      case '\\':
        aSemicolon_iter.advance(2);
        break;
      case ';':
        semicolonFound = true;
        break;
      default:
        ++aSemicolon_iter;
        break;
    }
  }
  return NS_OK;
}

static nsresult ParseMIMEType(const nsAString::const_iterator& aStart_iter,
                              nsAString::const_iterator& aMajorTypeStart,
                              nsAString::const_iterator& aMajorTypeEnd,
                              nsAString::const_iterator& aMinorTypeStart,
                              nsAString::const_iterator& aMinorTypeEnd,
                              const nsAString::const_iterator& aEnd_iter) {
  nsAString::const_iterator iter(aStart_iter);

  // skip leading whitespace
  while (iter != aEnd_iter && nsCRT::IsAsciiSpace(*iter)) {
    ++iter;
  }

  if (iter == aEnd_iter) {
    return NS_ERROR_INVALID_ARG;
  }

  aMajorTypeStart = iter;

  // find major/minor separator ('/')
  while (iter != aEnd_iter && *iter != '/') {
    ++iter;
  }

  if (iter == aEnd_iter) {
    return NS_ERROR_INVALID_ARG;
  }

  aMajorTypeEnd = iter;

  // skip '/'
  ++iter;

  if (iter == aEnd_iter) {
    return NS_ERROR_INVALID_ARG;
  }

  aMinorTypeStart = iter;

  // find end of minor type, delimited by whitespace or ';'
  while (iter != aEnd_iter && !nsCRT::IsAsciiSpace(*iter) && *iter != ';') {
    ++iter;
  }

  aMinorTypeEnd = iter;

  return NS_OK;
}

// static
nsresult nsOSHelperAppService::GetFileLocation(const char* aPrefName,
                                               const char* aEnvVarName,
                                               nsAString& aFileLocation) {
  LOG(("-- GetFileLocation.  Pref: '%s'  EnvVar: '%s'\n", aPrefName,
       aEnvVarName));
  MOZ_ASSERT(aPrefName, "Null pref name passed; don't do that!");

  aFileLocation.Truncate();
  /* The lookup order is:
     1) user pref
     2) env var
     3) pref
  */
  NS_ENSURE_TRUE(Preferences::GetRootBranch(), NS_ERROR_FAILURE);

  /*
    If we have an env var we should check whether the pref is a user
    pref.  If we do not, we don't care.
  */
  if (Preferences::HasUserValue(aPrefName) &&
      NS_SUCCEEDED(Preferences::GetString(aPrefName, aFileLocation))) {
    return NS_OK;
  }

  if (aEnvVarName && *aEnvVarName) {
    char* prefValue = PR_GetEnv(aEnvVarName);
    if (prefValue && *prefValue) {
      // the pref is in the system charset and it's a filepath... The
      // natural way to do the charset conversion is by just initing
      // an nsIFile with the native path and asking it for the Unicode
      // version.
      nsresult rv;
      nsCOMPtr<nsIFile> file(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = file->InitWithNativePath(nsDependentCString(prefValue));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = file->GetPath(aFileLocation);
      NS_ENSURE_SUCCESS(rv, rv);

      return NS_OK;
    }
  }

  return Preferences::GetString(aPrefName, aFileLocation);
}

/* Get the mime.types file names from prefs and look up info in them
   based on extension */
// static
nsresult nsOSHelperAppService::LookUpTypeAndDescription(
    const nsAString& aFileExtension, nsAString& aMajorType,
    nsAString& aMinorType, nsAString& aDescription, bool aUserData) {
  LOG(("-- LookUpTypeAndDescription for extension '%s'\n",
       NS_LossyConvertUTF16toASCII(aFileExtension).get()));
  nsAutoString mimeFileName;

  const char* filenamePref = aUserData ? "helpers.private_mime_types_file"
                                       : "helpers.global_mime_types_file";

  nsresult rv = GetFileLocation(filenamePref, nullptr, mimeFileName);
  if (NS_SUCCEEDED(rv) && !mimeFileName.IsEmpty()) {
    rv = GetTypeAndDescriptionFromMimetypesFile(
        mimeFileName, aFileExtension, aMajorType, aMinorType, aDescription);
  } else {
    rv = NS_ERROR_NOT_AVAILABLE;
  }

  return rv;
}

inline bool IsNetscapeFormat(const nsACString& aBuffer) {
  return StringBeginsWith(
             aBuffer,
             nsLiteralCString(
                 "#--Netscape Communications Corporation MIME Information")) ||
         StringBeginsWith(aBuffer, "#--MCOM MIME Information"_ns);
}

/*
 * Create a file stream and line input stream for the filename.
 * Leaves the first line of the file in aBuffer and sets the format to
 *  true for netscape files and false for normail ones
 */
// static
nsresult nsOSHelperAppService::CreateInputStream(
    const nsAString& aFilename, nsIFileInputStream** aFileInputStream,
    nsILineInputStream** aLineInputStream, nsACString& aBuffer,
    bool* aNetscapeFormat, bool* aMore) {
  LOG(("-- CreateInputStream"));
  nsresult rv = NS_OK;

  nsCOMPtr<nsIFile> file(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
  if (NS_FAILED(rv)) return rv;
  rv = file->InitWithPath(aFilename);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIFileInputStream> fileStream(
      do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID, &rv));
  if (NS_FAILED(rv)) return rv;
  rv = fileStream->Init(file, -1, -1, false);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsILineInputStream> lineStream(do_QueryInterface(fileStream, &rv));

  if (NS_FAILED(rv)) {
    LOG(("Interface trouble in stream land!"));
    return rv;
  }

  rv = lineStream->ReadLine(aBuffer, aMore);
  if (NS_FAILED(rv)) {
    fileStream->Close();
    return rv;
  }

  *aNetscapeFormat = IsNetscapeFormat(aBuffer);

  *aFileInputStream = fileStream;
  NS_ADDREF(*aFileInputStream);
  *aLineInputStream = lineStream;
  NS_ADDREF(*aLineInputStream);

  return NS_OK;
}

/* Open the file, read the first line, decide what type of file it is,
   then get info based on extension */
// static
nsresult nsOSHelperAppService::GetTypeAndDescriptionFromMimetypesFile(
    const nsAString& aFilename, const nsAString& aFileExtension,
    nsAString& aMajorType, nsAString& aMinorType, nsAString& aDescription) {
  LOG(("-- GetTypeAndDescriptionFromMimetypesFile\n"));
  LOG(("Getting type and description from types file '%s'\n",
       NS_LossyConvertUTF16toASCII(aFilename).get()));
  LOG(("Using extension '%s'\n",
       NS_LossyConvertUTF16toASCII(aFileExtension).get()));
  nsCOMPtr<nsIFileInputStream> mimeFile;
  nsCOMPtr<nsILineInputStream> mimeTypes;
  bool netscapeFormat;
  nsAutoString buf;
  nsAutoCString cBuf;
  bool more = false;
  nsresult rv = CreateInputStream(aFilename, getter_AddRefs(mimeFile),
                                  getter_AddRefs(mimeTypes), cBuf,
                                  &netscapeFormat, &more);

  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoString extensions;
  nsAutoStringN<101> entry;
  nsAString::const_iterator majorTypeStart, majorTypeEnd, minorTypeStart,
      minorTypeEnd, descriptionStart, descriptionEnd;

  do {
    CopyASCIItoUTF16(cBuf, buf);
    // read through, building up an entry.  If we finish an entry, check for
    // a match and return out of the loop if we match

    // skip comments and empty lines
    if (!buf.IsEmpty() && buf.First() != '#') {
      entry.Append(buf);
      if (entry.Last() == '\\') {
        entry.Truncate(entry.Length() - 1);
        entry.Append(char16_t(
            ' '));  // in case there is no trailing whitespace on this line
      } else {      // we have a full entry
        LOG(("Current entry: '%s'\n",
             NS_LossyConvertUTF16toASCII(entry).get()));
        if (netscapeFormat) {
          rv = ParseNetscapeMIMETypesEntry(
              entry, majorTypeStart, majorTypeEnd, minorTypeStart, minorTypeEnd,
              extensions, descriptionStart, descriptionEnd);
          if (NS_FAILED(rv)) {
            // We sometimes get things like RealPlayer appending
            // "normal" entries to "Netscape" .mime.types files.  Try
            // to handle that.  Bug 106381.
            LOG(("Bogus entry; trying 'normal' mode\n"));
            rv = ParseNormalMIMETypesEntry(
                entry, majorTypeStart, majorTypeEnd, minorTypeStart,
                minorTypeEnd, extensions, descriptionStart, descriptionEnd);
          }
        } else {
          rv = ParseNormalMIMETypesEntry(
              entry, majorTypeStart, majorTypeEnd, minorTypeStart, minorTypeEnd,
              extensions, descriptionStart, descriptionEnd);
          if (NS_FAILED(rv)) {
            // We sometimes get things like StarOffice prepending
            // "normal" entries to "Netscape" .mime.types files.  Try
            // to handle that.  Bug 136670.
            LOG(("Bogus entry; trying 'Netscape' mode\n"));
            rv = ParseNetscapeMIMETypesEntry(
                entry, majorTypeStart, majorTypeEnd, minorTypeStart,
                minorTypeEnd, extensions, descriptionStart, descriptionEnd);
          }
        }

        if (NS_SUCCEEDED(rv)) {  // entry parses
          nsAString::const_iterator start, end;
          extensions.BeginReading(start);
          extensions.EndReading(end);
          nsAString::const_iterator iter(start);

          while (start != end) {
            FindCharInReadable(',', iter, end);
            if (Substring(start, iter)
                    .Equals(aFileExtension,
                            nsCaseInsensitiveStringComparator)) {
              // it's a match.  Assign the type and description and run
              aMajorType.Assign(Substring(majorTypeStart, majorTypeEnd));
              aMinorType.Assign(Substring(minorTypeStart, minorTypeEnd));
              aDescription.Assign(Substring(descriptionStart, descriptionEnd));
              mimeFile->Close();
              return NS_OK;
            }
            if (iter != end) {
              ++iter;
            }
            start = iter;
          }
        } else {
          LOG(("Failed to parse entry: %s\n",
               NS_LossyConvertUTF16toASCII(entry).get()));
        }
        // truncate the entry for the next iteration
        entry.Truncate();
      }
    }
    if (!more) {
      rv = NS_ERROR_NOT_AVAILABLE;
      break;
    }
    // read the next line
    rv = mimeTypes->ReadLine(cBuf, &more);
  } while (NS_SUCCEEDED(rv));

  mimeFile->Close();
  return rv;
}

/* Get the mime.types file names from prefs and look up info in them
   based on mimetype  */
// static
nsresult nsOSHelperAppService::LookUpExtensionsAndDescription(
    const nsAString& aMajorType, const nsAString& aMinorType,
    nsAString& aFileExtensions, nsAString& aDescription) {
  LOG(("-- LookUpExtensionsAndDescription for type '%s/%s'\n",
       NS_LossyConvertUTF16toASCII(aMajorType).get(),
       NS_LossyConvertUTF16toASCII(aMinorType).get()));
  nsAutoString mimeFileName;

  nsresult rv =
      GetFileLocation("helpers.private_mime_types_file", nullptr, mimeFileName);
  if (NS_SUCCEEDED(rv) && !mimeFileName.IsEmpty()) {
    rv = GetExtensionsAndDescriptionFromMimetypesFile(
        mimeFileName, aMajorType, aMinorType, aFileExtensions, aDescription);
  } else {
    rv = NS_ERROR_NOT_AVAILABLE;
  }
  if (NS_FAILED(rv) || aFileExtensions.IsEmpty()) {
    rv = GetFileLocation("helpers.global_mime_types_file", nullptr,
                         mimeFileName);
    if (NS_SUCCEEDED(rv) && !mimeFileName.IsEmpty()) {
      rv = GetExtensionsAndDescriptionFromMimetypesFile(
          mimeFileName, aMajorType, aMinorType, aFileExtensions, aDescription);
    } else {
      rv = NS_ERROR_NOT_AVAILABLE;
    }
  }
  return rv;
}

/* Open the file, read the first line, decide what type of file it is,
   then get info based on extension */
// static
nsresult nsOSHelperAppService::GetExtensionsAndDescriptionFromMimetypesFile(
    const nsAString& aFilename, const nsAString& aMajorType,
    const nsAString& aMinorType, nsAString& aFileExtensions,
    nsAString& aDescription) {
  LOG(("-- GetExtensionsAndDescriptionFromMimetypesFile\n"));
  LOG(("Getting extensions and description from types file '%s'\n",
       NS_LossyConvertUTF16toASCII(aFilename).get()));
  LOG(("Using type '%s/%s'\n", NS_LossyConvertUTF16toASCII(aMajorType).get(),
       NS_LossyConvertUTF16toASCII(aMinorType).get()));
  nsCOMPtr<nsIFileInputStream> mimeFile;
  nsCOMPtr<nsILineInputStream> mimeTypes;
  bool netscapeFormat;
  nsAutoCString cBuf;
  nsAutoString buf;
  bool more = false;
  nsresult rv = CreateInputStream(aFilename, getter_AddRefs(mimeFile),
                                  getter_AddRefs(mimeTypes), cBuf,
                                  &netscapeFormat, &more);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoString extensions;
  nsAutoStringN<101> entry;
  nsAString::const_iterator majorTypeStart, majorTypeEnd, minorTypeStart,
      minorTypeEnd, descriptionStart, descriptionEnd;

  do {
    CopyASCIItoUTF16(cBuf, buf);
    // read through, building up an entry.  If we finish an entry, check for
    // a match and return out of the loop if we match

    // skip comments and empty lines
    if (!buf.IsEmpty() && buf.First() != '#') {
      entry.Append(buf);
      if (entry.Last() == '\\') {
        entry.Truncate(entry.Length() - 1);
        entry.Append(char16_t(
            ' '));  // in case there is no trailing whitespace on this line
      } else {      // we have a full entry
        LOG(("Current entry: '%s'\n",
             NS_LossyConvertUTF16toASCII(entry).get()));
        if (netscapeFormat) {
          rv = ParseNetscapeMIMETypesEntry(
              entry, majorTypeStart, majorTypeEnd, minorTypeStart, minorTypeEnd,
              extensions, descriptionStart, descriptionEnd);

          if (NS_FAILED(rv)) {
            // We sometimes get things like RealPlayer appending
            // "normal" entries to "Netscape" .mime.types files.  Try
            // to handle that.  Bug 106381.
            LOG(("Bogus entry; trying 'normal' mode\n"));
            rv = ParseNormalMIMETypesEntry(
                entry, majorTypeStart, majorTypeEnd, minorTypeStart,
                minorTypeEnd, extensions, descriptionStart, descriptionEnd);
          }
        } else {
          rv = ParseNormalMIMETypesEntry(
              entry, majorTypeStart, majorTypeEnd, minorTypeStart, minorTypeEnd,
              extensions, descriptionStart, descriptionEnd);

          if (NS_FAILED(rv)) {
            // We sometimes get things like StarOffice prepending
            // "normal" entries to "Netscape" .mime.types files.  Try
            // to handle that.  Bug 136670.
            LOG(("Bogus entry; trying 'Netscape' mode\n"));
            rv = ParseNetscapeMIMETypesEntry(
                entry, majorTypeStart, majorTypeEnd, minorTypeStart,
                minorTypeEnd, extensions, descriptionStart, descriptionEnd);
          }
        }

        if (NS_SUCCEEDED(rv) &&
            Substring(majorTypeStart, majorTypeEnd)
                .Equals(aMajorType, nsCaseInsensitiveStringComparator) &&
            Substring(minorTypeStart, minorTypeEnd)
                .Equals(aMinorType, nsCaseInsensitiveStringComparator)) {
          // it's a match
          aFileExtensions.Assign(extensions);
          aDescription.Assign(Substring(descriptionStart, descriptionEnd));
          mimeFile->Close();
          return NS_OK;
        }
        if (NS_FAILED(rv)) {
          LOG(("Failed to parse entry: %s\n",
               NS_LossyConvertUTF16toASCII(entry).get()));
        }

        entry.Truncate();
      }
    }
    if (!more) {
      rv = NS_ERROR_NOT_AVAILABLE;
      break;
    }
    // read the next line
    rv = mimeTypes->ReadLine(cBuf, &more);
  } while (NS_SUCCEEDED(rv));

  mimeFile->Close();
  return rv;
}

/*
 * This parses a Netscape format mime.types entry.  There are two
 * possible formats:
 *
 * type=foo/bar; options exts="baz" description="Some type"
 *
 * and
 *
 * type=foo/bar; options description="Some type" exts="baz"
 */
// static
nsresult nsOSHelperAppService::ParseNetscapeMIMETypesEntry(
    const nsAString& aEntry, nsAString::const_iterator& aMajorTypeStart,
    nsAString::const_iterator& aMajorTypeEnd,
    nsAString::const_iterator& aMinorTypeStart,
    nsAString::const_iterator& aMinorTypeEnd, nsAString& aExtensions,
    nsAString::const_iterator& aDescriptionStart,
    nsAString::const_iterator& aDescriptionEnd) {
  LOG(("-- ParseNetscapeMIMETypesEntry\n"));
  NS_ASSERTION(!aEntry.IsEmpty(),
               "Empty Netscape MIME types entry being parsed.");

  nsAString::const_iterator start_iter, end_iter, match_start, match_end;

  aEntry.BeginReading(start_iter);
  aEntry.EndReading(end_iter);

  // skip trailing whitespace
  do {
    --end_iter;
  } while (end_iter != start_iter && nsCRT::IsAsciiSpace(*end_iter));
  // if we're pointing to a quote, don't advance -- we don't want to
  // include the quote....
  if (*end_iter != '"') ++end_iter;
  match_start = start_iter;
  match_end = end_iter;

  // Get the major and minor types
  // First the major type
  if (!FindInReadable(u"type="_ns, match_start, match_end)) {
    return NS_ERROR_FAILURE;
  }

  match_start = match_end;

  while (match_end != end_iter && *match_end != '/') {
    ++match_end;
  }
  if (match_end == end_iter) {
    return NS_ERROR_FAILURE;
  }

  aMajorTypeStart = match_start;
  aMajorTypeEnd = match_end;

  // now the minor type
  if (++match_end == end_iter) {
    return NS_ERROR_FAILURE;
  }

  match_start = match_end;

  while (match_end != end_iter && !nsCRT::IsAsciiSpace(*match_end) &&
         *match_end != ';') {
    ++match_end;
  }
  if (match_end == end_iter) {
    return NS_ERROR_FAILURE;
  }

  aMinorTypeStart = match_start;
  aMinorTypeEnd = match_end;

  // ignore everything up to the end of the mime type from here on
  start_iter = match_end;

  // get the extensions
  match_start = match_end;
  match_end = end_iter;
  if (FindInReadable(u"exts="_ns, match_start, match_end)) {
    nsAString::const_iterator extStart, extEnd;

    if (match_end == end_iter ||
        (*match_end == '"' && ++match_end == end_iter)) {
      return NS_ERROR_FAILURE;
    }

    extStart = match_end;
    match_start = extStart;
    match_end = end_iter;
    if (FindInReadable(u"desc=\""_ns, match_start, match_end)) {
      // exts= before desc=, so we have to find the actual end of the extensions
      extEnd = match_start;
      if (extEnd == extStart) {
        return NS_ERROR_FAILURE;
      }

      do {
        --extEnd;
      } while (extEnd != extStart && nsCRT::IsAsciiSpace(*extEnd));

      if (extEnd != extStart && *extEnd == '"') {
        --extEnd;
      }
    } else {
      // desc= before exts=, so we can use end_iter as the end of the extensions
      extEnd = end_iter;
    }
    aExtensions = Substring(extStart, extEnd);
  } else {
    // no extensions
    aExtensions.Truncate();
  }

  // get the description
  match_start = start_iter;
  match_end = end_iter;
  if (FindInReadable(u"desc=\""_ns, match_start, match_end)) {
    aDescriptionStart = match_end;
    match_start = aDescriptionStart;
    match_end = end_iter;
    if (FindInReadable(u"exts="_ns, match_start, match_end)) {
      // exts= after desc=, so have to find actual end of description
      aDescriptionEnd = match_start;
      if (aDescriptionEnd == aDescriptionStart) {
        return NS_ERROR_FAILURE;
      }

      do {
        --aDescriptionEnd;
      } while (aDescriptionEnd != aDescriptionStart &&
               nsCRT::IsAsciiSpace(*aDescriptionEnd));
    } else {
      // desc= after exts=, so use end_iter for the description end
      aDescriptionEnd = end_iter;
    }
  } else {
    // no description
    aDescriptionStart = start_iter;
    aDescriptionEnd = start_iter;
  }

  return NS_OK;
}

/*
 * This parses a normal format mime.types entry.  The format is:
 *
 * major/minor    ext1 ext2 ext3
 */
// static
nsresult nsOSHelperAppService::ParseNormalMIMETypesEntry(
    const nsAString& aEntry, nsAString::const_iterator& aMajorTypeStart,
    nsAString::const_iterator& aMajorTypeEnd,
    nsAString::const_iterator& aMinorTypeStart,
    nsAString::const_iterator& aMinorTypeEnd, nsAString& aExtensions,
    nsAString::const_iterator& aDescriptionStart,
    nsAString::const_iterator& aDescriptionEnd) {
  LOG(("-- ParseNormalMIMETypesEntry\n"));
  NS_ASSERTION(!aEntry.IsEmpty(),
               "Empty Normal MIME types entry being parsed.");

  nsAString::const_iterator start_iter, end_iter, iter;

  aEntry.BeginReading(start_iter);
  aEntry.EndReading(end_iter);

  // no description
  aDescriptionStart = start_iter;
  aDescriptionEnd = start_iter;

  // skip leading whitespace
  while (start_iter != end_iter && nsCRT::IsAsciiSpace(*start_iter)) {
    ++start_iter;
  }
  if (start_iter == end_iter) {
    return NS_ERROR_FAILURE;
  }
  // skip trailing whitespace
  do {
    --end_iter;
  } while (end_iter != start_iter && nsCRT::IsAsciiSpace(*end_iter));

  ++end_iter;  // point to first whitespace char (or to end of string)
  iter = start_iter;

  // get the major type
  if (!FindCharInReadable('/', iter, end_iter)) return NS_ERROR_FAILURE;

  nsAString::const_iterator equals_sign_iter(start_iter);
  if (FindCharInReadable('=', equals_sign_iter, iter))
    return NS_ERROR_FAILURE;  // see bug 136670

  aMajorTypeStart = start_iter;
  aMajorTypeEnd = iter;

  // get the minor type
  if (++iter == end_iter) {
    return NS_ERROR_FAILURE;
  }
  start_iter = iter;

  while (iter != end_iter && !nsCRT::IsAsciiSpace(*iter)) {
    ++iter;
  }
  aMinorTypeStart = start_iter;
  aMinorTypeEnd = iter;

  // get the extensions
  aExtensions.Truncate();
  while (iter != end_iter) {
    while (iter != end_iter && nsCRT::IsAsciiSpace(*iter)) {
      ++iter;
    }

    start_iter = iter;
    while (iter != end_iter && !nsCRT::IsAsciiSpace(*iter)) {
      ++iter;
    }
    aExtensions.Append(Substring(start_iter, iter));
    if (iter != end_iter) {  // not the last extension
      aExtensions.Append(char16_t(','));
    }
  }

  return NS_OK;
}

// static
nsresult nsOSHelperAppService::LookUpHandlerAndDescription(
    const nsAString& aMajorType, const nsAString& aMinorType,
    nsAString& aHandler, nsAString& aDescription, nsAString& aMozillaFlags) {
  // The mailcap lookup is two-pass to handle the case of mailcap files
  // that have something like:
  //
  // text/*; emacs %s
  // text/rtf; soffice %s
  //
  // in that order.  We want to pick up "soffice" for text/rtf in such cases
  nsresult rv = DoLookUpHandlerAndDescription(
      aMajorType, aMinorType, aHandler, aDescription, aMozillaFlags, true);
  if (NS_FAILED(rv)) {
    rv = DoLookUpHandlerAndDescription(aMajorType, aMinorType, aHandler,
                                       aDescription, aMozillaFlags, false);
  }

  // maybe we have an entry for "aMajorType/*"?
  if (NS_FAILED(rv)) {
    rv = DoLookUpHandlerAndDescription(aMajorType, u"*"_ns, aHandler,
                                       aDescription, aMozillaFlags, true);
  }

  if (NS_FAILED(rv)) {
    rv = DoLookUpHandlerAndDescription(aMajorType, u"*"_ns, aHandler,
                                       aDescription, aMozillaFlags, false);
  }

  return rv;
}

// static
nsresult nsOSHelperAppService::DoLookUpHandlerAndDescription(
    const nsAString& aMajorType, const nsAString& aMinorType,
    nsAString& aHandler, nsAString& aDescription, nsAString& aMozillaFlags,
    bool aUserData) {
  LOG(("-- LookUpHandlerAndDescription for type '%s/%s'\n",
       NS_LossyConvertUTF16toASCII(aMajorType).get(),
       NS_LossyConvertUTF16toASCII(aMinorType).get()));
  nsAutoString mailcapFileName;

  const char* filenamePref = aUserData ? "helpers.private_mailcap_file"
                                       : "helpers.global_mailcap_file";
  const char* filenameEnvVar = aUserData ? "PERSONAL_MAILCAP" : "MAILCAP";

  nsresult rv = GetFileLocation(filenamePref, filenameEnvVar, mailcapFileName);
  if (NS_SUCCEEDED(rv) && !mailcapFileName.IsEmpty()) {
    rv = GetHandlerAndDescriptionFromMailcapFile(mailcapFileName, aMajorType,
                                                 aMinorType, aHandler,
                                                 aDescription, aMozillaFlags);
  } else {
    rv = NS_ERROR_NOT_AVAILABLE;
  }

  return rv;
}

// static
nsresult nsOSHelperAppService::GetHandlerAndDescriptionFromMailcapFile(
    const nsAString& aFilename, const nsAString& aMajorType,
    const nsAString& aMinorType, nsAString& aHandler, nsAString& aDescription,
    nsAString& aMozillaFlags) {
  LOG(("-- GetHandlerAndDescriptionFromMailcapFile\n"));
  LOG(("Getting handler and description from mailcap file '%s'\n",
       NS_LossyConvertUTF16toASCII(aFilename).get()));
  LOG(("Using type '%s/%s'\n", NS_LossyConvertUTF16toASCII(aMajorType).get(),
       NS_LossyConvertUTF16toASCII(aMinorType).get()));

  nsresult rv = NS_OK;
  bool more = false;

  nsCOMPtr<nsIFile> file(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
  if (NS_FAILED(rv)) return rv;
  rv = file->InitWithPath(aFilename);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIFileInputStream> mailcapFile(
      do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID, &rv));
  if (NS_FAILED(rv)) return rv;
  rv = mailcapFile->Init(file, -1, -1, false);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsILineInputStream> mailcap(do_QueryInterface(mailcapFile, &rv));

  if (NS_FAILED(rv)) {
    LOG(("Interface trouble in stream land!"));
    return rv;
  }

  nsAutoStringN<129> entry;
  nsAutoStringN<81> buffer;
  nsAutoCStringN<81> cBuffer;
  rv = mailcap->ReadLine(cBuffer, &more);
  if (NS_FAILED(rv)) {
    mailcapFile->Close();
    return rv;
  }

  do {  // return on end-of-file in the loop

    CopyASCIItoUTF16(cBuffer, buffer);
    if (!buffer.IsEmpty() && buffer.First() != '#') {
      entry.Append(buffer);
      if (entry.Last() == '\\') {  // entry continues on next line
        entry.Truncate(entry.Length() - 1);
        entry.Append(char16_t(
            ' '));  // in case there is no trailing whitespace on this line
      } else {      // we have a full entry in entry.  Check it for the type
        LOG(("Current entry: '%s'\n",
             NS_LossyConvertUTF16toASCII(entry).get()));

        nsAString::const_iterator semicolon_iter, start_iter, end_iter,
            majorTypeStart, majorTypeEnd, minorTypeStart, minorTypeEnd;
        entry.BeginReading(start_iter);
        entry.EndReading(end_iter);
        semicolon_iter = start_iter;
        FindSemicolon(semicolon_iter, end_iter);
        if (semicolon_iter !=
            end_iter) {  // we have something resembling a valid entry
          rv = ParseMIMEType(start_iter, majorTypeStart, majorTypeEnd,
                             minorTypeStart, minorTypeEnd, semicolon_iter);
          if (NS_SUCCEEDED(rv) &&
              Substring(majorTypeStart, majorTypeEnd)
                  .Equals(aMajorType, nsCaseInsensitiveStringComparator) &&
              Substring(minorTypeStart, minorTypeEnd)
                  .Equals(aMinorType, nsCaseInsensitiveStringComparator)) {
            // we have a match
            bool match = true;
            ++semicolon_iter;  // point at the first char past the semicolon
            start_iter = semicolon_iter;  // handler string starts here
            FindSemicolon(semicolon_iter, end_iter);
            while (start_iter != semicolon_iter &&
                   nsCRT::IsAsciiSpace(*start_iter)) {
              ++start_iter;
            }

            LOG(("The real handler is: '%s'\n",
                 NS_LossyConvertUTF16toASCII(
                     Substring(start_iter, semicolon_iter))
                     .get()));

            // XXX ugly hack.  Just grab the executable name
            nsAString::const_iterator end_handler_iter = semicolon_iter;
            nsAString::const_iterator end_executable_iter = start_iter;
            while (end_executable_iter != end_handler_iter &&
                   !nsCRT::IsAsciiSpace(*end_executable_iter)) {
              ++end_executable_iter;
            }
            // XXX End ugly hack

            aHandler = Substring(start_iter, end_executable_iter);

            nsAString::const_iterator start_option_iter, end_optionname_iter,
                equal_sign_iter;
            bool equalSignFound;
            while (match && semicolon_iter != end_iter &&
                   ++semicolon_iter !=
                       end_iter) {  // there are options left and we still match
              start_option_iter = semicolon_iter;
              // skip over leading whitespace
              while (start_option_iter != end_iter &&
                     nsCRT::IsAsciiSpace(*start_option_iter)) {
                ++start_option_iter;
              }
              if (start_option_iter == end_iter) {  // nothing actually here
                break;
              }
              semicolon_iter = start_option_iter;
              FindSemicolon(semicolon_iter, end_iter);
              equal_sign_iter = start_option_iter;
              equalSignFound = false;
              while (equal_sign_iter != semicolon_iter && !equalSignFound) {
                switch (*equal_sign_iter) {
                  case '\\':
                    equal_sign_iter.advance(2);
                    break;
                  case '=':
                    equalSignFound = true;
                    break;
                  default:
                    ++equal_sign_iter;
                    break;
                }
              }
              end_optionname_iter = start_option_iter;
              // find end of option name
              while (end_optionname_iter != equal_sign_iter &&
                     !nsCRT::IsAsciiSpace(*end_optionname_iter)) {
                ++end_optionname_iter;
              }
              nsDependentSubstring optionName(start_option_iter,
                                              end_optionname_iter);
              if (equalSignFound) {
                // This is an option that has a name and value
                if (optionName.EqualsLiteral("description")) {
                  aDescription = Substring(++equal_sign_iter, semicolon_iter);
                } else if (optionName.EqualsLiteral("x-mozilla-flags")) {
                  aMozillaFlags = Substring(++equal_sign_iter, semicolon_iter);
                } else if (optionName.EqualsLiteral("test")) {
                  nsAutoCString testCommand;
                  rv = UnescapeCommand(
                      Substring(++equal_sign_iter, semicolon_iter), aMajorType,
                      aMinorType, testCommand);
                  if (NS_FAILED(rv)) continue;
                  nsCOMPtr<nsIProcess> process =
                      do_CreateInstance(NS_PROCESS_CONTRACTID, &rv);
                  if (NS_FAILED(rv)) continue;
                  nsCOMPtr<nsIFile> file(
                      do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
                  if (NS_FAILED(rv)) continue;
                  rv = file->InitWithNativePath("/bin/sh"_ns);
                  if (NS_FAILED(rv)) continue;
                  rv = process->Init(file);
                  if (NS_FAILED(rv)) continue;
                  const char* args[] = {"-c", testCommand.get()};
                  LOG(("Running Test: %s\n", testCommand.get()));
                  rv = process->Run(true, args, 2);
                  if (NS_FAILED(rv)) continue;
                  int32_t exitValue;
                  rv = process->GetExitValue(&exitValue);
                  if (NS_FAILED(rv)) continue;
                  LOG(("Exit code: %d\n", exitValue));
                  if (exitValue) {
                    match = false;
                  }
                }
              } else {
                // This is an option that just has a name but no value (eg
                // "copiousoutput")
                if (optionName.EqualsLiteral("needsterminal")) {
                  match = false;
                }
              }
            }

            if (match) {  // we did not fail any test clauses; all is good
              // get out of here
              mailcapFile->Close();
              return NS_OK;
            }
            // pretend that this match never happened
            aDescription.Truncate();
            aMozillaFlags.Truncate();
            aHandler.Truncate();
          }
        }
        // zero out the entry for the next cycle
        entry.Truncate();
      }
    }
    if (!more) {
      rv = NS_ERROR_NOT_AVAILABLE;
      break;
    }
    rv = mailcap->ReadLine(cBuffer, &more);
  } while (NS_SUCCEEDED(rv));
  mailcapFile->Close();
  return rv;
}

nsresult nsOSHelperAppService::OSProtocolHandlerExists(
    const char* aProtocolScheme, bool* aHandlerExists) {
  nsresult rv = NS_OK;

  if (!XRE_IsContentProcess()) {
#ifdef MOZ_WIDGET_GTK
    // Check the GNOME registry for a protocol handler
    *aHandlerExists = nsGNOMERegistry::HandlerExists(aProtocolScheme);
#else
    *aHandlerExists = false;
#endif
  } else {
    *aHandlerExists = false;
    nsCOMPtr<nsIHandlerService> handlerSvc =
        do_GetService(NS_HANDLERSERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv) && handlerSvc) {
      rv = handlerSvc->ExistsForProtocolOS(nsCString(aProtocolScheme),
                                           aHandlerExists);
    }
  }

  return rv;
}

NS_IMETHODIMP nsOSHelperAppService::GetApplicationDescription(
    const nsACString& aScheme, nsAString& _retval) {
#ifdef MOZ_WIDGET_GTK
  nsGNOMERegistry::GetAppDescForScheme(aScheme, _retval);
  return _retval.IsEmpty() ? NS_ERROR_NOT_AVAILABLE : NS_OK;
#else
  return NS_ERROR_NOT_AVAILABLE;
#endif
}

NS_IMETHODIMP nsOSHelperAppService::IsCurrentAppOSDefaultForProtocol(
    const nsACString& aScheme, bool* _retval) {
  *_retval = false;
#if defined(MOZ_BUILD_APP_IS_BROWSER) && defined(MOZ_WIDGET_GTK)
  if (nsCOMPtr<nsIGNOMEShellService> shell =
          do_GetService(NS_TOOLKITSHELLSERVICE_CONTRACTID)) {
    return shell->IsDefaultForScheme(aScheme, _retval);
  }
#endif
  return NS_OK;
}

nsresult nsOSHelperAppService::GetFileTokenForPath(
    const char16_t* platformAppPath, nsIFile** aFile) {
  LOG(("-- nsOSHelperAppService::GetFileTokenForPath: '%s'\n",
       NS_LossyConvertUTF16toASCII(platformAppPath).get()));
  if (!*platformAppPath) {  // empty filename--return error
    NS_WARNING("Empty filename passed in.");
    return NS_ERROR_INVALID_ARG;
  }

  // first check if the base class implementation finds anything
  nsresult rv =
      nsExternalHelperAppService::GetFileTokenForPath(platformAppPath, aFile);
  if (NS_SUCCEEDED(rv)) return rv;
  // If the reason for failure was that the file doesn't exist, return too
  // (because it means the path was absolute, and so that we shouldn't search in
  // the path)
  if (rv == NS_ERROR_FILE_NOT_FOUND) return rv;

  // If we get here, we really should have a relative path.
  NS_ASSERTION(*platformAppPath != char16_t('/'), "Unexpected absolute path");

  nsCOMPtr<nsIFile> localFile(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));

  if (!localFile) return NS_ERROR_NOT_INITIALIZED;

  bool exists = false;
  // ugly hack.  Walk the PATH variable...
  char* unixpath = PR_GetEnv("PATH");
  nsAutoCString path(unixpath);

  const char* start_iter = path.BeginReading(start_iter);
  const char* colon_iter = start_iter;
  const char* end_iter = path.EndReading(end_iter);

  while (start_iter != end_iter && !exists) {
    while (colon_iter != end_iter && *colon_iter != ':') {
      ++colon_iter;
    }
    localFile->InitWithNativePath(Substring(start_iter, colon_iter));
    rv = localFile->AppendRelativePath(nsDependentString(platformAppPath));
    // Failing AppendRelativePath is a bad thing - it should basically always
    // succeed given a relative path. Show a warning if it does fail.
    // To prevent infinite loops when it does fail, return at this point.
    NS_ENSURE_SUCCESS(rv, rv);
    localFile->Exists(&exists);
    if (!exists) {
      if (colon_iter == end_iter) {
        break;
      }
      ++colon_iter;
      start_iter = colon_iter;
    }
  }

  if (exists) {
    rv = NS_OK;
  } else {
    rv = NS_ERROR_NOT_AVAILABLE;
  }

  *aFile = localFile;
  NS_IF_ADDREF(*aFile);

  return rv;
}

already_AddRefed<nsMIMEInfoBase> nsOSHelperAppService::GetFromExtension(
    const nsCString& aFileExt) {
  // if the extension is empty, return immediately
  if (aFileExt.IsEmpty()) return nullptr;

  LOG(("Here we do an extension lookup for '%s'\n", aFileExt.get()));

  nsAutoString majorType, minorType, mime_types_description,
      mailcap_description, handler, mozillaFlags;

  nsresult rv =
      LookUpTypeAndDescription(NS_ConvertUTF8toUTF16(aFileExt), majorType,
                               minorType, mime_types_description, true);

  if (NS_FAILED(rv) || majorType.IsEmpty()) {
#ifdef MOZ_WIDGET_GTK
    LOG(("Looking in GNOME registry\n"));
    RefPtr<nsMIMEInfoBase> gnomeInfo =
        nsGNOMERegistry::GetFromExtension(aFileExt);
    if (gnomeInfo) {
      LOG(("Got MIMEInfo from GNOME registry\n"));
      return gnomeInfo.forget();
    }
#endif

    rv = LookUpTypeAndDescription(NS_ConvertUTF8toUTF16(aFileExt), majorType,
                                  minorType, mime_types_description, false);
  }

  if (NS_FAILED(rv)) return nullptr;

  NS_LossyConvertUTF16toASCII asciiMajorType(majorType);
  NS_LossyConvertUTF16toASCII asciiMinorType(minorType);

  LOG(
      ("Type/Description results:  majorType='%s', minorType='%s', "
       "description='%s'\n",
       asciiMajorType.get(), asciiMinorType.get(),
       NS_LossyConvertUTF16toASCII(mime_types_description).get()));

  if (majorType.IsEmpty() && minorType.IsEmpty()) {
    // we didn't get a type mapping, so we can't do anything useful
    return nullptr;
  }

  nsAutoCString mimeType(asciiMajorType + "/"_ns + asciiMinorType);
  RefPtr<nsMIMEInfoUnix> mimeInfo = new nsMIMEInfoUnix(mimeType);

  mimeInfo->AppendExtension(aFileExt);
  rv = LookUpHandlerAndDescription(majorType, minorType, handler,
                                   mailcap_description, mozillaFlags);
  LOG(
      ("Handler/Description results:  handler='%s', description='%s', "
       "mozillaFlags='%s'\n",
       NS_LossyConvertUTF16toASCII(handler).get(),
       NS_LossyConvertUTF16toASCII(mailcap_description).get(),
       NS_LossyConvertUTF16toASCII(mozillaFlags).get()));
  mailcap_description.Trim(" \t\"");
  mozillaFlags.Trim(" \t");
  if (!mime_types_description.IsEmpty()) {
    mimeInfo->SetDescription(mime_types_description);
  } else {
    mimeInfo->SetDescription(mailcap_description);
  }

  if (NS_SUCCEEDED(rv) && handler.IsEmpty()) {
    rv = NS_ERROR_NOT_AVAILABLE;
  }

  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIFile> handlerFile;
    rv = GetFileTokenForPath(handler.get(), getter_AddRefs(handlerFile));

    if (NS_SUCCEEDED(rv)) {
      mimeInfo->SetDefaultApplication(handlerFile);
      mozilla::StaticPrefs::browser_download_improvements_to_download_panel()
          ? mimeInfo->SetPreferredAction(nsIMIMEInfo::saveToDisk)
          : mimeInfo->SetPreferredAction(nsIMIMEInfo::useSystemDefault);
      mimeInfo->SetDefaultDescription(handler);
    }
  }

  if (NS_FAILED(rv)) {
    mimeInfo->SetPreferredAction(nsIMIMEInfo::saveToDisk);
  }

  return mimeInfo.forget();
}

already_AddRefed<nsMIMEInfoBase> nsOSHelperAppService::GetFromType(
    const nsCString& aMIMEType) {
  // if the type is empty, return immediately
  if (aMIMEType.IsEmpty()) return nullptr;

  LOG(("Here we do a mimetype lookup for '%s'\n", aMIMEType.get()));

  // extract the major and minor types
  NS_ConvertASCIItoUTF16 mimeType(aMIMEType);
  nsAString::const_iterator start_iter, end_iter, majorTypeStart, majorTypeEnd,
      minorTypeStart, minorTypeEnd;

  mimeType.BeginReading(start_iter);
  mimeType.EndReading(end_iter);

  // XXX FIXME: add typeOptions parsing in here
  nsresult rv = ParseMIMEType(start_iter, majorTypeStart, majorTypeEnd,
                              minorTypeStart, minorTypeEnd, end_iter);

  if (NS_FAILED(rv)) {
    return nullptr;
  }

  nsDependentSubstring majorType(majorTypeStart, majorTypeEnd);
  nsDependentSubstring minorType(minorTypeStart, minorTypeEnd);

  // First check the user's private mailcap file
  nsAutoString mailcap_description, handler, mozillaFlags;
  DoLookUpHandlerAndDescription(majorType, minorType, handler,
                                mailcap_description, mozillaFlags, true);

  LOG(("Private Handler/Description results:  handler='%s', description='%s'\n",
       NS_LossyConvertUTF16toASCII(handler).get(),
       NS_LossyConvertUTF16toASCII(mailcap_description).get()));

  // Now look up our extensions
  nsAutoString extensions, mime_types_description;
  LookUpExtensionsAndDescription(majorType, minorType, extensions,
                                 mime_types_description);

#ifdef MOZ_WIDGET_GTK
  if (handler.IsEmpty()) {
    RefPtr<nsMIMEInfoBase> gnomeInfo = nsGNOMERegistry::GetFromType(aMIMEType);
    if (gnomeInfo) {
      LOG(
          ("Got MIMEInfo from GNOME registry without extensions; setting them "
           "to %s\n",
           NS_LossyConvertUTF16toASCII(extensions).get()));

      NS_ASSERTION(!gnomeInfo->HasExtensions(), "How'd that happen?");
      gnomeInfo->SetFileExtensions(NS_ConvertUTF16toUTF8(extensions));
      return gnomeInfo.forget();
    }
  }
#endif

  if (handler.IsEmpty()) {
    DoLookUpHandlerAndDescription(majorType, minorType, handler,
                                  mailcap_description, mozillaFlags, false);
  }

  if (handler.IsEmpty()) {
    DoLookUpHandlerAndDescription(majorType, u"*"_ns, handler,
                                  mailcap_description, mozillaFlags, true);
  }

  if (handler.IsEmpty()) {
    DoLookUpHandlerAndDescription(majorType, u"*"_ns, handler,
                                  mailcap_description, mozillaFlags, false);
  }

  LOG(
      ("Handler/Description results:  handler='%s', description='%s', "
       "mozillaFlags='%s'\n",
       NS_LossyConvertUTF16toASCII(handler).get(),
       NS_LossyConvertUTF16toASCII(mailcap_description).get(),
       NS_LossyConvertUTF16toASCII(mozillaFlags).get()));

  mailcap_description.Trim(" \t\"");
  mozillaFlags.Trim(" \t");

  if (handler.IsEmpty() && extensions.IsEmpty() &&
      mailcap_description.IsEmpty() && mime_types_description.IsEmpty()) {
    // No real useful info
    return nullptr;
  }

  RefPtr<nsMIMEInfoUnix> mimeInfo = new nsMIMEInfoUnix(aMIMEType);

  mimeInfo->SetFileExtensions(NS_ConvertUTF16toUTF8(extensions));
  if (!mime_types_description.IsEmpty()) {
    mimeInfo->SetDescription(mime_types_description);
  } else {
    mimeInfo->SetDescription(mailcap_description);
  }

  rv = NS_ERROR_NOT_AVAILABLE;
  nsCOMPtr<nsIFile> handlerFile;
  if (!handler.IsEmpty()) {
    rv = GetFileTokenForPath(handler.get(), getter_AddRefs(handlerFile));
  }

  if (NS_SUCCEEDED(rv)) {
    mimeInfo->SetDefaultApplication(handlerFile);
    StaticPrefs::browser_download_improvements_to_download_panel()
        ? mimeInfo->SetPreferredAction(nsIMIMEInfo::saveToDisk)
        : mimeInfo->SetPreferredAction(nsIMIMEInfo::useSystemDefault);
    mimeInfo->SetDefaultDescription(handler);
  } else {
    mimeInfo->SetPreferredAction(nsIMIMEInfo::saveToDisk);
  }

  return mimeInfo.forget();
}

nsresult nsOSHelperAppService::GetMIMEInfoFromOS(const nsACString& aType,
                                                 const nsACString& aFileExt,
                                                 bool* aFound,
                                                 nsIMIMEInfo** aMIMEInfo) {
  *aFound = true;
  RefPtr<nsMIMEInfoBase> retval;
  // Fallback to lookup by extension when generic 'application/octet-stream'
  // content type is received.
  if (!aType.EqualsLiteral(APPLICATION_OCTET_STREAM)) {
    retval = GetFromType(PromiseFlatCString(aType));
  }
  bool hasDefault = false;
  if (retval) retval->GetHasDefaultHandler(&hasDefault);
  if (!retval || !hasDefault) {
    RefPtr<nsMIMEInfoBase> miByExt =
        GetFromExtension(PromiseFlatCString(aFileExt));
    // If we had no extension match, but a type match, use that
    if (!miByExt && retval) {
      retval.forget(aMIMEInfo);
      return NS_OK;
    }
    // If we had an extension match but no type match, set the mimetype and use
    // it
    if (!retval && miByExt) {
      if (!aType.IsEmpty()) miByExt->SetMIMEType(aType);
      miByExt.swap(retval);

      retval.forget(aMIMEInfo);
      return NS_OK;
    }
    // If we got nothing, make a new mimeinfo
    if (!retval) {
      *aFound = false;
      retval = new nsMIMEInfoUnix(aType);
      if (retval) {
        if (!aFileExt.IsEmpty()) retval->AppendExtension(aFileExt);
      }

      retval.forget(aMIMEInfo);
      return NS_OK;
    }

    // Copy the attributes of retval (mimeinfo from type) onto miByExt, to
    // return it
    // but reset to just collected mDefaultAppDescription (from ext)
    nsAutoString byExtDefault;
    miByExt->GetDefaultDescription(byExtDefault);
    retval->SetDefaultDescription(byExtDefault);
    retval->CopyBasicDataTo(miByExt);

    miByExt.swap(retval);
  }
  retval.forget(aMIMEInfo);
  return NS_OK;
}

NS_IMETHODIMP
nsOSHelperAppService::GetProtocolHandlerInfoFromOS(const nsACString& aScheme,
                                                   bool* found,
                                                   nsIHandlerInfo** _retval) {
  NS_ASSERTION(!aScheme.IsEmpty(), "No scheme was specified!");

  nsresult rv =
      OSProtocolHandlerExists(nsPromiseFlatCString(aScheme).get(), found);
  if (NS_FAILED(rv)) return rv;

  nsMIMEInfoUnix* handlerInfo =
      new nsMIMEInfoUnix(aScheme, nsMIMEInfoBase::eProtocolInfo);
  NS_ENSURE_TRUE(handlerInfo, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*_retval = handlerInfo);

  if (!*found) {
    // Code that calls this requires an object regardless if the OS has
    // something for us, so we return the empty object.
    return NS_OK;
  }

  nsAutoString desc;
  GetApplicationDescription(aScheme, desc);
  handlerInfo->SetDefaultDescription(desc);

  return NS_OK;
}
