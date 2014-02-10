# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import base64
import cgi
import datetime
import json
import os
import pkg_resources
import sys

from xmlgen import html
from xmlgen import raw


class HTMLReportingTestRunnerMixin(object):

    def __init__(self, name=None, version=None, html_output=None, **kwargs):
        """
        Name should be the name of the name of the testrunner, version should correspond
        to the testrunner version.
        html_output is the file to output to
        """
        # for HTML output
        self.html_output = html_output
        self.html_name = name
        self.html_version = version
        self.testvars['html_output'] = self.html_output
        self.mixin_run_tests.append(self.html_run_tests)

    def html_run_tests(self, tests):
        if self.html_output:
            # change default encoding to avoid encoding problem for page source
            reload(sys)
            sys.setdefaultencoding('utf-8')
            html_dir = os.path.dirname(os.path.abspath(self.html_output))
            if not os.path.exists(html_dir):
                os.makedirs(html_dir)
            with open(self.html_output, 'w') as f:
                f.write(self.generate_html(self.results))

    def generate_html(self, results_list):
        tests = sum([results.testsRun for results in results_list])
        failures = sum([len(results.failures) for results in results_list])
        expected_failures = sum([len(results.expectedFailures) for results in results_list])
        skips = sum([len(results.skipped) for results in results_list]) + len(self.manifest_skipped_tests)
        errors = sum([len(results.errors) for results in results_list])
        passes = sum([results.passed for results in results_list])
        unexpected_passes = sum([len(results.unexpectedSuccesses) for results in results_list])
        test_time = self.elapsedtime.total_seconds()
        test_logs = []

        def _extract_html_from_result(result):
            _extract_html(
                result=result.result,
                test_name=result.name,
                test_class=result.test_class,
                debug=result.debug,
                output='\n'.join(result.output))

        def _extract_html_from_skipped_manifest_test(test):
            _extract_html(
                result='skipped',
                test_name=test['name'],
                output=test.get('disabled'))

        def _extract_html(result, test_name, test_class='', duration=0,
                          debug=None, output=''):
            additional_html = []
            debug = debug or {}
            links_html = []

            result_map = {
                'KNOWN-FAIL': 'expected failure',
                'PASS': 'passed',
                'UNEXPECTED-FAIL': 'failure',
                'UNEXPECTED-PASS': 'unexpected pass'}

            if result.upper() in ['SKIPPED', 'UNEXPECTED-FAIL', 'KNOWN-FAIL', 'ERROR']:
                if debug.get('screenshot'):
                    screenshot = 'data:image/png;base64,%s' % debug['screenshot']
                    additional_html.append(html.div(
                        html.a(html.img(src=screenshot), href="#"),
                        class_='screenshot'))
                for name, content in debug.items():
                    try:
                        if 'screenshot' in name:
                            href = '#'
                        else:
                            # use base64 to avoid that some browser (such as Firefox, Opera)
                            # treats '#' as the start of another link if the data URL contains.
                            # use 'charset=utf-8' to show special characters like Chinese.
                            href = 'data:text/plain;charset=utf-8;base64,%s' % base64.b64encode(content)
                        links_html.append(html.a(
                            name.title(),
                            class_=name,
                            href=href,
                            target='_blank'))
                        links_html.append(' ')
                    except:
                        pass

                log = html.div(class_='log')
                for line in output.splitlines():
                    separator = line.startswith(' ' * 10)
                    if separator:
                        log.append(line[:80])
                    else:
                        if line.lower().find("error") != -1 or line.lower().find("exception") != -1:
                            log.append(html.span(raw(cgi.escape(line)), class_='error'))
                        else:
                            log.append(raw(cgi.escape(line)))
                    log.append(html.br())
                additional_html.append(log)

            test_logs.append(html.tr([
                html.td(result_map.get(result, result).title(), class_='col-result'),
                html.td(test_class, class_='col-class'),
                html.td(test_name, class_='col-name'),
                html.td(str(duration), class_='col-duration'),
                html.td(links_html, class_='col-links'),
                html.td(additional_html, class_='debug')],
                class_=result_map.get(result, result).lower() + ' results-table-row'))

        for results in results_list:
            [_extract_html_from_result(test) for test in results.tests]

        for test in self.manifest_skipped_tests:
            _extract_html_from_skipped_manifest_test(test)

        generated = datetime.datetime.now()
        doc = html.html(
            html.head(
                html.meta(charset='utf-8'),
                html.title('Test Report'),
                #TODO: must redisgn this to use marionette's resourcs, instead of the caller folder's
                html.style(raw(pkg_resources.resource_string(
                    __name__, os.path.sep.join(['resources', 'htmlreport', 'style.css']))),
                    type='text/css')),
            html.body(
                html.script(raw(pkg_resources.resource_string(
                    __name__, os.path.sep.join(['resources', 'htmlreport', 'jquery.js']))),
                    type='text/javascript'),
                html.script(raw(pkg_resources.resource_string(
                    __name__, os.path.sep.join(['resources', 'htmlreport', 'main.js']))),
                    type='text/javascript'),
                html.p('Report generated on %s at %s by %s %s' % (
                    generated.strftime('%d-%b-%Y'),
                    generated.strftime('%H:%M:%S'),
                    self.html_name, self.html_version)),
                html.h2('Summary'),
                html.p('%i tests ran in %i seconds.' % (tests, test_time),
                       html.br(),
                       html.span('%i passed' % passes, class_='passed'), ', ',
                       html.span('%i skipped' % skips, class_='skipped'), ', ',
                       html.span('%i failed' % failures, class_='failed'), ', ',
                       html.span('%i errors' % errors, class_='error'), '.',
                       html.br(),
                       html.span('%i expected failures' % expected_failures,
                                 class_='expected failure'), ', ',
                       html.span('%i unexpected passes' % unexpected_passes,
                                 class_='unexpected pass'), '.'),
                html.h2('Results'),
                html.table([html.thead(
                    html.tr([
                        html.th('Result', class_='sortable', col='result'),
                        html.th('Class', class_='sortable', col='class'),
                        html.th('Test Name', class_='sortable', col='name'),
                        html.th('Duration', class_='sortable numeric', col='duration'),
                        html.th('Links')]), id='results-table-head'),
                    html.tbody(test_logs, id='results-table-body')], id='results-table')))
        return doc.unicode(indent=2)


class HTMLReportingOptionsMixin(object):

    def __init__(self, **kwargs):
        group = self.add_option_group('htmlreporting')
        group.add_option('--html-output',
                         action='store',
                         dest='html_output',
                         help='html output',
                         metavar='path')


class HTMLReportingTestResultMixin(object):

    def __init__(self, *args, **kwargs):
        self.result_modifiers.append(self.html_modifier)

    def html_modifier(self, test, result_expected, result_actual, output, context):
        test.debug = None
        if result_actual is not 'PASS':
            test.debug = self.gather_debug()
        return result_expected, result_actual, output, context

    def gather_debug(self):
        debug = {}
        try:
            # TODO make screenshot consistant size by using full viewport
            # Bug 883294 - Add ability to take full viewport screenshots
            debug['screenshot'] = self.marionette.screenshot()
            debug['source'] = self.marionette.page_source
            self.marionette.switch_to_frame()
            debug['settings'] = json.dumps(self.marionette.execute_async_script("""
SpecialPowers.addPermission('settings-read', true, document);
var req = window.navigator.mozSettings.createLock().get('*');
req.onsuccess = function() {
  marionetteScriptFinished(req.result);
}""", special_powers=True), sort_keys=True, indent=4, separators=(',', ': '))
        except:
            pass
        return debug

