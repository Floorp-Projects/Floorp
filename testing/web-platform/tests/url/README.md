The test for browsers is a-element.html. The reusable format is urltestdata.txt, which is
not documented in detail, but is documented through urltestparser.js, which parses it.
Documentation welcome!

https://github.com/annevk/url/ hosts some other files that might be of interest if you
want to create additional tests. Please try to keep urltestdata.txt in sync with that
repository.

Similar to a-element.html it would be trivial to add more tests for other objects that
expose links (e.g. URL and <area>). There's also room for enhancement and bits that
require independent tests:

* The encoding part of the URL parser
* The state override part of the URL parser (setting individual properties of a URL)
* Origin serialization
