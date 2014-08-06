// [node-http2][homepage] is an [HTTP/2 (draft 14)][http2] implementation for [node.js][node].
//
// The core of the protocol is implemented by the [http2-protocol] module. This module provides
// two important features on top of http2-protocol:
//
// * Implementation of different negotiation schemes that can be used to start a HTTP2 connection.
//   These include TLS ALPN, Upgrade and Plain TCP.
//
// * Providing an API very similar to the standard node.js [HTTPS module API][node-https]
//   (which is in turn very similar to the [HTTP module API][node-http]).
//
// [homepage]:            https://github.com/molnarg/node-http2
// [http2-protocol]:      https://github.com/molnarg/node-http2-protocol
// [http2]:               http://tools.ietf.org/html/draft-ietf-httpbis-http2-14
// [node]:                http://nodejs.org/
// [node-https]:          http://nodejs.org/api/https.html
// [node-http]:           http://nodejs.org/api/http.html

module.exports   = require('./http');

/*
                  HTTP API

               |            ^
               |            |
 +-------------|------------|------------------------------------------------------+
 |             |            |        Server/Agent                                  |
 |             v            |                                                      |
 |        +----------+ +----------+                                                |
 |        | Outgoing | | Incoming |                                                |
 |        | req/res. | | req/res. |                                                |
 |        +----------+ +----------+                                                |
 |             |            ^                                                      |
 |             |            |                                                      |
 |   +---------|------------|-------------------------------------+   +-----       |
 |   |         |            |   Endpoint                          |   |            |
 |   |         |            |                                     |   |            |
 |   |         v            |                                     |   |            |
 |   |    +-----------------------+  +--------------------        |   |            |
 |   |    |        Stream         |  |         Stream      ...    |   |            |
 |   |    +-----------------------+  +--------------------        |   |            |
 |   |                                                            |   |            |
 |   +------------------------------------------------------------+   +-----       |
 |                             |        |                                          |
 |                             |        |                                          |
 |                             v        |                                          |
 |   +------------------------------------------------------------+   +-----       |
 |   |                         TCP stream                         |   |      ...   |
 |   +------------------------------------------------------------+   +-----       |
 |                                                                                 |
 +---------------------------------------------------------------------------------+

*/
