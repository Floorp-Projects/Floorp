# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys

import mozfile
import mozinstall

import firefox_ui_tests
from firefox_puppeteer.testcases import UpdateTestCase
from firefox_ui_harness.runners import FirefoxUITestRunner


DEFAULT_PREFS = {
    'app.update.log': True,
    'startup.homepage_override_url': 'about:blank',
}


class UpdateTestRunner(FirefoxUITestRunner):

    def __init__(self, **kwargs):
        FirefoxUITestRunner.__init__(self, **kwargs)

        self.original_bin = self.bin

        self.prefs.update(DEFAULT_PREFS)

        # In case of overriding the update URL, set the appropriate preference
        override_url = kwargs.pop('update_override_url', None)
        if override_url:
            self.prefs.update({'app.update.url.override': override_url})

        self.run_direct_update = not kwargs.pop('update_fallback_only', False)
        self.run_fallback_update = not kwargs.pop('update_direct_only', False)

        self.test_handlers = [UpdateTestCase]

    def run_tests(self, tests):
        # Used to store the last occurred exception because we execute
        # run_tests() multiple times
        self.exc_info = None

        failed = 0
        source_folder = self.get_application_folder(self.original_bin)

        results = {}

        def _run_tests(manifest):
            application_folder = None

            try:
                application_folder = self.duplicate_application(source_folder)
                self.bin = mozinstall.get_binary(application_folder, 'Firefox')

                FirefoxUITestRunner.run_tests(self, [manifest])

            except Exception:
                self.exc_info = sys.exc_info()
                self.logger.error('Failure during execution of the update test.',
                                  exc_info=self.exc_info)

            finally:
                self.logger.info('Removing copy of the application at "%s"' % application_folder)
                try:
                    mozfile.remove(application_folder)
                except IOError as e:
                    self.logger.error('Cannot remove copy of application: "%s"' % str(e))

        # Run direct update tests if wanted
        if self.run_direct_update:
            _run_tests(manifest=firefox_ui_tests.manifest_update_direct)
            failed += self.failed
            results['Direct'] = False if self.failed else True

        # Run fallback update tests if wanted
        if self.run_fallback_update:
            _run_tests(manifest=firefox_ui_tests.manifest_update_fallback)
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
