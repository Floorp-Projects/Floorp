import datetime
import uuid
import os

import taskcluster.utils as subject
import dateutil.parser
import httmock
import mock
import requests

import base
from unittest import TestCase
from hypothesis import given
import hypothesis.strategies as st


# https://docs.python.org/2/library/datetime.html#tzinfo-objects
class UTC(datetime.tzinfo):
    """UTC"""

    def utcoffset(self, dt):
        return datetime.timedelta(0)

    def tzname(self, dt):
        return 'UTC'

    def dst(self, dt):
        return datetime.timedelta(0)


utc = UTC()


class StringDateTests(base.TCTest):
    def test_naive(self):
        dateObj = datetime.datetime(
            year=2000,
            month=1,
            day=1,
            hour=1,
            minute=1,
            second=1
        )
        expected = '2000-01-01T01:01:01Z'
        actual = subject.stringDate(dateObj)
        self.assertEqual(expected, actual)

    def test_aware(self):
        dateObj = datetime.datetime(
            year=2000,
            month=1,
            day=1,
            hour=1,
            minute=1,
            second=1,
            tzinfo=utc
        )
        expected = '2000-01-01T01:01:01Z'
        actual = subject.stringDate(dateObj)
        self.assertEqual(expected, actual)


class DumpJsonTests(base.TCTest):
    def test_has_no_spaces(self):
        expected = [
            '{"test":"works","doesit":"yes"}',
            '{"doesit":"yes","test":"works"}'
        ]
        actual = subject.dumpJson({'test': 'works', 'doesit': 'yes'})
        self.assertTrue(actual in expected)

    def test_serializes_naive_date(self):
        dateObj = datetime.datetime(
            year=2000,
            month=1,
            day=1,
            hour=1,
            minute=1,
            second=1
        )
        expected = '{"date":"2000-01-01T01:01:01Z"}'
        actual = subject.dumpJson({'date': dateObj})
        self.assertEqual(expected, actual)

    def test_serializes_aware_date(self):
        dateObj = datetime.datetime(
            year=2000,
            month=1,
            day=1,
            hour=1,
            minute=1,
            second=1,
            tzinfo=utc
        )
        expected = '{"date":"2000-01-01T01:01:01Z"}'
        actual = subject.dumpJson({'date': dateObj})
        self.assertEqual(expected, actual)


class TestBase64Utils(base.TCTest):
    def test_encode_string_for_b64_header(self):
        # Really long strings trigger newlines every 72 ch
        expected = 'YWJjZGVm' * 500
        expected = expected.encode('ascii')
        actual = subject.encodeStringForB64Header('abcdef' * 500)
        self.assertEqual(expected, actual)

    def test_makeb64urlsafe(self):
        expected = b'-_'
        actual = subject.makeB64UrlSafe('+/')
        self.assertEqual(expected, actual)

    def test_makeb64urlunsafe(self):
        expected = b'+/'
        actual = subject.makeB64UrlUnsafe('-_')
        self.assertEqual(expected, actual)


class TestSlugId(base.TCTest):
    def test_slug_id_is_always_nice(self):
        with mock.patch('uuid.uuid4') as p:
            # first bit of uuid set, which should get unset
            p.return_value = uuid.UUID('bed97923-7616-4ec8-85ed-4b695f67ac2e')
            expected = b'Ptl5I3YWTsiF7UtpX2esLg'
            actual = subject.slugId()
            self.assertEqual(expected, actual)

    def test_slug_id_nice_stays_nice(self):
        with mock.patch('uuid.uuid4') as p:
            # first bit of uuid unset, should remain unset
            p.return_value = uuid.UUID('3ed97923-7616-4ec8-85ed-4b695f67ac2e')
            expected = b'Ptl5I3YWTsiF7UtpX2esLg'
            actual = subject.slugId()
            self.assertEqual(expected, actual)


