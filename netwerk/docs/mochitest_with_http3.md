# Introduction to Mochitest framework with HTTP/2 and HTTP/3 support

The Mochitest framework currently utilizes [httpd.js](https://searchfox.org/mozilla-central/source/netwerk/test/httpserver/httpd.js) as its primary HTTP server, which only provides support for HTTP/1.1. To boost our testing capacity for HTTP/2 and HTTP/3 within necko, we improved the Mochitest framework to enable Firefox to connect to the test server using HTTP/2 or HTTP/3 while running Mochitest files.

## Mochitest Framework Server Setup for HTTP/1.1
As the diagram below, there is a back-end HTTP server running at `http://127.0.0.1:8888`. To ensure that Firefox can access multiple origins using a single HTTP server, the Mochitest framework employs proxy autoconfig (PAC). This [PAC script](https://searchfox.org/mozilla-central/rev/986024d59bff59819a3ed2f7c1d0f5254cdc3f3d/testing/mozbase/mozprofile/mozprofile/permissions.py#282-326) ensures that all plain HTTP connections are proxied to `127.0.0.1:8888`.

When it comes to HTTPS connections, the Mochitest framework incorporates an additional SSL proxy between the HTTP server and the browser. Initially, Firefox sends a CONNECT request to the proxy to establish the tunnel. Upon successful setup, the proxy proceeds to relay data to the server.

```{mermaid}
graph LR
    A[Firefox] -->|Request| B[SSL Proxy]
    B -->|Request| C["Back-end Server (127.0.0.1:8888)<br/>Handles *.sjs, *.html, *.jpg, ..."]
    C -->|Response| B
    B -->|Response| A
```

## Mochitest Framework Server Setup for HTTP/2 and HTTP/3

The diagram below depicts the architecture for HTTP/2 and HTTP/3.

```{mermaid}
graph LR
    A[Firefox] -->|Request| B[Reverse Proxy]
    B -->|Request| C["Back-end Server (127.0.0.1:8888)<br/>Handles *.sjs, *.html, *.jpg, ..."]
    C -->|Response| B
    B -->|Response| A
    A -->|DNS lookup| D[DoH Server]
    D -->|DNS response| A
```

### Back-end Server

This is the same as the existing httpd.js.

### Reverse Proxy

Our reverse proxy, positioned in front of the back-end server, intercepts Firefox requests. Acting as the gateway for Firefox's HTTP/2 or HTTP/3 connections, the reverse proxy accepts these requests and translates them into HTTP/1.1 format before forwarding to the back-end server. Upon receiving a response from the back-end server, the reverse proxy subsequently relays this response back to Firefox.

### DoH Server

In order to route HTTP requests to the reverse proxy server, we’ll need a DoH server to be configured. The DoH server should return 127.0.0.1 for every A/AAAA DNS lookup.
Moreover, the DoH server will also return an HTTPS RR for two reasons below:
With the port information provided in the HTTPS RR, we can map all different port numbers in server-locations.txt to the port number that is used by the reverse proxy.
With the “alpn” defined in the HTTPS RR, Firefox will automatically perform HTTPS upgrade and establish HTTP/2 or HTTP/3 connection to the reverse proxy server.

### How to run test with HTTP/2 or HTTP/3 locally

To execute tests with HTTP/2, include the `--use-http2-server` option. Here's an example:

```
./mach mochitest --use-http2-server PATH_TO_TEST_FILE
```

For HTTP/3 testing, switch the option to `--use-http3-server`. Like this:

```
./mach mochitest --use-http3-server PATH_TO_TEST_FILE
```

### Reasons for skipped tests

We have several tests that are currently failing with HTTP/2 and HTTP/3 servers and they are skipped for now. There are a few reasons contributing to these failures:

1. **Unexpected HTTPS upgrade**

   HTTP/2 and HTTP/3 only support HTTPS, which has led us to upgrade all plain HTTP requests to HTTPS without exception. This change has caused some tests to fail because they expected the scheme to remain HTTP.
   For example, this [test](https://searchfox.org/mozilla-central/rev/d31e56f7b3c2c18b8071a7b2a2fb6b4e01e3d3e8/netwerk/test/mochitests/file_domain_hierarchy_inner.html#8) expects the receiver's origin to be ``http://mochi.test:8888``.

2. **Lack of Server Support for Some Features**

   One example is that the HTTP/3 server doesn't support websocket for now, so tests in ``dom/websocket/test`` are all skipped.

3. **Incompatibility with HTTPS**

   Some tests were not designed to run with HTTPS. For these tests, skipping them is our only option.
