biplist
=======
`biplist` is a binary plist parser/generator for Python.

## About

Binary Property List (plist) files provide a faster and smaller serialization
format for property lists on OS X. This is a library for generating binary
plists which can be read by OS X, iOS, or other clients.

## API

The API models the `plistlib` API, and will call through to plistlib when
XML serialization or deserialization is required.

To generate plists with UID values, wrap the values with the `Uid` object. The
value must be an int.

To generate plists with `NSData`/`CFData` values, wrap the values with the
`Data` object. The value must be a string.

Date values can only be `datetime.datetime` objects.

The exceptions `InvalidPlistException` and `NotBinaryPlistException` may be 
thrown to indicate that the data cannot be serialized or deserialized as
a binary plist.

## Installation

To install the latest release version:

`sudo easy_install biplist`

## Examples

Plist generation example:

```python
from biplist import *
from datetime import datetime
plist = {'aKey':'aValue',
         '0':1.322,
         'now':datetime.now(),
         'list':[1,2,3],
         'tuple':('a','b','c')
         }
try:
    writePlist(plist, "example.plist")
except (InvalidPlistException, NotBinaryPlistException), e:
    print "Something bad happened:", e
```

Plist parsing example:

```python
from biplist import *
try:
    plist = readPlist("example.plist")
    print plist
except (InvalidPlistException, NotBinaryPlistException), e:
    print "Not a plist:", e
```
