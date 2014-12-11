/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This is a file that test_xpcshell_debugging.js debugs.

// We should hit this dump as it is the first debuggable line
dump("hello from the debugee!\n")

debugger; // and why not check we hit this!?
