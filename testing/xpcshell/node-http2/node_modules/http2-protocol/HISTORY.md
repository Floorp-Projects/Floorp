Version history
===============

### 0.9.0 (2013-12-25) ###

* Upgrade to the latest draft: [draft-ietf-httpbis-http2-09][draft-09]
* [Tarball](https://github.com/molnarg/node-http2-protocol/archive/node-http2-protocol-0.9.0.tar.gz)

[draft-09]: http://tools.ietf.org/html/draft-ietf-httpbis-http2-09

### 0.7.0 (2013-11-10) ###

* Upgrade to the latest draft: [draft-ietf-httpbis-http2-07][draft-07]
* [Tarball](https://github.com/molnarg/node-http2-protocol/archive/node-http2-protocol-0.7.0.tar.gz)

[draft-07]: http://tools.ietf.org/html/draft-ietf-httpbis-http2-07

### 0.6.0 (2013-11-09) ###

* Splitting out node-http2-protocol from node-http2
* The only exported class is `Endpoint`
* Versioning will be based on the implemented protocol version with a '0.' prefix
* [Tarball](https://github.com/molnarg/node-http2-protocol/archive/node-http2-protocol-0.6.0.tar.gz)

Version history as part of the [node-http](https://github.com/molnarg/node-http2) module
----------------------------------------------------------------------------------------

### 1.0.1 (2013-10-14) ###

* Support for ALPN if node supports it (currently needs a custom build)
* Fix for a few small issues
* [Tarball](https://github.com/molnarg/node-http2/archive/node-http2-1.0.1.tar.gz)

### 1.0.0 (2013-09-23) ###

* Exporting Endpoint class
* Support for 'filters' in Endpoint
* The last time-based release
* [Tarball](https://github.com/molnarg/node-http2/archive/node-http2-1.0.0.tar.gz)

### 0.4.1 (2013-09-15) ###

* Major performance improvements
* Minor improvements to error handling
* [Blog post](http://gabor.molnar.es/blog/2013/09/15/gsoc-week-number-13/)
* [Tarball](https://github.com/molnarg/node-http2/archive/node-http2-0.4.1.tar.gz)

### 0.4.0 (2013-09-09) ###

* Upgrade to the latest draft: [draft-ietf-httpbis-http2-06][draft-06]
* Support for HTTP trailers
* Support for TLS SNI (Server Name Indication)
* Improved stream scheduling algorithm
* [Blog post](http://gabor.molnar.es/blog/2013/09/09/gsoc-week-number-12/)
* [Tarball](https://github.com/molnarg/node-http2/archive/node-http2-0.4.0.tar.gz)

[draft-06]: http://tools.ietf.org/html/draft-ietf-httpbis-http2-06

### 0.3.1 (2013-09-03) ###

* Lot of testing, bugfixes
* [Blog post](http://gabor.molnar.es/blog/2013/09/03/gsoc-week-number-11/)
* [Tarball](https://github.com/molnarg/node-http2/archive/node-http2-0.3.1.tar.gz)

### 0.3.0 (2013-08-27) ###

* Support for prioritization
* Small API compatibility improvements (compatibility with the standard node.js HTTP API)
* Minor push API change
* Ability to pass an external bunyan logger when creating a Server or Agent
* [Blog post](http://gabor.molnar.es/blog/2013/08/27/gsoc-week-number-10/)
* [Tarball](https://github.com/molnarg/node-http2/archive/node-http2-0.3.0.tar.gz)

### 0.2.1 (2013-08-20) ###

* Fixing a flow control bug
* [Tarball](https://github.com/molnarg/node-http2/archive/node-http2-0.2.1.tar.gz)

### 0.2.0 (2013-08-19) ###

* Exposing server push in the public API
* Connection pooling when operating as client
* Much better API compatibility with the standard node.js HTTPS module
* Logging improvements
* [Blog post](http://gabor.molnar.es/blog/2013/08/19/gsoc-week-number-9/)
* [Tarball](https://github.com/molnarg/node-http2/archive/node-http2-0.2.0.tar.gz)

### 0.1.1 (2013-08-12) ###

* Lots of bugfixes
* Proper flow control for outgoing frames
* Basic flow control for incoming frames
* [Blog post](http://gabor.molnar.es/blog/2013/08/12/gsoc-week-number-8/)
* [Tarball](https://github.com/molnarg/node-http2/archive/node-http2-0.1.1.tar.gz)

### 0.1.0 (2013-08-06) ###

* First release with public API (similar to the standard node HTTPS module)
* Support for NPN negotiation (no ALPN or Upgrade yet)
* Stream number limitation is in place
* Push streams works but not exposed yet in the public API
* [Blog post](http://gabor.molnar.es/blog/2013/08/05/gsoc-week-number-6-and-number-7/)
* [Tarball](https://github.com/molnarg/node-http2/archive/node-http2-0.1.0.tar.gz)

### 0.0.6 (2013-07-19) ###

* `Connection` and `Endpoint` classes are usable, but not yet ready
* Addition of an exmaple server and client
* Using [istanbul](https://github.com/gotwarlost/istanbul) for measuring code coverage
* [Blog post](http://gabor.molnar.es/blog/2013/07/19/gsoc-week-number-5/)
* [Tarball](https://github.com/molnarg/node-http2/archive/node-http2-0.0.6.tar.gz)

### 0.0.5 (2013-07-14) ###

* `Stream` class is done
* Public API stubs are in place
* [Blog post](http://gabor.molnar.es/blog/2013/07/14/gsoc-week-number-4/)
* [Tarball](https://github.com/molnarg/node-http2/archive/node-http2-0.0.5.tar.gz)

### 0.0.4 (2013-07-08) ###

* Added logging
* Started `Stream` class implementation
* [Blog post](http://gabor.molnar.es/blog/2013/07/08/gsoc-week-number-3/)
* [Tarball](https://github.com/molnarg/node-http2/archive/node-http2-0.0.4.tar.gz)

### 0.0.3 (2013-07-03) ###

* Header compression is ready
* [Blog post](http://gabor.molnar.es/blog/2013/07/03/the-http-slash-2-header-compression-implementation-of-node-http2/)
* [Tarball](https://github.com/molnarg/node-http2/archive/node-http2-0.0.3.tar.gz)

### 0.0.2 (2013-07-01) ###

* Frame serialization and deserialization ready and updated to match the newest spec
* Header compression implementation started
* [Blog post](http://gabor.molnar.es/blog/2013/07/01/gsoc-week-number-2/)
* [Tarball](https://github.com/molnarg/node-http2/archive/node-http2-0.0.2.tar.gz)

### 0.0.1 (2013-06-23) ###

* Frame serialization and deserialization largely done
* [Blog post](http://gabor.molnar.es/blog/2013/06/23/gsoc-week-number-1/)
* [Tarball](https://github.com/molnarg/node-http2/archive/node-http2-0.0.1.tar.gz)
