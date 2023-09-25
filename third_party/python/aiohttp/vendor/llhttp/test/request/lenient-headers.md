Lenient header value parsing
============================

Parsing with header value token checks off.

## Header value with lenient

<!-- meta={"type": "request-lenient-headers"} -->
```http
GET /url HTTP/1.1
Header1: \f


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=4 span[url]="/url"
off=9 url complete
off=14 len=3 span[version]="1.1"
off=17 version complete
off=19 len=7 span[header_field]="Header1"
off=27 header_field complete
off=28 len=1 span[header_value]="\f"
off=31 header_value complete
off=33 headers complete method=1 v=1/1 flags=0 content_length=0
off=33 message complete
```

## Second request header value with lenient

<!-- meta={"type": "request-lenient-headers"} -->
```http
GET /url HTTP/1.1
Header1: Okay


GET /url HTTP/1.1
Header1: \f


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=4 span[url]="/url"
off=9 url complete
off=14 len=3 span[version]="1.1"
off=17 version complete
off=19 len=7 span[header_field]="Header1"
off=27 header_field complete
off=28 len=4 span[header_value]="Okay"
off=34 header_value complete
off=36 headers complete method=1 v=1/1 flags=0 content_length=0
off=36 message complete
off=38 reset
off=38 message begin
off=38 len=3 span[method]="GET"
off=41 method complete
off=42 len=4 span[url]="/url"
off=47 url complete
off=52 len=3 span[version]="1.1"
off=55 version complete
off=57 len=7 span[header_field]="Header1"
off=65 header_field complete
off=66 len=1 span[header_value]="\f"
off=69 header_value complete
off=71 headers complete method=1 v=1/1 flags=0 content_length=0
off=71 message complete
```

## Header value without lenient

<!-- meta={"type": "request"} -->
```http
GET /url HTTP/1.1
Header1: \f



```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=4 span[url]="/url"
off=9 url complete
off=14 len=3 span[version]="1.1"
off=17 version complete
off=19 len=7 span[header_field]="Header1"
off=27 header_field complete
off=28 len=0 span[header_value]=""
off=28 error code=10 reason="Invalid header value char"
```

### Empty headers separated by CR (Lenient)

<!-- meta={"type": "request-lenient-headers"} -->
```http
POST / HTTP/1.1
Connection: Close
Host: localhost:5000
x:\rTransfer-Encoding: chunked

1
A
0

```

```log
off=0 message begin
off=0 len=4 span[method]="POST"
off=4 method complete
off=5 len=1 span[url]="/"
off=7 url complete
off=12 len=3 span[version]="1.1"
off=15 version complete
off=17 len=10 span[header_field]="Connection"
off=28 header_field complete
off=29 len=5 span[header_value]="Close"
off=36 header_value complete
off=36 len=4 span[header_field]="Host"
off=41 header_field complete
off=42 len=14 span[header_value]="localhost:5000"
off=58 header_value complete
off=58 len=1 span[header_field]="x"
off=60 header_field complete
off=61 len=0 span[header_value]=""
off=61 header_value complete
off=61 len=17 span[header_field]="Transfer-Encoding"
off=79 header_field complete
off=80 len=7 span[header_value]="chunked"
off=89 header_value complete
off=91 headers complete method=3 v=1/1 flags=20a content_length=0
off=94 chunk header len=1
off=94 len=1 span[body]="A"
off=97 chunk complete
off=100 chunk header len=0
```