/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>  (Added mailcap and mime.types support)
 */

#include "nsOSHelperAppService.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsXPIDLString.h"
#include "nsIURL.h"
#include "nsNetCID.h"
#include "nsIFileStreams.h"
#include "nsILineInputStream.h"
#include "nsILocalFile.h"
#include "nsEscape.h"
#include "nsIProcess.h"
#include "nsIPref.h"
#include "nsHashtable.h"
#include "nsCRT.h"
#include "prenv.h"      // for PR_GetEnv()
#include <stdlib.h>		// for system()

#include <os2.h>

static NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);

nsresult
UnescapeCommand(const nsAString& aEscapedCommand,
                const nsAString& aMajorType,
                const nsAString& aMinorType,
                nsHashtable& aTypeOptions,
                nsACString& aUnEscapedCommand);
nsresult
FindSemicolon(nsAString::const_iterator& aSemicolon_iter,
              const nsAString::const_iterator& aEnd_iter);
nsresult
ParseMIMEType(const nsAString::const_iterator& aStart_iter,
              nsAString::const_iterator& aMajorTypeStart,
              nsAString::const_iterator& aMajorTypeEnd,
              nsAString::const_iterator& aMinorTypeStart,
              nsAString::const_iterator& aMinorTypeEnd,
              const nsAString::const_iterator& aEnd_iter);
nsresult
LookUpTypeAndDescription(const nsAString& aFileExtension,
                         nsAString& aMajorType,
                         nsAString& aMinorType,
                         nsAString& aDescription);

inline PRBool
IsNetscapeFormat(const nsAString& aBuffer);

nsresult
CreateInputStream(const nsAString& aFilename,
                  nsIFileInputStream** aFileInputStream,
                  nsILineInputStream** aLineInputStream,
                  nsAString& aBuffer,
                  PRBool* aNetscapeFormat,
                  PRBool* aMore);

nsresult
GetTypeAndDescriptionFromMimetypesFile(const nsAString& aFilename,
                                       const nsAString& aFileExtension,
                                       nsAString& aMajorType,
                                       nsAString& aMinorType,
                                       nsAString& aDescription);

nsresult
LookUpExtensionsAndDescription(const nsAString& aMajorType,
                               const nsAString& aMinorType,
                               nsAString& aFileExtensions,
                               nsAString& aDescription);
nsresult
GetExtensionsAndDescriptionFromMimetypesFile(const nsAString& aFilename,
                                             const nsAString& aMajorType,
                                             const nsAString& aMinorType,
                                             nsAString& aFileExtensions,
                                             nsAString& aDescription);

nsresult
ParseNetscapeMIMETypesEntry(const nsAString& aEntry,
                            nsAString::const_iterator& aMajorTypeStart,
                            nsAString::const_iterator& aMajorTypeEnd,
                            nsAString::const_iterator& aMinorTypeStart,
                            nsAString::const_iterator& aMinorTypeEnd,
                            nsAString& aExtensions,
                            nsAString::const_iterator& aDescriptionStart,
                            nsAString::const_iterator& aDescriptionEnd);
                                      

nsresult
ParseNormalMIMETypesEntry(const nsAString& aEntry,
                          nsAString::const_iterator& aMajorTypeStart,
                          nsAString::const_iterator& aMajorTypeEnd,
                          nsAString::const_iterator& aMinorTypeStart,
                          nsAString::const_iterator& aMinorTypeEnd,
                          nsAString& aExtensions,
                          nsAString::const_iterator& aDescriptionStart,
                          nsAString::const_iterator& aDescriptionEnd);

nsresult
LookUpHandlerAndDescription(const nsAString& aMajorType,
                            const nsAString& aMinorType,
                            nsHashtable& aTypeOptions,
                            nsAString& aHandler,
                            nsAString& aDescription,
                            nsAString& aMozillaFlags);
nsresult
GetHandlerAndDescriptionFromMailcapFile(const nsAString& aFilename,
                                        const nsAString& aMajorType,
                                        const nsAString& aMinorType,
                                        nsHashtable& aTypeOptions,
                                        nsAString& aHandler,
                                        nsAString& aDescription,
                                        nsAString& aMozillaFlags);

nsOSHelperAppService::nsOSHelperAppService() : nsExternalHelperAppService()
{
}

nsOSHelperAppService::~nsOSHelperAppService()
{}

