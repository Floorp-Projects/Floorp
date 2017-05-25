import os
import requests
import six
import sys
import tempfile
import threading

from contextlib import closing
from six.moves.urllib.parse import urlparse

from dlmanager import fs
from dlmanager.persist_limit import PersistLimit


class DownloadInterrupt(Exception):
    "Raised when a download is interrupted."


class Download(object):
    """
    Download is reponsible of downloading one file in the background.

    Example of use: ::

      dl = Download(url, dest)
      dl.start()
      dl.wait() # this will block until completion / cancel / error

    If a download fail or is canceled, the temporary dest is removed from
    the disk.

    Usually, Downloads are created by using :meth:`DownloadManager.download`.

    :param url: the url of the file to download
    :param dest: the local file path destination
    :param finished_callback: a callback that will be called in the thread
                              when the thread work is done. Takes the download
                              instance as a parameter.
    :param chunk_size: size of the chunk that will be read. The thread can
                        not be stopped while we are reading that chunk size.
    :param session: a requests.Session instance that will do do the real
                    downloading work. If None, `requests` module is used.
    :param progress: A callable to report the progress (default to None).
                     see :meth:`set_progress`.
    """
    def __init__(self, url, dest, finished_callback=None,
                 chunk_size=16 * 1024, session=None, progress=None):
        self.thread = threading.Thread(
            target=self._download,
            args=(url, dest, finished_callback, chunk_size,
                  session or requests)
        )
        self._lock = threading.Lock()
        self.__url = url
        self.__dest = dest
        self.__progress = progress
        self.__canceled = False
        self.__error = None

    def start(self):
        """
        Start the thread that will do the download.
        """
        self.thread.start()

    def cancel(self):
        """
        Cancel a previously started download.
        """
        self.__canceled = True

    def is_canceled(self):
        """
        Returns True if we canceled this download.
        """
        return self.__canceled

    def is_running(self):
        """
        Returns True if the downloading thread is running.
        """
        return self.thread.is_alive()

    def wait(self, raise_if_error=True):
        """
        Block until the downloading thread is finished.

        :param raise_if_error: if True (the default), :meth:`raise_if_error`
                               will be called and raise an error if any.
        """
        while self.thread.is_alive():
            try:
                # in case of exception here (like KeyboardInterrupt),
                # cancel the task.
                self.thread.join(0.02)
            except:
                self.cancel()
                raise
        # this will raise exception that may happen inside the thread.
        if raise_if_error:
            self.raise_if_error()

    def error(self):
        """
        Returns None or a tuple of three values (type, value, traceback)
        that give information about the exception.
        """
        return self.__error

    def raise_if_error(self):
        """
        Raise an error if any. If the download was canceled, raise
        :class:`DownloadInterrupt`.
        """
        if self.__error:
            six.reraise(*self.__error)
        if self.__canceled:
            raise DownloadInterrupt()

    def set_progress(self, progress):
        """
        set a callable to report the progress of the download, or None to
        disable any report.

        The callable must take three parameters (download, current, total).
        Note that this method is thread safe, you can call it during a
        download.
        """
        with self._lock:
            self.__progress = progress

    def get_dest(self):
        """
        Returns the dest.
        """
        return self.__dest

    def get_url(self):
        """
        Returns the url.
        """
        return self.__url

    def _update_progress(self, current, total):
        with self._lock:
            if self.__progress:
                self.__progress(self, current, total)

    def _download(self, url, dest, finished_callback, chunk_size, session):
        # save the file under a temporary name
        # this allow to not use a broken file in case things went really bad
        # while downloading the file (ie the python interpreter is killed
        # abruptly)
        temp = None
        bytes_so_far = 0
        try:
            with closing(session.get(url, stream=True)) as response:
                total_size = response.headers.get('Content-length', '').strip()
                total_size = int(total_size) if total_size else None
                self._update_progress(bytes_so_far, total_size)
                # we use NamedTemporaryFile as raw open() call was causing
                # issues on windows - see:
                # https://bugzilla.mozilla.org/show_bug.cgi?id=1185756
                with tempfile.NamedTemporaryFile(
                        delete=False,
                        suffix='.tmp',
                        dir=os.path.dirname(dest)) as temp:
                    for chunk in response.iter_content(chunk_size):
                        if self.is_canceled():
                            break
                        if chunk:
                            temp.write(chunk)
                        bytes_so_far += len(chunk)
                        self._update_progress(bytes_so_far, total_size)
            response.raise_for_status()
        except:
            self.__error = sys.exc_info()
        try:
            if temp is None:
                pass  # not even opened the temp file, nothing to do
            elif self.is_canceled() or self.__error:
                fs.remove(temp.name)
            else:
                # if all goes well, then rename the file to the real dest
                fs.remove(dest)  # just in case it already existed
                fs.move(temp.name, dest)
        finally:
            if finished_callback:
                finished_callback(self)


