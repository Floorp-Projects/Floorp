Content-Length header
=====================

## `Content-Length` with zeroes

<!-- meta={"type": "request"} -->
```http
PUT /url HTTP/1.1
Content-Length: 003

abc
```

```log
off=0 message begin
off=0 len=3 span[method]="PUT"
off=3 method complete
off=4 len=4 span[url]="/url"
off=9 url complete
off=14 len=3 span[version]="1.1"
off=17 version complete
off=19 len=14 span[header_field]="Content-Length"
off=34 header_field complete
off=35 len=3 span[header_value]="003"
off=40 header_value complete
off=42 headers complete method=4 v=1/1 flags=20 content_length=3
off=42 len=3 span[body]="abc"
off=45 message complete
```

## `Content-Length` with follow-up headers

The way the parser works is that special headers (like `Content-Length`) first
set `header_state` to appropriate value, and then apply custom parsing using
that value. For `Content-Length`, in particular, the `header_state` is used for
setting the flag too.

Make sure that `header_state` is reset to `0`, so that the flag won't be
attempted to set twice (and error).

<!-- meta={"type": "request"} -->
```http
PUT /url HTTP/1.1
Content-Length: 003
Ohai: world

abc
```

```log
off=0 message begin
off=0 len=3 span[method]="PUT"
off=3 method complete
off=4 len=4 span[url]="/url"
off=9 url complete
off=14 len=3 span[version]="1.1"
off=17 version complete
off=19 len=14 span[header_field]="Content-Length"
off=34 header_field complete
off=35 len=3 span[header_value]="003"
off=40 header_value complete
off=40 len=4 span[header_field]="Ohai"
off=45 header_field complete
off=46 len=5 span[header_value]="world"
off=53 header_value complete
off=55 headers complete method=4 v=1/1 flags=20 content_length=3
off=55 len=3 span[body]="abc"
off=58 message complete
```

## Error on `Content-Length` overflow

<!-- meta={"type": "request"} -->
```http
PUT /url HTTP/1.1
Content-Length: 1000000000000000000000

```

```log
off=0 message begin
off=0 len=3 span[method]="PUT"
off=3 method complete
off=4 len=4 span[url]="/url"
off=9 url complete
off=14 len=3 span[version]="1.1"
off=17 version complete
off=19 len=14 span[header_field]="Content-Length"
off=34 header_field complete
off=35 len=21 span[header_value]="100000000000000000000"
off=56 error code=11 reason="Content-Length overflow"
```

## Error on duplicate `Content-Length`

<!-- meta={"type": "request"} -->
```http
PUT /url HTTP/1.1
Content-Length: 1
Content-Length: 2

```

```log
off=0 message begin
off=0 len=3 span[method]="PUT"
off=3 method complete
off=4 len=4 span[url]="/url"
off=9 url complete
off=14 len=3 span[version]="1.1"
off=17 version complete
off=19 len=14 span[header_field]="Content-Length"
off=34 header_field complete
off=35 len=1 span[header_value]="1"
off=38 header_value complete
off=38 len=14 span[header_field]="Content-Length"
off=53 header_field complete
off=54 error code=4 reason="Duplicate Content-Length"
```

## Error on simultaneous `Content-Length` and `Transfer-Encoding: identity`

<!-- meta={"type": "request"} -->
```http
PUT /url HTTP/1.1
Content-Length: 1
Transfer-Encoding: identity


```

```log
off=0 message begin
off=0 len=3 span[method]="PUT"
off=3 method complete
off=4 len=4 span[url]="/url"
off=9 url complete
off=14 len=3 span[version]="1.1"
off=17 version complete
off=19 len=14 span[header_field]="Content-Length"
off=34 header_field complete
off=35 len=1 span[header_value]="1"
off=38 header_value complete
off=38 len=17 span[header_field]="Transfer-Encoding"
off=56 header_field complete
off=57 len=8 span[header_value]="identity"
off=67 header_value complete
off=69 error code=4 reason="Content-Length can't be present with Transfer-Encoding"
```

## Invalid whitespace token with `Content-Length` header field

<!-- meta={"type": "request"} -->
```http
PUT /url HTTP/1.1
Connection: upgrade
Content-Length : 4
Upgrade: ws

abcdefgh
```

```log
off=0 message begin
off=0 len=3 span[method]="PUT"
off=3 method complete
off=4 len=4 span[url]="/url"
off=9 url complete
off=14 len=3 span[version]="1.1"
off=17 version complete
off=19 len=10 span[header_field]="Connection"
off=30 header_field complete
off=31 len=7 span[header_value]="upgrade"
off=40 header_value complete
off=40 len=14 span[header_field]="Content-Length"
off=55 error code=10 reason="Invalid header field char"
```

## Invalid whitespace token with `Content-Length` header field (lenient)

<!-- meta={"type": "request-lenient-headers"} -->
```http
PUT /url HTTP/1.1
Connection: upgrade
Content-Length : 4
Upgrade: ws

abcdefgh
```

```log
off=0 message begin
off=0 len=3 span[method]="PUT"
off=3 method complete
off=4 len=4 span[url]="/url"
off=9 url complete
off=14 len=3 span[version]="1.1"
off=17 version complete
off=19 len=10 span[header_field]="Connection"
off=30 header_field complete
off=31 len=7 span[header_value]="upgrade"
off=40 header_value complete
off=40 len=15 span[header_field]="Content-Length "
off=56 header_field complete
off=57 len=1 span[header_value]="4"
off=60 header_value complete
off=60 len=7 span[header_field]="Upgrade"
off=68 header_field complete
off=69 len=2 span[header_value]="ws"
off=73 header_value complete
off=75 headers complete method=4 v=1/1 flags=34 content_length=4
off=75 len=4 span[body]="abcd"
off=79 message complete
off=79 error code=22 reason="Pause on CONNECT/Upgrade"
```