NS_IMETHODIMP nsOSHelperAppService::LaunchAppWithTempFile(nsIMIMEInfo * aMIMEInfo, nsIFile * aTempFile)
{
  nsresult rv = NS_OK;
  if (aMIMEInfo)
  {
    nsCOMPtr<nsIFile> application;
    nsCAutoString path;
    aTempFile->GetNativePath(path);
    
    aMIMEInfo->GetPreferredApplicationHandler(getter_AddRefs(application));
    if (application)
    {
      // if we were given an application to use then use it....otherwise
      // make the registry call to launch the app
      /* Put quotes around path to handle spaces */

      path.Insert('\"', 0);
      path.Append('\"');
      const char * strPath = path.get();

      nsCOMPtr<nsIProcess> process = do_CreateInstance(NS_PROCESS_CONTRACTID);
      if (NS_FAILED(rv = process->Init(application)))
        return rv;
      PRUint32 pid;
      if (NS_FAILED(rv = process->Run(PR_FALSE, &strPath, 1, &pid)))
        return rv;
    }    
    else
    {
      HOBJECT hobject = WinQueryObject(path.get());
      if (WinSetObjectData( hobject, "OPEN=DEFAULT" ))
        rv = NS_OK;
      else
        rv = NS_ERROR_FAILURE;
    }
  }

  return rv;
}

/*
 * Take a command with all the mailcap escapes in it and unescape it
 * Ideally this needs the mime type, mime type options, and location of the
 * temporary file, but this last can't be got from here
 */
nsresult
UnescapeCommand(const nsAString& aEscapedCommand,
                const nsAString& aMajorType,
                const nsAString& aMinorType,
                nsHashtable& aTypeOptions,
                nsACString& aUnEscapedCommand) {
  //  XXX This function will need to get the mime type and various stuff like that being passed in to work properly
  
#ifdef DEBUG_bzbarsky
  fprintf(stderr, "UnescapeCommand really needs some work -- it should actually do some unescaping\n");
#endif
  aUnEscapedCommand = NS_ConvertUCS2toUTF8(aEscapedCommand);
  return NS_OK;
}

/* Put aSemicolon_iter at the first non-escaped semicolon after
 * aStart_iter but before aEnd_iter
 */

nsresult
FindSemicolon(nsAString::const_iterator& aSemicolon_iter,
              const nsAString::const_iterator& aEnd_iter) {
  PRBool semicolonFound = PR_FALSE;
  while (aSemicolon_iter != aEnd_iter && !semicolonFound) {
    switch(*aSemicolon_iter) {
    case '\\':
      aSemicolon_iter.advance(2);
      break;
    case ';':
      semicolonFound = PR_TRUE;
      break;
    default:
      ++aSemicolon_iter;
      break;
    }
  }
  return NS_OK;
}

nsresult
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

/* Get the mime.types file names from prefs and look up info in them
   based on extension */
nsresult
LookUpTypeAndDescription(const nsAString& aFileExtension,
                         nsAString& aMajorType,
                         nsAString& aMinorType,
                         nsAString& aDescription) {
  nsresult rv = NS_OK;
  nsXPIDLString mimeFileName;

  nsCOMPtr<nsIPref> thePrefsService(do_GetService(NS_PREF_CONTRACTID));
  if (thePrefsService) {
    rv =
      thePrefsService->CopyUnicharPref("helpers.private_mime_types_file",
                                       getter_Copies(mimeFileName));
    if (NS_SUCCEEDED(rv) && !mimeFileName.IsEmpty()) {
      rv = GetTypeAndDescriptionFromMimetypesFile(mimeFileName,
                                                  aFileExtension,
                                                  aMajorType,
                                                  aMinorType,
                                                  aDescription);
    }
    if (aMajorType.IsEmpty()) {
      rv =
        thePrefsService->CopyUnicharPref("helpers.global_mime_types_file",
                                         getter_Copies(mimeFileName));
      if (NS_SUCCEEDED(rv) && !mimeFileName.IsEmpty()) {
        rv = GetTypeAndDescriptionFromMimetypesFile(mimeFileName,
                                                    aFileExtension,
                                                    aMajorType,
                                                    aMinorType,
                                                    aDescription);
      }
    }
  } else {
    rv = NS_ERROR_NOT_AVAILABLE;
  }
  return rv;
}

inline PRBool
IsNetscapeFormat(const nsAString& aBuffer) {
  NS_NAMED_LITERAL_STRING(netscapeHeader,
                          "#--Netscape Communications Corporation MIME Information");
  NS_NAMED_LITERAL_STRING(MCOMHeader, "#--MCOM MIME Information");

  return Substring(aBuffer, 0, netscapeHeader.Length()).Equals(netscapeHeader) ||
         Substring(aBuffer, 0, MCOMHeader.Length()).Equals(MCOMHeader);  
}

