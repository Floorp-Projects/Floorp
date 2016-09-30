# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.


from mozautolog import RESTfulAutologTestGroup

from base import Output, count, long_name


class AutologOutput(Output):

    def __init__(self, es_server='buildbot-es.metrics.scl3.mozilla.com:9200',
                 rest_server='http://brasstacks.mozilla.com/autologserver',
                 name='moztest',
                 harness='moztest'):
        self.es_server = es_server
        self.rest_server = rest_server

    def serialize(self, results_collection, file_obj):
        grps = self.make_testgroups(results_collection)
        for g in grps:
            file_obj.write(g.serialize())

    def make_testgroups(self, results_collection):
        testgroups = []
        for context in results_collection.contexts:
            coll = results_collection.subset(lambda t: t.context == context)
            passed = coll.tests_with_result('PASS')
            failed = coll.tests_with_result('UNEXPECTED-FAIL')
            unexpected_passes = coll.tests_with_result('UNEXPECTED-PASS')
            errors = coll.tests_with_result('ERROR')
            skipped = coll.tests_with_result('SKIPPED')
            known_fails = coll.tests_with_result('KNOWN-FAIL')

            testgroup = RESTfulAutologTestGroup(
                testgroup=context.testgroup,
                os=context.os,
                platform=context.arch,
                harness=context.harness,
                server=self.es_server,
                restserver=self.rest_server,
                machine=context.hostname,
                logfile=context.logfile,
            )
            testgroup.add_test_suite(
                testsuite=results_collection.suite_name,
                elapsedtime=coll.time_taken,
                passed=count(passed),
                failed=count(failed) + count(errors) + count(unexpected_passes),
                todo=count(skipped) + count(known_fails),
            )
            testgroup.set_primary_product(
                tree=context.tree,
                revision=context.revision,
                productname=context.product,
                buildtype=context.buildtype,
            )
            # need to call this again since we already used the generator
            for f in coll.tests_with_result('UNEXPECTED-FAIL'):
                testgroup.add_test_failure(
                    test=long_name(f),
                    text='\n'.join(f.output),
                    status=f.result,
                )
            testgroups.append(testgroup)
        return testgroups

    def post(self, data):
        msg = "Must pass in a list returned by make_testgroups."
        for d in data:
            assert isinstance(d, RESTfulAutologTestGroup), msg
            d.submit()
