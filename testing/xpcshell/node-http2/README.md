node-http2
==========

An HTTP/2 ([draft-ietf-httpbis-http2-07](http://tools.ietf.org/html/draft-ietf-httpbis-http2-76))
client and server implementation for node.js.

Installation
------------

```
npm install http2
```

API
---

The API is very similar to the [standard node.js HTTPS API](http://nodejs.org/api/https.html). The
goal is the perfect API compatibility, with additional HTTP2 related extensions (like server push).

Detailed API documentation is primarily maintained in the `lib/http.js` file and is [available in
the wiki](https://github.com/molnarg/node-http2/wiki/Public-API) as well.

Examples
--------

### Using as a server ###

```javascript
var options = {
  key: fs.readFileSync('./example/localhost.key'),
  cert: fs.readFileSync('./example/localhost.crt')
};

require('http2').createServer(options, function(request, response) {
  response.end('Hello world!');
}).listen(8080);
```

### Using as a client ###

```javascript
process.env.NODE_TLS_REJECT_UNAUTHORIZED = "0";

require('http2').get('https://localhost:8080/', function(response) {
  response.pipe(process.stdout);
});
```

### Simple static file server ###

An simple static file server serving up content from its own directory is available in the `example`
directory. Running the server:

```bash
$ node ./example/server.js
```

### Simple command line client ###

An example client is also available. Downloading the server's own source code from the server:

```bash
$ node ./example/client.js 'https://localhost:8080/server.js' >/tmp/server.js
```

### Server push ###

For a server push example, see the source code of the example
[server](https://github.com/molnarg/node-http2/blob/master/example/server.js) and
[client](https://github.com/molnarg/node-http2/blob/master/example/client.js).

Status
------

* ALPN is not yet supported in node.js (see
  [this issue](https://github.com/joyent/node/issues/5945)). For ALPN support, you will have to use
  [Shigeki Ohtsu's node.js fork](https://github.com/shigeki/node/tree/alpn_support) until this code
  gets merged upstream.
* Upgrade mechanism to start HTTP/2 over unencrypted channel is not implemented yet
  (issue [#4](https://github.com/molnarg/node-http2/issues/4))
* Other minor features found in
  [this list](https://github.com/molnarg/node-http2/issues?labels=feature) are not implemented yet

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
There's a hosted version which is located [here](http://molnarg.github.io/node-http2/doc/).

### Running the tests ###

It's easy, just run `npm test`. The tests are written in BDD style, so they are a good starting
point to understand the code.

### Test coverage ###

To generate a code coverage report, run `npm test --coverage` (which runs very slowly, be patient).
Code coverage summary as of version 1.0.1:
```
Statements   : 93.26% ( 1563/1676 )
Branches     : 84.85% ( 605/713 )
Functions    : 94.81% ( 201/212 )
Lines        : 93.23% ( 1557/1670 )
```

There's a hosted version of the detailed (line-by-line) coverage report
[here](http://molnarg.github.io/node-http2/coverage/lcov-report/lib/).

### Logging ###

Logging is turned off by default. You can turn it on by passing a bunyan logger as `log` option when
creating a server or agent.

When using the example server or client, it's very easy to turn logging on: set the `HTTP2_LOG`
environment variable to `fatal`, `error`, `warn`, `info`, `debug` or `trace` (the logging level).
To log every single incoming and outgoing data chunk, use `HTTP2_LOG_DATA=1` besides
`HTTP2_LOG=trace`. Log output goes to the standard error output. If the standard error is redirected
into a file, then the log output is in bunyan's JSON format for easier post-mortem analysis.

Running the example server and client with `info` level logging output:

```bash
$ HTTP2_LOG=info node ./example/server.js
```

```bash
$ HTTP2_LOG=info node ./example/client.js 'http://localhost:8080/server.js' >/dev/null
```

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
