<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

[Peptest](https://wiki.mozilla.org/Auto-tools/Projects/peptest) 
is a Mozilla automated testing harness for running responsiveness tests.
These tests measure how long events spend away from the event loop.

# Running Tests

Currently tests are run from the command line with python. 
Peptest currently depends on some external Mozilla python packages, namely: 
mozrunner, mozprocess, mozprofile, mozinfo, mozlog, mozhttpd and manifestdestiny. 

See [running tests](https://wiki.mozilla.org/Auto-tools/Projects/peptest#Running_Tests) 
for more information.
