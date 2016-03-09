# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from firefox_puppeteer.ui_base_lib import UIBaseLib


class Panel(UIBaseLib):

    def __eq__(self, other):
        return self.element.get_attribute('id') == other.element.get_attribute('id')

    def __ne__(self, other):
        return self.element.get_attribute('id') != other.element.get_attribute('id')

    def __str__(self):
        return self.element.get_attribute('id')