class TestMakeSingleHttpRequest(base.TCTest):
    def test_success_no_payload(self):
        @httmock.all_requests
        def response_content(url, request):
            return {'status_code': 200, 'content': {}}

        with httmock.HTTMock(response_content):
            d = subject.makeSingleHttpRequest('GET', 'http://www.example.com', {}, {})
            self.assertEqual(d.json(), {})
            self.assertEqual(d.status_code, 200)
            d.raise_for_status()

    def test_success_payload(self):
        @httmock.all_requests
        def response_content(url, request):
            self.assertEqual(request.body, 'i=j')
            return {'status_code': 200, 'content': {'k': 'l'}}

        with httmock.HTTMock(response_content):
            d = subject.makeSingleHttpRequest('GET', 'http://www.example.com', {'i': 'j'}, {})
            self.assertEqual(d.json(), {'k': 'l'})
            self.assertEqual(d.status_code, 200)
            d.raise_for_status()

    def test_failure(self):
        @httmock.all_requests
        def response_content(url, requet):
            return {'status_code': 404}

        with httmock.HTTMock(response_content):
            d = subject.makeSingleHttpRequest('GET', 'http://www.example.com', {}, {})
            with self.assertRaises(requests.exceptions.RequestException):
                d.raise_for_status()


class TestPutfile(base.TCTest):
    def test_success_put_file(self):
        with mock.patch.object(subject, 'makeSingleHttpRequest') as p:
            class FakeResp:
                status_code = 200

                def raise_for_status(self):
                    pass

            p.return_value = FakeResp()
            subject.putFile('setup.py', 'http://www.example.com', 'text/plain')
            p.assert_called_once_with('put', 'http://www.example.com', mock.ANY, mock.ANY, mock.ANY)


class TestStableSlugIdClosure(TestCase):

    @given(st.text())
    def test_repeat(self, text):
        s = subject.stableSlugId()
        self.assertEqual(s(text), s(text))

    def test_not_equal(self):
        s = subject.stableSlugId()
        self.assertNotEqual(s("first"), s("second"))

    @given(st.text())
    def test_invalidate(self, text):
        s1 = subject.stableSlugId()
        s2 = subject.stableSlugId()
        self.assertNotEqual(s1(text), s2(text))


class TestFromNow(TestCase):

    examples = [
        {"expr": '1 hour', "from": '2017-01-19T16:27:20.974Z', "result": '2017-01-19T17:27:20.974Z'},
        {"expr": '3h', "from": '2017-01-19T16:27:20.974Z', "result": '2017-01-19T19:27:20.974Z'},
        {"expr": '1 hours', "from": '2017-01-19T16:27:20.974Z', "result": '2017-01-19T17:27:20.974Z'},
        {"expr": '-1 hour', "from": '2017-01-19T16:27:20.974Z', "result": '2017-01-19T15:27:20.974Z'},
        {"expr": '1 m', "from": '2017-01-19T16:27:20.974Z', "result": '2017-01-19T16:28:20.974Z'},
        {"expr": '1m', "from": '2017-01-19T16:27:20.974Z', "result": '2017-01-19T16:28:20.974Z'},
        {"expr": '12 min', "from": '2017-01-19T16:27:20.974Z', "result": '2017-01-19T16:39:20.974Z'},
        {"expr": '12min', "from": '2017-01-19T16:27:20.974Z', "result": '2017-01-19T16:39:20.974Z'},
        {"expr": '11m', "from": '2017-01-19T16:27:20.974Z', "result": '2017-01-19T16:38:20.974Z'},
        {"expr": '11 m', "from": '2017-01-19T16:27:20.974Z', "result": '2017-01-19T16:38:20.974Z'},
        {"expr": '1 day', "from": '2017-01-19T16:27:20.974Z', "result": '2017-01-20T16:27:20.974Z'},
        {"expr": '2 days', "from": '2017-01-19T16:27:20.974Z', "result": '2017-01-21T16:27:20.974Z'},
        {"expr": '1 second', "from": '2017-01-19T16:27:20.974Z', "result": '2017-01-19T16:27:21.974Z'},
        {"expr": '1 week', "from": '2017-01-19T16:27:20.974Z', "result": '2017-01-26T16:27:20.974Z'},
        {"expr": '1 month', "from": '2017-01-19T16:27:20.974Z', "result": '2017-02-18T16:27:20.974Z'},
        {"expr": '30 mo', "from": '2017-01-19T16:27:20.974Z', "result": '2019-07-08T16:27:20.974Z'},
        {"expr": '-30 mo', "from": '2017-01-19T16:27:20.974Z', "result": '2014-08-03T16:27:20.974Z'},
        {"expr": '1 year', "from": '2017-01-19T16:27:20.974Z', "result": '2018-01-19T16:27:20.974Z'},
    ]

    def test_examples(self):
        for example in self.examples:
            from_ = dateutil.parser.parse(example['from'])
            res = dateutil.parser.parse(example['result'])
            self.assertEqual(subject.fromNow(example['expr'], from_), res)


