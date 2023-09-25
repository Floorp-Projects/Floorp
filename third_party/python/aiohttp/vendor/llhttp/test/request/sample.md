Sample requests
===============

Lots of sample requests, most ported from [http_parser][0] test suite.

## Simple request

<!-- meta={"type": "request"} -->
```http
OPTIONS /url HTTP/1.1
Header1: Value1
Header2:\t Value2


```

```log
off=0 message begin
off=0 len=7 span[method]="OPTIONS"
off=7 method complete
off=8 len=4 span[url]="/url"
off=13 url complete
off=18 len=3 span[version]="1.1"
off=21 version complete
off=23 len=7 span[header_field]="Header1"
off=31 header_field complete
off=32 len=6 span[header_value]="Value1"
off=40 header_value complete
off=40 len=7 span[header_field]="Header2"
off=48 header_field complete
off=50 len=6 span[header_value]="Value2"
off=58 header_value complete
off=60 headers complete method=6 v=1/1 flags=0 content_length=0
off=60 message complete
```

## Request with method starting with `H`

There's a optimization in `start_req_or_res` that passes execution to
`start_req` when the first character is not `H` (because response must start
with `HTTP/`). However, there're still methods like `HEAD` that should get
to `start_req`. Verify that it still works after optimization.

<!-- meta={"type": "request", "noScan": true } -->
```http
HEAD /url HTTP/1.1


```

```log
off=0 message begin
off=0 len=4 span[method]="HEAD"
off=4 method complete
off=5 len=4 span[url]="/url"
off=10 url complete
off=15 len=3 span[version]="1.1"
off=18 version complete
off=22 headers complete method=2 v=1/1 flags=0 content_length=0
off=22 message complete
```

## curl GET

<!-- meta={"type": "request"} -->
```http
GET /test HTTP/1.1
User-Agent: curl/7.18.0 (i486-pc-linux-gnu) libcurl/7.18.0 OpenSSL/0.9.8g zlib/1.2.3.3 libidn/1.1
Host: 0.0.0.0=5000
Accept: */*


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=5 span[url]="/test"
off=10 url complete
off=15 len=3 span[version]="1.1"
off=18 version complete
off=20 len=10 span[header_field]="User-Agent"
off=31 header_field complete
off=32 len=85 span[header_value]="curl/7.18.0 (i486-pc-linux-gnu) libcurl/7.18.0 OpenSSL/0.9.8g zlib/1.2.3.3 libidn/1.1"
off=119 header_value complete
off=119 len=4 span[header_field]="Host"
off=124 header_field complete
off=125 len=12 span[header_value]="0.0.0.0=5000"
off=139 header_value complete
off=139 len=6 span[header_field]="Accept"
off=146 header_field complete
off=147 len=3 span[header_value]="*/*"
off=152 header_value complete
off=154 headers complete method=1 v=1/1 flags=0 content_length=0
off=154 message complete
```

## Firefox GET

<!-- meta={"type": "request"} -->
```http
GET /favicon.ico HTTP/1.1
Host: 0.0.0.0=5000
User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9) Gecko/2008061015 Firefox/3.0
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8
Accept-Language: en-us,en;q=0.5
Accept-Encoding: gzip,deflate
Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7
Keep-Alive: 300
Connection: keep-alive


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=12 span[url]="/favicon.ico"
off=17 url complete
off=22 len=3 span[version]="1.1"
off=25 version complete
off=27 len=4 span[header_field]="Host"
off=32 header_field complete
off=33 len=12 span[header_value]="0.0.0.0=5000"
off=47 header_value complete
off=47 len=10 span[header_field]="User-Agent"
off=58 header_field complete
off=59 len=76 span[header_value]="Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9) Gecko/2008061015 Firefox/3.0"
off=137 header_value complete
off=137 len=6 span[header_field]="Accept"
off=144 header_field complete
off=145 len=63 span[header_value]="text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"
off=210 header_value complete
off=210 len=15 span[header_field]="Accept-Language"
off=226 header_field complete
off=227 len=14 span[header_value]="en-us,en;q=0.5"
off=243 header_value complete
off=243 len=15 span[header_field]="Accept-Encoding"
off=259 header_field complete
off=260 len=12 span[header_value]="gzip,deflate"
off=274 header_value complete
off=274 len=14 span[header_field]="Accept-Charset"
off=289 header_field complete
off=290 len=30 span[header_value]="ISO-8859-1,utf-8;q=0.7,*;q=0.7"
off=322 header_value complete
off=322 len=10 span[header_field]="Keep-Alive"
off=333 header_field complete
off=334 len=3 span[header_value]="300"
off=339 header_value complete
off=339 len=10 span[header_field]="Connection"
off=350 header_field complete
off=351 len=10 span[header_value]="keep-alive"
off=363 header_value complete
off=365 headers complete method=1 v=1/1 flags=1 content_length=0
off=365 message complete
```

