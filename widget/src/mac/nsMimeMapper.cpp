/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

//
// Mike Pinkerton
// Netscape Communications
//
// See header file for details
//

#include "nsMimeMapper.h"
#include "nsITransferable.h"
#include "nsString.h"


nsMimeMapperMac :: nsMimeMapperMac ( const char* inMappings )
  : mCounter(0)
{
  if (inMappings && strlen(inMappings) )
    ParseMappings ( inMappings );
}


nsMimeMapperMac :: ~nsMimeMapperMac ( )
{

}   


//
// MapMimeTypeToMacOSType
//
// Given a mime type, map this into the appropriate MacOS clipboard type. For
// types that we don't know about intrinsicly, use a trick to get a unique 4
// character code.
//
// When a mapping is made, store it in the current mapping list so it can
// be exported along with the data.
//
ResType
nsMimeMapperMac :: MapMimeTypeToMacOSType ( const char* aMimeStr, PRBool inAddIfNotPresent )
{
  ResType format = 0;

  // first check if it is already in our list.
  for ( MimeMapConstIterator it = mMappings.begin(); it != mMappings.end(); ++it ) {
    if ( it->second == aMimeStr ) {
      format = it->first;
      break;
    }
  }

  // if it's not there, do a mapping and put it there. Some things we want to map
  // to specific MacOS types, like 'TEXT'. Others are internal and we don't really
  // care about what they look like, just that they have a fairly unique flavor type.
  //
  // Don't put standard MacOS flavor mappings into the mapping list because we'll
  // pick them up by special casing MapMacsOSTypeToMimeType(). This means that
  // the low two bytes of the generated flavor can be used as an index into the list.
  if ( !format ) {
    if ( PL_strcmp(aMimeStr, kTextMime) == 0 )
      format = 'TEXT';
    if ( PL_strcmp(aMimeStr, kFileMime) == 0 )
      format = flavorTypeHFS;

    else if ( inAddIfNotPresent ) {
      // create the flavor based on the unique id in the lower two bytes and 'MZ' in the
      // upper two bytes.
      format = mCounter++;
      format |= ('..MZ' << 16);     
 
      // stick it in the mapping list
      mMappings.push_back ( MimePair(format, nsCAutoString(aMimeStr)) );
    }
  
  }

  if ( inAddIfNotPresent )
    NS_ASSERTION ( format, "Didn't map mimeType to a macOS type for some reason" );   
  return format;
  
} // MapMimeTypeToMacOSType


//
// MapMacOSTypeToMimeType
//
// Given a MacOS flavor, map this back into the Mozilla mimetype. Uses the mappings
// that have already been loaded into this class. If there aren't any, that's ok too, but
// we probably won't get a match in that case.
//
void
nsMimeMapperMac :: MapMacOSTypeToMimeType ( ResType inMacType, nsCAutoString & outMimeStr )
{
  switch ( inMacType ) {
  
    case 'TEXT': outMimeStr = kTextMime; break;
    case flavorTypeHFS: outMimeStr = kFileMime; break;
    
    // This flavor is the old 4.x Composer flavor for HTML. The actual data is a binary
    // data structure which we do NOT want to deal with in any way shape or form. I am
    // only including this flavor here so we don't accidentally use it ourselves and
    // get very very confused. 
    case 'EHTM':
      // Fall through to the unknown case.
  
    default:

      outMimeStr = "unknown";

      // if the flavor starts with 'MZ' then it's probably one of our encoded ones. If not,
      // we have no idea what it is. The way this was encoded means that we can use the
      // lower two byts of the flavor as an index into our mapping list.
      if ( inMacType & ('..MZ' << 16) ) {
        short index = inMacType & 0x0000FFFF;    // extract the index into our internal list
        if ( index < mMappings.size() )
          outMimeStr = mMappings[index].second;
        else
          NS_WARNING("Found a flavor starting with 'MZ..' that isn't one of ours!");
      }
        
  } // case of which flavor

} // MapMacOSTypeToMimeType


//
// ParseMappings
//
// The mappings are of the form
//   1..N of (<4 char code> <space> <mime type>).
//
// It is perfectly acceptable for there to be no mappings (either |inMappings|
// is null or an emtpy string).
//
void
nsMimeMapperMac :: ParseMappings ( const char* inMappings )
{
  if ( !inMappings )
    return;

  const char* currPosition = inMappings;
  while ( *currPosition ) {
    char mimeType[100];
    ResType flavor = nsnull;

    sscanf ( currPosition, "%ld %s ", &flavor, mimeType );
    mMappings.push_back( MimePair(flavor, nsCAutoString(mimeType)) );

    currPosition += 10 + 2 + strlen(mimeType);  // see ExportMapping() for explanation of this calculation
    
    ++mCounter;
  } // while we're not at the end yet
  
} // ParseMappings


//
// ExportMapping
//
// The mappings are of the form
//   <# of pairs> 1..N of (<4 char code> <space> <mime type> <space>)
//
// Caller is responsible for disposing of the memory allocated here. |outLength| counts
// the null at the end of the string.
//
char*
nsMimeMapperMac :: ExportMapping ( short * outLength ) const
{
  NS_WARN_IF_FALSE ( outLength, "No out param provided" );
  if ( outLength )
    *outLength = 0;

#if 0
// I'm leaving this code in here just to prove a point. If we were allowed to
// use string stream's (we're not, because of bloat), this is all the code I'd have
// to write to do all the crap down below.
  ostringstream ostr;

  // for each pair, write out flavor and mime types
  for ( MimeMapConstIterator it = mMappings.begin(); it != mMappings.end(); ++it ) {
    const char* mimeType = it->second.ToNewCString();
  	ostr << it->first << ' ' << mimeType << ' ';
  	delete [] mimeType;
  }
  
  return ostr.str().c_str();
#endif

  char* exportBuffer = nsnull;
  
  // figure out about how long the composed string will be
  short len = 0;
  for ( MimeMapConstIterator it = mMappings.begin(); it != mMappings.end(); ++it ) {
    len += 10;  // <4 char code> (any decimal representation of 'MZXX' will be 10 digits)
    len += 2;   // for the two spaces
    len += it->second.Length();  // <mime type>
  }  

  // create a string of that length and fill it in with each mapping. We have to
  // consider the possibility that there aren't any generic (internal mozilla) flavors
  // so the map could be empty.
  exportBuffer = NS_STATIC_CAST(char*, nsMemory::Alloc(len + 1));      // don't forget the NULL
  if ( !exportBuffer )
    return nsnull;
  *exportBuffer = '\0';                          // null terminate at the start for strcat()
  if ( len ) {
    char* posInString = exportBuffer;
    for ( MimeMapConstIterator it = mMappings.begin(); it != mMappings.end(); ++it ) {
      // create a buffer for this mapping, fill it in, and append it to our
      // ongoing result buffer, |exportBuffer|. 
      char* currMapping = new char[10 + 2 + it->second.Length() + 1];  // same computation as above, plus NULL
      char* mimeType = it->second.ToNewCString();
      if ( currMapping && mimeType ) {
        sprintf(currMapping, "%ld %s ", it->first, mimeType);
        strcat(posInString, currMapping);
        posInString += strlen(currMapping);     // advance marker to get ready for next mapping
      }
      nsCRT::free ( mimeType );
      nsCRT::free ( currMapping );
    }
      
    *posInString = '\0';                        // null terminate our resulting string
  } // if there is anything in our list
  
  if ( outLength )
    *outLength = len + 1;  // don't forget the NULL
  return exportBuffer;
  
} // ExportMapping
