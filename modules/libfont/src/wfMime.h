/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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

