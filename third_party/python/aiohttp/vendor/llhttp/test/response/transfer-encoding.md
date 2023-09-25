Transfer-Encoding header
========================

## Trailing space on chunked body

<!-- meta={"type": "response"} -->
```http
HTTP/1.1 200 OK
Content-Type: text/plain
Transfer-Encoding: chunked

25  \r\n\
This is the data in the first chunk

1C
and this is the second one

0  \r\n\


```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=12 span[header_field]="Content-Type"
off=30 header_field complete
off=31 len=10 span[header_value]="text/plain"
off=43 header_value complete
off=43 len=17 span[header_field]="Transfer-Encoding"
off=61 header_field complete
off=62 len=7 span[header_value]="chunked"
off=71 header_value complete
off=73 headers complete status=200 v=1/1 flags=208 content_length=0
off=75 error code=12 reason="Invalid character in chunk size"
```

## `chunked` before other transfer-encoding

<!-- meta={"type": "response"} -->
```http
HTTP/1.1 200 OK
Accept: */*
Transfer-Encoding: chunked, deflate

World
```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=6 span[header_field]="Accept"
off=24 header_field complete
off=25 len=3 span[header_value]="*/*"
off=30 header_value complete
off=30 len=17 span[header_field]="Transfer-Encoding"
off=48 header_field complete
off=49 len=16 span[header_value]="chunked, deflate"
off=67 header_value complete
off=69 headers complete status=200 v=1/1 flags=200 content_length=0
off=69 len=5 span[body]="World"
```

## multiple transfer-encoding where chunked is not the last one

<!-- meta={"type": "response"} -->
```http
HTTP/1.1 200 OK
Accept: */*
Transfer-Encoding: chunked
Transfer-Encoding: identity

World
```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=6 span[header_field]="Accept"
off=24 header_field complete
off=25 len=3 span[header_value]="*/*"
off=30 header_value complete
off=30 len=17 span[header_field]="Transfer-Encoding"
off=48 header_field complete
off=49 len=7 span[header_value]="chunked"
off=58 header_value complete
off=58 len=17 span[header_field]="Transfer-Encoding"
off=76 header_field complete
off=77 len=8 span[header_value]="identity"
off=87 header_value complete
off=89 headers complete status=200 v=1/1 flags=200 content_length=0
off=89 len=5 span[body]="World"
```

## `chunkedchunked` transfer-encoding does not enable chunked enconding

This check that the word `chunked` repeat more than once (with or without spaces) does not mistakenly enables chunked encoding.

<!-- meta={"type": "response"} -->
```http
HTTP/1.1 200 OK
Accept: */*
Transfer-Encoding: chunkedchunked

2
OK
0


```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=6 span[header_field]="Accept"
off=24 header_field complete
off=25 len=3 span[header_value]="*/*"
off=30 header_value complete
off=30 len=17 span[header_field]="Transfer-Encoding"
off=48 header_field complete
off=49 len=14 span[header_value]="chunkedchunked"
off=65 header_value complete
off=67 headers complete status=200 v=1/1 flags=200 content_length=0
off=67 len=1 span[body]="2"
off=68 len=1 span[body]=cr
off=69 len=1 span[body]=lf
off=70 len=2 span[body]="OK"
off=72 len=1 span[body]=cr
off=73 len=1 span[body]=lf
off=74 len=1 span[body]="0"
off=75 len=1 span[body]=cr
off=76 len=1 span[body]=lf
off=77 len=1 span[body]=cr
off=78 len=1 span[body]=lf
```

## Chunk extensions

<!-- meta={"type": "response"} -->
```http
HTTP/1.1 200 OK
Host: localhost
Transfer-encoding: chunked

5;ilovew3;somuchlove=aretheseparametersfor
hello
6;blahblah;blah
 world
0


```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=4 span[header_field]="Host"
off=22 header_field complete
off=23 len=9 span[header_value]="localhost"
off=34 header_value complete
off=34 len=17 span[header_field]="Transfer-encoding"
off=52 header_field complete
off=53 len=7 span[header_value]="chunked"
off=62 header_value complete
off=64 headers complete status=200 v=1/1 flags=208 content_length=0
off=66 len=7 span[chunk_extension_name]="ilovew3"
off=74 chunk_extension_name complete
off=74 len=10 span[chunk_extension_name]="somuchlove"
off=85 chunk_extension_name complete
off=85 len=21 span[chunk_extension_value]="aretheseparametersfor"
off=107 chunk_extension_value complete
off=108 chunk header len=5
off=108 len=5 span[body]="hello"
off=115 chunk complete
off=117 len=8 span[chunk_extension_name]="blahblah"
off=126 chunk_extension_name complete
off=126 len=4 span[chunk_extension_name]="blah"
off=131 chunk_extension_name complete
off=132 chunk header len=6
off=132 len=6 span[body]=" world"
off=140 chunk complete
off=143 chunk header len=0
off=145 chunk complete
off=145 message complete
```

## No semicolon before chunk extensions

