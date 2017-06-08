try:
    import unittest2 as unittest  # python < 2.7 compat
except ImportError:
    import unittest
import tempfile
import shutil
import os
import time
import six
from mock import Mock

from dlmanager import manager as download_manager


def mock_session():
    response = Mock()
    session = Mock(get=Mock(return_value=response))
    return session, response


def mock_response(response, data, wait=0):
    data = six.b(data)

    def iter_content(chunk_size=4):
        rest = data
        while rest:
            time.sleep(wait)
            chunk = rest[:chunk_size]
            rest = rest[chunk_size:]
            yield chunk

    response.headers = {'Content-length': str(len(data))}
    response.iter_content = iter_content


class TestDownload(unittest.TestCase):
    def setUp(self):
        self.tempdir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tempdir)
        self.finished = Mock()
        self.session, self.session_response = mock_session()
        self.tempfile = os.path.join(self.tempdir, 'dest')
        self.dl = download_manager.Download('http://url', self.tempfile,
                                            finished_callback=self.finished,
                                            chunk_size=4,
                                            session=self.session)

    def test_creation(self):
        self.assertFalse(self.dl.is_canceled())
        self.assertFalse(self.dl.is_running())
        self.assertIsNone(self.dl.error())
        self.assertEquals(self.dl.get_url(), 'http://url')
        self.assertEquals(self.dl.get_dest(), self.tempfile)

    def create_response(self, data, wait=0):
        mock_response(self.session_response, data, wait)

    def test_download(self):
        self.create_response('1234' * 4, 0.01)

        # no file present yet
        self.assertFalse(os.path.exists(self.tempfile))

        self.dl.start()
        self.assertTrue(self.dl.is_running())
        self.dl.wait()

        self.assertFalse(self.dl.is_running())
        self.finished.assert_called_with(self.dl)
        # file has been downloaded
        with open(self.tempfile) as f:
            self.assertEquals(f.read(), '1234' * 4)

    def test_download_cancel(self):
        self.create_response('1234' * 1000, wait=0.01)

        start = time.time()
        self.dl.start()
        time.sleep(0.1)
        self.dl.cancel()

        with self.assertRaises(download_manager.DownloadInterrupt):
            self.dl.wait()

        self.assertTrue(self.dl.is_canceled())

        # response generation should have taken 1000 * 0.01 = 10 seconds.
        # since we canceled, this must be lower.
        self.assertTrue((time.time() - start) < 1.0)

        # file was deleted
        self.assertFalse(os.path.exists(self.tempfile))
        # finished callback was called
        self.finished.assert_called_with(self.dl)

    def test_download_with_progress(self):
        data = []

        def update_progress(_dl, current, total):
            data.append((_dl, current, total))

        self.create_response('1234' * 4)

        self.dl.set_progress(update_progress)
        self.dl.start()
        self.dl.wait()

        self.assertEquals(data, [
            (self.dl, 0, 16),
            (self.dl, 4, 16),
            (self.dl, 8, 16),
            (self.dl, 12, 16),
            (self.dl, 16, 16),
        ])
        # file has been downloaded
        with open(self.tempfile) as f:
            self.assertEquals(f.read(), '1234' * 4)
        # finished callback was called
        self.finished.assert_called_with(self.dl)

    def test_download_error_in_thread(self):
        self.session_response.headers = {'Content-length': '24'}
        self.session_response.iter_content.side_effect = IOError

        self.dl.start()
        with self.assertRaises(IOError):
            self.dl.wait()

        self.assertEquals(self.dl.error()[0], IOError)
        # finished callback was called
        self.finished.assert_called_with(self.dl)

    def test_wait_does_not_block_on_exception(self):
        # this test the case when a user may hit CTRL-C for example
        # during a dl.wait() call.
        self.create_response('1234' * 1000, wait=0.01)

        original_join = self.dl.thread.join
        it = iter('123')

        def join(timeout=None):
            next(it)  # will throw StopIteration after a few calls
            original_join(timeout)

        self.dl.thread.join = join

        start = time.time()
        self.dl.start()

        with self.assertRaises(StopIteration):
            self.dl.wait()

        self.assertTrue(self.dl.is_canceled())
        # wait for the thread to finish
        original_join()

        # response generation should have taken 1000 * 0.01 = 10 seconds.
        # since we got an error, this must be lower.
        self.assertTrue((time.time() - start) < 1.0)

        # file was deleted
        self.assertFalse(os.path.exists(self.tempfile))
        # finished callback was called
        self.finished.assert_called_with(self.dl)


