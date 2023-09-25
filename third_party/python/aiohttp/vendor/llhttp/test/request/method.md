Methods
=======

### REPORT request

<!-- meta={"type": "request"} -->
```http
REPORT /test HTTP/1.1


```

```log
off=0 message begin
off=0 len=6 span[method]="REPORT"
off=6 method complete
off=7 len=5 span[url]="/test"
off=13 url complete
off=18 len=3 span[version]="1.1"
off=21 version complete
off=25 headers complete method=20 v=1/1 flags=0 content_length=0
off=25 message complete
```

### CONNECT request

<!-- meta={"type": "request"} -->
```http
CONNECT 0-home0.netscape.com:443 HTTP/1.0
User-agent: Mozilla/1.1N
Proxy-authorization: basic aGVsbG86d29ybGQ=

some data
and yet even more data
```

```log
off=0 message begin
off=0 len=7 span[method]="CONNECT"
off=7 method complete
off=8 len=24 span[url]="0-home0.netscape.com:443"
off=33 url complete
off=38 len=3 span[version]="1.0"
off=41 version complete
off=43 len=10 span[header_field]="User-agent"
off=54 header_field complete
off=55 len=12 span[header_value]="Mozilla/1.1N"
off=69 header_value complete
off=69 len=19 span[header_field]="Proxy-authorization"
off=89 header_field complete
off=90 len=22 span[header_value]="basic aGVsbG86d29ybGQ="
off=114 header_value complete
off=116 headers complete method=5 v=1/0 flags=0 content_length=0
off=116 message complete
off=116 error code=22 reason="Pause on CONNECT/Upgrade"
```

### CONNECT request with CAPS

<!-- meta={"type": "request"} -->
```http
CONNECT HOME0.NETSCAPE.COM:443 HTTP/1.0
User-agent: Mozilla/1.1N
Proxy-authorization: basic aGVsbG86d29ybGQ=


```

```log
off=0 message begin
off=0 len=7 span[method]="CONNECT"
off=7 method complete
off=8 len=22 span[url]="HOME0.NETSCAPE.COM:443"
off=31 url complete
off=36 len=3 span[version]="1.0"
off=39 version complete
off=41 len=10 span[header_field]="User-agent"
off=52 header_field complete
off=53 len=12 span[header_value]="Mozilla/1.1N"
off=67 header_value complete
off=67 len=19 span[header_field]="Proxy-authorization"
off=87 header_field complete
off=88 len=22 span[header_value]="basic aGVsbG86d29ybGQ="
off=112 header_value complete
off=114 headers complete method=5 v=1/0 flags=0 content_length=0
off=114 message complete
off=114 error code=22 reason="Pause on CONNECT/Upgrade"
```

### CONNECT with body

<!-- meta={"type": "request"} -->
```http
CONNECT foo.bar.com:443 HTTP/1.0
User-agent: Mozilla/1.1N
Proxy-authorization: basic aGVsbG86d29ybGQ=
Content-Length: 10

blarfcicle"
```

```log
off=0 message begin
off=0 len=7 span[method]="CONNECT"
off=7 method complete
off=8 len=15 span[url]="foo.bar.com:443"
off=24 url complete
off=29 len=3 span[version]="1.0"
off=32 version complete
off=34 len=10 span[header_field]="User-agent"
off=45 header_field complete
off=46 len=12 span[header_value]="Mozilla/1.1N"
off=60 header_value complete
off=60 len=19 span[header_field]="Proxy-authorization"
off=80 header_field complete
off=81 len=22 span[header_value]="basic aGVsbG86d29ybGQ="
off=105 header_value complete
off=105 len=14 span[header_field]="Content-Length"
off=120 header_field complete
off=121 len=2 span[header_value]="10"
off=125 header_value complete
off=127 headers complete method=5 v=1/0 flags=20 content_length=10
off=127 message complete
off=127 error code=22 reason="Pause on CONNECT/Upgrade"
```

### M-SEARCH request

<!-- meta={"type": "request"} -->
```http
M-SEARCH * HTTP/1.1
HOST: 239.255.255.250:1900
MAN: "ssdp:discover"
ST: "ssdp:all"


```

