Lenient HTTP version parsing
============================

### Invalid HTTP version with lenient

<!-- meta={"type": "response-lenient-version"} -->
```http
HTTP/5.6 200 OK


```

```log
off=0 message begin
off=5 len=3 span[version]="5.6"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=19 headers complete status=200 v=5/6 flags=0 content_length=0
```
