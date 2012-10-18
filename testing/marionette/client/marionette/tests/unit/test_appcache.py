#Copyright 2007-2009 WebDriver committers
#Copyright 2007-2009 Google Inc.
#
#Licensed under the Apache License, Version 2.0 (the "License");
#you may not use this file except in compliance with the License.
#You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
#Unless required by applicable law or agreed to in writing, software
#distributed under the License is distributed on an "AS IS" BASIS,
#WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#See the License for the specific language governing permissions and
#limitations under the License.

from application_cache import ApplicationCache
from marionette_test import MarionetteTestCase


class AppCacheTests(MarionetteTestCase):

    def testWeCanGetTheStatusOfTheAppCache(self):
        test_url = self.marionette.absolute_url('html5Page')
        self.marionette.navigate(test_url)
        app_cache = self.marionette.application_cache

        status = app_cache.status 
        while status == ApplicationCache.DOWNLOADING:
            status = app_cache.status

        self.assertEquals(ApplicationCache.UNCACHED, app_cache.status)
