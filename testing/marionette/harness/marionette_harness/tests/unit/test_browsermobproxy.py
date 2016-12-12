import datetime

from marionette_harness.runner import BrowserMobTestCase


class TestBrowserMobProxy(BrowserMobTestCase):
    """To run this test, you'll need to download the browsermob-proxy from
       http://bmp.lightbody.net/, and then pass the path to the startup
       script (typically /path/to/browsermob-proxy-2.0.0/bin/browsermob-proxy)
       as the --browsermob-script argument when running runtests.py.

       You can additionally pass --browsermob-port to specify the port that
       the proxy will run on; it defaults to 8080.

       This test is NOT run in CI, as bmp and dependencies aren't available
       there.
    """

    def test_browsermob_proxy_limits(self):
        """This illustrates the use of download limits in the proxy,
           and verifies that it's slower to load a page @100kbps
           than it is to download the same page @1000kbps.
        """
        proxy = self.create_browsermob_proxy()
        proxy.limits({'downstream_kbps': 1000})
        time1 = datetime.datetime.now()
        self.marionette.navigate('http://forecast.weather.gov')
        time2 = datetime.datetime.now()
        proxy.limits({'downstream_kbps': 100})
        time3 = datetime.datetime.now()
        self.marionette.refresh()
        time4 = datetime.datetime.now()
        self.assertTrue(time4 - time3 > time2 - time1,
                        "page load @ 100kbps not slower than page load @ 1000kbps")
