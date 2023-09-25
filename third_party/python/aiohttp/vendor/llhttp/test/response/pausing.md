Pausing
=======

### on_message_begin

<!-- meta={"type": "response", "pause": "on_message_begin"} -->
```http
HTTP/1.1 200 OK
Content-Length: 3

abc
```

```log
off=0 message begin
off=0 pause
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=14 span[header_field]="Content-Length"
off=32 header_field complete
off=33 len=1 span[header_value]="3"
off=36 header_value complete
off=38 headers complete status=200 v=1/1 flags=20 content_length=3
off=38 len=3 span[body]="abc"
off=41 message complete
```

### on_message_complete

<!-- meta={"type": "response", "pause": "on_message_complete"} -->
```http
HTTP/1.1 200 OK
Content-Length: 3

abc
```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=14 span[header_field]="Content-Length"
off=32 header_field complete
off=33 len=1 span[header_value]="3"
off=36 header_value complete
off=38 headers complete status=200 v=1/1 flags=20 content_length=3
off=38 len=3 span[body]="abc"
off=41 message complete
off=41 pause
```

### on_version_complete

<!-- meta={"type": "response", "pause": "on_version_complete"} -->
```http
HTTP/1.1 200 OK
Content-Length: 3

abc
```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=8 pause
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=14 span[header_field]="Content-Length"
off=32 header_field complete
off=33 len=1 span[header_value]="3"
off=36 header_value complete
off=38 headers complete status=200 v=1/1 flags=20 content_length=3
off=38 len=3 span[body]="abc"
off=41 message complete
```

### on_status_complete

<!-- meta={"type": "response", "pause": "on_status_complete"} -->
```http
HTTP/1.1 200 OK
Content-Length: 3

abc
```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 pause
off=17 len=14 span[header_field]="Content-Length"
off=32 header_field complete
off=33 len=1 span[header_value]="3"
off=36 header_value complete
off=38 headers complete status=200 v=1/1 flags=20 content_length=3
off=38 len=3 span[body]="abc"
off=41 message complete
```

### on_header_field_complete

<!-- meta={"type": "response", "pause": "on_header_field_complete"} -->
```http
HTTP/1.1 200 OK
Content-Length: 3

abc
```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=14 span[header_field]="Content-Length"
off=32 header_field complete
off=32 pause
off=33 len=1 span[header_value]="3"
off=36 header_value complete
off=38 headers complete status=200 v=1/1 flags=20 content_length=3
off=38 len=3 span[body]="abc"
off=41 message complete
```

### on_header_value_complete

<!-- meta={"type": "response", "pause": "on_header_value_complete"} -->
```http
HTTP/1.1 200 OK
Content-Length: 3

abc
```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=14 span[header_field]="Content-Length"
off=32 header_field complete
off=33 len=1 span[header_value]="3"
off=36 header_value complete
off=36 pause
off=38 headers complete status=200 v=1/1 flags=20 content_length=3
off=38 len=3 span[body]="abc"
off=41 message complete
```

### on_headers_complete

<!-- meta={"type": "response", "pause": "on_headers_complete"} -->
```http
HTTP/1.1 200 OK
Content-Length: 3

abc
```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=14 span[header_field]="Content-Length"
off=32 header_field complete
off=33 len=1 span[header_value]="3"
off=36 header_value complete
off=38 headers complete status=200 v=1/1 flags=20 content_length=3
off=38 pause
off=38 len=3 span[body]="abc"
off=41 message complete
```

### on_chunk_header

<!-- meta={"type": "response", "pause": "on_chunk_header"} -->
```http
HTTP/1.1 200 OK
Transfer-Encoding: chunked

a
0123456789
0


```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=17 span[header_field]="Transfer-Encoding"
off=35 header_field complete
off=36 len=7 span[header_value]="chunked"
off=45 header_value complete
off=47 headers complete status=200 v=1/1 flags=208 content_length=0
off=50 chunk header len=10
off=50 pause
off=50 len=10 span[body]="0123456789"
off=62 chunk complete
off=65 chunk header len=0
off=65 pause
off=67 chunk complete
off=67 message complete
```

### on_chunk_extension_name

<!-- meta={"type": "response", "pause": "on_chunk_extension_name"} -->
```http
HTTP/1.1 200 OK
Transfer-Encoding: chunked

a;foo=bar
0123456789
0


```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=17 span[header_field]="Transfer-Encoding"
off=35 header_field complete
off=36 len=7 span[header_value]="chunked"
off=45 header_value complete
off=47 headers complete status=200 v=1/1 flags=208 content_length=0
off=49 len=3 span[chunk_extension_name]="foo"
off=53 chunk_extension_name complete
off=53 pause
off=53 len=3 span[chunk_extension_value]="bar"
off=57 chunk_extension_value complete
off=58 chunk header len=10
off=58 len=10 span[body]="0123456789"
off=70 chunk complete
off=73 chunk header len=0
off=75 chunk complete
off=75 message complete
```

### on_chunk_extension_value

<!-- meta={"type": "response", "pause": "on_chunk_extension_value"} -->
```http
HTTP/1.1 200 OK
Transfer-Encoding: chunked

a;foo=bar
0123456789
0


```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=17 span[header_field]="Transfer-Encoding"
off=35 header_field complete
off=36 len=7 span[header_value]="chunked"
off=45 header_value complete
off=47 headers complete status=200 v=1/1 flags=208 content_length=0
off=49 len=3 span[chunk_extension_name]="foo"
off=53 chunk_extension_name complete
off=53 len=3 span[chunk_extension_value]="bar"
off=57 chunk_extension_value complete
off=57 pause
off=58 chunk header len=10
off=58 len=10 span[body]="0123456789"
off=70 chunk complete
off=73 chunk header len=0
off=75 chunk complete
off=75 message complete
```

### on_chunk_complete

<!-- meta={"type": "response", "pause": "on_chunk_complete"} -->
```http
HTTP/1.1 200 OK
Transfer-Encoding: chunked

a
0123456789
0


```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=17 span[header_field]="Transfer-Encoding"
off=35 header_field complete
off=36 len=7 span[header_value]="chunked"
off=45 header_value complete
off=47 headers complete status=200 v=1/1 flags=208 content_length=0
off=50 chunk header len=10
off=50 len=10 span[body]="0123456789"
off=62 chunk complete
off=62 pause
off=65 chunk header len=0
off=67 chunk complete
off=67 pause
off=67 message complete
```
