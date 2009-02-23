document.write("This is insecure XSS script " + document.cookie);
todoSecurityState("broken", "security broken after document write from unsecure script");
finish();
