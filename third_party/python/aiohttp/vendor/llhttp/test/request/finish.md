Finish
======

Those tests check the return codes and the behavior of `llhttp_finish()` C API.

## It should be safe to finish after GET request

<!-- meta={"type": "request-finish"} -->
```http
GET / HTTP/1.1


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=1 span[url]="/"
off=6 url complete
off=11 len=3 span[version]="1.1"
off=14 version complete
off=18 headers complete method=1 v=1/1 flags=0 content_length=0
off=18 message complete
off=NULL finish=0
```

## It should be unsafe to finish after incomplete PUT request

<!-- meta={"type": "request-finish"} -->
```http
PUT / HTTP/1.1
Content-Length: 100

```

```log
off=0 message begin
off=0 len=3 span[method]="PUT"
off=3 method complete
off=4 len=1 span[url]="/"
off=6 url complete
off=11 len=3 span[version]="1.1"
off=14 version complete
off=16 len=14 span[header_field]="Content-Length"
off=31 header_field complete
off=32 len=3 span[header_value]="100"
off=NULL finish=2
```

## It should be unsafe to finish inside of the header

<!-- meta={"type": "request-finish"} -->
```http
PUT / HTTP/1.1
Content-Leng
```

```log
off=0 message begin
off=0 len=3 span[method]="PUT"
off=3 method complete
off=4 len=1 span[url]="/"
off=6 url complete
off=11 len=3 span[version]="1.1"
off=14 version complete
off=16 len=12 span[header_field]="Content-Leng"
off=NULL finish=2
```
