# Binary HTTP Messages

This is a rust implementation of [Binary HTTP
Messages](https://httpwg.org/http-extensions/draft-ietf-httpbis-binary-message.html).

This work is undergoing active revision in the IETF and so are these
implementations.  Use at your own risk.

## Using

The API documentation is currently sparse, but the API is fairly small and
descriptive.

The `bhttp` crate has the following features:

- `read-bhttp` enables parsing of binary HTTP messages.  This is enabled by
  default.

- `write-bhttp` enables writing of binary HTTP messages.  This is enabled by
  default.

- `read-http` enables a simple HTTP/1.1 message parser.  This parser is fairly
  basic and is not recommended for production use.  Getting an HTTP/1.1 parser
  right is a massive enterprise; this one only does the basics.  This is
  disabled by default.

- `write-http` enables writing of HTTP/1.1 messages.  This is disabled by
  default.
