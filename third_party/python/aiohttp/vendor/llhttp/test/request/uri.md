URI
===

## Quotes in URI

<!-- meta={"type": "request"} -->
```http
GET /with_"lovely"_quotes?foo=\"bar\" HTTP/1.1


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=33 span[url]="/with_"lovely"_quotes?foo=\"bar\""
off=38 url complete
off=43 len=3 span[version]="1.1"
off=46 version complete
off=50 headers complete method=1 v=1/1 flags=0 content_length=0
off=50 message complete
```

## Query URL with question mark

Some clients include `?` characters in query strings.

<!-- meta={"type": "request"} -->
```http
GET /test.cgi?foo=bar?baz HTTP/1.1


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=21 span[url]="/test.cgi?foo=bar?baz"
off=26 url complete
off=31 len=3 span[version]="1.1"
off=34 version complete
off=38 headers complete method=1 v=1/1 flags=0 content_length=0
off=38 message complete
```

## Host terminated by a query string

<!-- meta={"type": "request"} -->
```http
GET http://hypnotoad.org?hail=all HTTP/1.1\r\n


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=29 span[url]="http://hypnotoad.org?hail=all"
off=34 url complete
off=39 len=3 span[version]="1.1"
off=42 version complete
off=46 headers complete method=1 v=1/1 flags=0 content_length=0
off=46 message complete
```

## `host:port` terminated by a query string

<!-- meta={"type": "request"} -->
```http
GET http://hypnotoad.org:1234?hail=all HTTP/1.1


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=34 span[url]="http://hypnotoad.org:1234?hail=all"
off=39 url complete
off=44 len=3 span[version]="1.1"
off=47 version complete
off=51 headers complete method=1 v=1/1 flags=0 content_length=0
off=51 message complete
```

## Query URL with vertical bar character

It should be allowed to have vertical bar symbol in URI: `|`.

See: https://github.com/nodejs/node/issues/27584

<!-- meta={"type": "request"} -->
```http
GET /test.cgi?query=| HTTP/1.1


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=17 span[url]="/test.cgi?query=|"
off=22 url complete
off=27 len=3 span[version]="1.1"
off=30 version complete
off=34 headers complete method=1 v=1/1 flags=0 content_length=0
off=34 message complete
```

## `host:port` terminated by a space

<!-- meta={"type": "request"} -->
```http
GET http://hypnotoad.org:1234 HTTP/1.1


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=25 span[url]="http://hypnotoad.org:1234"
off=30 url complete
off=35 len=3 span[version]="1.1"
off=38 version complete
off=42 headers complete method=1 v=1/1 flags=0 content_length=0
off=42 message complete
```

## UTF-8 in URI path in loose mode

<!-- meta={"type": "request", "mode": "loose", "noScan": true} -->
```http
GET /δ¶/δt/космос/pope?q=1#narf HTTP/1.1
Host: github.com


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=36 span[url]="/δ¶/δt/космос/pope?q=1#narf"
off=41 url complete
off=46 len=3 span[version]="1.1"
off=49 version complete
off=51 len=4 span[header_field]="Host"
off=56 header_field complete
off=57 len=10 span[header_value]="github.com"
off=69 header_value complete
off=71 headers complete method=1 v=1/1 flags=0 content_length=0
off=71 message complete
```

## Disallow UTF-8 in URI path in strict mode

<!-- meta={"type": "request", "mode": "strict", "noScan": true} -->
```http
GET /δ¶/δt/pope?q=1#narf HTTP/1.1
Host: github.com


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=5 error code=7 reason="Invalid char in url path"
```

## Fragment in URI

<!-- meta={"type": "request"} -->
```http
GET /forums/1/topics/2375?page=1#posts-17408 HTTP/1.1


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=40 span[url]="/forums/1/topics/2375?page=1#posts-17408"
off=45 url complete
off=50 len=3 span[version]="1.1"
off=53 version complete
off=57 headers complete method=1 v=1/1 flags=0 content_length=0
off=57 message complete
```

## Underscore in hostname

<!-- meta={"type": "request"} -->
```http
CONNECT home_0.netscape.com:443 HTTP/1.0
User-agent: Mozilla/1.1N
Proxy-authorization: basic aGVsbG86d29ybGQ=


```

```log
off=0 message begin
off=0 len=7 span[method]="CONNECT"
off=7 method complete
off=8 len=23 span[url]="home_0.netscape.com:443"
off=32 url complete
off=37 len=3 span[version]="1.0"
off=40 version complete
off=42 len=10 span[header_field]="User-agent"
off=53 header_field complete
off=54 len=12 span[header_value]="Mozilla/1.1N"
off=68 header_value complete
off=68 len=19 span[header_field]="Proxy-authorization"
off=88 header_field complete
off=89 len=22 span[header_value]="basic aGVsbG86d29ybGQ="
off=113 header_value complete
off=115 headers complete method=5 v=1/0 flags=0 content_length=0
off=115 message complete
off=115 error code=22 reason="Pause on CONNECT/Upgrade"
```

## `host:port` and basic auth

<!-- meta={"type": "request"} -->
```http
GET http://a%12:b!&*$@hypnotoad.org:1234/toto HTTP/1.1


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=41 span[url]="http://a%12:b!&*$@hypnotoad.org:1234/toto"
off=46 url complete
off=51 len=3 span[version]="1.1"
off=54 version complete
off=58 headers complete method=1 v=1/1 flags=0 content_length=0
off=58 message complete
```

## Space in URI

<!-- meta={"type": "request", "noScan": true} -->
```http
GET /foo bar/ HTTP/1.1


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=4 span[url]="/foo"
off=9 url complete
off=9 error code=8 reason="Expected HTTP/"
```