class TestDownloadManager(unittest.TestCase):
    def setUp(self):
        self.tempdir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tempdir)

        self.dl_manager = download_manager.DownloadManager(self.tempdir)

    def do_download(self, url, fname, data, wait=0):
        session, response = mock_session()
        mock_response(response, data, wait)
        # patch the session, so the download will use that
        self.dl_manager.session = session
        return self.dl_manager.download(url, fname)

    def test_download(self):
        dl1 = self.do_download('http://foo', 'foo', 'hello' * 4, wait=0.02)
        self.assertIsInstance(dl1, download_manager.Download)
        self.assertTrue(dl1.is_running())

        # with the same fname, no new download is started. The same instance
        # is returned since the download is running.
        dl2 = self.do_download('http://bar', 'foo', 'hello2' * 4, wait=0.02)
        self.assertEquals(dl1, dl2)

        # starting a download with another fname will trigger a new download
        dl3 = self.do_download('http://bar', 'foo2', 'hello you' * 4)
        self.assertIsInstance(dl3, download_manager.Download)
        self.assertNotEquals(dl3, dl1)

        # let's wait for the downloads to finish
        dl3.wait()
        dl1.wait()

        # now if we try to download a fname that exists, None is returned
        dl4 = self.do_download('http://bar', 'foo', 'hello2' * 4, wait=0.02)
        self.assertIsNone(dl4)

        # downloaded files are what is expected
        def content(fname):
            with open(os.path.join(self.tempdir, fname)) as f:
                return f.read()
        self.assertEquals(content('foo'), 'hello' * 4)
        self.assertEquals(content('foo2'), 'hello you' * 4)

        # download instances are removed from the manager (internal test)
        self.assertEquals(self.dl_manager._downloads, {})

    def test_cancel(self):
        dl1 = self.do_download('http://foo', 'foo', 'foo' * 50000, wait=0.02)
        dl2 = self.do_download('http://foo', 'bar', 'bar' * 50000, wait=0.02)
        dl3 = self.do_download('http://foo', 'foobar', 'foobar' * 4)

        # let's cancel only one
        def cancel_if(dl):
            if os.path.basename(dl.get_dest()) == 'foo':
                return True
        self.dl_manager.cancel(cancel_if=cancel_if)

        self.assertTrue(dl1.is_canceled())
        self.assertFalse(dl2.is_canceled())
        self.assertFalse(dl3.is_canceled())

        # wait for dl3
        dl3.wait()

        # cancel everything
        self.dl_manager.cancel()

        self.assertTrue(dl1.is_canceled())
        self.assertTrue(dl2.is_canceled())
        # dl3 is not canceled since it finished before
        self.assertFalse(dl3.is_canceled())

        # wait for the completion of dl1 and dl2 threads
        dl1.wait(raise_if_error=False)
        dl2.wait(raise_if_error=False)

        # at the end, only dl3 has been downloaded
        self.assertEquals(os.listdir(self.tempdir), ["foobar"])

        with open(os.path.join(self.tempdir, 'foobar')) as f:
            self.assertEquals(f.read(), 'foobar' * 4)

        # download instances are removed from the manager (internal test)
        self.assertEquals(self.dl_manager._downloads, {})
