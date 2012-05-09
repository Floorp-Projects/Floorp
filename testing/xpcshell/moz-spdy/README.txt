Test server for SPDY unit tests. To run it, you need node >= 0.7.0 (not provided)
and node-spdy (provided). Just run

node /path/to/moz-spdy.js

And you will get a SPDY server listening on port 4443, then you can run the
xpcshell unit tests in netwerk/test/unit/test_spdy.js

*** A NOTE ON TLS CERTIFICATES ***

The certificates used for this test (*.pem in this directory) are the ones
provided as examples by node-spdy, and are copied directly from keys/ under
its top-level source directory (slightly renamed to match the option names
in the options dictionary passed to spdy.createServer).
