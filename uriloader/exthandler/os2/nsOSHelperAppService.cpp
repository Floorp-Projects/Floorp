/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsOSHelperAppService.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsXPIDLString.h"
#include "nsIURL.h"
#include "nsIFileStreams.h"
#include "nsILineInputStream.h"
#include "nsIFile.h"
#include "nsIProcess.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsHashtable.h"
#include "nsCRT.h"
#include "prenv.h"      // for PR_GetEnv()
#include "nsMIMEInfoOS2.h"
#include "nsAutoPtr.h"
#include "nsIRwsService.h"
#include "nsIStringBundle.h"
#include "nsLocalHandlerApp.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"
#include <stdlib.h>     // for system()

using namespace mozilla;

//------------------------------------------------------------------------

// reduces overhead by preventing calls to nsRws when it isn't present
static bool sUseRws = true;

static nsresult
FindSemicolon(nsAString::const_iterator& aSemicolon_iter,
              const nsAString::const_iterator& aEnd_iter);
static nsresult
ParseMIMEType(const nsAString::const_iterator& aStart_iter,
              nsAString::const_iterator& aMajorTypeStart,
              nsAString::const_iterator& aMajorTypeEnd,
              nsAString::const_iterator& aMinorTypeStart,
              nsAString::const_iterator& aMinorTypeEnd,
              const nsAString::const_iterator& aEnd_iter);

inline bool
IsNetscapeFormat(const nsACString& aBuffer);

//------------------------------------------------------------------------

nsOSHelperAppService::nsOSHelperAppService() : nsExternalHelperAppService()
{
}

nsOSHelperAppService::~nsOSHelperAppService()
{}

/*
 * Take a command with all the mailcap escapes in it and unescape it
 * Ideally this needs the mime type, mime type options, and location of the
 * temporary file, but this last can't be got from here
 */
// static
nsresult
nsOSHelperAppService::UnescapeCommand(const nsAString& aEscapedCommand,
                                      const nsAString& aMajorType,
                                      const nsAString& aMinorType,
                                      nsHashtable& aTypeOptions,
                                      nsACString& aUnEscapedCommand) {
  LOG(("-- UnescapeCommand"));
  LOG(("Command to escape: '%s'\n",
       NS_LossyConvertUTF16toASCII(aEscapedCommand).get()));
  //  XXX This function will need to get the mime type and various stuff like that being passed in to work properly
  
  LOG(("UnescapeCommand really needs some work -- it should actually do some unescaping\n"));

  CopyUTF16toUTF8(aEscapedCommand, aUnEscapedCommand);
  LOG(("Escaped command: '%s'\n",
       PromiseFlatCString(aUnEscapedCommand).get()));
  return NS_OK;
}

/* Put aSemicolon_iter at the first non-escaped semicolon after
 * aStart_iter but before aEnd_iter
 */

