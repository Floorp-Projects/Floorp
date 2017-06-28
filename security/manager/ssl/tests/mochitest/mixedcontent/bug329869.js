/* import-globals-from mixedContentTest.js */
"use strict";

document.open();
// eslint-disable-next-line no-unsanitized/method
document.write("This is insecure XSS script " + document.cookie);
isSecurityState("broken", "security broken after document write from unsecure script");
finish();
