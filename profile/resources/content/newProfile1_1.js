/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger (30/09/99)
 */

// although these functions aren't used on this intro page, they're actually
// essential for the wizard progress. They do nothing and simply exist so that
// they are defined so that the wizard can call them.

// the getting procedure is unique to each page, since each page can different
// types of elements (not necessarily form elements). So each page must provide
// its own GetFields function
function GetFields() { }

// the setting procedure is unique to each page, and as such each page
// must provide its own SetFields function
function SetFields(element,value) { }
