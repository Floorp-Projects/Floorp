/* import-globals-from mixedContentTest.js */
"use strict";

document.open();
document.write("This is insecure XSS script " + document.cookie);
isSecurityState("broken", "security broken after document write from unsecure script");
finish();
