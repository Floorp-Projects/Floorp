
var utils = Cc["@mozilla.org/url-classifier/utils;1"].createInstance(Ci.nsIUrlClassifierUtils);

function testKey(spec, expect)
{
  var uri = iosvc.newURI(spec, null, null);
  do_check_eq('"' + expect + '"', '"' + utils.getKeyForURI(uri) + '"');
}

function run_test() {
//  testKey("http://poseidon.marinet.gr/~elani", "poseidon.marinet.gr/%7Eelani"),
  testKey("http://www.google.com..", "www.google.com/");
  testKey("https://www.yaho%6F.com", "www.yahoo.com/");
  testKey("http://012.034.01.0xa", "10.28.1.10/");
  testKey("ftp://wierd..chars...%0f,%fa", "wierd.chars.%2c/");
  testKey("http://0x18ac89d5/http.www.paypal.com/", "24.172.137.213/http.www.paypal.com/");
  testKey("http://413960661/http.www.paypal.com/", "24.172.137.213/http.www.paypal.com/");
  testKey("http://03053104725/http.www.paypal.com/", "24.172.137.213/http.www.paypal.com/");
  testKey("http://www.barclays.co.uk.brccontrol.assruspede.org.bz/detailsconfirm", "www.barclays.co.uk.brccontrol.assruspede.org.bz/detailsconfirm");
  testKey("http://www.mozilla.org/foo", "www.mozilla.org/foo");
  testKey("http://,=.mozilla.org/foo", "%2c%3d.mozilla.org/foo");
  testKey("http://f00.b4r.mozi=lla.org/", "f00.b4r.mozi%3dlla.org/");
  testKey("http://a-_b.mozilla.org/", "a-%5fb.mozilla.org/");
  testKey("http://z%38bl%61h%%2F.com/", "z8blah%25%2f.com/");
  testKey("http://moZilla.Org/", "mozilla.org/");
  testKey("http://030.0254.0x89d5./", "24.172.137.213/");
  testKey("http://030.0254.0x89d5.../", "24.172.137.213/");
  testKey("http://...030.0254.0x89d5.../", "24.172.137.213/");
  testKey("http://127.0.0.1./", "127.0.0.1/");
  testKey("http://127.0.0.1/", "127.0.0.1/");
  testKey("http://a.b.c.d.e.f.g/path", "a.b.c.d.e.f.g/path");
  testKey("http://a.b.c.d.e.f.g...../path", "a.b.c.d.e.f.g/path");
  testKey("http://a.b.c.d.e.f.g./path", "a.b.c.d.e.f.g/path");
  testKey("http://a.b.c.d.e.f.g./path/to/../", "a.b.c.d.e.f.g/path/");
}
