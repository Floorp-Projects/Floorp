# -*- coding: utf-8 -*-
import os
ccdir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# based on https://github.com/web-platform-tests/wpt/blob/275544eab54a0d0c7f74ccc2baae9711293d8908/url/urltestdata.txt
invalid = {
    b"scheme-trailing-space": b"a: foo.com",
    b"scheme-trailing-tab": b"a:\tfoo.com",
    b"scheme-trailing-newline": b"a:\nfoo.com",
    b"scheme-trailing-cr": b"a:\rfoo.com",
    b"scheme-http-no-slash": b"http:foo.com",
    b"scheme-http-no-slash-colon": b"http::@c:29",
    b"scheme-http-no-slash-square-bracket": b"http:[61:27]/:foo",
    b"scheme-http-backslash": b"http:\\\\foo.com\\",
    b"scheme-http-single-slash": b"http:/example.com/",
    b"scheme-ftp-single-slash": b"ftp:/example.com/",
    b"scheme-https-single-slash": b"https:/example.com/",
    b"scheme-data-single-slash": b"data:/example.com/",
    b"scheme-ftp-no-slash": b"ftp:example.com/",
    b"scheme-https-no-slash": b"https:example.com/",
    b"userinfo-password-bad-chars": b"http://&a:foo(b]c@d:2/",
    b"userinfo-username-contains-at-sign": b"http://::@c@d:2",
    b"userinfo-backslash": b"http://a\\b:c\\d@foo.com",
    b"host-space": b"http://example .org",
    b"host-tab": b"http://example\t.org",
    b"host-newline": b"http://example.\norg",
    b"host-cr": b"http://example.\rorg",
    b"host-square-brackets-port-contains-colon": b"http://[1::2]:3:4",
    b"port-999999": b"http://f:999999/c",
    b"port-single-letter": b"http://f:b/c",
    b"port-multiple-letters": b"http://f:fifty-two/c",
    b"port-leading-colon": b"http://2001::1",
    b"port-leading-colon-bracket-colon": b"http://2001::1]:80",
    b"path-leading-backslash-at-sign": b"http://foo.com/\\@",
    b"path-leading-colon-backslash": b":\\",
    b"path-leading-colon-chars-backslash": b":foo.com\\",
    b"path-relative-square-brackets": b"[61:24:74]:98",
    b"fragment-contains-hash": b"http://foo/path#f#g",
    b"path-percent-encoded-malformed": b"http://example.com/foo/%2e%2",
    b"path-bare-percent-sign": b"http://example.com/foo%",
    b"path-u0091": "http://example.com/foo\u0091".encode('utf-8'),
    b"userinfo-username-contains-pile-of-poo": "http://💩:foo@example.com".encode('utf-8'),
    b"userinfo-password-contains-pile-of-poo": "http://foo:💩@example.com".encode('utf-8'),
    b"host-hostname-in-brackets": b"http://[www.google.com]/",
    b"host-empty": b"http://",
    b"host-empty-with-userinfo": b"http://user:pass@/",
    b"port-leading-dash": b"http://foo:-80/",
    b"host-empty-userinfo-empty": b"http://@/www.example.com",
    b"host-invalid-unicode": "http://\ufdd0zyx.com".encode('utf-8'),
    b"host-invalid-unicode-percent-encoded": b"http://%ef%b7%90zyx.com",
    b"host-double-percent-encoded": "http://\uff05\uff14\uff11.com".encode('utf-8'),
    b"host-double-percent-encoded-percent-encoded": b"http://%ef%bc%85%ef%bc%94%ef%bc%91.com",
    b"host-u0000-percent-encoded": "http://\uff05\uff10\uff10.com".encode('utf-8'),
    b"host-u0000-percent-encoded-percent-encoded": b"http://%ef%bc%85%ef%bc%90%ef%bc%90.com",
}
invalid_absolute = invalid.copy()

invalid_url_code_points = {
    b"fragment-backslash": b"#\\",
    b"fragment-leading-space": b"http://f:21/b# e",
    b"path-contains-space": b"/a/ /c",
    b"path-leading-space": b"http://f:21/ b",
    b"path-tab": b"http://example.com/foo\tbar",
    b"path-trailing-space": b"http://f:21/b ?",
    b"port-cr": b"http://f:\r/c",
    b"port-newline": b"http://f:\n/c",
    b"port-space": b"http://f: /c",
    b"port-tab": b"http://f:\t/c",
    b"query-leading-space": b"http://f:21/b? d",
    b"query-trailing-space": b"http://f:21/b?d #",
}
invalid.update(invalid_url_code_points)
invalid_absolute.update(invalid_url_code_points)