## DUMBPACK

<!-- meta={"type": "request"} -->
```http
GET /dumbpack HTTP/1.1
aaaaaaaaaaaaa:++++++++++


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=9 span[url]="/dumbpack"
off=14 url complete
off=19 len=3 span[version]="1.1"
off=22 version complete
off=24 len=13 span[header_field]="aaaaaaaaaaaaa"
off=38 header_field complete
off=38 len=10 span[header_value]="++++++++++"
off=50 header_value complete
off=52 headers complete method=1 v=1/1 flags=0 content_length=0
off=52 message complete
```

## No headers and no body

<!-- meta={"type": "request"} -->
```http
GET /get_no_headers_no_body/world HTTP/1.1


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=29 span[url]="/get_no_headers_no_body/world"
off=34 url complete
off=39 len=3 span[version]="1.1"
off=42 version complete
off=46 headers complete method=1 v=1/1 flags=0 content_length=0
off=46 message complete
```

## One header and no body

<!-- meta={"type": "request"} -->
```http
GET /get_one_header_no_body HTTP/1.1
Accept: */*


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=23 span[url]="/get_one_header_no_body"
off=28 url complete
off=33 len=3 span[version]="1.1"
off=36 version complete
off=38 len=6 span[header_field]="Accept"
off=45 header_field complete
off=46 len=3 span[header_value]="*/*"
off=51 header_value complete
off=53 headers complete method=1 v=1/1 flags=0 content_length=0
off=53 message complete
```

## Apache bench GET

The server receiving this request SHOULD NOT wait for EOF to know that
`Content-Length == 0`.

<!-- meta={"type": "request"} -->
```http
GET /test HTTP/1.0
Host: 0.0.0.0:5000
User-Agent: ApacheBench/2.3
Accept: */*


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=5 span[url]="/test"
off=10 url complete
off=15 len=3 span[version]="1.0"
off=18 version complete
off=20 len=4 span[header_field]="Host"
off=25 header_field complete
off=26 len=12 span[header_value]="0.0.0.0:5000"
off=40 header_value complete
off=40 len=10 span[header_field]="User-Agent"
off=51 header_field complete
off=52 len=15 span[header_value]="ApacheBench/2.3"
off=69 header_value complete
off=69 len=6 span[header_field]="Accept"
off=76 header_field complete
off=77 len=3 span[header_value]="*/*"
off=82 header_value complete
off=84 headers complete method=1 v=1/0 flags=0 content_length=0
off=84 message complete
```

## Prefix newline

Some clients, especially after a POST in a keep-alive connection,
will send an extra CRLF before the next request.

<!-- meta={"type": "request"} -->
```http
\r\nGET /test HTTP/1.1


```

```log
off=2 message begin
off=2 len=3 span[method]="GET"
off=5 method complete
off=6 len=5 span[url]="/test"
off=12 url complete
off=17 len=3 span[version]="1.1"
off=20 version complete
off=24 headers complete method=1 v=1/1 flags=0 content_length=0
off=24 message complete
```

## No HTTP version

