Invalid responses
=================

### Incomplete HTTP protocol

*TODO(indutny): test `req_or_res` mode too*

<!-- meta={"type": "response-only"} -->
```http
HTP/1.1 200 OK


```

```log
off=0 message begin
off=2 error code=8 reason="Expected HTTP/"
```

### Extra digit in HTTP major version

<!-- meta={"type": "response"} -->
```http
HTTP/01.1 200 OK


```

```log
off=0 message begin
off=5 len=1 span[version]="0"
off=6 error code=9 reason="Expected dot"
```

### Extra digit in HTTP major version #2

<!-- meta={"type": "response"} -->
```http
HTTP/11.1 200 OK


```

```log
off=0 message begin
off=5 len=1 span[version]="1"
off=6 error code=9 reason="Expected dot"
```

### Extra digit in HTTP minor version

<!-- meta={"type": "response"} -->
```http
HTTP/1.01 200 OK


```

```log
off=0 message begin
off=5 len=3 span[version]="1.0"
off=8 version complete
off=8 error code=9 reason="Expected space after version"
```
-->

### Tab after HTTP version

<!-- meta={"type": "response"} -->
```http
HTTP/1.1\t200 OK


```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=8 error code=9 reason="Expected space after version"
```

### CR before response and tab after HTTP version

<!-- meta={"type": "response"} -->
```http
\rHTTP/1.1\t200 OK


```

```log
off=1 message begin
off=6 len=3 span[version]="1.1"
off=9 version complete
off=9 error code=9 reason="Expected space after version"
```

### Headers separated by CR

<!-- meta={"type": "response"} -->
```http
HTTP/1.1 200 OK
Foo: 1\rBar: 2


```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=3 span[header_field]="Foo"
off=21 header_field complete
off=22 len=1 span[header_value]="1"
off=24 error code=3 reason="Missing expected LF after header value"
```

### Invalid HTTP version

<!-- meta={"type": "response"} -->
```http
HTTP/5.6 200 OK


```

```log
off=0 message begin
off=5 len=3 span[version]="5.6"
off=8 error code=9 reason="Invalid HTTP version"
```

## Invalid space after start line

<!-- meta={"type": "response"} -->
```http
HTTP/1.1 200 OK
 Host: foo
```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=18 error code=30 reason="Unexpected space after start line"
```

### Extra space between HTTP version and status code

<!-- meta={"type": "response-only"} -->
```http
HTTP/1.1  200 OK


```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=9 error code=13 reason="Invalid status code"
```

### Extra space between status code and reason

<!-- meta={"type": "response-only"} -->
```http
HTTP/1.1 200  OK


```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=3 span[status]=" OK"
off=18 status complete
off=20 headers complete status=200 v=1/1 flags=0 content_length=0
```

### One-digit status code

<!-- meta={"type": "response-only"} -->
```http
HTTP/1.1 2 OK


```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=10 error code=13 reason="Invalid status code"
```