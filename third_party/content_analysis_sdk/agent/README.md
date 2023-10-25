# Google Chrome Content Analysis Connector Agent SDK

This directory holds the Google Chrome Content Analysis Connector Agent SDK.
An Agent is an OS process running on the same computer as Google Chrome
that listens for and processes content analysis requests from the browser.
Supported OSes are Windows, Mac, and Linux.

## Google Protocol Buffers

This SDK depends on Google Protocol Buffers version 3.18 or later.  It may be
installed from Google's [download page](https://developers.google.com/protocol-buffers/docs/downloads#release-packages)
for your developement platform.  It may also be installed using a package
manager.

The included demo uses the Microsoft [vcpkg](https://github.com/microsoft/vcpkg)
package manager to install protobuf.  vcpkg is available on all supported
platforms.  See the demo for more details.

## Adding the SDK into an agent

Add the SDK to a content analysis agent as follows:

1. Clone the SDK from [Github](https://github.com/chromium/content_analysis_sdk).
This document assumes that the SDK is downloaded and extracted into the
directory $SDK_DIR.

2. Add the directory $SDK_DIR/include to the include path of the agent
code base.

3. Add all the source files in the directory $SDK_DIR/src to the agent build.
Note that files ending in _win.cc or _win.h are meant only for Windows, files
ending in _posix.cc or _posix.h are meant only for Linux, and files ending in
_mac.cc or _mac.h are meant only for Mac.

4. Reference the SDK in agent code using:
```
#include "content_analysis/sdk/local_analysis.h"
```