class TestScopeMatch(TestCase):
    def assertScopeMatch(self, assumed, requiredScopeSets, expected):
        try:
            result = subject.scopeMatch(assumed, requiredScopeSets)
            self.assertEqual(result, expected)
        except:
            if expected != 'exception':
                raise

    def test_single_exact_match_string_except_1(self):
        self.assertScopeMatch(["foo:bar"], "foo:bar", "exception")

    def test_single_exact_match_string_except_2(self):
        self.assertScopeMatch(["foo:bar"], ["foo:bar"], "exception")

    def test_single_exact_match_string(self):
        self.assertScopeMatch(["foo:bar"], [["foo:bar"]], True)

    def test_empty_string_in_scopesets_except_1(self):
        self.assertScopeMatch(["foo:bar"], "", "exception")

    def test_empty_string_in_scopesets_except_2(self):
        self.assertScopeMatch(["foo:bar"], [""], "exception")

    def test_empty_string_in_scopesets(self):
        self.assertScopeMatch(["foo:bar"], [[""]], False)

    def test_prefix(self):
        self.assertScopeMatch(["foo:*"], [["foo:bar"]], True)

    def test_star_not_at_end(self):
        self.assertScopeMatch(["foo:*:bing"], [["foo:bar:bing"]], False)

    def test_star_at_beginnging(self):
        self.assertScopeMatch(["*:bar"], [["foo:bar"]], False)

    def test_prefix_with_no_star(self):
        self.assertScopeMatch(["foo:"], [["foo:bar"]], False)

    def test_star_but_not_prefix_1(self):
        self.assertScopeMatch(["foo:bar:*"], [["bar:bing"]], False)

    def test_star_but_not_prefix_2(self):
        self.assertScopeMatch(["bar:*"], [["foo:bar:bing"]], False)

    def test_disjunction_strings_except(self):
        self.assertScopeMatch(["bar:*"], ["foo:x", "bar:x"], "exception")

    def test_disjunction_strings_2(self):
        self.assertScopeMatch(["bar:*"], [["foo:x"], ["bar:x"]], True)

    def test_conjunction(self):
        self.assertScopeMatch(["bar:*", "foo:x"], [["foo:x", "bar:y"]], True)

    def test_empty_pattern(self):
        self.assertScopeMatch([""], [["foo:bar"]], False)

    def test_empty_patterns(self):
        self.assertScopeMatch([], [["foo:bar"]], False)

    def test_bare_star(self):
        self.assertScopeMatch(["*"], [["foo:bar", "bar:bing"]], True)

    def test_empty_conjunction_in_scopesets(self):
        self.assertScopeMatch(["foo:bar"], [[]], True)

    def test_non_string_scopesets(self):
        self.assertScopeMatch(["foo:bar"], {}, "exception")

    def test_non_string_scopeset(self):
        self.assertScopeMatch(["foo:bar"], [{}], "exception")

    def test_non_string_scope(self):
        self.assertScopeMatch(["foo:bar"], [[{}]], "exception")

    def test_empty_disjunction_in_scopesets(self):
        self.assertScopeMatch(["foo:bar"], [], False)


class TestIsExpired(TestCase):

    def test_not_expired(self):
        isExpired = subject.isExpired("""
          {
            "version":1,
            "scopes":["*"],
            "start":1450740520182,
            "expiry":2451000620182,
            "seed":"90PyTwYxS96-lBPc0f_MqQGV-hHCUsTYWpXZilv6EqDg",
            "signature":"HocA2IiCoGzjUQZbrbLSwKMXZSYWCu/hfMPCa/ovggQ="
          }
        """)
        self.assertEqual(isExpired, False)

    def test_expired(self):
        # Warning we have to test with expiry: 0 as magic python spy thing
        # mess up time.time() so it won't work.
        isExpired = subject.isExpired("""
          {
            "version":1,
            "scopes":["*"],
            "start":1450740520182,
            "expiry":0,
            "seed":"90PyTwYxS96-lBPc0f_MqQGV-hHCUsTYWpXZilv6EqDg",
            "signature":"HocA2IiCoGzjUQZbrbLSwKMXZSYWCu/hfMPCa/ovggQ="
          }
        """)
        self.assertEqual(isExpired, True)