<!-- meta={"type": "request"} -->
```http
GET /


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=1 span[url]="/"
off=7 url complete
off=9 headers complete method=1 v=0/9 flags=0 content_length=0
off=9 message complete
```

## Line folding in header value with CRLF

<!-- meta={"type": "request-lenient-headers"} -->
```http
GET / HTTP/1.1
Line1:   abc
\tdef
 ghi
\t\tjkl
  mno 
\t \tqrs
Line2: \t line2\t
Line3:
 line3
Line4: 
 
Connection:
 close


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=1 span[url]="/"
off=6 url complete
off=11 len=3 span[version]="1.1"
off=14 version complete
off=16 len=5 span[header_field]="Line1"
off=22 header_field complete
off=25 len=3 span[header_value]="abc"
off=30 len=4 span[header_value]="\tdef"
off=36 len=4 span[header_value]=" ghi"
off=42 len=5 span[header_value]="\t\tjkl"
off=49 len=6 span[header_value]="  mno "
off=57 len=6 span[header_value]="\t \tqrs"
off=65 header_value complete
off=65 len=5 span[header_field]="Line2"
off=71 header_field complete
off=74 len=6 span[header_value]="line2\t"
off=82 header_value complete
off=82 len=5 span[header_field]="Line3"
off=88 header_field complete
off=91 len=5 span[header_value]="line3"
off=98 header_value complete
off=98 len=5 span[header_field]="Line4"
off=104 header_field complete
off=110 len=0 span[header_value]=""
off=110 header_value complete
off=110 len=10 span[header_field]="Connection"
off=121 header_field complete
off=124 len=5 span[header_value]="close"
off=131 header_value complete
off=133 headers complete method=1 v=1/1 flags=2 content_length=0
off=133 message complete
```

## Line folding in header value with LF

<!-- meta={"type": "request"} -->
```http
GET / HTTP/1.1\n\
Line1:   abc\n\
\tdef\n\
 ghi\n\
\t\tjkl\n\
  mno \n\
\t \tqrs\n\
Line2: \t line2\t\n\
Line3:\n\
 line3\n\
Line4: \n\
 \n\
Connection:\n\
 close\n\
\n
```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=1 span[url]="/"
off=6 url complete
off=11 len=3 span[version]="1.1"
off=14 version complete
off=15 len=5 span[header_field]="Line1"
off=21 header_field complete
off=24 len=3 span[header_value]="abc"
off=27 error code=10 reason="Invalid header value char"
```

## Request starting with CRLF

<!-- meta={"type": "request"} -->
```http
\r\nGET /url HTTP/1.1
Header1: Value1


```

```log
off=2 message begin
off=2 len=3 span[method]="GET"
off=5 method complete
off=6 len=4 span[url]="/url"
off=11 url complete
off=16 len=3 span[version]="1.1"
off=19 version complete
off=21 len=7 span[header_field]="Header1"
off=29 header_field complete
off=30 len=6 span[header_value]="Value1"
off=38 header_value complete
off=40 headers complete method=1 v=1/1 flags=0 content_length=0
off=40 message complete
```

## Extended Characters

See nodejs/test/parallel/test-http-headers-obstext.js

<!-- meta={"type": "request", "noScan": true} -->
```http
GET / HTTP/1.1
Test: Düsseldorf


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=1 span[url]="/"
off=6 url complete
off=11 len=3 span[version]="1.1"
off=14 version complete
off=16 len=4 span[header_field]="Test"
off=21 header_field complete
off=22 len=11 span[header_value]="Düsseldorf"
off=35 header_value complete
off=37 headers complete method=1 v=1/1 flags=0 content_length=0
off=37 message complete
```

## 255 ASCII in header value

Note: `Buffer.from([ 0xff ]).toString('latin1') === 'ÿ'`.

<!-- meta={"type": "request", "noScan": true} -->
```http
OPTIONS /url HTTP/1.1
Header1: Value1
Header2: \xffValue2


