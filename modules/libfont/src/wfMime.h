/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* 
 * wfMime.h (wfMimeList.h)
 *
 * Implements a list of mime types. This doesn't know anything about how the
 * mime type list is being used. It can track mimetypes being enabled or not.
 *
 * dp Suresh <dp@netscape.com>
 */


#ifndef _wfMime_H_
#define _wfMime_H_

#include <string.h>

#include "libfont.h"
#include "wfList.h"

struct mime_store {
	char *mimetype;
	char *extensions;		// comma seperated list of extension. Dot is optional
	char *description;
	int isEnabled;			// 0 if disabled; else enabled
};

class wfMimeList : public wfList {
private:
	struct mime_store *find(const char *mimetype);
	const char *scanToken(const char *str, char *buf, int len, char *stopChars,
		int noSpacesOnOutput);
	const char *skipSpaces(const char *str);

public:
	// Constructor
	wfMimeList(const char *mimeString = NULL);

	// Destructor
	~wfMimeList();

	int finalize(void);
	
	// Sets the enabled status for mimetype. Valid status values are
	//	0	: Disabled
	// +ve	: Enabled
	// Returns 0 if success; -1 if error (mimetype not found, ...)
	int setEnabledStatus(const char *mimetype, int enabledStatus);
  
	int isEnabled(const char *mimetype);
	const char *getMimetypeFromExtension(const char *ext);

	// Describes the status and attributes of all mimetypes as a string
	// This string can be stored and later used to recreate the exact state of
	// this wfMimeList using wfMimeList::recreate()
	char *describe();
	int reconstruct(const char *describeString);
};

#endif /* _wfMime_H_ */