class TestFromEnv(TestCase):

    def clear_env(self):
        for v in 'ROOT_URL', 'CLIENT_ID', 'ACCESS_TOKEN', 'CERTIFICATE':
            v = 'TASKCLUSTER_' + v
            if v in os.environ:
                del os.environ[v]

    @mock.patch.dict(os.environ)
    def test_empty(self):
        self.clear_env()
        self.assertEqual(subject.optionsFromEnvironment(), {})

    @mock.patch.dict(os.environ)
    def test_all(self):
        os.environ['TASKCLUSTER_ROOT_URL'] = 'https://tc.example.com'
        os.environ['TASKCLUSTER_CLIENT_ID'] = 'me'
        os.environ['TASKCLUSTER_ACCESS_TOKEN'] = 'shave-and-a-haircut'
        os.environ['TASKCLUSTER_CERTIFICATE'] = '{"bits":2}'
        self.assertEqual(subject.optionsFromEnvironment(), {
            'rootUrl': 'https://tc.example.com',
            'credentials': {
                'clientId': 'me',
                'accessToken': 'shave-and-a-haircut',
                'certificate': '{"bits":2}',
            },
        })

    @mock.patch.dict(os.environ)
    def test_cred_only(self):
        os.environ['TASKCLUSTER_ACCESS_TOKEN'] = 'shave-and-a-haircut'
        self.assertEqual(subject.optionsFromEnvironment(), {
            'credentials': {
                'accessToken': 'shave-and-a-haircut',
            },
        })

    @mock.patch.dict(os.environ)
    def test_rooturl_only(self):
        os.environ['TASKCLUSTER_ROOT_URL'] = 'https://tc.example.com'
        self.assertEqual(subject.optionsFromEnvironment(), {
            'rootUrl': 'https://tc.example.com',
        })

    @mock.patch.dict(os.environ)
    def test_default_rooturl(self):
        os.environ['TASKCLUSTER_CLIENT_ID'] = 'me'
        os.environ['TASKCLUSTER_ACCESS_TOKEN'] = 'shave-and-a-haircut'
        os.environ['TASKCLUSTER_CERTIFICATE'] = '{"bits":2}'
        self.assertEqual(
            subject.optionsFromEnvironment({'rootUrl': 'https://other.example.com'}), {
                'rootUrl': 'https://other.example.com',
                'credentials': {
                    'clientId': 'me',
                    'accessToken': 'shave-and-a-haircut',
                    'certificate': '{"bits":2}',
                    },
                })

    @mock.patch.dict(os.environ)
    def test_default_rooturl_overridden(self):
        os.environ['TASKCLUSTER_ROOT_URL'] = 'https://tc.example.com'
        self.assertEqual(
            subject.optionsFromEnvironment({'rootUrl': 'https://other.example.com'}),
            {'rootUrl': 'https://tc.example.com'})

    @mock.patch.dict(os.environ)
    def test_default_creds(self):
        os.environ['TASKCLUSTER_ROOT_URL'] = 'https://tc.example.com'
        os.environ['TASKCLUSTER_ACCESS_TOKEN'] = 'shave-and-a-haircut'
        os.environ['TASKCLUSTER_CERTIFICATE'] = '{"bits":2}'
        self.assertEqual(
            subject.optionsFromEnvironment({'credentials': {'clientId': 'them'}}), {
                'rootUrl': 'https://tc.example.com',
                'credentials': {
                    'clientId': 'them',
                    'accessToken': 'shave-and-a-haircut',
                    'certificate': '{"bits":2}',
                    },
                })

    @mock.patch.dict(os.environ)
    def test_default_creds_overridden(self):
        os.environ['TASKCLUSTER_ROOT_URL'] = 'https://tc.example.com'
        os.environ['TASKCLUSTER_CLIENT_ID'] = 'me'
        os.environ['TASKCLUSTER_ACCESS_TOKEN'] = 'shave-and-a-haircut'
        os.environ['TASKCLUSTER_CERTIFICATE'] = '{"bits":2}'
        self.assertEqual(
            subject.optionsFromEnvironment({'credentials': {'clientId': 'them'}}), {
                'rootUrl': 'https://tc.example.com',
                'credentials': {
                    'clientId': 'me',
                    'accessToken': 'shave-and-a-haircut',
                    'certificate': '{"bits":2}',
                    },
                })