```log
off=0 message begin
off=0 len=8 span[method]="M-SEARCH"
off=8 method complete
off=9 len=1 span[url]="*"
off=11 url complete
off=16 len=3 span[version]="1.1"
off=19 version complete
off=21 len=4 span[header_field]="HOST"
off=26 header_field complete
off=27 len=20 span[header_value]="239.255.255.250:1900"
off=49 header_value complete
off=49 len=3 span[header_field]="MAN"
off=53 header_field complete
off=54 len=15 span[header_value]=""ssdp:discover""
off=71 header_value complete
off=71 len=2 span[header_field]="ST"
off=74 header_field complete
off=75 len=10 span[header_value]=""ssdp:all""
off=87 header_value complete
off=89 headers complete method=24 v=1/1 flags=0 content_length=0
off=89 message complete
```

### PATCH request

<!-- meta={"type": "request"} -->
```http
PATCH /file.txt HTTP/1.1
Host: www.example.com
Content-Type: application/example
If-Match: "e0023aa4e"
Content-Length: 10

cccccccccc
```

```log
off=0 message begin
off=0 len=5 span[method]="PATCH"
off=5 method complete
off=6 len=9 span[url]="/file.txt"
off=16 url complete
off=21 len=3 span[version]="1.1"
off=24 version complete
off=26 len=4 span[header_field]="Host"
off=31 header_field complete
off=32 len=15 span[header_value]="www.example.com"
off=49 header_value complete
off=49 len=12 span[header_field]="Content-Type"
off=62 header_field complete
off=63 len=19 span[header_value]="application/example"
off=84 header_value complete
off=84 len=8 span[header_field]="If-Match"
off=93 header_field complete
off=94 len=11 span[header_value]=""e0023aa4e""
off=107 header_value complete
off=107 len=14 span[header_field]="Content-Length"
off=122 header_field complete
off=123 len=2 span[header_value]="10"
off=127 header_value complete
off=129 headers complete method=28 v=1/1 flags=20 content_length=10
off=129 len=10 span[body]="cccccccccc"
off=139 message complete
```

### PURGE request

<!-- meta={"type": "request"} -->
```http
PURGE /file.txt HTTP/1.1
Host: www.example.com


```

```log
off=0 message begin
off=0 len=5 span[method]="PURGE"
off=5 method complete
off=6 len=9 span[url]="/file.txt"
off=16 url complete
off=21 len=3 span[version]="1.1"
off=24 version complete
off=26 len=4 span[header_field]="Host"
off=31 header_field complete
off=32 len=15 span[header_value]="www.example.com"
off=49 header_value complete
off=51 headers complete method=29 v=1/1 flags=0 content_length=0
off=51 message complete
```

### SEARCH request

<!-- meta={"type": "request"} -->
```http
SEARCH / HTTP/1.1
Host: www.example.com


```

```log
off=0 message begin
off=0 len=6 span[method]="SEARCH"
off=6 method complete
off=7 len=1 span[url]="/"
off=9 url complete
off=14 len=3 span[version]="1.1"
off=17 version complete
off=19 len=4 span[header_field]="Host"
off=24 header_field complete
off=25 len=15 span[header_value]="www.example.com"
off=42 header_value complete
off=44 headers complete method=14 v=1/1 flags=0 content_length=0
off=44 message complete
```

### LINK request

<!-- meta={"type": "request"} -->
```http
LINK /images/my_dog.jpg HTTP/1.1
Host: example.com
Link: <http://example.com/profiles/joe>; rel="tag"
Link: <http://example.com/profiles/sally>; rel="tag"


```

```log
off=0 message begin
off=0 len=4 span[method]="LINK"
off=4 method complete
off=5 len=18 span[url]="/images/my_dog.jpg"
off=24 url complete
off=29 len=3 span[version]="1.1"
off=32 version complete
off=34 len=4 span[header_field]="Host"
off=39 header_field complete
off=40 len=11 span[header_value]="example.com"
off=53 header_value complete
off=53 len=4 span[header_field]="Link"
off=58 header_field complete
off=59 len=44 span[header_value]="<http://example.com/profiles/joe>; rel="tag""
off=105 header_value complete
off=105 len=4 span[header_field]="Link"
off=110 header_field complete
off=111 len=46 span[header_value]="<http://example.com/profiles/sally>; rel="tag""
off=159 header_value complete
off=161 headers complete method=31 v=1/1 flags=0 content_length=0
off=161 message complete
```

