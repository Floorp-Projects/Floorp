/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

// This is processed after unix.js and anything here supercedes unix.js
// and all the other pref files.

pref("mail.use_movemail", false);
pref("mail.use_builtin_movemail", false);

pref("helpers.global_mime_types_file", "/sys$manager/netscape/mime.types");
pref("helpers.global_mailcap_file", "/sys$manager/netscape/mailcap");
pref("helpers.private_mime_types_file", "/sys$login/.mime.types");
pref("helpers.private_mailcaptypes_file", "/sys$login/.mailcap");

pref("applications.telnet", "create /term /detach \"telnet %h %p\"");
pref("applications.tn3270", "create /term /detach \"telnet /term=IBM-3278-5 %h %p\"");
pref("applications.rlogin", "create /term /detach \"rlogin %h\"");
pref("applications.rlogin_with_user", "create /term /detach \"rlogin %h -l %u\"");

pref("print.print_command", "print");
pref("print.print_color", false);

pref("security.enable_java", false);

pref("browser.cache.disk.capacity", 4000);