<!-- meta={"type": "response"} -->
```http
HTTP/1.1 200 OK
Host: localhost
Transfer-encoding: chunked

2 erfrferferf
aa
0 rrrr


```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=4 span[header_field]="Host"
off=22 header_field complete
off=23 len=9 span[header_value]="localhost"
off=34 header_value complete
off=34 len=17 span[header_field]="Transfer-encoding"
off=52 header_field complete
off=53 len=7 span[header_value]="chunked"
off=62 header_value complete
off=64 headers complete status=200 v=1/1 flags=208 content_length=0
off=65 error code=12 reason="Invalid character in chunk size"
```


## No extension after semicolon

<!-- meta={"type": "response"} -->
```http
HTTP/1.1 200 OK
Host: localhost
Transfer-encoding: chunked

2;
aa
0


```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=4 span[header_field]="Host"
off=22 header_field complete
off=23 len=9 span[header_value]="localhost"
off=34 header_value complete
off=34 len=17 span[header_field]="Transfer-encoding"
off=52 header_field complete
off=53 len=7 span[header_value]="chunked"
off=62 header_value complete
off=64 headers complete status=200 v=1/1 flags=208 content_length=0
off=67 error code=2 reason="Invalid character in chunk extensions"
```


## Chunk extensions quoting

<!-- meta={"type": "response"} -->
```http
HTTP/1.1 200 OK
Host: localhost
Transfer-Encoding: chunked

5;ilovew3="I love; extensions";somuchlove="aretheseparametersfor";blah;foo=bar
hello
6;blahblah;blah
 world
0

```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=4 span[header_field]="Host"
off=22 header_field complete
off=23 len=9 span[header_value]="localhost"
off=34 header_value complete
off=34 len=17 span[header_field]="Transfer-Encoding"
off=52 header_field complete
off=53 len=7 span[header_value]="chunked"
off=62 header_value complete
off=64 headers complete status=200 v=1/1 flags=208 content_length=0
off=66 len=7 span[chunk_extension_name]="ilovew3"
off=74 chunk_extension_name complete
off=74 len=20 span[chunk_extension_value]=""I love; extensions""
off=94 chunk_extension_value complete
off=95 len=10 span[chunk_extension_name]="somuchlove"
off=106 chunk_extension_name complete
off=106 len=23 span[chunk_extension_value]=""aretheseparametersfor""
off=129 chunk_extension_value complete
off=130 len=4 span[chunk_extension_name]="blah"
off=135 chunk_extension_name complete
off=135 len=3 span[chunk_extension_name]="foo"
off=139 chunk_extension_name complete
off=139 len=3 span[chunk_extension_value]="bar"
off=143 chunk_extension_value complete
off=144 chunk header len=5
off=144 len=5 span[body]="hello"
off=151 chunk complete
off=153 len=8 span[chunk_extension_name]="blahblah"
off=162 chunk_extension_name complete
off=162 len=4 span[chunk_extension_name]="blah"
off=167 chunk_extension_name complete
off=168 chunk header len=6
off=168 len=6 span[body]=" world"
off=176 chunk complete
off=179 chunk header len=0
```


## Unbalanced chunk extensions quoting

<!-- meta={"type": "response"} -->
```http
HTTP/1.1 200 OK
Host: localhost
Transfer-Encoding: chunked

5;ilovew3="abc";somuchlove="def; ghi
hello
6;blahblah;blah
 world
0

```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=4 span[header_field]="Host"
off=22 header_field complete
off=23 len=9 span[header_value]="localhost"
off=34 header_value complete
off=34 len=17 span[header_field]="Transfer-Encoding"
off=52 header_field complete
off=53 len=7 span[header_value]="chunked"
off=62 header_value complete
off=64 headers complete status=200 v=1/1 flags=208 content_length=0
off=66 len=7 span[chunk_extension_name]="ilovew3"
off=74 chunk_extension_name complete
off=74 len=5 span[chunk_extension_value]=""abc""
off=79 chunk_extension_value complete
off=80 len=10 span[chunk_extension_name]="somuchlove"
off=91 chunk_extension_name complete
off=91 len=9 span[chunk_extension_value]=""def; ghi"
off=101 error code=2 reason="Invalid character in chunk extensions quoted value"
```


## Invalid OBS fold after chunked value

<!-- meta={"type": "response" } -->
```http
HTTP/1.1 200 OK
Transfer-Encoding: chunked
  abc

5
World
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
off=45 len=5 span[header_value]="  abc"
off=52 header_value complete
off=54 headers complete status=200 v=1/1 flags=200 content_length=0
off=54 len=1 span[body]="5"
off=55 len=1 span[body]=cr
off=56 len=1 span[body]=lf
off=57 len=5 span[body]="World"
off=62 len=1 span[body]=cr
off=63 len=1 span[body]=lf
off=64 len=1 span[body]="0"
off=65 len=1 span[body]=cr
off=66 len=1 span[body]=lf
off=67 len=1 span[body]=cr
off=68 len=1 span[body]=lf
```

