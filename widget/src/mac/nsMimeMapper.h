/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//
// Mike Pinkerton
// Netscape Communications
//
// Contains the interface for |nsMimeMapper|, which is used to map between
// mozilla defined mime types (such as text/html or mozilla/toolbaritem) and
// MacOS flavors which are 4-character codes. Since's it's not easy to go
// from N characters down to 4 and back to N without loss, we need a different
// mechanism than just some hash algorithm.
//
// The way this is generally used is to instantiate a mime mapper when you're
// about to hand data to the OS and then ask it to map the mime type into a
// 4-character code. If it's not one that the mapper knows about intrinsically,
// it creates a unique type and remembers the mapping internally. Do this for each
// different flavor of data you have. Then, when you're ready to hand the
// data to the OS for real, ask the mapper to export its mapping list (with
// ExportMapping()) which can be put into a special, well known, data flavor 
// that goes into the OS along with the data.
//
// When pulling data out of the OS and into mozilla, the mapper is used to
// convert from the 4-character codes the OS knows about back to the original mime
// types. At this stage, depending the originator of the data (mozilla or some other
// mac app), you may or may not have a mapping flavor which explicitly lays out the 
// conversions. If you do, load it (you have to do this yourself) and pass it to the
// ctor. If you don't, that's ok too and the well-known mappings can still be performed.
//

#ifndef nsMimeMapper_h__
#define nsMimeMapper_h__

#include <utility>
#include <vector>
#include <Types.h>
#include "nsString.h"

using std::pair;

class nsMimeMapperMac 
{
public:
  enum { kMappingFlavor = 'MOZm' } ;
  
  nsMimeMapperMac ( const char* inMappings = nsnull ) ;
  ~nsMimeMapperMac ( ) ;
   
    // Converts from mime type (eg: text/plain) to MacOS type (eg: 'TEXT'). If
    // the mapping failed, the resulting restype will be null.
  ResType MapMimeTypeToMacOSType ( const char* aMimeStr, PRBool inAddIfNotPresent = PR_TRUE ) ;
  void MapMacOSTypeToMimeType ( ResType inMacType, nsCAutoString & outMimeStr ) ;
 
    // Takes the internal mappings and converts them to a string for
    // placing on the clipboard or in a drag item. |outLength| includes 
    // the NULL at the end of the string.
  char* ExportMapping ( short * outLength ) const;

  static ResType MappingFlavor ( ) { return kMappingFlavor; }
  
private:

  void ParseMappings ( const char* inMappings ) ;
  
  typedef pair<ResType, nsCAutoString>	MimePair;
  typedef std::vector<MimePair>		MimeMap;
  typedef MimeMap::iterator			MimeMapIterator;
  typedef MimeMap::const_iterator	MimeMapConstIterator;
  
    // the keeper of the data
  MimeMap mMappings;
    // an increasing counter for generating unique flavors
  short mCounter;

   // don't want people copying this until copy ctor does the right thing
  nsMimeMapperMac ( const nsMimeMapperMac & ) ;
 
}; // nsMimeMapperMac


#endif