valid_absolute = {
    b"scheme-private": b"a:foo.com",
    b"scheme-private-slash": b"foo:/",
    b"scheme-private-slash-slash": b"foo://",
    b"scheme-private-path": b"foo:/bar.com/",
    b"scheme-private-path-leading-slashes-only": b"foo://///////",
    b"scheme-private-path-leading-slashes-chars": b"foo://///////bar.com/",
    b"scheme-private-path-leading-slashes-colon-slashes": b"foo:////://///",
    b"scheme-private-single-letter": b"c:/foo",
    b"scheme-private-single-slash": b"madeupscheme:/example.com/",
    b"scheme-file-single-slash": b"file:/example.com/",
    b"scheme-ftps-single-slash": b"ftps:/example.com/",
    b"scheme-gopher-single-slash": b"gopher:/example.com/",
    b"scheme-ws-single-slash": b"ws:/example.com/",
    b"scheme-wss-single-slash": b"wss:/example.com/",
    b"scheme-javascript-single-slash": b"javascript:/example.com/",
    b"scheme-mailto-single-slash": b"mailto:/example.com/",
    b"scheme-private-no-slash": b"madeupscheme:example.com/",
    b"scheme-ftps-no-slash": b"ftps:example.com/",
    b"scheme-gopher-no-slash": b"gopher:example.com/",
    b"scheme-wss-no-slash": b"wss:example.com/",
    b"scheme-mailto-no-slash": b"mailto:example.com/",
    b"scheme-data-no-slash": b"data:text/plain,foo",
    b"userinfo": b"http://user:pass@foo:21/bar;par?b#c",
    b"host-ipv6": b"http://[2001::1]",
    b"host-ipv6-port": b"http://[2001::1]:80",
    b"port-none-but-colon": b"http://f:/c",
    b"port-0": b"http://f:0/c",
    b"port-00000000000000": b"http://f:00000000000000/c",
    b"port-00000000000000000000080": b"http://f:00000000000000000000080/c",
    b"userinfo-host-port-path": b"http://a:b@c:29/d",
    b"userinfo-username-non-alpha": b"http://foo.com:b@d/",
    b"query-contains-question-mark": b"http://foo/abcd?efgh?ijkl",
    b"fragment-contains-question-mark": b"http://foo/abcd#foo?bar",
    b"path-percent-encoded-dot": b"http://example.com/foo/%2e",
    b"path-percent-encoded-space": b"http://example.com/%20foo",
    b"path-non-ascii": "http://example.com/\u00C2\u00A9zbar".encode('utf-8'),
    b"path-percent-encoded-multiple": b"http://example.com/foo%41%7a",
    b"path-percent-encoded-u0091": b"http://example.com/foo%91",
    b"path-percent-encoded-u0000": b"http://example.com/foo%00",
    b"path-percent-encoded-mixed-case": b"http://example.com/%3A%3a%3C%3c",
    b"path-unicode-han": "http://example.com/\u4F60\u597D\u4F60\u597D".encode('utf-8'),
    b"path-uFEFF": "http://example.com/\uFEFF/foo".encode('utf-8'),
    b"path-u202E-u202D": "http://example.com/\u202E/foo/\u202D/bar".encode('utf-8'),
    b"host-is-pile-of-poo": "http://💩".encode('utf-8'),
    b"path-contains-pile-of-poo": "http://example.com/foo/💩".encode('utf-8'),
    b"query-contains-pile-of-poo": "http://example.com/foo?💩".encode('utf-8'),
    b"fragment-contains-pile-of-poo": "http://example.com/foo#💩".encode('utf-8'),
    b"host-192.0x00A80001": b"http://192.0x00A80001",
    b"userinfo-username-contains-percent-encoded": b"http://%25DOMAIN:foobar@foodomain.com",
    b"userinfo-empty": b"http://@www.example.com",
    b"userinfo-user-empty": b"http://:b@www.example.com",
    b"userinfo-password-empty": b"http://a:@www.example.com",
    b"host-exotic-whitespace": "http://GOO\u200b\u2060\ufeffgoo.com".encode('utf-8'),
    b"host-exotic-dot": "http://www.foo\u3002bar.com".encode('utf-8'),
    b"host-fullwidth": "http://\uff27\uff4f.com".encode('utf-8'),
    b"host-idn-unicode-han": "http://\u4f60\u597d\u4f60\u597d".encode('utf-8'),
    b"host-IP-address-broken": b"http://192.168.0.257/",
}
valid = valid_absolute.copy()