static nsresult
FindSemicolon(nsAString::const_iterator& aSemicolon_iter,
              const nsAString::const_iterator& aEnd_iter) {
  bool semicolonFound = false;
  while (aSemicolon_iter != aEnd_iter && !semicolonFound) {
    switch(*aSemicolon_iter) {
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

static nsresult
ParseMIMEType(const nsAString::const_iterator& aStart_iter,
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
nsresult
nsOSHelperAppService::GetFileLocation(const char* aPrefName,
                                      const char* aEnvVarName,
                                      nsAString& aFileLocation) {
  LOG(("-- GetFileLocation.  Pref: '%s'  EnvVar: '%s'\n",
       aPrefName,
       aEnvVarName));
  NS_PRECONDITION(aPrefName, "Null pref name passed; don't do that!");

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
      NS_SUCCEEDED(Preferences::GetString(aPrefName, &aFileLocation))) {
    return NS_OK;
  }

  if (aEnvVarName && *aEnvVarName) {
    char* prefValue = PR_GetEnv(aEnvVarName);
    if (prefValue && *prefValue) {
      // the pref is in the system charset and it's a filepath... The
      // natural way to do the charset conversion is by just initing
      // an nsIFile with the native path and asking it for the Unicode
      // version.
      nsCOMPtr<nsIFile> file(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = file->InitWithNativePath(nsDependentCString(prefValue));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = file->GetPath(aFileLocation);
      NS_ENSURE_SUCCESS(rv, rv);
      return NS_OK;
    }
  }

  return Preferences::GetString(aPrefName, &aFileLocation);
}


/* Get the mime.types file names from prefs and look up info in them
   based on extension */
// static
nsresult
nsOSHelperAppService::LookUpTypeAndDescription(const nsAString& aFileExtension,
                                               nsAString& aMajorType,
                                               nsAString& aMinorType,
                                               nsAString& aDescription) {
  LOG(("-- LookUpTypeAndDescription for extension '%s'\n",
       NS_LossyConvertUTF16toASCII(aFileExtension).get()));
  nsresult rv = NS_OK;
  nsAutoString mimeFileName;

  rv = GetFileLocation("helpers.private_mime_types_file",
                       nullptr, mimeFileName);
  if (NS_SUCCEEDED(rv) && !mimeFileName.IsEmpty()) {
    rv = GetTypeAndDescriptionFromMimetypesFile(mimeFileName,
                                                aFileExtension,
                                                aMajorType,
                                                aMinorType,
                                                aDescription);
  } else {
    rv = NS_ERROR_NOT_AVAILABLE;
  }
  if (NS_FAILED(rv) || aMajorType.IsEmpty()) {
    rv = GetFileLocation("helpers.global_mime_types_file",
                         nullptr, mimeFileName);
    if (NS_SUCCEEDED(rv) && !mimeFileName.IsEmpty()) {
      rv = GetTypeAndDescriptionFromMimetypesFile(mimeFileName,
                                                  aFileExtension,
                                                  aMajorType,
                                                  aMinorType,
                                                  aDescription);
    } else {
      rv = NS_ERROR_NOT_AVAILABLE;
    }
  }
  return rv;
}

inline bool
IsNetscapeFormat(const nsACString& aBuffer) {
  return StringBeginsWith(aBuffer, NS_LITERAL_CSTRING("#--Netscape Communications Corporation MIME Information")) ||
         StringBeginsWith(aBuffer, NS_LITERAL_CSTRING("#--MCOM MIME Information"));
}

/*
 * Create a file stream and line input stream for the filename.
 * Leaves the first line of the file in aBuffer and sets the format to
 *  true for netscape files and false for normail ones
 */
// static
nsresult
nsOSHelperAppService::CreateInputStream(const nsAString& aFilename,
                                        nsIFileInputStream ** aFileInputStream,
                                        nsILineInputStream ** aLineInputStream,
                                        nsACString& aBuffer,
                                        bool * aNetscapeFormat,
                                        bool * aMore) {
  LOG(("-- CreateInputStream"));
  nsresult rv = NS_OK;

  nsCOMPtr<nsIFile> file(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  rv = file->InitWithPath(aFilename);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIFileInputStream> fileStream(do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  rv = fileStream->Init(file, -1, -1, false);
  if (NS_FAILED(rv))
    return rv;

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
nsresult
nsOSHelperAppService::GetTypeAndDescriptionFromMimetypesFile(const nsAString& aFilename,
                                                             const nsAString& aFileExtension,
                                                             nsAString& aMajorType,
                                                             nsAString& aMinorType,
                                                             nsAString& aDescription) {
  LOG(("-- GetTypeAndDescriptionFromMimetypesFile\n"));
  LOG(("Getting type and description from types file '%s'\n",
       NS_LossyConvertUTF16toASCII(aFilename).get()));
  LOG(("Using extension '%s'\n",
       NS_LossyConvertUTF16toASCII(aFileExtension).get()));
  nsresult rv = NS_OK;
  nsCOMPtr<nsIFileInputStream> mimeFile;
  nsCOMPtr<nsILineInputStream> mimeTypes;
  bool netscapeFormat;
  nsAutoString buf;
  nsCAutoString cBuf;
  bool more = false;
  rv = CreateInputStream(aFilename, getter_AddRefs(mimeFile), getter_AddRefs(mimeTypes),
                         cBuf, &netscapeFormat, &more);

  if (NS_FAILED(rv)) {
    return rv;
  }
  

  nsAutoString extensions;
  nsString entry;
  entry.SetCapacity(100);
  nsAString::const_iterator majorTypeStart, majorTypeEnd,
                            minorTypeStart, minorTypeEnd,
                            descriptionStart, descriptionEnd;

  do {
    CopyASCIItoUTF16(cBuf, buf);
    // read through, building up an entry.  If we finish an entry, check for
    // a match and return out of the loop if we match

    // skip comments and empty lines
    if (!buf.IsEmpty() && buf.First() != '#') {
      entry.Append(buf);
      if (entry.Last() == '\\') {
        entry.Truncate(entry.Length() - 1);
        entry.Append(PRUnichar(' '));  // in case there is no trailing whitespace on this line
      } else {  // we have a full entry
        LOG(("Current entry: '%s'\n",
             NS_LossyConvertUTF16toASCII(entry).get()));
        if (netscapeFormat) {
          rv = ParseNetscapeMIMETypesEntry(entry,
                                           majorTypeStart, majorTypeEnd,
                                           minorTypeStart, minorTypeEnd,
                                           extensions,
                                           descriptionStart, descriptionEnd);
          if (NS_FAILED(rv)) {
            // We sometimes get things like RealPlayer appending
            // "normal" entries to "Netscape" .mime.types files.  Try
            // to handle that.  Bug 106381.
            LOG(("Bogus entry; trying 'normal' mode\n"));
            rv = ParseNormalMIMETypesEntry(entry,
                                           majorTypeStart, majorTypeEnd,
                                           minorTypeStart, minorTypeEnd,
                                           extensions,
                                           descriptionStart, descriptionEnd);
          }
        } else {
          rv = ParseNormalMIMETypesEntry(entry,
                                         majorTypeStart, majorTypeEnd,
                                         minorTypeStart, minorTypeEnd,
                                         extensions,
                                         descriptionStart, descriptionEnd);
          if (NS_FAILED(rv)) {
            // We sometimes get things like StarOffice prepending
            // "normal" entries to "Netscape" .mime.types files.  Try
            // to handle that.  Bug 136670.
            LOG(("Bogus entry; trying 'Netscape' mode\n"));
            rv = ParseNetscapeMIMETypesEntry(entry,
                                             majorTypeStart, majorTypeEnd,
                                             minorTypeStart, minorTypeEnd,
                                             extensions,
                                             descriptionStart, descriptionEnd);
          }
        }

        if (NS_SUCCEEDED(rv)) { // entry parses
          nsAString::const_iterator start, end;
          extensions.BeginReading(start);
          extensions.EndReading(end);
          nsAString::const_iterator iter(start);

          while (start != end) {
            FindCharInReadable(',', iter, end);
            if (Substring(start, iter).Equals(aFileExtension,
                                              nsCaseInsensitiveStringComparator())) {
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
          LOG(("Failed to parse entry: %s\n", NS_LossyConvertUTF16toASCII(entry).get()));
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
nsresult
nsOSHelperAppService::LookUpExtensionsAndDescription(const nsAString& aMajorType,
                                                     const nsAString& aMinorType,
                                                     nsAString& aFileExtensions,
                                                     nsAString& aDescription) {
  LOG(("-- LookUpExtensionsAndDescription for type '%s/%s'\n",
       NS_LossyConvertUTF16toASCII(aMajorType).get(),
       NS_LossyConvertUTF16toASCII(aMinorType).get()));
  nsresult rv = NS_OK;
  nsAutoString mimeFileName;

  rv = GetFileLocation("helpers.private_mime_types_file",
                       nullptr, mimeFileName);
  if (NS_SUCCEEDED(rv) && !mimeFileName.IsEmpty()) {
    rv = GetExtensionsAndDescriptionFromMimetypesFile(mimeFileName,
                                                      aMajorType,
                                                      aMinorType,
                                                      aFileExtensions,
                                                      aDescription);
  } else {
    rv = NS_ERROR_NOT_AVAILABLE;
  }
  if (NS_FAILED(rv) || aFileExtensions.IsEmpty()) {
    rv = GetFileLocation("helpers.global_mime_types_file",
                         nullptr, mimeFileName);
    if (NS_SUCCEEDED(rv) && !mimeFileName.IsEmpty()) {
      rv = GetExtensionsAndDescriptionFromMimetypesFile(mimeFileName,
                                                        aMajorType,
                                                        aMinorType,
                                                        aFileExtensions,
                                                        aDescription);
    } else {
      rv = NS_ERROR_NOT_AVAILABLE;
    }
  }
  return rv;
}

/* Open the file, read the first line, decide what type of file it is,
   then get info based on extension */
// static
nsresult
nsOSHelperAppService::GetExtensionsAndDescriptionFromMimetypesFile(const nsAString& aFilename,
                                                                   const nsAString& aMajorType,
                                                                   const nsAString& aMinorType,
                                                                   nsAString& aFileExtensions,
                                                                   nsAString& aDescription) {
  LOG(("-- GetExtensionsAndDescriptionFromMimetypesFile\n"));
  LOG(("Getting extensions and description from types file '%s'\n",
       NS_LossyConvertUTF16toASCII(aFilename).get()));
  LOG(("Using type '%s/%s'\n",
       NS_LossyConvertUTF16toASCII(aMajorType).get(),
       NS_LossyConvertUTF16toASCII(aMinorType).get()));

  nsresult rv = NS_OK;
  nsCOMPtr<nsIFileInputStream> mimeFile;
  nsCOMPtr<nsILineInputStream> mimeTypes;
  bool netscapeFormat;
  nsAutoString buf;
  nsCAutoString cBuf;
  bool more = false;
  rv = CreateInputStream(aFilename, getter_AddRefs(mimeFile), getter_AddRefs(mimeTypes),
                         cBuf, &netscapeFormat, &more);

  if (NS_FAILED(rv)) {
    return rv;
  }
  
  nsAutoString extensions;
  nsString entry;
  entry.SetCapacity(100);
  nsAString::const_iterator majorTypeStart, majorTypeEnd,
                            minorTypeStart, minorTypeEnd,
                            descriptionStart, descriptionEnd;
  
  do {
    CopyASCIItoUTF16(cBuf, buf);
    // read through, building up an entry.  If we finish an entry, check for
    // a match and return out of the loop if we match

    // skip comments and empty lines
    if (!buf.IsEmpty() && buf.First() != '#') {
      entry.Append(buf);
      if (entry.Last() == '\\') {
        entry.Truncate(entry.Length() - 1);
        entry.Append(PRUnichar(' '));  // in case there is no trailing whitespace on this line
      } else {  // we have a full entry
        LOG(("Current entry: '%s'\n",
             NS_LossyConvertUTF16toASCII(entry).get()));
        if (netscapeFormat) {
          rv = ParseNetscapeMIMETypesEntry(entry,
                                           majorTypeStart, majorTypeEnd,
                                           minorTypeStart, minorTypeEnd,
                                           extensions,
                                           descriptionStart, descriptionEnd);
          
          if (NS_FAILED(rv)) {
            // We sometimes get things like RealPlayer appending
            // "normal" entries to "Netscape" .mime.types files.  Try
            // to handle that.  Bug 106381.
            LOG(("Bogus entry; trying 'normal' mode\n"));
            rv = ParseNormalMIMETypesEntry(entry,
                                           majorTypeStart, majorTypeEnd,
                                           minorTypeStart, minorTypeEnd,
                                           extensions,
                                           descriptionStart, descriptionEnd);
          }
        } else {
          rv = ParseNormalMIMETypesEntry(entry,
                                         majorTypeStart, majorTypeEnd,
                                         minorTypeStart,
                                         minorTypeEnd, extensions,
                                         descriptionStart, descriptionEnd);
          
          if (NS_FAILED(rv)) {
            // We sometimes get things like StarOffice prepending
            // "normal" entries to "Netscape" .mime.types files.  Try
            // to handle that.  Bug 136670.
            LOG(("Bogus entry; trying 'Netscape' mode\n"));
            rv = ParseNetscapeMIMETypesEntry(entry,
                                             majorTypeStart, majorTypeEnd,
                                             minorTypeStart, minorTypeEnd,
                                             extensions,
                                             descriptionStart, descriptionEnd);
          }
        }
        
        if (NS_SUCCEEDED(rv) &&
            Substring(majorTypeStart,
                      majorTypeEnd).Equals(aMajorType,
                                           nsCaseInsensitiveStringComparator())&&
            Substring(minorTypeStart,
                      minorTypeEnd).Equals(aMinorType,
                                           nsCaseInsensitiveStringComparator())) {
          // it's a match
          aFileExtensions.Assign(extensions);
          aDescription.Assign(Substring(descriptionStart, descriptionEnd));
          mimeFile->Close();
          return NS_OK;
        } else if (NS_FAILED(rv)) {
          LOG(("Failed to parse entry: %s\n", NS_LossyConvertUTF16toASCII(entry).get()));
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
nsresult
nsOSHelperAppService::ParseNetscapeMIMETypesEntry(const nsAString& aEntry,
                                                  nsAString::const_iterator& aMajorTypeStart,
                                                  nsAString::const_iterator& aMajorTypeEnd,
                                                  nsAString::const_iterator& aMinorTypeStart,
                                                  nsAString::const_iterator& aMinorTypeEnd,
                                                  nsAString& aExtensions,
                                                  nsAString::const_iterator& aDescriptionStart,
                                                  nsAString::const_iterator& aDescriptionEnd) {
  LOG(("-- ParseNetscapeMIMETypesEntry\n"));
  NS_ASSERTION(!aEntry.IsEmpty(), "Empty Netscape MIME types entry being parsed.");
  
  nsAString::const_iterator start_iter, end_iter, match_start, match_end;

  aEntry.BeginReading(start_iter);
  aEntry.EndReading(end_iter);
  
  // skip trailing whitespace
  do {
    --end_iter;
  } while (end_iter != start_iter &&
           nsCRT::IsAsciiSpace(*end_iter));
  // if we're pointing to a quote, don't advance -- we don't want to
  // include the quote....
  if (*end_iter != '"')
    ++end_iter;
  match_start = start_iter;
  match_end = end_iter;
  
  // Get the major and minor types
  // First the major type
  if (! FindInReadable(NS_LITERAL_STRING("type="), match_start, match_end)) {
    return NS_ERROR_FAILURE;
  }
  
  match_start = match_end;
  
  while (match_end != end_iter &&
         *match_end != '/') {
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
  
  while (match_end != end_iter &&
         !nsCRT::IsAsciiSpace(*match_end) &&
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
  if (FindInReadable(NS_LITERAL_STRING("exts="), match_start, match_end)) {
    nsAString::const_iterator extStart, extEnd;

    if (match_end == end_iter ||
        (*match_end == '"' && ++match_end == end_iter)) {
      return NS_ERROR_FAILURE;
    }
  
    extStart = match_end;
    match_start = extStart;
    match_end = end_iter;
    if (FindInReadable(NS_LITERAL_STRING("desc=\""), match_start, match_end)) {
      // exts= before desc=, so we have to find the actual end of the extensions
      extEnd = match_start;
      if (extEnd == extStart) {
        return NS_ERROR_FAILURE;
      }
    
      do {
        --extEnd;
      } while (extEnd != extStart &&
               nsCRT::IsAsciiSpace(*extEnd));
      
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
  if (FindInReadable(NS_LITERAL_STRING("desc=\""), match_start, match_end)) {
    aDescriptionStart = match_end;
    match_start = aDescriptionStart;
    match_end = end_iter;
    if (FindInReadable(NS_LITERAL_STRING("exts="), match_start, match_end)) {
      // exts= after desc=, so have to find actual end of description
      aDescriptionEnd = match_start;
      if (aDescriptionEnd == aDescriptionStart) {
        return NS_ERROR_FAILURE;
      }
      
      do {
        --aDescriptionEnd;
      } while (aDescriptionEnd != aDescriptionStart &&
               nsCRT::IsAsciiSpace(*aDescriptionEnd));
      
      if (aDescriptionStart != aDescriptionStart && *aDescriptionEnd == '"') {
        --aDescriptionEnd;
      }
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
nsresult
nsOSHelperAppService::ParseNormalMIMETypesEntry(const nsAString& aEntry,
                                                nsAString::const_iterator& aMajorTypeStart,
                                                nsAString::const_iterator& aMajorTypeEnd,
                                                nsAString::const_iterator& aMinorTypeStart,
                                                nsAString::const_iterator& aMinorTypeEnd,
                                                nsAString& aExtensions,
                                                nsAString::const_iterator& aDescriptionStart,
                                                nsAString::const_iterator& aDescriptionEnd) {
  LOG(("-- ParseNormalMIMETypesEntry\n"));
  NS_ASSERTION(!aEntry.IsEmpty(), "Empty Normal MIME types entry being parsed.");

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
           
  ++end_iter; // point to first whitespace char (or to end of string)
  iter = start_iter;

  // get the major type
  if (! FindCharInReadable('/', iter, end_iter))
    return NS_ERROR_FAILURE;

  nsAString::const_iterator equals_sign_iter(start_iter);
  if (FindCharInReadable('=', equals_sign_iter, iter))
    return NS_ERROR_FAILURE; // see bug 136670
  
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
    if (iter != end_iter) { // not the last extension
      aExtensions.Append(PRUnichar(','));
    }
  }

  return NS_OK;
}

// static
nsresult
nsOSHelperAppService::LookUpHandlerAndDescription(const nsAString& aMajorType,
                                                  const nsAString& aMinorType,
                                                  nsHashtable& aTypeOptions,
                                                  nsAString& aHandler,
                                                  nsAString& aDescription,
                                                  nsAString& aMozillaFlags) {
  LOG(("-- LookUpHandlerAndDescription for type '%s/%s'\n",
       NS_LossyConvertUTF16toASCII(aMajorType).get(),
       NS_LossyConvertUTF16toASCII(aMinorType).get()));
  nsresult rv = NS_OK;
  nsAutoString mailcapFileName;

  rv = GetFileLocation("helpers.private_mailcap_file",
                       "PERSONAL_MAILCAP", mailcapFileName);
  if (NS_SUCCEEDED(rv) && !mailcapFileName.IsEmpty()) {
    rv = GetHandlerAndDescriptionFromMailcapFile(mailcapFileName,
                                                 aMajorType,
                                                 aMinorType,
                                                 aTypeOptions,
                                                 aHandler,
                                                 aDescription,
                                                 aMozillaFlags);
  } else {
    rv = NS_ERROR_NOT_AVAILABLE;
  }
  if (NS_FAILED(rv) || aHandler.IsEmpty()) {
    rv = GetFileLocation("helpers.global_mailcap_file",
                         "MAILCAP", mailcapFileName);
    if (NS_SUCCEEDED(rv) && !mailcapFileName.IsEmpty()) {
      rv = GetHandlerAndDescriptionFromMailcapFile(mailcapFileName,
                                                   aMajorType,
                                                   aMinorType,
                                                   aTypeOptions,
                                                   aHandler,
                                                   aDescription,
                                                   aMozillaFlags);
    } else {
      rv = NS_ERROR_NOT_AVAILABLE;
    }
  }
  return rv;
}

// static
nsresult
nsOSHelperAppService::GetHandlerAndDescriptionFromMailcapFile(const nsAString& aFilename,
                                                              const nsAString& aMajorType,
                                                              const nsAString& aMinorType,
                                                              nsHashtable& aTypeOptions,
                                                              nsAString& aHandler,
                                                              nsAString& aDescription,
                                                              nsAString& aMozillaFlags) {

  LOG(("-- GetHandlerAndDescriptionFromMailcapFile\n"));
  LOG(("Getting handler and description from mailcap file '%s'\n",
       NS_LossyConvertUTF16toASCII(aFilename).get()));
  LOG(("Using type '%s/%s'\n",
       NS_LossyConvertUTF16toASCII(aMajorType).get(),
       NS_LossyConvertUTF16toASCII(aMinorType).get()));

  nsresult rv = NS_OK;
  bool more = false;
  
  nsCOMPtr<nsIFile> file(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  rv = file->InitWithPath(aFilename);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIFileInputStream> mailcapFile(do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  rv = mailcapFile->Init(file, -1, -1, false);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsILineInputStream> mailcap (do_QueryInterface(mailcapFile, &rv));

  if (NS_FAILED(rv)) {
    LOG(("Interface trouble in stream land!"));
    return rv;
  }

  nsString entry, buffer;
  nsCAutoString cBuffer;
  entry.SetCapacity(128);
  cBuffer.SetCapacity(80);
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
        entry.Truncate(entry.Length()-1);
        entry.Append(PRUnichar(' ')); // in case there is no trailing whitespace on this line
      } else {  // we have a full entry in entry.  Check it for the type
        LOG(("Current entry: '%s'\n",
             NS_LossyConvertUTF16toASCII(entry).get()));

        nsAString::const_iterator semicolon_iter,
                                  start_iter, end_iter,
                                  majorTypeStart, majorTypeEnd,
                                  minorTypeStart, minorTypeEnd;
        entry.BeginReading(start_iter);
        entry.EndReading(end_iter);
        semicolon_iter = start_iter;
        FindSemicolon(semicolon_iter, end_iter);
        if (semicolon_iter != end_iter) { // we have something resembling a valid entry
          rv = ParseMIMEType(start_iter, majorTypeStart, majorTypeEnd,
                             minorTypeStart, minorTypeEnd, semicolon_iter);
          if (NS_SUCCEEDED(rv) &&
              Substring(majorTypeStart,
                        majorTypeEnd).Equals(aMajorType,
                                             nsCaseInsensitiveStringComparator()) &&
              Substring(minorTypeStart,
                        minorTypeEnd).Equals(aMinorType,
                                             nsCaseInsensitiveStringComparator())) { // we have a match
            bool match = true;
            ++semicolon_iter;             // point at the first char past the semicolon
            start_iter = semicolon_iter;  // handler string starts here
            FindSemicolon(semicolon_iter, end_iter);
            while (start_iter != semicolon_iter &&
                   nsCRT::IsAsciiSpace(*start_iter)) {
              ++start_iter;
            }

            LOG(("The real handler is: '%s'\n",
                 NS_LossyConvertUTF16toASCII(Substring(start_iter,
                                                      semicolon_iter)).get()));
              
            // XXX ugly hack.  Just grab the executable name
            nsAString::const_iterator end_handler_iter = semicolon_iter;
            nsAString::const_iterator end_executable_iter = start_iter;
            while (end_executable_iter != end_handler_iter &&
                   !nsCRT::IsAsciiSpace(*end_executable_iter)) {
              ++end_executable_iter;
            }
            // XXX End ugly hack
            
            aHandler = Substring(start_iter, end_executable_iter);
            
            nsAString::const_iterator start_option_iter, end_optionname_iter, equal_sign_iter;
            bool equalSignFound;
            while (match &&
                   semicolon_iter != end_iter &&
                   ++semicolon_iter != end_iter) { // there are options left and we still match
              start_option_iter = semicolon_iter;
              // skip over leading whitespace
              while (start_option_iter != end_iter &&
                     nsCRT::IsAsciiSpace(*start_option_iter)) {
                ++start_option_iter;
              }
              if (start_option_iter == end_iter) { // nothing actually here
                break;
              }
              semicolon_iter = start_option_iter;
              FindSemicolon(semicolon_iter, end_iter);
              equal_sign_iter = start_option_iter;
              equalSignFound = false;
              while (equal_sign_iter != semicolon_iter && !equalSignFound) {
                switch(*equal_sign_iter) {
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
              nsDependentSubstring optionName(start_option_iter, end_optionname_iter);
              if (equalSignFound) {
                // This is an option that has a name and value
                if (optionName.EqualsLiteral("description")) {
                  aDescription = Substring(++equal_sign_iter, semicolon_iter);
                } else if (optionName.EqualsLiteral("x-mozilla-flags")) {
                  aMozillaFlags = Substring(++equal_sign_iter, semicolon_iter);
                } else if (optionName.EqualsLiteral("test")) {
                  nsCAutoString testCommand;
                  rv = UnescapeCommand(Substring(++equal_sign_iter, semicolon_iter),
                                       aMajorType,
                                       aMinorType,
                                       aTypeOptions,
                                       testCommand);
                  LOG(("Running Test: %s\n", testCommand.get()));
                  // XXX this should not use system(), since that can block the UI thread!
                  if (NS_SUCCEEDED(rv) && system(testCommand.get()) != 0) {
                    match = false;
                  }
                }
              } else {
                // This is an option that just has a name but no value (eg "copiousoutput")
              }
            }
            

            if (match) { // we did not fail any test clauses; all is good
              // get out of here
              mailcapFile->Close();
              return NS_OK;
            } else { // pretend that this match never happened
              aDescription.Truncate();
              aMozillaFlags.Truncate();
              aHandler.Truncate();
            }
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

nsresult nsOSHelperAppService::OSProtocolHandlerExists(const char * aProtocolScheme, bool * aHandlerExists)
{
  LOG(("-- nsOSHelperAppService::OSProtocolHandlerExists for '%s'\n",
       aProtocolScheme));
  *aHandlerExists = false;

  /* if applications.protocol is in prefs, then we have an external protocol handler */
  nsresult rv;
  nsCAutoString branchName =
    NS_LITERAL_CSTRING("applications.") + nsDependentCString(aProtocolScheme);
  nsCAutoString prefName = branchName + branchName;

  nsAdoptingCString prefString = Preferences::GetCString(prefName.get());
  *aHandlerExists = !prefString.IsEmpty();
  if (*aHandlerExists) {
    return NS_OK;
  }

  /* Check the OS/2 INI for the protocol */
  char szAppFromINI[CCHMAXPATH];
  char szParamsFromINI[MAXINIPARAMLENGTH];
  rv = GetApplicationAndParametersFromINI(nsDependentCString(aProtocolScheme),
                                          szAppFromINI, sizeof(szAppFromINI),
                                          szParamsFromINI, sizeof(szParamsFromINI));
  if (NS_SUCCEEDED(rv)) {
    *aHandlerExists = true;
  }

  return NS_OK;
}

already_AddRefed<nsMIMEInfoOS2>
nsOSHelperAppService::GetFromExtension(const nsCString& aFileExt) {
  // if the extension is empty, return immediately
  if (aFileExt.IsEmpty())
    return nullptr;
  
  LOG(("Here we do an extension lookup for '%s'\n", aFileExt.get()));

  nsresult rv;

  nsAutoString majorType, minorType,
               mime_types_description, mailcap_description,
               handler, mozillaFlags;
  
  rv = LookUpTypeAndDescription(NS_ConvertUTF8toUTF16(aFileExt),
                                majorType,
                                minorType,
                                mime_types_description);
  if (NS_FAILED(rv))
    return nullptr;

  NS_LossyConvertUTF16toASCII asciiMajorType(majorType);
  NS_LossyConvertUTF16toASCII asciiMinorType(minorType);

  LOG(("Type/Description results:  majorType='%s', minorType='%s', description='%s'\n",
          asciiMajorType.get(),
          asciiMinorType.get(),
          NS_LossyConvertUTF16toASCII(mime_types_description).get()));

  if (majorType.IsEmpty() && minorType.IsEmpty()) {
    // we didn't get a type mapping, so we can't do anything useful
    return nullptr;
  }

  nsCAutoString mimeType(asciiMajorType + NS_LITERAL_CSTRING("/") + asciiMinorType);
  nsMIMEInfoOS2* mimeInfo = new nsMIMEInfoOS2(mimeType);
  if (!mimeInfo)
    return nullptr;
  NS_ADDREF(mimeInfo);
  
  mimeInfo->AppendExtension(aFileExt);
  nsHashtable typeOptions; // empty hash table
  // The mailcap lookup is two-pass to handle the case of mailcap files
  // that have something like:
  //
  // text/*; emacs %s
  // text/rtf; soffice %s
  //
  // in that order.  We want to pick up "soffice" for text/rtf in such cases
  rv = LookUpHandlerAndDescription(majorType, minorType, typeOptions,
                                   handler, mailcap_description,
                                   mozillaFlags);
  if (NS_FAILED(rv)) {
    // maybe we have an entry for "majorType/*"?
    rv = LookUpHandlerAndDescription(majorType, NS_LITERAL_STRING("*"),
                                     typeOptions, handler, mailcap_description,
                                     mozillaFlags);
  }
  LOG(("Handler/Description results:  handler='%s', description='%s', mozillaFlags='%s'\n",
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
  if (NS_SUCCEEDED(rv) && !handler.IsEmpty()) {
    nsCOMPtr<nsIFile> handlerFile;
    rv = GetFileTokenForPath(handler.get(), getter_AddRefs(handlerFile));
    
    if (NS_SUCCEEDED(rv)) {
      mimeInfo->SetDefaultApplication(handlerFile);
      mimeInfo->SetPreferredAction(nsIMIMEInfo::useSystemDefault);
      mimeInfo->SetDefaultDescription(handler);
    }
  } else {
    mimeInfo->SetPreferredAction(nsIMIMEInfo::saveToDisk);
  }

  return mimeInfo;
}

already_AddRefed<nsMIMEInfoOS2>
nsOSHelperAppService::GetFromType(const nsCString& aMIMEType) {
  // if the extension is empty, return immediately
  if (aMIMEType.IsEmpty())
    return nullptr;
  
  LOG(("Here we do a mimetype lookup for '%s'\n", aMIMEType.get()));
  nsresult rv;
  nsAutoString extensions,
    mime_types_description, mailcap_description,
    handler, mozillaFlags;

  nsHashtable typeOptions;
  
  // extract the major and minor types
  NS_ConvertASCIItoUTF16 mimeType(aMIMEType);
  nsAString::const_iterator start_iter, end_iter,
                            majorTypeStart, majorTypeEnd,
                            minorTypeStart, minorTypeEnd;

  mimeType.BeginReading(start_iter);
  mimeType.EndReading(end_iter);

  // XXX FIXME: add typeOptions parsing in here
  rv = ParseMIMEType(start_iter, majorTypeStart, majorTypeEnd,
                     minorTypeStart, minorTypeEnd, end_iter);

  if (NS_FAILED(rv)) {
    return nullptr;
  }

  nsDependentSubstring majorType(majorTypeStart, majorTypeEnd);
  nsDependentSubstring minorType(minorTypeStart, minorTypeEnd);
  // The mailcap lookup is two-pass to handle the case of mailcap files
  // that have something like:
  //
  // text/*; emacs %s
  // text/rtf; soffice %s
  //
  // in that order.  We want to pick up "soffice" for text/rtf in such cases
  rv = LookUpHandlerAndDescription(majorType,
                                   minorType,
                                   typeOptions,
                                   handler,
                                   mailcap_description,
                                   mozillaFlags);
  if (NS_FAILED(rv)) {
    // maybe we have an entry for "majorType/*"?
    rv = LookUpHandlerAndDescription(majorType,
                                     NS_LITERAL_STRING("*"),
                                     typeOptions,
                                     handler,
                                     mailcap_description,
                                     mozillaFlags);
  }
  LOG(("Handler/Description results:  handler='%s', description='%s', mozillaFlags='%s'\n",
          NS_LossyConvertUTF16toASCII(handler).get(),
          NS_LossyConvertUTF16toASCII(mailcap_description).get(),
          NS_LossyConvertUTF16toASCII(mozillaFlags).get()));
  
  if (handler.IsEmpty()) {
    // we have no useful info....
    return nullptr;
  }
  
  mailcap_description.Trim(" \t\"");
  mozillaFlags.Trim(" \t");
  LookUpExtensionsAndDescription(majorType,
                                 minorType,
                                 extensions,
                                 mime_types_description);

  nsMIMEInfoOS2* mimeInfo = new nsMIMEInfoOS2(aMIMEType);
  if (!mimeInfo)
    return nullptr;
  NS_ADDREF(mimeInfo);

  mimeInfo->SetFileExtensions(NS_ConvertUTF16toUTF8(extensions));
  if (! mime_types_description.IsEmpty()) {
    mimeInfo->SetDescription(mime_types_description);
  } else {
    mimeInfo->SetDescription(mailcap_description);
  }

  nsCOMPtr<nsIFile> handlerFile;
  rv = GetFileTokenForPath(handler.get(), getter_AddRefs(handlerFile));
  
  if (NS_SUCCEEDED(rv)) {
    mimeInfo->SetDefaultApplication(handlerFile);
    mimeInfo->SetPreferredAction(nsIMIMEInfo::useSystemDefault);
    mimeInfo->SetDefaultDescription(handler);
  } else {
    mimeInfo->SetPreferredAction(nsIMIMEInfo::saveToDisk);
  }

  return mimeInfo;
}

//------------------------------------------------------------------------

// returns a localized string from unknownContentType.properties

static nsresult
GetNLSString(const PRUnichar *aKey, nsAString& result)
{
  nsCOMPtr<nsIStringBundleService> bundleSvc =
    mozilla::services::GetStringBundleService();
  if (!bundleSvc)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIStringBundle> bundle;
  nsresult rv = bundleSvc->CreateBundle(
    "chrome://mozapps/locale/downloads/unknownContentType.properties",
    getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLString string;
  rv = bundle->GetStringFromName(aKey, getter_Copies(string));
  NS_ENSURE_SUCCESS(rv, rv);

  result.Assign(string);

  return rv;
}

//------------------------------------------------------------------------

// returns the handle of the WPS object associated with a file extension;
// if RWS isn't being used or there's no association, returns zero;
// also constructs a description of the handler based on available info

static PRUint32
WpsGetDefaultHandler(const char *aFileExt, nsAString& aDescription)
{
  aDescription.Truncate();

  if (sUseRws) {
    nsCOMPtr<nsIRwsService> rwsSvc(do_GetService("@mozilla.org/rwsos2;1"));
    if (!rwsSvc)
      sUseRws = false;
    else {
      PRUint32 handle;
      // the handle may be zero if the WPS class provides the default handler
      if (NS_SUCCEEDED(rwsSvc->HandlerFromExtension(aFileExt, &handle, aDescription)))
        return handle;
    }
  }

  // no RWS or RWS failed to return at least a description
  if (NS_FAILED(GetNLSString(NS_LITERAL_STRING("wpsDefaultOS2").get(),
                             aDescription)))
    aDescription.Assign(NS_LITERAL_STRING("WPS default"));

  return 0;
}

//------------------------------------------------------------------------

// this constructs the system-default entry when neither mailcap nor
// mime.types provided any info

static void
WpsMimeInfoFromExtension(const char *aFileExt, nsMIMEInfoOS2 *aMI)
{
  // this identifies whether the mimetype is a bogus
  // "application/octet-stream" generated when no match
  // could be found for this extension
  bool exists;
  aMI->ExtensionExists(nsDependentCString(aFileExt), &exists);

  // get the default app's description and WPS handle (if any)
  nsAutoString ustr;
  PRUint32 handle = WpsGetDefaultHandler(aFileExt, ustr);
  aMI->SetDefaultDescription(ustr);
  aMI->SetDefaultAppHandle(handle);

  // if the mimeinfo is bogus, change the mimetype & extensions list
  if (!exists) {
    nsCAutoString extLower;
    nsCAutoString cstr;
    ToLowerCase(nsDependentCString(aFileExt), extLower);
    cstr.Assign(NS_LITERAL_CSTRING("application/x-") + extLower);
    aMI->SetMIMEType(cstr);
    aMI->SetFileExtensions(extLower);
  }

  // if the mimetype is valid, perhaps we can supply a description;
  // if it's bogus, replace the description
  if (exists)
    aMI->GetDescription(ustr);
  else
    ustr.Truncate();

  if (ustr.IsEmpty()) {
    nsCAutoString extUpper;
    ToUpperCase(nsDependentCString(aFileExt), extUpper);
    CopyUTF8toUTF16(extUpper, ustr);

    nsAutoString fileType;
    if (NS_FAILED(GetNLSString(NS_LITERAL_STRING("fileType").get(), fileType)))
      ustr.Assign(NS_LITERAL_STRING("%S file"));
    int pos = -1;
    if ((pos = fileType.Find("%S")) > -1)
      fileType.Replace(pos, 2, ustr);
    aMI->SetDescription(fileType);
  }
}

//------------------------------------------------------------------------

// this is an override of nsExternalHelperAppService's method;
// after the parent's method has looked for a handler, add
// an entry for the WPS's handler if there's room and one exists;
// it will never replace entries from mailcap or mimetypes.rdf

NS_IMETHODIMP
nsOSHelperAppService::GetFromTypeAndExtension(const nsACString& aMIMEType,
                                              const nsACString& aFileExt,
                                              nsIMIMEInfo **_retval)
{
  // let the existing code do its thing
  nsresult rv = nsExternalHelperAppService::GetFromTypeAndExtension(
                                            aMIMEType, aFileExt, _retval);
  NS_ENSURE_SUCCESS(rv, rv);

  // this is needed for Get/SetDefaultApplication()
  nsMIMEInfoOS2 *mi = static_cast<nsMIMEInfoOS2*>(*_retval);

  // do lookups using the original extension if present;
  // otherwise use the extension derived from the mimetype
  nsCAutoString ext;
  if (!aFileExt.IsEmpty())
    ext.Assign(aFileExt);
  else {
    mi->GetPrimaryExtension(ext);
    if (ext.IsEmpty())
      return NS_OK;
  }

  nsCOMPtr<nsIFile> defApp;
  nsCOMPtr<nsIHandlerApp> prefApp;
  mi->GetDefaultApplication(getter_AddRefs(defApp));
  mi->GetPreferredApplicationHandler(getter_AddRefs(prefApp));
  nsCOMPtr<nsILocalHandlerApp> locPrefApp = do_QueryInterface(prefApp, &rv);

  // if neither mailcap nor mimetypes.rdf had an entry,
  // create a default entry using the WPS handler
  if (!defApp && !locPrefApp) {
    WpsMimeInfoFromExtension(ext.get(), mi);
    return NS_OK;
  }

  bool gotPromoted = false;

  // both mailcap & mimetypes.rdf have an entry;  if they're
  // different, exit;  otherwise, clear the default entry
  if (defApp && locPrefApp) {
    bool sameFile;
    nsCOMPtr<nsIFile> app;
    rv = locPrefApp->GetExecutable(getter_AddRefs(app));
    NS_ENSURE_SUCCESS(rv, rv);
    defApp->Equals(app, &sameFile);
    if (!sameFile)
      return NS_OK;

    defApp = 0;
    mi->SetDefaultApplication(0);
    mi->SetDefaultDescription(EmptyString());
    gotPromoted = true;
  }

  nsAutoString description;

  // mailcap has an entry but mimetypes.rdf doesn't;
  // promote mailcap's entry to preferred
  if (defApp && !locPrefApp) {
    mi->GetDefaultDescription(description);
    nsLocalHandlerApp *handlerApp(new nsLocalHandlerApp(description, defApp));
    mi->SetPreferredApplicationHandler(handlerApp);
    gotPromoted = true;
  }

  // if the former default app was promoted to preferred app,
  // update preferred action if appropriate
  if (gotPromoted) {
    nsHandlerInfoAction action;
    mi->GetPreferredAction(&action);
    if (action == nsIMIMEInfo::useSystemDefault) {
      mi->SetPreferredAction(nsIMIMEInfo::useHelperApp);
      mi->SetPreferredAction(nsIMIMEInfo::useHelperApp);
    }
  }

  // use the WPS default as the system default handler
  PRUint32 handle = WpsGetDefaultHandler(ext.get(), description);
  mi->SetDefaultDescription(description);
  mi->SetDefaultApplication(0);
  mi->SetDefaultAppHandle(handle);

  return NS_OK;
}

//------------------------------------------------------------------------

already_AddRefed<nsIMIMEInfo>
nsOSHelperAppService::GetMIMEInfoFromOS(const nsACString& aType,
                                        const nsACString& aFileExt,
                                        bool       *aFound) {
  *aFound = true;
  nsMIMEInfoOS2* retval = GetFromType(PromiseFlatCString(aType)).get();
  bool hasDefault = false;
  if (retval)
    retval->GetHasDefaultHandler(&hasDefault);
  if (!retval || !hasDefault) {
    nsRefPtr<nsMIMEInfoOS2> miByExt = GetFromExtension(PromiseFlatCString(aFileExt));
    // If we had no extension match, but a type match, use that
    if (!miByExt && retval)
      return retval;
    // If we had an extension match but no type match, set the mimetype and use
    // it
    if (!retval && miByExt) {
      if (!aType.IsEmpty())
        miByExt->SetMIMEType(aType);
      miByExt.swap(retval);

      return retval;
    }
    // If we got nothing, make a new mimeinfo
    if (!retval) {
      *aFound = false;
      retval = new nsMIMEInfoOS2(aType);
      if (retval) {
        NS_ADDREF(retval);
        if (!aFileExt.IsEmpty())
          retval->AppendExtension(aFileExt);
      }
      
      return retval;
    }

    // Copy the attributes of retval onto miByExt, to return it
    retval->CopyBasicDataTo(miByExt);

    miByExt.swap(retval);
  }
  return retval;
}

NS_IMETHODIMP
nsOSHelperAppService::GetProtocolHandlerInfoFromOS(const nsACString &aScheme,
                                                   bool *found,
                                                   nsIHandlerInfo **_retval)
{
  NS_ASSERTION(!aScheme.IsEmpty(), "No scheme was specified!");

  nsresult rv = OSProtocolHandlerExists(nsPromiseFlatCString(aScheme).get(),
                                        found);
  if (NS_FAILED(rv))
    return rv;

  nsMIMEInfoOS2 *handlerInfo =
    new nsMIMEInfoOS2(aScheme, nsMIMEInfoBase::eProtocolInfo);
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


NS_IMETHODIMP
nsOSHelperAppService::GetApplicationDescription(const nsACString& aScheme, nsAString& _retval)
{
  nsresult rv;
  nsCAutoString branchName = NS_LITERAL_CSTRING("applications.") + aScheme;
  nsCAutoString applicationName;

  nsCAutoString prefName = branchName + branchName;
  nsAdoptingCString prefString = Preferences::GetCString(prefName.get());
  if (!prefString) { // failed
    char szAppFromINI[CCHMAXPATH];
    char szParamsFromINI[MAXINIPARAMLENGTH];
    /* did OS2.INI contain application? */
    rv = GetApplicationAndParametersFromINI(aScheme,
                                            szAppFromINI, sizeof(szAppFromINI),
                                            szParamsFromINI, sizeof(szParamsFromINI));
    if (NS_SUCCEEDED(rv)) {
      applicationName = szAppFromINI;
    } else {
      return NS_ERROR_NOT_AVAILABLE;
    }
  } else if (!prefString.IsEmpty()) { // succeeded and not empty
    applicationName.Append(prefString);
  }


  nsCOMPtr<nsIFile> application;
  rv = NS_NewNativeLocalFile(nsDependentCString(applicationName.get()),
                             false,
                             getter_AddRefs(application));
  if (NS_FAILED(rv)) {
    char szAppPath[CCHMAXPATH];
    APIRET rc = DosSearchPath(SEARCH_IGNORENETERRS | SEARCH_ENVIRONMENT,
                              "PATH",
                              applicationName.get(),
                              szAppPath,
                              sizeof(szAppPath));
    /* if that worked, we can use the full pathname */
    if (rc == NO_ERROR) {
      _retval.Assign(NS_ConvertUTF8toUTF16(nsDependentCString(szAppPath)));
      return NS_OK;
    }
  }
  /* if the full path is given or not, use what we have */
  _retval.Assign(NS_ConvertUTF8toUTF16(applicationName));
  return NS_OK;
}

// Check OS/2 INI for application and parameters for the protocol
// return NS_OK, if application exists for protocol in INI and is not empty
nsresult GetApplicationAndParametersFromINI(const nsACString& aProtocol,
                                            char* app, ULONG appLength,
                                            char* param, ULONG paramLength)
{
  /* initialize app to '\0' for later check */
  *app = '\0';

  /* http or https */
  if ((aProtocol == NS_LITERAL_CSTRING("http")) ||
      (aProtocol == NS_LITERAL_CSTRING("https"))) {
    PrfQueryProfileString(HINI_USER,
                          "WPURLDEFAULTSETTINGS",
                          "DefaultBrowserExe",
                          "",
                          app,
                          appLength);
    PrfQueryProfileString(HINI_USER,
                          "WPURLDEFAULTSETTINGS",
                          "DefaultParameters",
                          "",
                          param,
                          paramLength);
  }
  /* mailto: */
  else if (aProtocol == NS_LITERAL_CSTRING("mailto")) {
    PrfQueryProfileString(HINI_USER,
                          "WPURLDEFAULTSETTINGS",
                          "DefaultMailExe",
                          "",
                          app,
                          appLength);
    PrfQueryProfileString(HINI_USER,
                          "WPURLDEFAULTSETTINGS",
                          "DefaultMailParameters",
                          "",
                          param,
                          paramLength);
  }
  /* ftp */
  else if (aProtocol == NS_LITERAL_CSTRING("ftp")) {
    PrfQueryProfileString(HINI_USER,
                          "WPURLDEFAULTSETTINGS",
                          "DefaultFTPExe",
                          "",
                          app,
                          appLength);
    PrfQueryProfileString(HINI_USER,
                          "WPURLDEFAULTSETTINGS",
                          "DefaultFTPParameters",
                          "",
                          param,
                          paramLength);
  }
  /* news: or snews: */
  else if ((aProtocol == NS_LITERAL_CSTRING("news")) ||
           (aProtocol == NS_LITERAL_CSTRING("snews"))) {
    PrfQueryProfileString(HINI_USER,
                          "WPURLDEFAULTSETTINGS",
                          "DefaultNewsExe",
                          "",
                          app,
                          appLength);
    PrfQueryProfileString(HINI_USER,
                          "WPURLDEFAULTSETTINGS",
                          "DefaultNewsParameters",
                          "",
                          param,
                          paramLength);
  }
  /* irc: */
  else if (aProtocol == NS_LITERAL_CSTRING("irc")) {
    PrfQueryProfileString(HINI_USER,
                          "WPURLDEFAULTSETTINGS",
                          "DefaultIRCExe",
                          "",
                          app,
                          appLength);
    PrfQueryProfileString(HINI_USER,
                          "WPURLDEFAULTSETTINGS",
                          "DefaultIRCParameters",
                          "",
                          param,
                          paramLength);
  }
  else {
#ifdef DEBUG
    // output by hand instead of NS_WARNING() to easily add the protocol
    fprintf(stderr, "GetApplicationAndParametersFromINI(): unsupported protocol"
            " scheme \"%s\"\n", nsPromiseFlatCString(aProtocol).get());
#endif
    return NS_ERROR_FAILURE;
  }

  /* application string in INI was empty */
  if (app[0] == '\0')
    return NS_ERROR_FAILURE;

  return NS_OK;
}
