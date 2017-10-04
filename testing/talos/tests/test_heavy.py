#!/usr/bin/env python

"""
test talos' heavy module:

http://hg.mozilla.org/build/talos/file/tip/talos/heavy.py
"""
from __future__ import absolute_import
import unittest
import tempfile
import shutil
import datetime
import contextlib
import os
import time

import talos.heavy


archive = os.path.join(os.path.dirname(__file__), 'profile.tgz')
archive_size = os.stat(archive).st_size


@contextlib.contextmanager
def mock_requests(**kw):
    class Session:
        def mount(self, *args, **kw):
            pass

    kw['Session'] = Session
    old = {}
    for meth, func in kw.items():
        curr = getattr(talos.heavy.requests, meth)
        old[meth] = curr
        setattr(talos.heavy.requests, meth, func)
        setattr(Session, meth, func)
    try:
        yield
    finally:
        for meth, func in old.items():
            setattr(talos.heavy.requests, meth, func)


class _Response(object):
    def __init__(self, code, headers=None, file=None):
        if headers is None:
            headers = {}
        self.headers = headers
        self.status_code = code
        self.file = file

    def raise_for_status(self):
        pass

    def iter_content(self, chunk_size):
        with open(self.file, 'rb') as f:
            yield f.read(chunk_size)


class Logger:
    def __init__(self):
        self.data = []

    def info(self, msg):
        self.data.append(msg)


class TestFilter(unittest.TestCase):

    def setUp(self):
        self.temp = tempfile.mkdtemp()
        self.logs = talos.heavy.LOG.logger = Logger()

    def tearDown(self):
        shutil.rmtree(self.temp)

    def test_profile_age(self):
        """test profile_age function"""
        days = talos.heavy.profile_age(self.temp)
        self.assertEqual(days, 0)

        _8_days = datetime.datetime.now() + datetime.timedelta(days=8)
        days = talos.heavy.profile_age(self.temp, _8_days)
        self.assertEqual(days, 8)

    def test_directory_age(self):
        """make sure it detects changes in files in subdirs"""
        with open(os.path.join(self.temp, 'file'), 'w') as f:
            f.write('xxx')

        current_age = talos.heavy._recursive_mtime(self.temp)
        time.sleep(1.1)

        with open(os.path.join(self.temp, 'file'), 'w') as f:
            f.write('----')

        self.assertTrue(current_age < talos.heavy._recursive_mtime(self.temp))

    def test_follow_redirect(self):
        """test follow_redirect function"""
        _8_days = datetime.datetime.now() + datetime.timedelta(days=8)
        _8_days = _8_days.strftime('%a, %d %b %Y %H:%M:%S UTC')

        resps = [_Response(303, {'Location': 'blah'}),
                 _Response(303, {'Location': 'bli'}),
                 _Response(200, {'Last-Modified': _8_days})]

        class Counter:
            c = 0

        def _head(url, curr=Counter()):
            curr.c += 1
            return resps[curr.c]

        with mock_requests(head=_head):
            loc, lm = talos.heavy.follow_redirects('https://example.com')
            days = talos.heavy.profile_age(self.temp, lm)
            self.assertEqual(days, 8)

    def _test_download(self, age):

        def _days(num):
            d = datetime.datetime.now() + datetime.timedelta(days=num)
            return d.strftime('%a, %d %b %Y %H:%M:%S UTC')

        resps = [_Response(303, {'Location': 'blah'}),
                 _Response(303, {'Location': 'bli'}),
                 _Response(200, {'Last-Modified': _days(age)})]

        class Counter:
            c = 0

        def _head(url, curr=Counter()):
            curr.c += 1
            return resps[curr.c]

        def _get(url, *args, **kw):
            return _Response(200, {'Last-Modified': _days(age),
                                   'content-length': str(archive_size)},
                             file=archive)

        with mock_requests(head=_head, get=_get):
            target = talos.heavy.download_profile('simple',
                                                  profiles_dir=self.temp)
            profile = os.path.join(self.temp, 'simple')
            self.assertTrue(os.path.exists(profile))
            return target

    def test_download_profile(self):
        """test downloading heavy profile"""
        # a 12 days old profile gets updated
        self._test_download(12)

        # a 8 days two
        self._test_download(8)

        # a 2 days sticks
        self._test_download(2)
        self.assertTrue("fresh enough" in self.logs.data[-2])


if __name__ == '__main__':
    unittest.main()