valid_relative = {
    b"scheme-schemeless-relative": b"//foo/bar",
    b"path-slash-only-relative": b"/",
    b"path-simple-relative": b"/a/b/c",
    b"path-percent-encoded-slash-relative": b"/a%2fc",
    b"path-percent-encoded-slash-plus-slashes-relative": b"/a/%2f/c",
    b"query-empty-no-path-relative": b"?",
    b"fragment-empty-hash-only-no-path-relative": b"#",
    b"fragment-slash-relative": b"#/",
    b"fragment-semicolon-question-mark-relative": b"#;?",
    b"fragment-non-ascii-relative": "#\u03B2".encode('utf-8'),
}
valid.update(valid_relative)
invalid_absolute.update(valid_relative)

valid_relative_colon_dot = {
    b"scheme-none-relative": b"foo.com",
    b"path-colon-relative": b":",
    b"path-leading-colon-letter-relative": b":a",
    b"path-leading-colon-chars-relative": b":foo.com",
    b"path-leading-colon-slash-relative": b":/",
    b"path-leading-colon-hash-relative": b":#",
    b"path-leading-colon-number-relative": b":23",
    b"path-slash-colon-number-relative": b"/:23",
    b"path-leading-colon-colon-relative": b"::",
    b"path-colon-colon-number-relative": b"::23",
    b"path-starts-with-pile-of-poo": "💩http://foo".encode('utf-8'),
    b"path-contains-pile-of-poo": "http💩//:foo".encode('utf-8'),
}
valid.update(valid_relative_colon_dot)

invalid_file = {
    b"scheme-file-backslash": b"file:c:\\foo\\bar.html",
    b"scheme-file-single-slash-c-bar": b"file:/C|/foo/bar",
    b"scheme-file-triple-slash-c-bar": b"file:///C|/foo/bar",
}
invalid.update(invalid_file)

valid_file = {
    b"scheme-file-uppercase": b"File://foo/bar.html",
    b"scheme-file-slash-slash-c-bar": b"file://C|/foo/bar",
    b"scheme-file-slash-slash-abc-bar": b"file://abc|/foo/bar",
    b"scheme-file-host-included": b"file://server/foo/bar",
    b"scheme-file-host-empty": b"file:///foo/bar.txt",
    b"scheme-file-scheme-only": b"file:",
    b"scheme-file-slash-only": b"file:/",
    b"scheme-file-slash-slash-only": b"file://",
    b"scheme-file-slash-slash-slash-only": b"file:///",
    b"scheme-file-no-slash": b"file:test",
}
valid.update(valid_file)
valid_absolute.update(valid_file)

warnings = {
    b"scheme-data-contains-fragment": b"data:text/html,test#test",
}

element_attribute_pairs = [
    b"a href",
    # b"a ping", space-separated list of URLs; tested elsewhere
    b"area href",
    # b"area ping", space-separated list of URLs; tested elsewhere
    b"audio src",
    b"base href",
    b"blockquote cite",
    b"button formaction",
    b"del cite",
    b"embed src",
    b"form action",
    b"iframe src",
    b"img src", # srcset is tested elsewhere
    b"input formaction", # type=submit, type=image
    b"input src", # type=image
    b"input value", # type=url
    b"ins cite",
    b"link href",
    b"object data",
    b"q cite",
    b"script src",
    b"source src",
    b"track src",
    b"video poster",
    b"video src",
]

template = b"<!DOCTYPE html>\n<meta charset=utf-8>\n"

