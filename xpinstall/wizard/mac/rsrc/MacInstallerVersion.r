/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): jj@netscape.com
 */

#include "Types.r"

resource 'vers' (1, "XPInstall version") {
        1,
        0x10,
        release,	// {alpha, beta, release}
        0,			// alpha or beta number; 0 for release
        0,			// language
        "1.1",		// short version string
        "XPInstall 1.1"	// long version string
};

resource 'vers' (2, "Mozilla Installer version") {
        1,
        0x30,
        beta,	// {alpha, beta, release}
        0,			// alpha or beta number; 0 for release
        0,			// language
        "1.3b",		// short version string
        "Mozilla 1.3b Installer"	// long version string
};

