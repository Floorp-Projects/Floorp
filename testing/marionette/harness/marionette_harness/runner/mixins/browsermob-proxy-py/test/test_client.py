from __future__ import absolute_import

import os.path
import pytest
import sys


def setup_module(module):
    sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

class TestClient(object):
    def setup_method(self, method):
        from browsermobproxy.client import Client
        self.client = Client("localhost:9090")

    def teardown_method(self, method):
        self.client.close()

    def test_headers_type(self):
        """
        /proxy/:port/headers needs to take a dictionary
        """
        with pytest.raises(TypeError):
            self.client.headers(['foo'])

    def test_headers_content(self):
        """
        /proxy/:port/headers needs to take a dictionary
        and returns 200 when its successful
        """
        s = self.client.headers({'User-Agent': 'rubber ducks floating in a row'})
        assert(s == 200)

    def test_new_har(self):
        """
        /proxy/:port/har
        and returns 204 when creating a har with a particular name the first time
        and returns 200 and the previous har when creating one with the same name
        """
        status_code, har = self.client.new_har()
        assert(status_code == 204)
        assert(har is None)
        status_code, har = self.client.new_har()
        assert(status_code == 200)
        assert('log' in har)

    def test_new_har(self):
        """
        /proxy/:port/har
        and returns 204 when creating a har with a particular name the first time
        and returns 200 and the previous har when creating one with the same name
        """
        status_code, har = self.client.new_har("elephants")
        assert(status_code == 204)
        assert(har is None)
        status_code, har = self.client.new_har("elephants")
        assert(status_code == 200)
        assert('elephants' == har["log"]["pages"][0]['id'])

    def test_new_page_defaults(self):
        """
        /proxy/:port/pageRef
        adds a new page of 'Page N' when no page name is given
        """
        self.client.new_har()
        self.client.new_page()
        har = self.client.har
        assert(len(har["log"]["pages"]) == 2)
        assert(har["log"]["pages"][1]["id"] == "Page 2")

    def test_new_named_page(self):
        """
        /proxy/:port/pageRef
        adds a new page of 'buttress'
        """
        self.client.new_har()
        self.client.new_page('buttress')
        har = self.client.har
        assert(len(har["log"]["pages"]) == 2)
        assert(har["log"]["pages"][1]["id"] == "buttress")

    def test_single_whitelist(self):
        """
        /proxy/:port/whitelist
        adds a whitelist
        """
        status_code = self.client.whitelist("http://www\\.facebook\\.com/.*", 200)
        assert(status_code == 200)

    def test_multiple_whitelists(self):
        """
        /proxy/:port/whitelist
        adds a whitelist
        """
        status_code = self.client.whitelist("http://www\\.facebook\\.com/.*,http://cdn\\.twitter\\.com", 200)
        assert(status_code == 200)

    def test_blacklist(self):
        """
        /proxy/:port/blacklist
        adds a blacklist
        """
        status_code = self.client.blacklist("http://www\\.facebook\\.com/.*", 200)
        assert(status_code == 200)

    def test_basic_authentication(self):
        """
        /proxy/:port/auth/basic
        adds automatic basic authentication
        """
        status_code = self.client.basic_authentication("www.example.com", "myUsername", "myPassword")
        assert(status_code == 200)

    def test_limits_invalid_key(self):
        """
        /proxy/:port/limits
        pre-sending checking that the parameter is correct
        """
        with pytest.raises(KeyError):
            self.client.limits({"hurray": "explosions"})

    def test_limits_key_no_value(self):
        """
        /proxy/:port/limits
        pre-sending checking that a parameter exists
        """
        with pytest.raises(KeyError):
            self.client.limits({})

    def test_limits_all_key_values(self):
        """
        /proxy/:port/limits
        can send all 3 at once based on the proxy implementation
        """
        limits = {"upstream_kbps": 320, "downstream_kbps": 560, "latency": 30}
        status_code = self.client.limits(limits)
        assert(status_code == 200)

    def test_rewrite(self):
        """
        /proxy/:port/rewrite

        """
        match = "/foo"
        replace = "/bar"
        status_code = self.client.rewrite_url(match, replace)
        assert(status_code == 200)

    def test_close(self):
        """
        /proxy/:port
        close the proxy port
        """
        status_code = self.client.close()
        assert(status_code == 200)
        status_code = self.client.close()
        assert(status_code == 404)

    def test_response_interceptor_with_parsing_js(self):
        """
        /proxy/:port/interceptor/response
        """
        js = 'alert("foo")'
        status_code = self.client.response_interceptor(js)
        assert(status_code == 200)

    def test_response_interceptor_with_invalid_js(self):
        """
        /proxy/:port/interceptor/response
        """
        js = 'alert("foo"'
        status_code = self.client.response_interceptor(js)
        assert(status_code == 500)

    def test_request_interceptor_with_parsing_js(self):
        """
        /proxy/:port/interceptor/request
        """
        js = 'alert("foo")'
        status_code = self.client.request_interceptor(js)
        assert(status_code == 200)

    def test_request_interceptor_with_invalid_js(self):
        """
        /proxy/:port/interceptor/request
        """
        js = 'alert("foo"'
        status_code = self.client.request_interceptor(js)
        assert(status_code == 500)

    def test_timeouts_invalid_timeouts(self):
        """
        /proxy/:port/timeout
        pre-sending checking that the parameter is correct
        """
        with pytest.raises(KeyError):
            self.client.timeouts({"hurray": "explosions"})

    def test_timeouts_key_no_value(self):
        """
        /proxy/:port/timeout
        pre-sending checking that a parameter exists
        """
        with pytest.raises(KeyError):
            self.client.timeouts({})

    def test_timeouts_all_key_values(self):
        """
        /proxy/:port/timeout
        can send all 3 at once based on the proxy implementation
        """
        timeouts = {"request": 2, "read": 2, "connection": 2, "dns": 3}
        status_code = self.client.timeouts(timeouts)
        assert(status_code == 200)

    def test_remap_hosts(self):
        """
        /proxy/:port/hosts
        """
        status_code = self.client.remap_hosts("example.com", "1.2.3.4")
        assert(status_code == 200)

    def test_wait_for_traffic_to_stop(self):
        """
        /proxy/:port/wait
        """
        status_code = self.client.wait_for_traffic_to_stop(2000, 10000)
        assert(status_code == 200)

    def test_clear_dns_cache(self):
        """
        /proxy/:port/dns/cache
        """
        status_code = self.client.clear_dns_cache()
        assert(status_code == 200)

    def test_rewrite_url(self):
        """
        /proxy/:port/rewrite
        """
        status_code = self.client.rewrite_url('http://www.facebook\.com', 'http://microsoft.com')
        assert(status_code == 200)

    def test_retry(self):
        """
        /proxy/:port/retry
        """
        status_code = self.client.retry(4)
        assert(status_code == 200)
