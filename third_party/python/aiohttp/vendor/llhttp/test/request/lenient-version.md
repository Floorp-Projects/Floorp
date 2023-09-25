Lenient HTTP version parsing
============================

### Invalid HTTP version with lenient

<!-- meta={"type": "request-lenient-version"} -->
```http
GET / HTTP/5.6


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=1 span[url]="/"
off=6 url complete
off=11 len=3 span[version]="5.6"
off=14 version complete
off=18 headers complete method=1 v=5/6 flags=0 content_length=0
off=18 message complete
```