/*
 * Create a file stream and line input stream for the filename.
 * Leaves the first line of the file in aBuffer and sets the format to
 *  PR_TRUE for netscape files and false for normail ones
 */
nsresult
CreateInputStream(const nsAString& aFilename,
                  nsIFileInputStream ** aFileInputStream,
                  nsILineInputStream ** aLineInputStream,
                  nsAString& aBuffer,
                  PRBool * aNetscapeFormat,
                  PRBool * aMore) {
  nsresult rv = NS_OK;

  nsCOMPtr<nsILocalFile> file(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  rv = file->InitWithPath(aFilename);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIFileInputStream> fileStream(do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  rv = fileStream->Init(file, -1, -1, PR_FALSE);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsILineInputStream> lineStream(do_QueryInterface(fileStream, &rv));

#ifdef DEBUG_bzbarsky
  if (NS_FAILED(rv)) {
    fprintf(stderr, "Interface trouble!\n\n");
  }
#endif // DEBUG_bzbarsky

  if (NS_FAILED(rv)) {
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
nsresult
GetTypeAndDescriptionFromMimetypesFile(const nsAString& aFilename,
                                       const nsAString& aFileExtension,
                                       nsAString& aMajorType,
                                       nsAString& aMinorType,
                                       nsAString& aDescription) {
  nsresult rv = NS_OK;
  nsCOMPtr<nsIFileInputStream> mimeFile;
  nsCOMPtr<nsILineInputStream> mimeTypes;
  PRBool netscapeFormat;
  nsAutoString buf;
  PRBool more = PR_FALSE;
  rv = CreateInputStream(aFilename, getter_AddRefs(mimeFile), getter_AddRefs(mimeTypes),
                         buf, &netscapeFormat, &more);

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
    // read through, building up an entry.  If we finish an entry, check for
    // a match and return out of the loop if we match

    // skip comments and empty lines
    if (!buf.IsEmpty() && buf.First() != '#') {
      entry.Append(buf);
      if (entry.Last() == '\\') {
        entry.Truncate(entry.Length() - 1);
        entry.Append(PRUnichar(' '));  // in case there is no trailing whitespace on this line
      } else {  // we have a full entry
        if (netscapeFormat) {
          rv = ParseNetscapeMIMETypesEntry(entry,
                                           majorTypeStart, majorTypeEnd,
                                           minorTypeStart, minorTypeEnd,
                                           extensions,
                                           descriptionStart, descriptionEnd);
          if (NS_FAILED(rv)) {
            // We sometimes get things like RealPlayer sticking
            // "normal" entries in "Netscape" .mime.types files.  Try
            // to handle that.  Bug 106381
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
          
        }
#ifdef DEBUG_bzbarsky
        if (NS_FAILED(rv)) {
          fprintf(stderr, "Failed to parse entry: %s\n", NS_ConvertUCS2toUTF8(entry).get());
        }
#endif
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
        }
        // truncate the entry for the next iteration
        entry.Truncate();
      }
    }
    if (!more) {
      break;
    }
    // read the next line
    rv = mimeTypes->ReadLine(buf, &more);
  } while (NS_SUCCEEDED(rv));

  mimeFile->Close();
  return rv;
}

/* Get the mime.types file names from prefs and look up info in them
   based on mimetype  */
nsresult
LookUpExtensionsAndDescription(const nsAString& aMajorType,
                               const nsAString& aMinorType,
                               nsAString& aFileExtensions,
                               nsAString& aDescription) {
  nsresult rv = NS_OK;
  nsXPIDLString mimeFileName;

  nsCOMPtr<nsIPref> thePrefsService(do_GetService(NS_PREF_CONTRACTID));
  if (thePrefsService) {
    rv =
      thePrefsService->CopyUnicharPref("helpers.private_mime_types_file",
                                       getter_Copies(mimeFileName));
    if (NS_SUCCEEDED(rv) && !mimeFileName.IsEmpty()) {
      rv = GetExtensionsAndDescriptionFromMimetypesFile(mimeFileName,
                                                        aMajorType,
                                                        aMinorType,
                                                        aFileExtensions,
                                                        aDescription);
    }
    if (aFileExtensions.IsEmpty()) {
      rv =
        thePrefsService->CopyUnicharPref("helpers.global_mime_types_file",
                                         getter_Copies(mimeFileName));
      if (NS_SUCCEEDED(rv) && !mimeFileName.IsEmpty()) {
        rv = GetExtensionsAndDescriptionFromMimetypesFile(mimeFileName,
                                                          aMajorType,
                                                          aMinorType,
                                                          aFileExtensions,
                                                          aDescription);
      }
    }
  } else {
    rv = NS_ERROR_NOT_AVAILABLE;
  }
  return rv;
}

/* Open the file, read the first line, decide what type of file it is,
   then get info based on extension */
nsresult
GetExtensionsAndDescriptionFromMimetypesFile(const nsAString& aFilename,
                                             const nsAString& aMajorType,
                                             const nsAString& aMinorType,
                                             nsAString& aFileExtensions,
                                             nsAString& aDescription) {
  nsresult rv = NS_OK;
  nsCOMPtr<nsIFileInputStream> mimeFile;
  nsCOMPtr<nsILineInputStream> mimeTypes;
  PRBool netscapeFormat;
  nsAutoString buf;
  PRBool more = PR_FALSE;
  rv = CreateInputStream(aFilename, getter_AddRefs(mimeFile), getter_AddRefs(mimeTypes),
                         buf, &netscapeFormat, &more);

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
    // read through, building up an entry.  If we finish an entry, check for
    // a match and return out of the loop if we match

    // skip comments and empty lines
    if (!buf.IsEmpty() && buf.First() != '#') {
      entry.Append(buf);
      if (entry.Last() == '\\') {
        entry.Truncate(entry.Length() - 1);
        entry.Append(PRUnichar(' '));  // in case there is no trailing whitespace on this line
      } else {  // we have a full entry
        if (netscapeFormat) {
          rv = ParseNetscapeMIMETypesEntry(entry,
                                           majorTypeStart, majorTypeEnd,
                                           minorTypeStart, minorTypeEnd,
                                           extensions,
                                           descriptionStart, descriptionEnd);
          
        } else {
          rv = ParseNormalMIMETypesEntry(entry,
                                         majorTypeStart, majorTypeEnd,
                                         minorTypeStart,
                                         minorTypeEnd, extensions,
                                         descriptionStart, descriptionEnd);
          
        }
        
#ifdef DEBUG_bzbarsky
        if (NS_FAILED(rv)) {
          fprintf(stderr, "Failed to parse entry: %s\n", NS_ConvertUCS2toUTF8(entry).get());
        }
#endif
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
        }
        
        entry.Truncate();
      }
    }
    if (!more) {
      break;
    }
    // read the next line
    rv = mimeTypes->ReadLine(buf, &more);
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
nsresult
ParseNetscapeMIMETypesEntry(const nsAString& aEntry,
                            nsAString::const_iterator& aMajorTypeStart,
                            nsAString::const_iterator& aMajorTypeEnd,
                            nsAString::const_iterator& aMinorTypeStart,
                            nsAString::const_iterator& aMinorTypeEnd,
                            nsAString& aExtensions,
                            nsAString::const_iterator& aDescriptionStart,
                            nsAString::const_iterator& aDescriptionEnd) {
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
nsresult
ParseNormalMIMETypesEntry(const nsAString& aEntry,
                          nsAString::const_iterator& aMajorTypeStart,
                          nsAString::const_iterator& aMajorTypeEnd,
                          nsAString::const_iterator& aMinorTypeStart,
                          nsAString::const_iterator& aMinorTypeEnd,
                          nsAString& aExtensions,
                          nsAString::const_iterator& aDescriptionStart,
                          nsAString::const_iterator& aDescriptionEnd) {

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
  while (iter != end_iter && *iter != '/') {
    ++iter;
  }
  if (iter == end_iter) {
    return NS_ERROR_FAILURE;
  }
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
      aExtensions.Append(NS_LITERAL_STRING(","));
    }
  }

  return NS_OK;
}

nsresult
LookUpHandlerAndDescription(const nsAString& aMajorType,
                            const nsAString& aMinorType,
                            nsHashtable& aTypeOptions,
                            nsAString& aHandler,
                            nsAString& aDescription,
                            nsAString& aMozillaFlags) {
  nsresult rv = NS_OK;
  nsXPIDLString mailcapFileName;

  nsCOMPtr<nsIPref> thePrefsService(do_GetService(NS_PREF_CONTRACTID));
  if (thePrefsService) {
    rv =
      thePrefsService->CopyUnicharPref("helpers.private_mailcap_file",
                                       getter_Copies(mailcapFileName));
    if (NS_SUCCEEDED(rv) && !mailcapFileName.IsEmpty()) {
      rv = GetHandlerAndDescriptionFromMailcapFile(mailcapFileName,
                                                   aMajorType,
                                                   aMinorType,
                                                   aTypeOptions,
                                                   aHandler,
                                                   aDescription,
                                                   aMozillaFlags);
    }
    if (aHandler.IsEmpty()) {
      rv =
        thePrefsService->CopyUnicharPref("helpers.global_mailcap_file",
                                         getter_Copies(mailcapFileName));
      if (NS_SUCCEEDED(rv) && !mailcapFileName.IsEmpty()) {
        rv = GetHandlerAndDescriptionFromMailcapFile(mailcapFileName,
                                                     aMajorType,
                                                     aMinorType,
                                                     aTypeOptions,
                                                     aHandler,
                                                     aDescription,
                                                     aMozillaFlags);
      }
    }
  } else {
    rv = NS_ERROR_NOT_AVAILABLE;
  }
  return rv;
}

nsresult
GetHandlerAndDescriptionFromMailcapFile(const nsAString& aFilename,
                                        const nsAString& aMajorType,
                                        const nsAString& aMinorType,
                                        nsHashtable& aTypeOptions,
                                        nsAString& aHandler,
                                        nsAString& aDescription,
                                        nsAString& aMozillaFlags) {

  nsresult rv = NS_OK;
  PRBool more = PR_FALSE;
  
  nsCOMPtr<nsILocalFile> file(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  rv = file->InitWithPath(aFilename);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIFileInputStream> mailcapFile(do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  rv = mailcapFile->Init(file, -1, -1, PR_FALSE);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsILineInputStream> mailcap (do_QueryInterface(mailcapFile, &rv));

#ifdef DEBUG_bzbarsky
  if (NS_FAILED(rv)) {
    fprintf(stderr, "Interface trouble!\n\n");
  }
#endif // DEBUG_bzbarsky

  if (NS_FAILED(rv)) {
    return rv;
  }

  nsString entry, buffer;
  entry.SetCapacity(128);
  buffer.SetCapacity(80);
  rv = mailcap->ReadLine(buffer, &more);
  if (NS_FAILED(rv)) {
    mailcapFile->Close();
    return rv;
  }

  do {  // return on end-of-file in the loop

    if (!buffer.IsEmpty() && buffer.First() != '#') {
      entry.Append(buffer);
      if (entry.Last() == '\\') {  // entry continues on next line
        entry.Truncate(entry.Length()-1);
        entry.Append(PRUnichar(' ')); // in case there is no trailing whitespace on this line
      } else {  // we have a full entry in entry.  Check it for the type
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
              (Substring(minorTypeStart,
                         minorTypeEnd).Equals(NS_LITERAL_STRING("*")) ||
               Substring(minorTypeStart,
                         minorTypeEnd).Equals(aMinorType,
                                              nsCaseInsensitiveStringComparator()))) { // we have a match
            PRBool match = PR_TRUE;
            ++semicolon_iter;             // point at the first char past the semicolon
            start_iter = semicolon_iter;  // handler string starts here
            FindSemicolon(semicolon_iter, end_iter);
            while (start_iter != semicolon_iter &&
                   nsCRT::IsAsciiSpace(*start_iter)) {
              ++start_iter;
            }

#ifdef DEBUG_bzbarsky
            fprintf(stderr, "The real handler is: %s\n", NS_ConvertUCS2toUTF8(Substring(start_iter, semicolon_iter)).get());
#endif // DEBUG_bzbarsky
              
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
            PRBool equalSignFound;
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
              equalSignFound = PR_FALSE;
              while (equal_sign_iter != semicolon_iter && !equalSignFound) {
                switch(*equal_sign_iter) {
                case '\\':
                  equal_sign_iter.advance(2);
                  break;
                case '=':
                  equalSignFound = PR_TRUE;
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
                if (optionName.Equals(NS_LITERAL_STRING("description"))) {
                  aDescription = Substring(++equal_sign_iter, semicolon_iter);
                } else if (optionName.Equals(NS_LITERAL_STRING("x-mozilla-flags"))) {
                  aMozillaFlags = Substring(++equal_sign_iter, semicolon_iter);
                } else if (optionName.Equals(NS_LITERAL_STRING("test"))) {
                  nsCAutoString testCommand;
                  rv = UnescapeCommand(Substring(++equal_sign_iter, semicolon_iter),
                                       aMajorType,
                                       aMinorType,
                                       aTypeOptions,
                                       testCommand);
#ifdef DEBUG_bzbarsky
                  fprintf(stderr, "Running Test: %s\n", testCommand.get());
#endif
                  if (NS_SUCCEEDED(rv) && system(testCommand.get()) != 0) {
                    match = PR_FALSE;
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
      break;
    }
    rv = mailcap->ReadLine(buffer, &more);
  } while (NS_SUCCEEDED(rv));
  mailcapFile->Close();
  return rv;
}

NS_IMETHODIMP nsOSHelperAppService::ExternalProtocolHandlerExists(const char * aProtocolScheme, PRBool * aHandlerExists)
{
  /* if applications.protocol is in prefs, then we have an external protocol handler */
  nsresult rv;
  nsCAutoString prefName;
  prefName = NS_LITERAL_CSTRING("applications.") + nsDependentCString(aProtocolScheme);

  nsCOMPtr<nsIPref> thePrefsService(do_GetService(NS_PREF_CONTRACTID));
  if (thePrefsService) {
    nsXPIDLCString prefString;
    rv = thePrefsService->CopyCharPref(prefName.get(), getter_Copies(prefString));
    *aHandlerExists = NS_SUCCEEDED(rv) && !prefString.IsEmpty();
    return NS_OK;
  }
  *aHandlerExists = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsOSHelperAppService::LoadUrl(nsIURI * aURL)
{
  nsCOMPtr<nsIPref> thePrefsService(do_GetService(NS_PREF_CONTRACTID));
  if (!thePrefsService) {
    return NS_ERROR_FAILURE;
  }

  /* Convert SimpleURI to StandardURL */
  nsresult rv;
  nsCOMPtr<nsIURI> uri = do_CreateInstance(kStandardURLCID, &rv);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }
  nsCAutoString urlSpec;
  aURL->GetSpec(urlSpec);
  uri->SetSpec(urlSpec);

  /* Get the protocol so we can look up the preferences */
  nsCAutoString uProtocol;
  uri->GetScheme(uProtocol);

  nsCAutoString prefName;
  prefName = NS_LITERAL_CSTRING("applications.") + uProtocol;
  nsXPIDLCString prefString;

  rv = thePrefsService->CopyCharPref(prefName.get(), getter_Copies(prefString));
  if (NS_FAILED(rv) || prefString.IsEmpty()) {
    return NS_ERROR_FAILURE;
  }

  nsCAutoString parameters;
  nsCAutoString applicationName;

  /* Put application name in parameters */
  applicationName.Append(prefString);

  nsCAutoString uPort;
  PRInt32 iPort;
  uri->GetPort(&iPort);
  /* GetPort returns -1 if there is no port in the URI */
  if (iPort != -1)
    uPort.AppendInt(iPort);

  nsCAutoString uUsername;
  uri->GetUsername(uUsername);
  NS_UnescapeURL(uUsername);

  nsCAutoString uPassword;
  uri->GetPassword(uPassword);
  NS_UnescapeURL(uPassword);

  nsCAutoString uHost;
  uri->GetAsciiHost(uHost);

  prefName.Append(".");
  nsCOMPtr<nsIPrefBranch> prefBranch;
  rv = thePrefsService->GetBranch(prefName.get(), getter_AddRefs(prefBranch));
  if (NS_SUCCEEDED(rv) && prefBranch) {
    rv = prefBranch->GetCharPref("parameters", getter_Copies(prefString));
    /* If parameters have been specified, use them instead of the separate entities */
    if (NS_SUCCEEDED(rv) && !prefString.IsEmpty()) {
      parameters.Append(" ");
      parameters.Append(prefString);

      NS_NAMED_LITERAL_CSTRING(url, "%url%");

      PRInt32 pos = parameters.Find(url.get());
      if (pos != kNotFound) {
        nsCAutoString uURL;
        aURL->GetSpec(uURL);
        NS_UnescapeURL(uURL);
        uURL.Cut(0, uProtocol.Length()+1);
        parameters.Replace(pos, url.Length(), uURL);
      }
    } else {
      /* port */
      if (!uPort.IsEmpty()) {
        rv = prefBranch->GetCharPref("port", getter_Copies(prefString));
        if (NS_SUCCEEDED(rv) && !prefString.IsEmpty()) {
          parameters.Append(" ");
          parameters.Append(prefString);
        }
      }
      /* username */
      if (!uUsername.IsEmpty()) {
        rv = prefBranch->GetCharPref("username", getter_Copies(prefString));
        if (NS_SUCCEEDED(rv) && !prefString.IsEmpty()) {
          parameters.Append(" ");
          parameters.Append(prefString);
        }
      }
      /* password */
      if (!uPassword.IsEmpty()) {
        rv = prefBranch->GetCharPref("password", getter_Copies(prefString));
        if (NS_SUCCEEDED(rv) && !prefString.IsEmpty()) {
          parameters.Append(" ");
          parameters.Append(prefString);
        }
      }
      /* host */
      if (!uHost.IsEmpty()) {
        rv = prefBranch->GetCharPref("host", getter_Copies(prefString));
        if (NS_SUCCEEDED(rv) && !prefString.IsEmpty()) {
          parameters.Append(" ");
          parameters.Append(prefString);
        }
      }
    }
  }

  PRInt32 pos;

  NS_NAMED_LITERAL_CSTRING(port, "%port%");
  NS_NAMED_LITERAL_CSTRING(username, "%username%");
  NS_NAMED_LITERAL_CSTRING(password, "%password%");
  NS_NAMED_LITERAL_CSTRING(host, "%host%");

  pos = parameters.Find(port.get());
  if (pos != kNotFound) {
    parameters.Replace(pos, port.Length(), uPort);
  }
  pos = parameters.Find(username);
  if (pos != kNotFound) {
    parameters.Replace(pos, username.Length(), uUsername);
  }
  pos = parameters.Find(password.get());
  if (pos != kNotFound) {
    parameters.Replace(pos, password.Length(), uPassword);
  }
  pos = parameters.Find(host.get());
  if (pos != kNotFound) {
    parameters.Replace(pos, host.Length(), uHost);
  }

  const char *params[3];
  params[0] = parameters.get();
  PRInt32 numParams = 1;

  nsCOMPtr<nsILocalFile> application;
  rv = NS_NewNativeLocalFile(nsDependentCString(applicationName.get()), PR_FALSE, getter_AddRefs(application));
  if (NS_FAILED(rv)) {
     /* Maybe they didn't qualify the name - use COMSPEC */
     rv = NS_NewNativeLocalFile(nsDependentCString(getenv("COMSPEC")), PR_FALSE, getter_AddRefs(application));
     if (NS_FAILED(rv)) {
       return rv;
     }

     params[0] = "/c";
     params[1] = applicationName.get();
     params[2] = parameters.get();
     numParams = 3;
  }

  nsCOMPtr<nsIProcess> process = do_CreateInstance(NS_PROCESS_CONTRACTID);

  if (NS_FAILED(rv = process->Init(application)))
     return rv;

  PRUint32 pid;
  if (NS_FAILED(rv = process->Run(PR_FALSE, params, numParams, &pid)))
    return rv;

  return NS_OK;
}

nsresult nsOSHelperAppService::GetFileTokenForPath(const PRUnichar * platformAppPath, nsIFile ** aFile)
{
  nsCOMPtr<nsILocalFile> localFile (do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));

  if (!localFile)
    return NS_ERROR_FAILURE;

  localFile->InitWithPath(nsDependentString(platformAppPath));
  *aFile = localFile;
  NS_IF_ADDREF(*aFile);

  return NS_OK;
}

nsresult nsOSHelperAppService::GetMIMEInfoForExtensionFromOS(const char *aFileExt, nsIMIMEInfo **_retval)
{
  // if the extension is null, return immediately
  if (!aFileExt || !*aFileExt)
    return NS_ERROR_INVALID_ARG;
  
  nsresult rv = NS_ERROR_FAILURE;

#ifdef DEBUG_bzbarsky
  fprintf(stderr, "Here we do an extension lookup for %s\n\n", aFileExt);
#endif // DEBUG_bzbarsky

  nsAutoString mimeType, majorType, minorType,
               mime_types_description, mailcap_description,
               handler, mozillaFlags;
  
  rv = LookUpTypeAndDescription(NS_ConvertUTF8toUCS2(aFileExt),
                                majorType,
                                minorType,
                                mime_types_description);

  if (NS_FAILED(rv))
    return rv;

  if (majorType.IsEmpty() && minorType.IsEmpty()) {
    // we didn't get a type mapping, so we can't do anything useful
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIMIMEInfo> mimeInfo(do_CreateInstance(NS_MIMEINFO_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  
  mimeType = majorType + NS_LITERAL_STRING("/") + minorType;
  mimeInfo->SetMIMEType(NS_ConvertUCS2toUTF8(mimeType).get());
  mimeInfo->AppendExtension(aFileExt);
  nsHashtable typeOptions; // empty hash table
  rv = LookUpHandlerAndDescription(majorType, minorType, typeOptions,
                                   handler, mailcap_description,
                                   mozillaFlags);
  mailcap_description.Trim(" \t\"");
  mozillaFlags.Trim(" \t");
  if (!mime_types_description.IsEmpty()) {
    mimeInfo->SetDescription(mime_types_description.get());
  } else {
    mimeInfo->SetDescription(mailcap_description.get());
  }
  if (NS_SUCCEEDED(rv) && !handler.IsEmpty()) {
    nsCOMPtr<nsIFile> handlerFile;
    rv = GetFileTokenForPath(handler.get(), getter_AddRefs(handlerFile));
    
    if (NS_SUCCEEDED(rv)) {
      mimeInfo->SetDefaultApplicationHandler(handlerFile);
      mimeInfo->SetPreferredAction(nsIMIMEInfo::useSystemDefault);
#ifdef DEBUG_bzbarsky
      fprintf(stderr, "Here we want to set handler to: %s\n", NS_ConvertUCS2toUTF8(handler).get());
#endif // DEBUG_bzbarsky
      mimeInfo->SetDefaultApplicationDescription(handler.get());
    }
  } else {
    mimeInfo->SetPreferredAction(nsIMIMEInfo::saveToDisk);
  }
  
  *_retval = mimeInfo;
  NS_ADDREF(*_retval);
  
  // XXX Once cache can be invalidated, add the mime info to it.  See bug 121644
  // AddMimeInfoToCache(*_retval);
  
  return NS_OK;
}

nsresult nsOSHelperAppService::GetMIMEInfoForMimeTypeFromOS(const char *aMIMEType, nsIMIMEInfo ** _retval)
{
  // if the extension is null, return immediately
  if (!aMIMEType)
    return NS_ERROR_INVALID_ARG;
  
  nsresult rv;
#ifdef DEBUG_bzbarsky
  fprintf(stderr, "Here we do a mimetype lookup for %s\n\n\n", aMIMEType);
#endif // DEBUG_bzbarsky
  nsAutoString extensions,
    mime_types_description, mailcap_description,
    handler, mozillaFlags;

  nsHashtable typeOptions;
  
  // extract the major and minor types
  nsAutoString mimeType;
  mimeType.AssignWithConversion(aMIMEType);
  nsAString::const_iterator start_iter, end_iter,
                            majorTypeStart, majorTypeEnd,
                            minorTypeStart, minorTypeEnd;

  mimeType.BeginReading(start_iter);
  mimeType.EndReading(end_iter);

  // XXX FIXME: add typeOptions parsing in here
  rv = ParseMIMEType(start_iter, majorTypeStart, majorTypeEnd,
                     minorTypeStart, minorTypeEnd, end_iter);

  if (NS_FAILED(rv)) {
    return rv;
  }

  nsDependentSubstring majorType(majorTypeStart, majorTypeEnd);
  nsDependentSubstring minorType(minorTypeStart, minorTypeEnd);
  LookUpHandlerAndDescription(majorType,
                              minorType,
                              typeOptions,
                              handler,
                              mailcap_description,
                              mozillaFlags);
  
  if (handler.IsEmpty()) {
    // we have no useful info....
    return NS_ERROR_FAILURE;
  }
  
  mailcap_description.Trim(" \t\"");
  mozillaFlags.Trim(" \t");
  LookUpExtensionsAndDescription(majorType,
                                 minorType,
                                 extensions,
                                 mime_types_description);
  
  nsCOMPtr<nsIMIMEInfo> mimeInfo(do_CreateInstance(NS_MIMEINFO_CONTRACTID, &rv));
    
  if (NS_FAILED(rv))
    return rv;
  
  mimeInfo->SetFileExtensions(PromiseFlatCString(NS_ConvertUCS2toUTF8(extensions)).get());
  mimeInfo->SetMIMEType(aMIMEType);
  if (! mime_types_description.IsEmpty()) {
    mimeInfo->SetDescription(mime_types_description.get());
  } else {
    mimeInfo->SetDescription(mailcap_description.get());
  }
    
  rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIFile> handlerFile;
  rv = GetFileTokenForPath(handler.get(), getter_AddRefs(handlerFile));
  
  if (NS_SUCCEEDED(rv)) {
    mimeInfo->SetDefaultApplicationHandler(handlerFile);
    mimeInfo->SetPreferredAction(nsIMIMEInfo::useSystemDefault);
    // FIXME set the handler
#ifdef DEBUG_bzbarsky
    fprintf(stderr, "Here we want to set handler to: %s\n", NS_ConvertUCS2toUTF8(handler).get());
#endif // DEBUG_bzbarsky
    mimeInfo->SetDefaultApplicationDescription(handler.get());
  } else {
    mimeInfo->SetPreferredAction(nsIMIMEInfo::saveToDisk);
  }
    
  *_retval = mimeInfo;
  NS_ADDREF(*_retval);
    
  // XXX Once cache can be invalidated, add the mime info to it.  See bug 121644
  // AddMimeInfoToCache(*_retval);
  return NS_OK;
}
