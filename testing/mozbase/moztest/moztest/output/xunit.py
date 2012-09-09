# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.


import xml.dom.minidom as dom

from base import Output, count
from moztest.results import TestResult


class XUnitOutput(Output):
    """ Class for writing xUnit formatted test results in an XML file """

    def serialize(self, results_collection, file_obj):
        """ Writes the xUnit formatted results to the given file object """

        def _extract_xml(test_result, text='', result='Pass'):
            if not isinstance(text, basestring):
                text = '\n'.join(text)

            cls_name = test_result.test_class

            # if the test class is not already created, create it
            if cls_name not in classes:
                cls = doc.createElement('class')
                cls.setAttribute('name', cls_name)
                assembly.appendChild(cls)
                classes[cls_name] = cls

            t = doc.createElement('test')
            t.setAttribute('name', test_result.name)
            t.setAttribute('result', result)

            if result == 'Fail':
                f = doc.createElement('failure')
                st = doc.createElement('stack-trace')
                st.appendChild(doc.createTextNode(text))

                f.appendChild(st)
                t.appendChild(f)

            elif result == 'Skip':
                r = doc.createElement('reason')
                msg = doc.createElement('message')
                msg.appendChild(doc.createTextNode(text))

                r.appendChild(msg)
                t.appendChild(f)

            cls = classes[cls_name]
            cls.appendChild(t)

        doc = dom.Document()

        failed = sum([count(results_collection.tests_with_result(t))
                     for t in TestResult.FAIL_RESULTS])
        passed = count(results_collection.tests_with_result('PASS'))
        skipped = count(results_collection.tests_with_result('SKIPPED'))

        assembly = doc.createElement('assembly')
        assembly.setAttribute('name', results_collection.suite_name)
        assembly.setAttribute('time', str(results_collection.time_taken))
        assembly.setAttribute('total', str(len(results_collection)))
        assembly.setAttribute('passed', str(passed))
        assembly.setAttribute('failed', str(failed))
        assembly.setAttribute('skipped', str(skipped))

        classes = {}  # str -> xml class element

        for tr in results_collection.tests_with_result('ERROR'):
            _extract_xml(tr, text=tr.output, result='Fail')

        for tr in results_collection.tests_with_result('UNEXPECTED-FAIL'):
            _extract_xml(tr, text=tr.output, result='Fail')

        for tr in results_collection.tests_with_result('UNEXPECTED-PASS'):
            _extract_xml(tr, text='UNEXPECTED-PASS', result='Fail')

        for tr in results_collection.tests_with_result('SKIPPED'):
            _extract_xml(tr, text=tr.output, result='Skip')

        for tr in results_collection.tests_with_result('KNOWN-FAIL'):
            _extract_xml(tr, text=tr.output, result='Pass')

        for tr in results_collection.tests_with_result('PASS'):
            _extract_xml(tr, result='Pass')

        for cls in classes.itervalues():
            assembly.appendChild(cls)

        doc.appendChild(assembly)
        file_obj.write(doc.toxml(encoding='utf-8'))
