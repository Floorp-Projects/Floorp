/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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



#include "Types.r"

/* When editing this file, make sure to match your changes on macpref.cp */

read 'TEXT' ( 3000, "initprefs", purgeable ) "initpref.js";

read 'TEXT' ( 3010, "allprefs", purgeable ) "all.js";

read 'TEXT' ( 3016, "mailnews.js", purgeable ) "mailnews.js";

read 'TEXT' ( 3017, "editor.js", purgeable ) "editor.js";

read 'TEXT' ( 3018, "security.js", purgeable ) "security.js";

#if defined(MOZ_MAIL_NEWS) || defined (EDITOR)

read 'TEXT' ( 3011, "config", purgeable ) "config.js";

read 'TEXT' ( 3015, "macprefs", purgeable ) "macprefs.js";

#else

read 'TEXT' ( 3011, "config", purgeable ) "configr.js";

read 'TEXT' ( 3015, "macprefs", purgeable ) "macprefsNav.js";

#endif // MOZ_MAIL_NEWS || EDITOR
