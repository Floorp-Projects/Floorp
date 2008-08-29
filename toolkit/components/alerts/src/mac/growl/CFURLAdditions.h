//
//  CFURLAdditions.h
//  Growl
//
//  Created by Karl Adam on Fri May 28 2004.
//  Copyright 2004-2006 The Growl Project. All rights reserved.
//
// This file is under the BSD License, refer to License.txt for details

#ifndef HAVE_CFURLADDITIONS_H
#define HAVE_CFURLADDITIONS_H

#include <CoreFoundation/CoreFoundation.h>
#include "CFGrowlDefines.h"

//'alias' as in the Alias Manager.
URL_TYPE createFileURLWithAliasData(DATA_TYPE aliasData);
DATA_TYPE createAliasDataWithURL(URL_TYPE theURL);

//these are the type of external representations used by Dock.app.
URL_TYPE createFileURLWithDockDescription(DICTIONARY_TYPE dict);
//createDockDescriptionWithURL returns NULL for non-file: URLs.
DICTIONARY_TYPE createDockDescriptionWithURL(URL_TYPE theURL);

#endif
