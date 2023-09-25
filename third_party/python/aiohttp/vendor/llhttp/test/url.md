# URL tests

## Absolute URL

```url
http://example.com/path?query=value#schema
```

```log
off=0 len=4 span[url.schema]="http"
off=7 len=11 span[url.host]="example.com"
off=18 len=5 span[url.path]="/path"
off=24 len=11 span[url.query]="query=value"
off=36 len=6 span[url.fragment]="schema"
```

## Relative URL

```url
/path?query=value#schema
```

```log
off=0 len=5 span[url.path]="/path"
off=6 len=11 span[url.query]="query=value"
off=18 len=6 span[url.fragment]="schema"
```

## Failing on broken schema

<!-- meta={"noScan": true} -->
```url
schema:/path?query=value#schema
```

```log
off=0 len=6 span[url.schema]="schema"
off=8 error code=7 reason="Unexpected char in url schema"
```

## Proxy request

```url
http://hostname/
```

```log
off=0 len=4 span[url.schema]="http"
off=7 len=8 span[url.host]="hostname"
off=15 len=1 span[url.path]="/"
```

## Proxy request with port

```url
http://hostname:444/
```

```log
off=0 len=4 span[url.schema]="http"
off=7 len=12 span[url.host]="hostname:444"
off=19 len=1 span[url.path]="/"
```

## Proxy IPv6 request

```url
http://[1:2::3:4]/
```

```log
off=0 len=4 span[url.schema]="http"
off=7 len=10 span[url.host]="[1:2::3:4]"
off=17 len=1 span[url.path]="/"
```

## Proxy IPv6 request with port

```url
http://[1:2::3:4]:67/
```

```log
off=0 len=4 span[url.schema]="http"
off=7 len=13 span[url.host]="[1:2::3:4]:67"
off=20 len=1 span[url.path]="/"
```

## IPv4 in IPv6 address

```url
http://[2001:0000:0000:0000:0000:0000:1.9.1.1]/
```

```log
off=0 len=4 span[url.schema]="http"
off=7 len=39 span[url.host]="[2001:0000:0000:0000:0000:0000:1.9.1.1]"
off=46 len=1 span[url.path]="/"
```

## Extra `?` in query string

```url
http://a.tbcdn.cn/p/fp/2010c/??fp-header-min.css,fp-base-min.css,\
fp-channel-min.css,fp-product-min.css,fp-mall-min.css,fp-category-min.css,\
fp-sub-min.css,fp-gdp4p-min.css,fp-css3-min.css,fp-misc-min.css?t=20101022.css
```

```log
off=0 len=4 span[url.schema]="http"
off=7 len=10 span[url.host]="a.tbcdn.cn"
off=17 len=12 span[url.path]="/p/fp/2010c/"
off=30 len=187 span[url.query]="?fp-header-min.css,fp-base-min.css,fp-channel-min.css,fp-product-min.css,fp-mall-min.css,fp-category-min.css,fp-sub-min.css,fp-gdp4p-min.css,fp-css3-min.css,fp-misc-min.css?t=20101022.css"
```

## URL encoded space

```url
/toto.html?toto=a%20b
```

```log
off=0 len=10 span[url.path]="/toto.html"
off=11 len=10 span[url.query]="toto=a%20b"
```

## URL fragment

```url
/toto.html#titi
```

```log
off=0 len=10 span[url.path]="/toto.html"
off=11 len=4 span[url.fragment]="titi"
```

## Complex URL fragment

```url
http://www.webmasterworld.com/r.cgi?f=21&d=8405&url=\
http://www.example.com/index.html?foo=bar&hello=world#midpage
```

```log
off=0 len=4 span[url.schema]="http"
off=7 len=22 span[url.host]="www.webmasterworld.com"
off=29 len=6 span[url.path]="/r.cgi"
off=36 len=69 span[url.query]="f=21&d=8405&url=http://www.example.com/index.html?foo=bar&hello=world"
off=106 len=7 span[url.fragment]="midpage"
```

## Complex URL from node.js url parser doc

```url
http://host.com:8080/p/a/t/h?query=string#hash
```

```log
off=0 len=4 span[url.schema]="http"
off=7 len=13 span[url.host]="host.com:8080"
off=20 len=8 span[url.path]="/p/a/t/h"
off=29 len=12 span[url.query]="query=string"
off=42 len=4 span[url.fragment]="hash"
```

## Complex URL with basic auth from node.js url parser doc

```url
http://a:b@host.com:8080/p/a/t/h?query=string#hash
```

```log
off=0 len=4 span[url.schema]="http"
off=7 len=17 span[url.host]="a:b@host.com:8080"
off=24 len=8 span[url.path]="/p/a/t/h"
off=33 len=12 span[url.query]="query=string"
off=46 len=4 span[url.fragment]="hash"
```

## Double `@`

<!-- meta={"noScan": true} -->
```url
http://a:b@@hostname:443/
```

```log
off=0 len=4 span[url.schema]="http"
off=12 error code=7 reason="Double @ in url"
```

## Proxy basic auth with url encoded space

```url
http://a%20:b@host.com/
```

```log
off=0 len=4 span[url.schema]="http"
off=7 len=15 span[url.host]="a%20:b@host.com"
off=22 len=1 span[url.path]="/"
```

## Proxy basic auth with unreserved chars

```url
http://a!;-_!=+$@host.com/
```

```log
off=0 len=4 span[url.schema]="http"
off=7 len=18 span[url.host]="a!;-_!=+$@host.com"
off=25 len=1 span[url.path]="/"
```

## IPv6 address with Zone ID

```url
http://[fe80::a%25eth0]/
```

```log
off=0 len=4 span[url.schema]="http"
off=7 len=16 span[url.host]="[fe80::a%25eth0]"
off=23 len=1 span[url.path]="/"
```

## IPv6 address with Zone ID, but `%` is not percent-encoded

```url
http://[fe80::a%eth0]/
```

```log
off=0 len=4 span[url.schema]="http"
off=7 len=14 span[url.host]="[fe80::a%eth0]"
off=21 len=1 span[url.path]="/"
```

## Disallow tab in URL in strict mode

<!-- meta={"mode": "strict", "noScan": true} -->
```url
/foo\tbar/
```

```log
off=5 error code=7 reason="Invalid characters in url (strict mode)"
```

## Tab in URL in loose mode

<!-- meta={"mode": "loose"} -->
```url
/foo\tbar/
```

```log
off=0 len=9 span[url.path]="/foo\tbar/"
```

## Disallow form-feed in URL in strict mode

<!-- meta={"mode": "strict", "noScan": true} -->
```url
/foo\fbar/
```

```log
off=5 error code=7 reason="Invalid characters in url (strict mode)"
```

## Form-feed in URL in loose mode

<!-- meta={"mode": "loose"} -->
```url
/foo\fbar/
```

```log
off=0 len=9 span[url.path]="/foo\fbar/"
```
