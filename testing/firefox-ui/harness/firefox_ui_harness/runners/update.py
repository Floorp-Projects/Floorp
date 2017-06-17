# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys

import mozfile
import mozinstall

from firefox_ui_harness.runners import FirefoxUITestRunner
from firefox_ui_harness.testcases import UpdateTestCase


DEFAULT_PREFS = {
    # Bug 1355026: Re-enable when support for the new simplified UI update is available
    'app.update.doorhanger': False,
    'app.update.log': True,
    'app.update.staging.enabled': True,
    'startup.homepage_override_url': 'about:blank',
}


class UpdateTestRunner(FirefoxUITestRunner):

    def __init__(self, **kwargs):
        super(UpdateTestRunner, self).__init__(**kwargs)

        self.original_bin = self.bin

        self.prefs.update(DEFAULT_PREFS)

        self.run_direct_update = not kwargs.pop('update_fallback_only', False)
        self.run_fallback_update = not kwargs.pop('update_direct_only', False)

        self.test_handlers = [UpdateTestCase]

        # With bug 1355888 Marionette uses an environment variable to identify
        # if it should be active. It's important especially for restarts of the
        # application to set this, because if it is not present Marionette will
        # not start. To allow updates from builds before this change, the
        # environment variable has to be pre-emptively set.
        # TODO: Can be removed once we no longer have to test updates from
        # Firefox 55.0 and earlier.
        os.environ['MOZ_MARIONETTE'] = '1'

    def run_tests(self, tests):
        # Used to store the last occurred exception because we execute
        # run_tests() multiple times
        self.exc_info = None

        failed = 0
        source_folder = self.get_application_folder(self.original_bin)

        results = {}

        def _run_tests(tags):
            application_folder = None

            try:
                # Backup current tags
                test_tags = self.test_tags

                application_folder = self.duplicate_application(source_folder)
                self.bin = mozinstall.get_binary(application_folder, 'Firefox')

                self.test_tags = tags
                super(UpdateTestRunner, self).run_tests(tests)

            except Exception:
                self.exc_info = sys.exc_info()
                self.logger.error('Failure during execution of the update test.',
                                  exc_info=self.exc_info)

            finally:
                self.test_tags = test_tags

                self.logger.info('Removing copy of the application at "%s"' % application_folder)
                try:
                    mozfile.remove(application_folder)
                except IOError as e:
                    self.logger.error('Cannot remove copy of application: "%s"' % str(e))

        # Run direct update tests if wanted
        if self.run_direct_update:
            _run_tests(tags=['direct'])
            failed += self.failed
            results['Direct'] = False if self.failed else True

        # Run fallback update tests if wanted
        if self.run_fallback_update:
            _run_tests(tags=['fallback'])
            failed += self.failed
            results['Fallback'] = False if self.failed else True

        self.logger.info("Summary of update tests:")
        for test_type, result in results.iteritems():
            self.logger.info("\t%s update test ran and %s" %
                             (test_type, 'PASSED' if result else 'FAILED'))

        # Combine failed tests for all run_test() executions
        self.failed = failed

        # If exceptions happened, re-throw the last one
        if self.exc_info:
            ex_type, exception, tb = self.exc_info
            raise ex_type, exception, tb