## No error on simultaneous `Content-Length` and `Transfer-Encoding: identity` (lenient)

<!-- meta={"type": "request-lenient-chunked-length"} -->
```http
PUT /url HTTP/1.1
Content-Length: 1
Transfer-Encoding: chunked


```

```log
off=0 message begin
off=0 len=3 span[method]="PUT"
off=3 method complete
off=4 len=4 span[url]="/url"
off=9 url complete
off=14 len=3 span[version]="1.1"
off=17 version complete
off=19 len=14 span[header_field]="Content-Length"
off=34 header_field complete
off=35 len=1 span[header_value]="1"
off=38 header_value complete
off=38 len=17 span[header_field]="Transfer-Encoding"
off=56 header_field complete
off=57 len=7 span[header_value]="chunked"
off=66 header_value complete
off=68 headers complete method=4 v=1/1 flags=228 content_length=1
```

## Funky `Content-Length` with body

<!-- meta={"type": "request"} -->
```http
GET /get_funky_content_length_body_hello HTTP/1.0
conTENT-Length: 5

HELLO
```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=36 span[url]="/get_funky_content_length_body_hello"
off=41 url complete
off=46 len=3 span[version]="1.0"
off=49 version complete
off=51 len=14 span[header_field]="conTENT-Length"
off=66 header_field complete
off=67 len=1 span[header_value]="5"
off=70 header_value complete
off=72 headers complete method=1 v=1/0 flags=20 content_length=5
off=72 len=5 span[body]="HELLO"
off=77 message complete
```

## Spaces in `Content-Length` (surrounding)

<!-- meta={"type": "request"} -->
```http
POST / HTTP/1.1
Content-Length:  42 


```

```log
off=0 message begin
off=0 len=4 span[method]="POST"
off=4 method complete
off=5 len=1 span[url]="/"
off=7 url complete
off=12 len=3 span[version]="1.1"
off=15 version complete
off=17 len=14 span[header_field]="Content-Length"
off=32 header_field complete
off=34 len=3 span[header_value]="42 "
off=39 header_value complete
off=41 headers complete method=3 v=1/1 flags=20 content_length=42
```

### Spaces in `Content-Length` #2

<!-- meta={"type": "request"} -->
```http
POST / HTTP/1.1
Content-Length: 4 2


```

```log
off=0 message begin
off=0 len=4 span[method]="POST"
off=4 method complete
off=5 len=1 span[url]="/"
off=7 url complete
off=12 len=3 span[version]="1.1"
off=15 version complete
off=17 len=14 span[header_field]="Content-Length"
off=32 header_field complete
off=33 len=2 span[header_value]="4 "
off=35 error code=11 reason="Invalid character in Content-Length"
```

### Spaces in `Content-Length` #3

<!-- meta={"type": "request"} -->
```http
POST / HTTP/1.1
Content-Length: 13 37


```

```log
off=0 message begin
off=0 len=4 span[method]="POST"
off=4 method complete
off=5 len=1 span[url]="/"
off=7 url complete
off=12 len=3 span[version]="1.1"
off=15 version complete
off=17 len=14 span[header_field]="Content-Length"
off=32 header_field complete
off=33 len=3 span[header_value]="13 "
off=36 error code=11 reason="Invalid character in Content-Length"
```

### Empty `Content-Length`

<!-- meta={"type": "request"} -->
```http
POST / HTTP/1.1
Content-Length:


```

```log
off=0 message begin
off=0 len=4 span[method]="POST"
off=4 method complete
off=5 len=1 span[url]="/"
off=7 url complete
off=12 len=3 span[version]="1.1"
off=15 version complete
off=17 len=14 span[header_field]="Content-Length"
off=32 header_field complete
off=34 error code=11 reason="Empty Content-Length"
```

## `Content-Length` with CR instead of dash

<!-- meta={"type": "request", "noScan": true} -->
```http
PUT /url HTTP/1.1
Content\rLength: 003

abc
```

```log
off=0 message begin
off=0 len=3 span[method]="PUT"
off=3 method complete
off=4 len=4 span[url]="/url"
off=9 url complete
off=14 len=3 span[version]="1.1"
off=17 version complete
off=26 error code=10 reason="Invalid header token"
```

## Content-Length reset when no body is received

<!-- meta={"type": "request", "skipBody": true} -->
```http
PUT /url HTTP/1.1
Content-Length: 123

POST /url HTTP/1.1
Content-Length: 456


```

```log
off=0 message begin
off=0 len=3 span[method]="PUT"
off=3 method complete
off=4 len=4 span[url]="/url"
off=9 url complete
off=14 len=3 span[version]="1.1"
off=17 version complete
off=19 len=14 span[header_field]="Content-Length"
off=34 header_field complete
off=35 len=3 span[header_value]="123"
off=40 header_value complete
off=42 headers complete method=4 v=1/1 flags=20 content_length=123
off=42 skip body
off=42 message complete
off=42 reset
off=42 message begin
off=42 len=4 span[method]="POST"
off=46 method complete
off=47 len=4 span[url]="/url"
off=52 url complete
off=57 len=3 span[version]="1.1"
off=60 version complete
off=62 len=14 span[header_field]="Content-Length"
off=77 header_field complete
off=78 len=3 span[header_value]="456"
off=83 header_value complete
off=85 headers complete method=3 v=1/1 flags=20 content_length=456
off=85 skip body
off=85 message complete
```
