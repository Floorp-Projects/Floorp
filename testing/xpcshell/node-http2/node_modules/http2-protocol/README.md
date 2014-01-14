node-http2-protocol
===================

An HTTP/2 ([draft-ietf-httpbis-http2-09](http://tools.ietf.org/html/draft-ietf-httpbis-http2-09))
framing layer implementaion for node.js.

Installation
------------

```
npm install http2-protocol
```

Examples
--------

API
---

Development
-----------

### Development dependencies ###

There's a few library you will need to have installed to do anything described in the following
sections. After installing/cloning node-http2, run `npm install` in its directory to install
development dependencies.

Used libraries:

* [mocha](http://visionmedia.github.io/mocha/) for tests
* [chai](http://chaijs.com/) for assertions
* [istanbul](https://github.com/gotwarlost/istanbul) for code coverage analysis
* [docco](http://jashkenas.github.io/docco/) for developer documentation
* [bunyan](https://github.com/trentm/node-bunyan) for logging

For pretty printing logs, you will also need a global install of bunyan (`npm install -g bunyan`).

### Developer documentation ###

The developer documentation is located in the `doc` directory. The docs are usually updated only
before releasing a new version. To regenerate them manually, run `npm run-script prepublish`.
There's a hosted version which is located [here](http://molnarg.github.io/node-http2-protocol/doc/).

### Running the tests ###

It's easy, just run `npm test`. The tests are written in BDD style, so they are a good starting
point to understand the code.

### Test coverage ###

To generate a code coverage report, run `npm test --coverage` (it may be slow, be patient).
Code coverage summary as of version 0.6.0:
```
Statements   : 92.43% ( 1257/1360 )
Branches     : 86.36% ( 500/579 )
Functions    : 90.12% ( 146/162 )
Lines        : 92.39% ( 1251/1354 )
```

There's a hosted version of the detailed (line-by-line) coverage report
[here](http://molnarg.github.io/node-http2-protocol/coverage/lcov-report/lib/).

### Logging ###

Contributors
------------

Code contributions are always welcome! People who contributed to node-http2 so far:

* Nick Hurley
* Mike Belshe

Special thanks to Google for financing the development of this module as part of their [Summer of
Code program](https://developers.google.com/open-source/soc/) (project: [HTTP/2 prototype server
implementation](https://google-melange.appspot.com/gsoc/project/google/gsoc2013/molnarg/5001)), and
Nick Hurley of Mozilla, my GSoC mentor, who helped with regular code review and technical advices.

License
-------

The MIT License

Copyright (C) 2013 Gábor Molnár <gabor@molnar.es>