```

```log
off=0 message begin
off=0 len=7 span[method]="OPTIONS"
off=7 method complete
off=8 len=4 span[url]="/url"
off=13 url complete
off=18 len=3 span[version]="1.1"
off=21 version complete
off=23 len=7 span[header_field]="Header1"
off=31 header_field complete
off=32 len=6 span[header_value]="Value1"
off=40 header_value complete
off=40 len=7 span[header_field]="Header2"
off=48 header_field complete
off=49 len=8 span[header_value]="ÿValue2"
off=59 header_value complete
off=61 headers complete method=6 v=1/1 flags=0 content_length=0
off=61 message complete
```

## X-SSL-Nonsense

See nodejs/test/parallel/test-http-headers-obstext.js

<!-- meta={"type": "request"} -->
```http
GET / HTTP/1.1
X-SSL-Nonsense:   -----BEGIN CERTIFICATE-----
\tMIIFbTCCBFWgAwIBAgICH4cwDQYJKoZIhvcNAQEFBQAwcDELMAkGA1UEBhMCVUsx
\tETAPBgNVBAoTCGVTY2llbmNlMRIwEAYDVQQLEwlBdXRob3JpdHkxCzAJBgNVBAMT
\tAkNBMS0wKwYJKoZIhvcNAQkBFh5jYS1vcGVyYXRvckBncmlkLXN1cHBvcnQuYWMu
\tdWswHhcNMDYwNzI3MTQxMzI4WhcNMDcwNzI3MTQxMzI4WjBbMQswCQYDVQQGEwJV
\tSzERMA8GA1UEChMIZVNjaWVuY2UxEzARBgNVBAsTCk1hbmNoZXN0ZXIxCzAJBgNV
\tBAcTmrsogriqMWLAk1DMRcwFQYDVQQDEw5taWNoYWVsIHBhcmQYJKoZIhvcNAQEB
\tBQADggEPADCCAQoCggEBANPEQBgl1IaKdSS1TbhF3hEXSl72G9J+WC/1R64fAcEF
\tW51rEyFYiIeZGx/BVzwXbeBoNUK41OK65sxGuflMo5gLflbwJtHBRIEKAfVVp3YR
\tgW7cMA/s/XKgL1GEC7rQw8lIZT8RApukCGqOVHSi/F1SiFlPDxuDfmdiNzL31+sL
\t0iwHDdNkGjy5pyBSB8Y79dsSJtCW/iaLB0/n8Sj7HgvvZJ7x0fr+RQjYOUUfrePP
\tu2MSpFyf+9BbC/aXgaZuiCvSR+8Snv3xApQY+fULK/xY8h8Ua51iXoQ5jrgu2SqR
\twgA7BUi3G8LFzMBl8FRCDYGUDy7M6QaHXx1ZWIPWNKsCAwEAAaOCAiQwggIgMAwG
\tA1UdEwEB/wQCMAAwEQYJYIZIAYb4QgHTTPAQDAgWgMA4GA1UdDwEB/wQEAwID6DAs
\tBglghkgBhvhCAQ0EHxYdVUsgZS1TY2llbmNlIFVzZXIgQ2VydGlmaWNhdGUwHQYD
\tVR0OBBYEFDTt/sf9PeMaZDHkUIldrDYMNTBZMIGaBgNVHSMEgZIwgY+AFAI4qxGj
\tloCLDdMVKwiljjDastqooXSkcjBwMQswCQYDVQQGEwJVSzERMA8GA1UEChMIZVNj
\taWVuY2UxEjAQBgNVBAsTCUF1dGhvcml0eTELMAkGA1UEAxMCQ0ExLTArBgkqhkiG
\t9w0BCQEWHmNhLW9wZXJhdG9yQGdyaWQtc3VwcG9ydC5hYy51a4IBADApBgNVHRIE
\tIjAggR5jYS1vcGVyYXRvckBncmlkLXN1cHBvcnQuYWMudWswGQYDVR0gBBIwEDAO
\tBgwrBgEEAdkvAQEBAQYwPQYJYIZIAYb4QgEEBDAWLmh0dHA6Ly9jYS5ncmlkLXN1
\tcHBvcnQuYWMudmT4sopwqlBWsvcHViL2NybC9jYWNybC5jcmwwPQYJYIZIAYb4QgEDBDAWLmh0
\tdHA6Ly9jYS5ncmlkLXN1cHBvcnQuYWMudWsvcHViL2NybC9jYWNybC5jcmwwPwYD
\tVR0fBDgwNjA0oDKgMIYuaHR0cDovL2NhLmdyaWQt5hYy51ay9wdWIv
\tY3JsL2NhY3JsLmNybDANBgkqhkiG9w0BAQUFAAOCAQEAS/U4iiooBENGW/Hwmmd3
\tXCy6Zrt08YjKCzGNjorT98g8uGsqYjSxv/hmi0qlnlHs+k/3Iobc3LjS5AMYr5L8
\tUO7OSkgFFlLHQyC9JzPfmLCAugvzEbyv4Olnsr8hbxF1MbKZoQxUZtMVu29wjfXk
\thTeApBv7eaKCWpSp7MCbvgzm74izKhu3vlDk9w6qVrxePfGgpKPqfHiOoGhFnbTK
\twTC6o2xq5y0qZ03JonF7OJspEd3I5zKY3E+ov7/ZhW6DqT8UFvsAdjvQbXyhV8Eu
\tYhixw1aKEPzNjNowuIseVogKOLXxWI5vAi5HgXdS0/ES5gDGsABo4fqovUKlgop3
\tRA==
\t-----END CERTIFICATE-----


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=1 span[url]="/"
off=6 url complete
off=11 len=3 span[version]="1.1"
off=14 version complete
off=16 len=14 span[header_field]="X-SSL-Nonsense"
off=31 header_field complete
off=34 len=27 span[header_value]="-----BEGIN CERTIFICATE-----"
off=63 len=65 span[header_value]="\tMIIFbTCCBFWgAwIBAgICH4cwDQYJKoZIhvcNAQEFBQAwcDELMAkGA1UEBhMCVUsx"
off=130 len=65 span[header_value]="\tETAPBgNVBAoTCGVTY2llbmNlMRIwEAYDVQQLEwlBdXRob3JpdHkxCzAJBgNVBAMT"
off=197 len=65 span[header_value]="\tAkNBMS0wKwYJKoZIhvcNAQkBFh5jYS1vcGVyYXRvckBncmlkLXN1cHBvcnQuYWMu"
off=264 len=65 span[header_value]="\tdWswHhcNMDYwNzI3MTQxMzI4WhcNMDcwNzI3MTQxMzI4WjBbMQswCQYDVQQGEwJV"
off=331 len=65 span[header_value]="\tSzERMA8GA1UEChMIZVNjaWVuY2UxEzARBgNVBAsTCk1hbmNoZXN0ZXIxCzAJBgNV"
off=398 len=65 span[header_value]="\tBAcTmrsogriqMWLAk1DMRcwFQYDVQQDEw5taWNoYWVsIHBhcmQYJKoZIhvcNAQEB"
off=465 len=65 span[header_value]="\tBQADggEPADCCAQoCggEBANPEQBgl1IaKdSS1TbhF3hEXSl72G9J+WC/1R64fAcEF"
off=532 len=65 span[header_value]="\tW51rEyFYiIeZGx/BVzwXbeBoNUK41OK65sxGuflMo5gLflbwJtHBRIEKAfVVp3YR"
off=599 len=65 span[header_value]="\tgW7cMA/s/XKgL1GEC7rQw8lIZT8RApukCGqOVHSi/F1SiFlPDxuDfmdiNzL31+sL"
off=666 len=65 span[header_value]="\t0iwHDdNkGjy5pyBSB8Y79dsSJtCW/iaLB0/n8Sj7HgvvZJ7x0fr+RQjYOUUfrePP"
off=733 len=65 span[header_value]="\tu2MSpFyf+9BbC/aXgaZuiCvSR+8Snv3xApQY+fULK/xY8h8Ua51iXoQ5jrgu2SqR"
off=800 len=65 span[header_value]="\twgA7BUi3G8LFzMBl8FRCDYGUDy7M6QaHXx1ZWIPWNKsCAwEAAaOCAiQwggIgMAwG"
off=867 len=66 span[header_value]="\tA1UdEwEB/wQCMAAwEQYJYIZIAYb4QgHTTPAQDAgWgMA4GA1UdDwEB/wQEAwID6DAs"
off=935 len=65 span[header_value]="\tBglghkgBhvhCAQ0EHxYdVUsgZS1TY2llbmNlIFVzZXIgQ2VydGlmaWNhdGUwHQYD"
off=1002 len=65 span[header_value]="\tVR0OBBYEFDTt/sf9PeMaZDHkUIldrDYMNTBZMIGaBgNVHSMEgZIwgY+AFAI4qxGj"
off=1069 len=65 span[header_value]="\tloCLDdMVKwiljjDastqooXSkcjBwMQswCQYDVQQGEwJVSzERMA8GA1UEChMIZVNj"
off=1136 len=65 span[header_value]="\taWVuY2UxEjAQBgNVBAsTCUF1dGhvcml0eTELMAkGA1UEAxMCQ0ExLTArBgkqhkiG"
off=1203 len=65 span[header_value]="\t9w0BCQEWHmNhLW9wZXJhdG9yQGdyaWQtc3VwcG9ydC5hYy51a4IBADApBgNVHRIE"
off=1270 len=65 span[header_value]="\tIjAggR5jYS1vcGVyYXRvckBncmlkLXN1cHBvcnQuYWMudWswGQYDVR0gBBIwEDAO"
off=1337 len=65 span[header_value]="\tBgwrBgEEAdkvAQEBAQYwPQYJYIZIAYb4QgEEBDAWLmh0dHA6Ly9jYS5ncmlkLXN1"
off=1404 len=75 span[header_value]="\tcHBvcnQuYWMudmT4sopwqlBWsvcHViL2NybC9jYWNybC5jcmwwPQYJYIZIAYb4QgEDBDAWLmh0"
off=1481 len=65 span[header_value]="\tdHA6Ly9jYS5ncmlkLXN1cHBvcnQuYWMudWsvcHViL2NybC9jYWNybC5jcmwwPwYD"
off=1548 len=55 span[header_value]="\tVR0fBDgwNjA0oDKgMIYuaHR0cDovL2NhLmdyaWQt5hYy51ay9wdWIv"
off=1605 len=65 span[header_value]="\tY3JsL2NhY3JsLmNybDANBgkqhkiG9w0BAQUFAAOCAQEAS/U4iiooBENGW/Hwmmd3"
off=1672 len=65 span[header_value]="\tXCy6Zrt08YjKCzGNjorT98g8uGsqYjSxv/hmi0qlnlHs+k/3Iobc3LjS5AMYr5L8"
off=1739 len=65 span[header_value]="\tUO7OSkgFFlLHQyC9JzPfmLCAugvzEbyv4Olnsr8hbxF1MbKZoQxUZtMVu29wjfXk"
off=1806 len=65 span[header_value]="\thTeApBv7eaKCWpSp7MCbvgzm74izKhu3vlDk9w6qVrxePfGgpKPqfHiOoGhFnbTK"
off=1873 len=65 span[header_value]="\twTC6o2xq5y0qZ03JonF7OJspEd3I5zKY3E+ov7/ZhW6DqT8UFvsAdjvQbXyhV8Eu"
off=1940 len=65 span[header_value]="\tYhixw1aKEPzNjNowuIseVogKOLXxWI5vAi5HgXdS0/ES5gDGsABo4fqovUKlgop3"
off=2007 len=5 span[header_value]="\tRA=="
off=2014 len=26 span[header_value]="\t-----END CERTIFICATE-----"
off=2042 header_value complete
off=2044 headers complete method=1 v=1/1 flags=0 content_length=0
off=2044 message complete
```

[0]: https://github.com/nodejs/http-parser
