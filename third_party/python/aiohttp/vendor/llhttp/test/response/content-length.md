Content-Length header
=====================

## Response without `Content-Length`, but with body

The client should wait for the server's EOF. That is, when
`Content-Length` is not specified, and `Connection: close`, the end of body is
specified by the EOF.

_(Compare with APACHEBENCH_GET)_

<!-- meta={"type": "response"} -->
```http
HTTP/1.1 200 OK
Date: Tue, 04 Aug 2009 07:59:32 GMT
Server: Apache
X-Powered-By: Servlet/2.5 JSP/2.1
Content-Type: text/xml; charset=utf-8
Connection: close

<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\
<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\">\n\
  <SOAP-ENV:Body>\n\
    <SOAP-ENV:Fault>\n\
       <faultcode>SOAP-ENV:Client</faultcode>\n\
       <faultstring>Client Error</faultstring>\n\
    </SOAP-ENV:Fault>\n\
  </SOAP-ENV:Body>\n\
</SOAP-ENV:Envelope>
```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=4 span[header_field]="Date"
off=22 header_field complete
off=23 len=29 span[header_value]="Tue, 04 Aug 2009 07:59:32 GMT"
off=54 header_value complete
off=54 len=6 span[header_field]="Server"
off=61 header_field complete
off=62 len=6 span[header_value]="Apache"
off=70 header_value complete
off=70 len=12 span[header_field]="X-Powered-By"
off=83 header_field complete
off=84 len=19 span[header_value]="Servlet/2.5 JSP/2.1"
off=105 header_value complete
off=105 len=12 span[header_field]="Content-Type"
off=118 header_field complete
off=119 len=23 span[header_value]="text/xml; charset=utf-8"
off=144 header_value complete
off=144 len=10 span[header_field]="Connection"
off=155 header_field complete
off=156 len=5 span[header_value]="close"
off=163 header_value complete
off=165 headers complete status=200 v=1/1 flags=2 content_length=0
off=165 len=42 span[body]="<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
off=207 len=1 span[body]=lf
off=208 len=80 span[body]="<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\">"
off=288 len=1 span[body]=lf
off=289 len=17 span[body]="  <SOAP-ENV:Body>"
off=306 len=1 span[body]=lf
off=307 len=20 span[body]="    <SOAP-ENV:Fault>"
off=327 len=1 span[body]=lf
off=328 len=45 span[body]="       <faultcode>SOAP-ENV:Client</faultcode>"
off=373 len=1 span[body]=lf
off=374 len=46 span[body]="       <faultstring>Client Error</faultstring>"
off=420 len=1 span[body]=lf
off=421 len=21 span[body]="    </SOAP-ENV:Fault>"
off=442 len=1 span[body]=lf
off=443 len=18 span[body]="  </SOAP-ENV:Body>"
off=461 len=1 span[body]=lf
off=462 len=20 span[body]="</SOAP-ENV:Envelope>"
```

## Content-Length-X

The header that starts with `Content-Length*` should not be treated as
`Content-Length`.

<!-- meta={"type": "response"} -->
```http
HTTP/1.1 200 OK
Content-Length-X: 0
Transfer-Encoding: chunked

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
off=17 len=16 span[header_field]="Content-Length-X"
off=34 header_field complete
off=35 len=1 span[header_value]="0"
off=38 header_value complete
off=38 len=17 span[header_field]="Transfer-Encoding"
off=56 header_field complete
off=57 len=7 span[header_value]="chunked"
off=66 header_value complete
off=68 headers complete status=200 v=1/1 flags=208 content_length=0
off=71 chunk header len=2
off=71 len=2 span[body]="OK"
off=75 chunk complete
off=78 chunk header len=0
off=80 chunk complete
off=80 message complete
```

## Content-Length reset when no body is received

<!-- meta={"type": "response", "skipBody": true} -->
```http
HTTP/1.1 200 OK
Content-Length: 123

HTTP/1.1 200 OK
Content-Length: 456


```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=14 span[header_field]="Content-Length"
off=32 header_field complete
off=33 len=3 span[header_value]="123"
off=38 header_value complete
off=40 headers complete status=200 v=1/1 flags=20 content_length=123
off=40 skip body
off=40 message complete
off=40 reset
off=40 message begin
off=45 len=3 span[version]="1.1"
off=48 version complete
off=53 len=2 span[status]="OK"
off=57 status complete
off=57 len=14 span[header_field]="Content-Length"
off=72 header_field complete
off=73 len=3 span[header_value]="456"
off=78 header_value complete
off=80 headers complete status=200 v=1/1 flags=20 content_length=456
off=80 skip body
off=80 message complete
```
