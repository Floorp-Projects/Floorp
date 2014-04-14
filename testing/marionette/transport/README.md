<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Marionette Transport Layer

[Marionette](https://developer.mozilla.org/en/Marionette) is a 
Mozilla project to enable remote automation in Gecko-based projects,
including desktop Firefox, mobile Firefox, and Firefox OS.   It's inspired
by [Selenium Webdriver](http://www.seleniumhq.org/projects/webdriver/).

This package defines the transport layer used by a Marionette client to
communicate with the Marionette server embedded in Gecko.  It has no entry
points; rather it's designed to be used by Marionette client implementations.
