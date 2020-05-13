import json
import os.path
import shutil
import socket
import tempfile
import warnings

import pytest
import trustme

from dummyserver.testcase import HTTPDummyProxyTestCase, IPv6HTTPDummyProxyTestCase
from dummyserver.server import DEFAULT_CA, HAS_IPV6, get_unreachable_address
from .. import TARPIT_HOST, requires_network

from urllib3._collections import HTTPHeaderDict
from urllib3.poolmanager import proxy_from_url, ProxyManager
from urllib3.exceptions import (
    MaxRetryError,
    SSLError,
    ProxyError,
    ConnectTimeoutError,
    InvalidProxyConfigurationWarning,
)
from urllib3.connectionpool import connection_from_url, VerifiedHTTPSConnection

from test import SHORT_TIMEOUT, LONG_TIMEOUT

# Retry failed tests
pytestmark = pytest.mark.flaky


class TestHTTPProxyManager(HTTPDummyProxyTestCase):
    @classmethod
    def setup_class(cls):
        super(TestHTTPProxyManager, cls).setup_class()
        cls.http_url = "http://%s:%d" % (cls.http_host, cls.http_port)
        cls.http_url_alt = "http://%s:%d" % (cls.http_host_alt, cls.http_port)
        cls.https_url = "https://%s:%d" % (cls.https_host, cls.https_port)
        cls.https_url_alt = "https://%s:%d" % (cls.https_host_alt, cls.https_port)
        cls.proxy_url = "http://%s:%d" % (cls.proxy_host, cls.proxy_port)

        # This URL is used only to test that a warning is
        # raised due to an improper config. urllib3 doesn't
        # support HTTPS proxies in v1.25.*
        cls.https_proxy_url = "https://%s:%d" % (cls.proxy_host, cls.proxy_port)

        # Generate another CA to test verification failure
        cls.certs_dir = tempfile.mkdtemp()
        bad_ca = trustme.CA()

        cls.bad_ca_path = os.path.join(cls.certs_dir, "ca_bad.pem")
        bad_ca.cert_pem.write_to_path(cls.bad_ca_path)

    @classmethod
    def teardown_class(cls):
        super(TestHTTPProxyManager, cls).teardown_class()
        shutil.rmtree(cls.certs_dir)

    def test_basic_proxy(self):
        with proxy_from_url(self.proxy_url, ca_certs=DEFAULT_CA) as http:
            r = http.request("GET", "%s/" % self.http_url)
            assert r.status == 200

            r = http.request("GET", "%s/" % self.https_url)
            assert r.status == 200

    def test_https_proxy_warning(self):
        with proxy_from_url(self.https_proxy_url, ca_certs=DEFAULT_CA) as http:
            with warnings.catch_warnings(record=True) as warn:
                r = http.request("GET", "%s/" % self.https_url)

            assert r.status == 200
            assert len(warn) == 1
            assert warn[0].category == InvalidProxyConfigurationWarning
            assert str(warn[0].message) == (
                "Your proxy configuration specified an HTTPS scheme for the proxy. "
                "Are you sure you want to use HTTPS to contact the proxy? "
                "This most likely indicates an error in your configuration. "
                "Read this issue for more info: "
                "https://github.com/urllib3/urllib3/issues/1850"
            )

    def test_nagle_proxy(self):
        """ Test that proxy connections do not have TCP_NODELAY turned on """
        with ProxyManager(self.proxy_url) as http:
            hc2 = http.connection_from_host(self.http_host, self.http_port)
            conn = hc2._get_conn()
            try:
                hc2._make_request(conn, "GET", "/")
                tcp_nodelay_setting = conn.sock.getsockopt(
                    socket.IPPROTO_TCP, socket.TCP_NODELAY
                )
                assert tcp_nodelay_setting == 0, (
                    "Expected TCP_NODELAY for proxies to be set "
                    "to zero, instead was %s" % tcp_nodelay_setting
                )
            finally:
                conn.close()

    def test_proxy_conn_fail(self):
        host, port = get_unreachable_address()
        with proxy_from_url(
            "http://%s:%s/" % (host, port), retries=1, timeout=LONG_TIMEOUT
        ) as http:
            with pytest.raises(MaxRetryError):
                http.request("GET", "%s/" % self.https_url)
            with pytest.raises(MaxRetryError):
                http.request("GET", "%s/" % self.http_url)

            with pytest.raises(MaxRetryError) as e:
                http.request("GET", "%s/" % self.http_url)
            assert type(e.value.reason) == ProxyError

    def test_oldapi(self):
        with ProxyManager(
            connection_from_url(self.proxy_url), ca_certs=DEFAULT_CA
        ) as http:
            r = http.request("GET", "%s/" % self.http_url)
            assert r.status == 200

            r = http.request("GET", "%s/" % self.https_url)
            assert r.status == 200

    def test_proxy_verified(self):
        with proxy_from_url(
            self.proxy_url, cert_reqs="REQUIRED", ca_certs=self.bad_ca_path
        ) as http:
            https_pool = http._new_pool("https", self.https_host, self.https_port)
            with pytest.raises(MaxRetryError) as e:
                https_pool.request("GET", "/", retries=0)
            assert isinstance(e.value.reason, SSLError)
            assert "certificate verify failed" in str(e.value.reason), (
                "Expected 'certificate verify failed', instead got: %r" % e.value.reason
            )

            http = proxy_from_url(
                self.proxy_url, cert_reqs="REQUIRED", ca_certs=DEFAULT_CA
            )
            https_pool = http._new_pool("https", self.https_host, self.https_port)

            conn = https_pool._new_conn()
            assert conn.__class__ == VerifiedHTTPSConnection
            https_pool.request("GET", "/")  # Should succeed without exceptions.

            http = proxy_from_url(
                self.proxy_url, cert_reqs="REQUIRED", ca_certs=DEFAULT_CA
            )
            https_fail_pool = http._new_pool("https", "127.0.0.1", self.https_port)

            with pytest.raises(MaxRetryError) as e:
                https_fail_pool.request("GET", "/", retries=0)
            assert isinstance(e.value.reason, SSLError)
            assert "doesn't match" in str(e.value.reason)

    def test_redirect(self):
        with proxy_from_url(self.proxy_url) as http:
            r = http.request(
                "GET",
                "%s/redirect" % self.http_url,
                fields={"target": "%s/" % self.http_url},
                redirect=False,
            )

            assert r.status == 303

            r = http.request(
                "GET",
                "%s/redirect" % self.http_url,
                fields={"target": "%s/" % self.http_url},
            )

            assert r.status == 200
            assert r.data == b"Dummy server!"

    def test_cross_host_redirect(self):
        with proxy_from_url(self.proxy_url) as http:
            cross_host_location = "%s/echo?a=b" % self.http_url_alt
            with pytest.raises(MaxRetryError):
                http.request(
                    "GET",
                    "%s/redirect" % self.http_url,
                    fields={"target": cross_host_location},
                    retries=0,
                )

            r = http.request(
                "GET",
                "%s/redirect" % self.http_url,
                fields={"target": "%s/echo?a=b" % self.http_url_alt},
                retries=1,
            )
            assert r._pool.host != self.http_host_alt

    def test_cross_protocol_redirect(self):
        with proxy_from_url(self.proxy_url, ca_certs=DEFAULT_CA) as http:
            cross_protocol_location = "%s/echo?a=b" % self.https_url
            with pytest.raises(MaxRetryError):
                http.request(
                    "GET",
                    "%s/redirect" % self.http_url,
                    fields={"target": cross_protocol_location},
                    retries=0,
                )

            r = http.request(
                "GET",
                "%s/redirect" % self.http_url,
                fields={"target": "%s/echo?a=b" % self.https_url},
                retries=1,
            )
            assert r._pool.host == self.https_host

    def test_headers(self):
        with proxy_from_url(
            self.proxy_url,
            headers={"Foo": "bar"},
            proxy_headers={"Hickory": "dickory"},
            ca_certs=DEFAULT_CA,
        ) as http:

            r = http.request_encode_url("GET", "%s/headers" % self.http_url)
            returned_headers = json.loads(r.data.decode())
            assert returned_headers.get("Foo") == "bar"
            assert returned_headers.get("Hickory") == "dickory"
            assert returned_headers.get("Host") == "%s:%s" % (
                self.http_host,
                self.http_port,
            )

            r = http.request_encode_url("GET", "%s/headers" % self.http_url_alt)
            returned_headers = json.loads(r.data.decode())
            assert returned_headers.get("Foo") == "bar"
            assert returned_headers.get("Hickory") == "dickory"
            assert returned_headers.get("Host") == "%s:%s" % (
                self.http_host_alt,
                self.http_port,
            )

            r = http.request_encode_url("GET", "%s/headers" % self.https_url)
            returned_headers = json.loads(r.data.decode())
            assert returned_headers.get("Foo") == "bar"
            assert returned_headers.get("Hickory") is None
            assert returned_headers.get("Host") == "%s:%s" % (
                self.https_host,
                self.https_port,
            )

            r = http.request_encode_body("POST", "%s/headers" % self.http_url)
            returned_headers = json.loads(r.data.decode())
            assert returned_headers.get("Foo") == "bar"
            assert returned_headers.get("Hickory") == "dickory"
            assert returned_headers.get("Host") == "%s:%s" % (
                self.http_host,
                self.http_port,
            )

            r = http.request_encode_url(
                "GET", "%s/headers" % self.http_url, headers={"Baz": "quux"}
            )
            returned_headers = json.loads(r.data.decode())
            assert returned_headers.get("Foo") is None
            assert returned_headers.get("Baz") == "quux"
            assert returned_headers.get("Hickory") == "dickory"
            assert returned_headers.get("Host") == "%s:%s" % (
                self.http_host,
                self.http_port,
            )

            r = http.request_encode_url(
                "GET", "%s/headers" % self.https_url, headers={"Baz": "quux"}
            )
            returned_headers = json.loads(r.data.decode())
            assert returned_headers.get("Foo") is None
            assert returned_headers.get("Baz") == "quux"
            assert returned_headers.get("Hickory") is None
            assert returned_headers.get("Host") == "%s:%s" % (
                self.https_host,
                self.https_port,
            )

            r = http.request_encode_body(
                "GET", "%s/headers" % self.http_url, headers={"Baz": "quux"}
            )
            returned_headers = json.loads(r.data.decode())
            assert returned_headers.get("Foo") is None
            assert returned_headers.get("Baz") == "quux"
            assert returned_headers.get("Hickory") == "dickory"
            assert returned_headers.get("Host") == "%s:%s" % (
                self.http_host,
                self.http_port,
            )

            r = http.request_encode_body(
                "GET", "%s/headers" % self.https_url, headers={"Baz": "quux"}
            )
            returned_headers = json.loads(r.data.decode())
            assert returned_headers.get("Foo") is None
            assert returned_headers.get("Baz") == "quux"
            assert returned_headers.get("Hickory") is None
            assert returned_headers.get("Host") == "%s:%s" % (
                self.https_host,
                self.https_port,
            )

    def test_headerdict(self):
        default_headers = HTTPHeaderDict(a="b")
        proxy_headers = HTTPHeaderDict()
        proxy_headers.add("foo", "bar")

        with proxy_from_url(
            self.proxy_url, headers=default_headers, proxy_headers=proxy_headers
        ) as http:
            request_headers = HTTPHeaderDict(baz="quux")
            r = http.request(
                "GET", "%s/headers" % self.http_url, headers=request_headers
            )
            returned_headers = json.loads(r.data.decode())
            assert returned_headers.get("Foo") == "bar"
            assert returned_headers.get("Baz") == "quux"

    def test_proxy_pooling(self):
        with proxy_from_url(self.proxy_url, cert_reqs="NONE") as http:
            for x in range(2):
                http.urlopen("GET", self.http_url)
            assert len(http.pools) == 1

            for x in range(2):
                http.urlopen("GET", self.http_url_alt)
            assert len(http.pools) == 1

            for x in range(2):
                http.urlopen("GET", self.https_url)
            assert len(http.pools) == 2

            for x in range(2):
                http.urlopen("GET", self.https_url_alt)
            assert len(http.pools) == 3

    def test_proxy_pooling_ext(self):
        with proxy_from_url(self.proxy_url) as http:
            hc1 = http.connection_from_url(self.http_url)
            hc2 = http.connection_from_host(self.http_host, self.http_port)
            hc3 = http.connection_from_url(self.http_url_alt)
            hc4 = http.connection_from_host(self.http_host_alt, self.http_port)
            assert hc1 == hc2
            assert hc2 == hc3
            assert hc3 == hc4

            sc1 = http.connection_from_url(self.https_url)
            sc2 = http.connection_from_host(
                self.https_host, self.https_port, scheme="https"
            )
            sc3 = http.connection_from_url(self.https_url_alt)
            sc4 = http.connection_from_host(
                self.https_host_alt, self.https_port, scheme="https"
            )
            assert sc1 == sc2
            assert sc2 != sc3
            assert sc3 == sc4

    @pytest.mark.timeout(0.5)
    @requires_network
    def test_https_proxy_timeout(self):
        with proxy_from_url("https://{host}".format(host=TARPIT_HOST)) as https:
            with pytest.raises(MaxRetryError) as e:
                https.request("GET", self.http_url, timeout=SHORT_TIMEOUT)
            assert type(e.value.reason) == ConnectTimeoutError

    @pytest.mark.timeout(0.5)
    @requires_network
    def test_https_proxy_pool_timeout(self):
        with proxy_from_url(
            "https://{host}".format(host=TARPIT_HOST), timeout=SHORT_TIMEOUT
        ) as https:
            with pytest.raises(MaxRetryError) as e:
                https.request("GET", self.http_url)
            assert type(e.value.reason) == ConnectTimeoutError

    def test_scheme_host_case_insensitive(self):
        """Assert that upper-case schemes and hosts are normalized."""
        with proxy_from_url(self.proxy_url.upper(), ca_certs=DEFAULT_CA) as http:
            r = http.request("GET", "%s/" % self.http_url.upper())
            assert r.status == 200

            r = http.request("GET", "%s/" % self.https_url.upper())
            assert r.status == 200


@pytest.mark.skipif(not HAS_IPV6, reason="Only runs on IPv6 systems")
class TestIPv6HTTPProxyManager(IPv6HTTPDummyProxyTestCase):
    @classmethod
    def setup_class(cls):
        HTTPDummyProxyTestCase.setup_class()
        cls.http_url = "http://%s:%d" % (cls.http_host, cls.http_port)
        cls.http_url_alt = "http://%s:%d" % (cls.http_host_alt, cls.http_port)
        cls.https_url = "https://%s:%d" % (cls.https_host, cls.https_port)
        cls.https_url_alt = "https://%s:%d" % (cls.https_host_alt, cls.https_port)
        cls.proxy_url = "http://[%s]:%d" % (cls.proxy_host, cls.proxy_port)

    def test_basic_ipv6_proxy(self):
        with proxy_from_url(self.proxy_url, ca_certs=DEFAULT_CA) as http:
            r = http.request("GET", "%s/" % self.http_url)
            assert r.status == 200

            r = http.request("GET", "%s/" % self.https_url)
            assert r.status == 200
