Pipelining
==========

## Should parse multiple events

<!-- meta={"type": "request"} -->
```http
POST /aaa HTTP/1.1
Content-Length: 3

AAA
PUT /bbb HTTP/1.1
Content-Length: 4

BBBB
PATCH /ccc HTTP/1.1
Content-Length: 5

CCCC
```

```log
off=0 message begin
off=0 len=4 span[method]="POST"
off=4 method complete
off=5 len=4 span[url]="/aaa"
off=10 url complete
off=15 len=3 span[version]="1.1"
off=18 version complete
off=20 len=14 span[header_field]="Content-Length"
off=35 header_field complete
off=36 len=1 span[header_value]="3"
off=39 header_value complete
off=41 headers complete method=3 v=1/1 flags=20 content_length=3
off=41 len=3 span[body]="AAA"
off=44 message complete
off=46 reset
off=46 message begin
off=46 len=3 span[method]="PUT"
off=49 method complete
off=50 len=4 span[url]="/bbb"
off=55 url complete
off=60 len=3 span[version]="1.1"
off=63 version complete
off=65 len=14 span[header_field]="Content-Length"
off=80 header_field complete
off=81 len=1 span[header_value]="4"
off=84 header_value complete
off=86 headers complete method=4 v=1/1 flags=20 content_length=4
off=86 len=4 span[body]="BBBB"
off=90 message complete
off=92 reset
off=92 message begin
off=92 len=5 span[method]="PATCH"
off=97 method complete
off=98 len=4 span[url]="/ccc"
off=103 url complete
off=108 len=3 span[version]="1.1"
off=111 version complete
off=113 len=14 span[header_field]="Content-Length"
off=128 header_field complete
off=129 len=1 span[header_value]="5"
off=132 header_value complete
off=134 headers complete method=28 v=1/1 flags=20 content_length=5
off=134 len=4 span[body]="CCCC"
```