/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * rc.cpp (RenderingContextObject)
 *
 * C++ implementation of the (rc) RenderingContextObject
 *
 * dp Suresh <dp@netscape.com>
 */


#include "rc.h"

// Constructor and Destructor
RenderingContextObject::
RenderingContextObject(jint majorType, jint minorType, void **arg, jsize nargs)
{
	rcData.majorType = NF_RC_INVALID;
	rcData.minorType = 0;
	rcData.displayer_data = NULL;
	if (majorType == NF_RC_DIRECT)
	{
		rcData.majorType = majorType;
		rcData.minorType = minorType;
#if defined(XP_UNIX)
		rcData.t.directRc.display = NULL;
		rcData.t.directRc.d = NULL;
		rcData.t.directRc.gc = NULL;
		rcData.t.directRc.mask = 0;
		if (nargs >= 1) rcData.t.directRc.display = arg[0];
		if (nargs >= 2) rcData.t.directRc.d = arg[1];
		if (nargs >= 3) rcData.t.directRc.gc = arg[2];
		if (nargs >= 4) rcData.t.directRc.mask = (jint)arg[3];
#elif defined(XP_WIN) || defined(XP_OS2)
		rcData.t.directRc.dc = NULL;
		if (nargs >= 1) rcData.t.directRc.dc = arg[0];
#elif defined(XP_MAC)
		rcData.t.directRc.port = NULL;
		if (nargs >= 1) rcData.t.directRc.port = arg[0];
#endif
	}
}


RenderingContextObject::~RenderingContextObject()
{
}

jint
RenderingContextObject::GetMajorType(void)
{
	return (rcData.majorType);
}

jint
RenderingContextObject::GetMinorType(void)
{
	return (rcData.minorType);
}

jint
RenderingContextObject::IsEquivalent(jint majorType, jint minorType)
{
	return ((minorType != NF_RC_ALWAYS_DIFFERENT) &&
			(majorType == rcData.majorType) &&
			(minorType == rcData.minorType));
}

//
// This will be used only by the Displayer & Consumers
//
struct rc_data
RenderingContextObject::GetPlatformData(void)
{
	return (rcData);
}

jint
RenderingContextObject::SetPlatformData(struct rc_data *newRcData)
{
	jint ret = -1;
	if (newRcData)
	{
		rcData = *newRcData;
		ret = 0;
	}
	return (ret);
}