def write_novalid_files():
    for el, attr in (pair.split() for pair in element_attribute_pairs):
        for desc, url in invalid.items():
            if (b"area" == el):
                f = open(os.path.join(ccdir, "html/elements/area/href/%s-novalid.html" % desc.decode()), 'wb')
                f.write(template + b'<title>invalid href: %b</title>\n' % desc)
                f.write(b'<map name=foo><%b %b="%b" alt></map>\n' % (el, attr, url))
                f.close()
            elif (b"base" == el or b"embed" == el):
                f = open(os.path.join(ccdir, "html/elements/%s/%s/%s-novalid.html" % (el.decode(), attr.decode(), desc.decode())), 'wb')
                f.write(template + b'<title>invalid %b: %b</title>\n' % (attr, desc))
                f.write(b'<%b %b="%b">\n' % (el, attr, url))
                f.close()
            elif (b"img" == el):
                f = open(os.path.join(ccdir, "html/elements/img/src/%s-novalid.html" % desc.decode()), 'wb')
                f.write(template + b'<title>invalid src: %b</title>\n' %  desc)
                f.write(b'<img src="%b" alt>\n' % url)
                f.close()
            elif (b"input" == el and b"src" == attr):
                f = open(os.path.join(ccdir, "html/elements/input/type-image-src/%s-novalid.html" % desc.decode()), 'wb')
                f.write(template + b'<title>invalid src: %b</title>\n' % desc)
                f.write(b'<%b type=image alt="foo" %b="%b">\n' % (el, attr, url))
                f.close()
            elif (b"input" == el and b"formaction" == attr):
                f = open(os.path.join(ccdir, "html/elements/input/type-submit-formaction/%s-novalid.html" % desc.decode()), 'wb')
                f.write(template + b'<title>invalid formaction: %b</title>\n' % desc)
                f.write(b'<%b type=submit %b="%b">\n' % (el, attr, url))
                f.close()
                f = open(os.path.join(ccdir, "html/elements/input/type-image-formaction/%s-novalid.html" % desc.decode()), 'wb')
                f.write(template + b'<title>invalid formaction: %b</title>\n' % desc)
                f.write(b'<%b type=image alt="foo" %b="%b">\n' % (el, attr, url))
                f.close()
            elif (b"input" == el and b"value" == attr):
                f = open(os.path.join(ccdir, "html/elements/input/type-url-value/%s-novalid.html" % desc.decode()), 'wb')
                f.write(template + b'<title>invalid value attribute: %b</title>\n' % desc)
                f.write(b'<%b type=url %b="%b">\n' % (el, attr, url))
                f.close()
            elif (b"link" == el):
                f = open(os.path.join(ccdir, "html/elements/link/href/%s-novalid.html" % desc.decode()), 'wb')
                f.write(template + b'<title>invalid href: %b</title>\n' %  desc)
                f.write(b'<link href="%b" rel=help>\n' % url)
                f.close()
            elif (b"source" == el or b"track" == el):
                f = open(os.path.join(ccdir, "html/elements/%s/%s/%s-novalid.html" % (el.decode(), attr.decode(), desc.decode())), 'wb')
                f.write(template + b'<title>invalid %b: %b</title>\n' % (attr, desc))
                f.write(b'<video><%b %b="%b"></video>\n' % (el, attr, url))
                f.close()
            else:
                f = open(os.path.join(ccdir, "html/elements/%s/%s/%s-novalid.html" % (el.decode(), attr.decode(), desc.decode())), 'wb')
                f.write(template + b'<title>invalid %b: %b</title>\n' % (attr, desc))
                f.write(b'<%b %b="%b"></%b>\n' % (el, attr, url, el))
                f.close()
    for desc, url in invalid.items():
        f = open(os.path.join(ccdir, "html/microdata/itemid/%s-novalid.html" % desc.decode()), 'wb')
        f.write(template + b'<title>invalid itemid: %b</title>\n' % desc)
        f.write(b'<div itemid="%b" itemtype="http://foo" itemscope></div>\n' % url)
        f.close()
    for desc, url in invalid_absolute.items():
        f = open(os.path.join(ccdir, "html/microdata/itemtype/%s-novalid.html" % desc.decode()), 'wb')
        f.write(template + b'<title>invalid itemtype: %b</title>\n' % desc)
        f.write(b'<div itemtype="%b" itemscope></div>\n' % url)
        f.close()
        f = open(os.path.join(ccdir, "html/elements/input/type-url-value/%s-novalid.html" % desc.decode()), 'wb')
        f.write(template + b'<title>invalid value attribute: %b</title>\n' %desc)
        f.write(b'<input type=url value="%b">\n' % url)
        f.close()

