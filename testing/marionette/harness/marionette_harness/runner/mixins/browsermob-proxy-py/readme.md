browsermob-proxy-py
===================

Python client for the BrowserMob Proxy 2.0 REST API.



How to use with selenium-webdriver
----------------------------------

Manually:

``` python 
from browsermobproxy import Server
server = Server("path/to/browsermob-proxy")
server.start()
proxy = server.create_proxy()

from selenium import webdriver
profile  = webdriver.FirefoxProfile()
profile.set_proxy(proxy.selenium_proxy())
driver = webdriver.Firefox(firefox_profile=profile)


proxy.new_har("google")
driver.get("http://www.google.co.uk")
proxy.har # returns a HAR JSON blob

server.stop()
driver.quit()

```

for Chrome use

```
chrome_options = webdriver.ChromeOptions()
chrome_options.add_argument("--proxy-server={0}".format(proxy.proxy))
browser = webdriver.Chrome(chrome_options = chrome_options)
```

Running Tests
-------------
To run the tests in a CI environment, disable the ones that require human
judgement by using

```bash
$ py.test -m "not human" test
```

If you are going to watch the test, the 'human' ones should display an english
muffin instead of the american flag on the 'pick your version' page. Or at
least it does from Canada.


See also
--------

* http://proxy.browsermob.com/
* https://github.com/webmetrics/browsermob-proxy

Note on Patches/Pull Requests
-----------------------------

* Fork the project.
* Make your feature addition or bug fix.
* Add tests for it. This is important so I don't break it in a
  future version unintentionally.
* Send me a pull request. Bonus points for topic branches.

Copyright
---------

Copyright 2011 David Burns 

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.


