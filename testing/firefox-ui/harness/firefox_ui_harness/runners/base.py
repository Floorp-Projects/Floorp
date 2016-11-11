# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import tempfile

import mozfile
import mozinfo

from marionette import BaseMarionetteTestRunner, MarionetteTestCase


class FirefoxUITestRunner(BaseMarionetteTestRunner):

    def __init__(self, **kwargs):
        super(FirefoxUITestRunner, self).__init__(**kwargs)

        # select the appropriate GeckoInstance
        self.app = 'fxdesktop'

        self.test_handlers = [MarionetteTestCase]

    def duplicate_application(self, application_folder):
        """Creates a copy of the specified binary."""

        if self.workspace:
            target_folder = os.path.join(self.workspace_path, 'application.copy')
        else:
            target_folder = tempfile.mkdtemp('.application.copy')

        self.logger.info('Creating a copy of the application at "%s".' % target_folder)
        mozfile.remove(target_folder)
        shutil.copytree(application_folder, target_folder)

        return target_folder

    def get_application_folder(self, binary):
        """Returns the directory of the application."""
        if mozinfo.isMac:
            end_index = binary.find('.app') + 4
            return binary[:end_index]
        else:
            return os.path.dirname(binary)