def write_haswarn_files():
    for el, attr in (pair.split() for pair in element_attribute_pairs):
        for desc, url in warnings.items():
            if (b"area" == el):
                f = open(os.path.join(ccdir, "html/elements/area/href/%s-haswarn.html" % desc.decode()), 'wb')
                f.write(template + b'<title>%b warning: %b</title>\n' % (attr, desc))
                f.write(b'<map name=foo><%b %b="%b" alt></map>\n' % (el, attr, url))
                f.close()
            elif (b"base" == el or b"embed" == el):
                f = open(os.path.join(ccdir, "html/elements/%s/%s/%s-haswarn.html" % (el.decode(), attr.decode(), desc.decode())), 'wb')
                f.write(template + b'<title>%b warning: %b</title>\n' % (attr, desc))
                f.write(b'<%b %b="%b">\n' % (el, attr, url))
                f.close()
            elif (b"img" == el):
                f = open(os.path.join(ccdir, "html/elements/img/src/%s-haswarn.html" % desc.decode()), 'wb')
                f.write(template + b'<title>%b warning: %b</title>\n' % (attr, desc))
                f.write(b'<%b %b="%b" alt>\n' % (el, attr, url))
                f.close()
            elif (b"input" == el and b"src" == attr):
                f = open(os.path.join(ccdir, "html/elements/input/type-image-src/%s-haswarn.html" % desc.decode()), 'wb')
                f.write(template + b'<title>%b warning: %b</title>\n' % (attr, desc))
                f.write(b'<%b type=image alt="foo" %b="%b">\n' % (el, attr, url))
                f.close()
            elif (b"input" == el and b"formaction" == attr):
                f = open(os.path.join(ccdir, "html/elements/input/type-submit-formaction/%s-haswarn.html" % desc.decode()), 'wb')
                f.write(template + b'<title>%b warning: %b</title>\n' % (attr, desc))
                f.write(b'<%b type=submit %b="%b">\n' % (el, attr, url))
                f.close()
                f = open(os.path.join(ccdir, "html/elements/input/type-image-formaction/%s-haswarn.html" % desc.decode()), 'wb')
                f.write(template + b'<title>%b warning: %b</title>\n' % (attr, desc))
                f.write(b'<%b type=image alt="foo" %b="%b">\n' % (el, attr, url))
                f.close()
            elif (b"input" == el and b"value" == attr):
                f = open(os.path.join(ccdir, "html/elements/input/type-url-value/%s-haswarn.html" % desc.decode()), 'wb')
                f.write(template + b'<title>%b warning: %b</title>\n' % (attr, desc))
                f.write(b'<%b type=url %b="%b">\n' % (el, attr, url))
                f.close()
            elif (b"link" == el):
                f = open(os.path.join(ccdir, "html/elements/link/href/%s-haswarn.html" % desc.decode()), 'wb')
                f.write(template + b'<title>%b warning: %b</title>\n' % (attr, desc))
                f.write(b'<%b %b="%b" rel=help>\n' % (el, attr, url))
                f.close()
            elif (b"source" == el or b"track" == el):
                f = open(os.path.join(ccdir, "html/elements/%s/%s/%s-haswarn.html" % (el.decode(), attr.decode(), desc.decode())), 'wb')
                f.write(template + b'<title>%b warning: %b</title>\n' % (attr, desc))
                f.write(b'<video><%b %b="%b"></video>\n' % (el, attr, url))
                f.close()
            else:
                f = open(os.path.join(ccdir, "html/elements/%s/%s/%s-haswarn.html" % (el.decode(), attr.decode(), desc.decode())), 'wb')
                f.write(template + b'<title>%b warning: %b</title>\n' % (url, desc))
                f.write(b'<%b %b="%b"></%b>\n' % (el, attr, url, el))
                f.close()
    for desc, url in warnings.items():
        f = open(os.path.join(ccdir, "html/microdata/itemtype-%s-haswarn.html" % desc.decode()), 'wb')
        f.write(template + b'<title>warning: %b</title>\n' % desc)
        f.write(b'<div itemtype="%b" itemscope></div>\n' % url)
        f.close()
        f = open(os.path.join(ccdir, "html/microdata/itemid-%s-haswarn.html" % desc.decode()), 'wb')
        f.write(template + b'<title>warning: %b</title>\n' % desc)
        f.write(b'<div itemid="%b" itemtype="http://foo" itemscope></div>\n' % url)
        f.close()