### LINK request

<!-- meta={"type": "request"} -->
```http
UNLINK /images/my_dog.jpg HTTP/1.1
Host: example.com
Link: <http://example.com/profiles/sally>; rel="tag"


```

```log
off=0 message begin
off=0 len=6 span[method]="UNLINK"
off=6 method complete
off=7 len=18 span[url]="/images/my_dog.jpg"
off=26 url complete
off=31 len=3 span[version]="1.1"
off=34 version complete
off=36 len=4 span[header_field]="Host"
off=41 header_field complete
off=42 len=11 span[header_value]="example.com"
off=55 header_value complete
off=55 len=4 span[header_field]="Link"
off=60 header_field complete
off=61 len=46 span[header_value]="<http://example.com/profiles/sally>; rel="tag""
off=109 header_value complete
off=111 headers complete method=32 v=1/1 flags=0 content_length=0
off=111 message complete
```

### SOURCE request

<!-- meta={"type": "request"} -->
```http
SOURCE /music/sweet/music HTTP/1.1
Host: example.com


```

```log
off=0 message begin
off=0 len=6 span[method]="SOURCE"
off=6 method complete
off=7 len=18 span[url]="/music/sweet/music"
off=26 url complete
off=31 len=3 span[version]="1.1"
off=34 version complete
off=36 len=4 span[header_field]="Host"
off=41 header_field complete
off=42 len=11 span[header_value]="example.com"
off=55 header_value complete
off=57 headers complete method=33 v=1/1 flags=0 content_length=0
off=57 message complete
```

### SOURCE request with ICE

<!-- meta={"type": "request"} -->
```http
SOURCE /music/sweet/music ICE/1.0
Host: example.com


```

```log
off=0 message begin
off=0 len=6 span[method]="SOURCE"
off=6 method complete
off=7 len=18 span[url]="/music/sweet/music"
off=26 url complete
off=30 len=3 span[version]="1.0"
off=33 version complete
off=35 len=4 span[header_field]="Host"
off=40 header_field complete
off=41 len=11 span[header_value]="example.com"
off=54 header_value complete
off=56 headers complete method=33 v=1/0 flags=0 content_length=0
off=56 message complete
```

### OPTIONS request with RTSP

NOTE: `OPTIONS` is a valid HTTP metho too.

<!-- meta={"type": "request"} -->
```http
OPTIONS /music/sweet/music RTSP/1.0
Host: example.com


```

```log
off=0 message begin
off=0 len=7 span[method]="OPTIONS"
off=7 method complete
off=8 len=18 span[url]="/music/sweet/music"
off=27 url complete
off=32 len=3 span[version]="1.0"
off=35 version complete
off=37 len=4 span[header_field]="Host"
off=42 header_field complete
off=43 len=11 span[header_value]="example.com"
off=56 header_value complete
off=58 headers complete method=6 v=1/0 flags=0 content_length=0
off=58 message complete
```

### ANNOUNCE request with RTSP

<!-- meta={"type": "request"} -->
```http
ANNOUNCE /music/sweet/music RTSP/1.0
Host: example.com


```

```log
off=0 message begin
off=0 len=8 span[method]="ANNOUNCE"
off=8 method complete
off=9 len=18 span[url]="/music/sweet/music"
off=28 url complete
off=33 len=3 span[version]="1.0"
off=36 version complete
off=38 len=4 span[header_field]="Host"
off=43 header_field complete
off=44 len=11 span[header_value]="example.com"
off=57 header_value complete
off=59 headers complete method=36 v=1/0 flags=0 content_length=0
off=59 message complete
```

### PRI request HTTP2

<!-- meta={"type": "request"} -->
```http
PRI * HTTP/1.1

SM


```

```log
off=0 message begin
off=0 len=3 span[method]="PRI"
off=3 method complete
off=4 len=1 span[url]="*"
off=6 url complete
off=11 len=3 span[version]="1.1"
off=14 version complete
off=24 error code=23 reason="Pause on PRI/Upgrade"
```
