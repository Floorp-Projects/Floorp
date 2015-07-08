import base64
import httplib
import socket
import sys
import traceback
import urllib
import urllib2
from xml.dom.minidom import parseString

from mozharness.base.log import FATAL


class BouncerSubmitterMixin(object):
    def query_credentials(self):
        if self.credentials:
            return self.credentials
        global_dict = {}
        local_dict = {}
        execfile(self.config["credentials_file"], global_dict, local_dict)
        self.credentials = (local_dict["tuxedoUsername"],
                            local_dict["tuxedoPassword"])
        return self.credentials

    def api_call(self, route, data, error_level=FATAL, retry_config=None):
        retry_args = dict(
            failure_status=None,
            retry_exceptions=(urllib2.HTTPError, urllib2.URLError,
                              httplib.BadStatusLine,
                              socket.timeout, socket.error),
            error_message="call to %s failed" % (route),
            error_level=error_level,
        )

        if retry_config:
            retry_args.update(retry_config)

        return self.retry(
            self._api_call,
            args=(route, data),
            **retry_args
        )

    def _api_call(self, route, data):
        api_prefix = self.config["bouncer-api-prefix"]
        api_url = "%s/%s" % (api_prefix, route)
        request = urllib2.Request(api_url)
        if data:
            post_data = urllib.urlencode(data, doseq=True)
            request.add_data(post_data)
            self.info("POST data: %s" % post_data)
        credentials = self.query_credentials()
        if credentials:
            auth = base64.encodestring('%s:%s' % credentials)
            request.add_header("Authorization", "Basic %s" % auth.strip())
        try:
            self.info("Submitting to %s" % api_url)
            res = urllib2.urlopen(request, timeout=60).read()
            self.info("Server response")
            self.info(res)
            return res
        except urllib2.HTTPError as e:
            self.warning("Cannot access %s" % api_url)
            traceback.print_exc(file=sys.stdout)
            self.warning("Returned page source:")
            self.warning(e.read())
            raise
        except urllib2.URLError:
            traceback.print_exc(file=sys.stdout)
            self.warning("Cannot access %s" % api_url)
            raise
        except socket.timeout as e:
            self.warning("Timed out accessing %s: %s" % (api_url, e))
            raise
        except socket.error as e:
            self.warning("Socket error when accessing %s: %s" % (api_url, e))
            raise
        except httplib.BadStatusLine as e:
            self.warning('BadStatusLine accessing %s: %s' % (api_url, e))
            raise

    def product_exists(self, product_name):
        self.info("Checking if %s already exists" % product_name)
        res = self.api_call("product_show?product=%s" %
                            urllib.quote(product_name), data=None)
        try:
            xml = parseString(res)
            # API returns <products/> if the product doesn't exist
            products_found = len(xml.getElementsByTagName("product"))
            self.info("Products found: %s" % products_found)
            return bool(products_found)
        except Exception as e:
            self.warning("Error parsing XML: %s" % e)
            self.warning("Assuming %s does not exist" % product_name)
            # ignore XML parsing errors
            return False

    def api_add_product(self, product_name, add_locales, ssl_only=False):
        data = {
            "product": product_name,
        }
        if self.locales and add_locales:
            data["languages"] = self.locales
        if ssl_only:
            # Send "true" as a string
            data["ssl_only"] = "true"
        self.api_call("product_add/", data)

    def api_add_location(self, product_name, bouncer_platform, path):
        data = {
            "product": product_name,
            "os": bouncer_platform,
            "path": path,
        }
        self.api_call("location_add/", data)