def write_isvalid_files():
    for el, attr in (pair.split() for pair in element_attribute_pairs):
        if (b"base" == el):
            continue
        if (b"html" == el):
            continue
        elif (b"input" == el and b"value" == attr):
            continue
        elif (b"input" == el and b"formaction" == attr):
            fs = open(os.path.join(ccdir, "html/elements/input/type-submit-formaction-isvalid.html"), 'wb')
            fs.write(template + b'<title>valid formaction</title>\n')
            fi = open(os.path.join(ccdir, "html/elements/input/type-image-formaction-isvalid.html"), 'wb')
            fi.write(template + b'<title>valid formaction</title>\n')
        elif (b"input" == el and b"src" == attr):
            f = open(os.path.join(ccdir, "html/elements/input/type-image-src-isvalid.html"), 'wb')
            f.write(template + b'<title>valid src</title>\n')
        else:
            f = open(os.path.join(ccdir, "html/elements/%s/%s-isvalid.html" % (el.decode(), attr.decode())), 'wb')
            f.write(template + b'<title>valid %b</title>\n' % attr)
        for desc, url in valid.items():
            if (b"area" == el):
                f.write(b'<map name=foo><%b %b="%b" alt></map><!-- %b -->\n' % (el, attr, url, desc))
            elif (b"embed" == el):
                f.write(b'<%b %b="%b"><!-- %b -->\n' % (el, attr, url, desc))
            elif (b"img" == el):
                f.write(b'<%b %b="%b" alt><!-- %b -->\n' % (el, attr, url, desc))
            elif (b"input" == el and b"src" == attr):
                f.write(b'<%b type=image alt="foo" %b="%b"><!-- %b -->\n' % (el, attr, url, desc))
            elif (b"input" == el and b"formaction" == attr):
                fs.write(b'<%b type=submit %b="%b"><!-- %b -->\n' % (el, attr, url, desc))
                fi.write(b'<%b type=image alt="foo" %b="%b"><!-- %b -->\n' % (el, attr, url, desc))
            elif (b"link" == el):
                f.write(b'<%b %b="%b" rel=help><!-- %b -->\n' % (el, attr, url, desc))
            elif (b"source" == el or b"track" == el):
                f.write(b'<video><%b %b="%b"></video><!-- %b -->\n' % (el, attr, url, desc))
            else:
                f.write(b'<%b %b="%b"></%b><!-- %b -->\n' % (el, attr, url, el, desc))
        if (b"input" == el and b"formaction" == attr):
            fs.close()
            fi.close()
        else:
            if (b"a" == el and b"href" == attr):
                f.write(b'<a href=""></a><!-- empty-href -->\n')
            f.close()
    for desc, url in valid.items():
        f = open(os.path.join(ccdir, "html/elements/base/href/%s-isvalid.html" % desc.decode()), 'wb')
        f.write(template + b'<title>valid href: %b</title>\n' % desc)
        f.write(b'<base href="%b">\n' % url)
        f.close()
    f = open(os.path.join(ccdir, "html/elements/meta/refresh-isvalid.html"), 'wb')
    f.write(template + b'<title>valid meta refresh</title>\n')
    for desc, url in valid.items():
        f.write(b'<meta http-equiv=refresh content="0; URL=%b"><!-- %b -->\n' % (url, desc))
    f.close()
    f = open(os.path.join(ccdir, "html/microdata/itemid-isvalid.html"), 'wb')
    f.write(template + b'<title>valid itemid</title>\n')
    for desc, url in valid.items():
        f.write(b'<div itemid="%b" itemtype="http://foo" itemscope></div><!-- %b -->\n' % (url, desc))
    f.close()
    f = open(os.path.join(ccdir, "html/microdata/itemtype-isvalid.html"), 'wb')
    f.write(template + b'<title>valid itemtype</title>\n')
    for desc, url in valid_absolute.items():
        f.write(b'<div itemtype="%b" itemscope></div><!-- %b -->\n' % (url, desc))
    f.close()
    f = open(os.path.join(ccdir, "html/elements/input/type-url-value-isvalid.html"), 'wb')
    f.write(template + b'<title>valid value attribute</title>\n')
    for desc, url in valid_absolute.items():
        f.write(b'<input type=url value="%b"><!-- %b -->\n' % (url, desc))
    f.close()

write_novalid_files()
write_haswarn_files()
write_isvalid_files()
# vim: ts=4:sw=4