class DownloadManager(object):
    """
    DownloadManager is responsible of starting and managing downloads inside
    a given directory. It will download a file only if a given filename
    is not already there.

    Note that background downloads needs to be stopped. For example, if
    you have an exception while a download is occuring, python will only
    exit when the download will finish. To get rid of that, there is a
    possible idiom: ::

      def download_things(manager):
          # do things with the manager
          manager.download(url1, f1)
          manager.download(url2, f2)
          ...

      manager = DownloadManager(destdir)
      try:
          download_things(manager)
      finally:
          # ensure we cancel all background downloads to ask the end
          # of possible remainings threads
          manager.cancel()

    :param destdir: a directory where files are downloaded. It will be created
                    if it does not exists.
    :param session: a requests session. If None, one will be created for you.
    :param persist_limit: an instance of :class:`PersistLimit`, to allow
                          limiting the size of the download dir. Defaults
                          to None, meaning no limit.
    """
    def __init__(self, destdir, session=None, persist_limit=None):
        self.destdir = destdir
        self.session = session or requests.Session()
        self._downloads = {}
        self._lock = threading.Lock()
        self.persist_limit = persist_limit or PersistLimit(0)
        self.persist_limit.register_dir_content(self.destdir)

        # if persist folder does not exist, create it
        if not os.path.isdir(destdir):
            os.makedirs(destdir)

    def get_dest(self, fname):
        return os.path.join(self.destdir, fname)

    def cancel(self, cancel_if=None):
        """
        Cancel downloads, if any.

        if cancel_if is given, it must be a callable that take the download
        instance as parameter, and return True if the download needs to be
        canceled.

        Note that download threads won't be stopped directly.
        """
        with self._lock:
            for download in six.itervalues(self._downloads):
                if cancel_if is None or cancel_if(download):
                    if download.is_running():
                        download.cancel()

    def wait(self, raise_if_error=True):
        """
        Wait for all downloads to be finished.
        """
        for download in self._downloads.values():
            download.wait(raise_if_error=raise_if_error)

    def download(self, url, fname=None, progress=None):
        """
        Returns a started :class:`Download` instance, or None if fname is
        already present in destdir.

        if a download is already running for the given fname, it is just
        returned. Else the download is created, started and returned.

        :param url: url of the file to download.
        :param fname: name to give for the downloaded file. If None, it will
                      be the name extracted in the url.
        :param progress: a callable to report the download progress, or None.
                         See :meth:`Download.set_progress`.
        """
        if fname is None:
            fname = urlparse(url).path.split('/')[-1]
        dest = self.get_dest(fname)
        with self._lock:
            # if we are downloading, returns the instance
            if dest in self._downloads:
                dl = self._downloads[dest]
                if progress:
                    dl.set_progress(progress)
                return dl

        if os.path.exists(dest):
            return None

        # else create the download (will be automatically removed of
        # the list on completion) start it, and returns that.
        with self._lock:
            download = Download(url, dest,
                                session=self.session,
                                finished_callback=self._download_finished,
                                progress=progress)
            self._downloads[dest] = download
            download.start()
            self._download_started(download)
            return download

    def _download_started(self, dl):
        """
        Useful when sub-classing. Report the start event of a download.

        :param dl: The :class:`Download` instance.
        """
        pass

    def _download_finished(self, dl):
        """
        Useful when sub-classing. Report the end of a download.

        Note that this is executed in the download thread. Also, you should
        make sure to call the base implementation.

        :param dl: The :class:`Download` instance.
        """
        with self._lock:
            dest = dl.get_dest()
            del self._downloads[dest]
            self.persist_limit.register_file(dest)
            self.persist_limit.remove_old_files()
