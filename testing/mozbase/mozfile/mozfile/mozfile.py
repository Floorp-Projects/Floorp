# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from contextlib import contextmanager
import os
import shutil
import tarfile
import tempfile
import urlparse
import urllib2
import zipfile

__all__ = ['extract_tarball',
           'extract_zip',
           'extract',
           'is_url',
           'load',
           'rmtree',
           'NamedTemporaryFile',
           'TemporaryDirectory']


### utilities for extracting archives

def extract_tarball(src, dest):
    """extract a .tar file"""

    bundle = tarfile.open(src)
    namelist = bundle.getnames()

    for name in namelist:
        bundle.extract(name, path=dest)
    bundle.close()
    return namelist


def extract_zip(src, dest):
    """extract a zip file"""

    if isinstance(src, zipfile.ZipFile):
        bundle = src
    else:
        try:
            bundle = zipfile.ZipFile(src)
        except Exception, e:
            print "src: %s" % src
            raise

    namelist = bundle.namelist()

    for name in namelist:
        filename = os.path.realpath(os.path.join(dest, name))
        if name.endswith('/'):
            if not os.path.isdir(filename):
                os.makedirs(filename)
        else:
            path = os.path.dirname(filename)
            if not os.path.isdir(path):
                os.makedirs(path)
            _dest = open(filename, 'wb')
            _dest.write(bundle.read(name))
            _dest.close()
        mode = bundle.getinfo(name).external_attr >> 16 & 0x1FF
        os.chmod(filename, mode)
    bundle.close()
    return namelist


def extract(src, dest=None):
    """
    Takes in a tar or zip file and extracts it to dest

    If dest is not specified, extracts to os.path.dirname(src)

    Returns the list of top level files that were extracted
    """

    assert os.path.exists(src), "'%s' does not exist" % src

    if dest is None:
        dest = os.path.dirname(src)
    elif not os.path.isdir(dest):
        os.makedirs(dest)
    assert not os.path.isfile(dest), "dest cannot be a file"

    if zipfile.is_zipfile(src):
        namelist = extract_zip(src, dest)
    elif tarfile.is_tarfile(src):
        namelist = extract_tarball(src, dest)
    else:
        raise Exception("mozfile.extract: no archive format found for '%s'" %
                        src)

    # namelist returns paths with forward slashes even in windows
    top_level_files = [os.path.join(dest, name.rstrip('/')) for name in namelist
                       if len(name.rstrip('/').split('/')) == 1]

    # namelist doesn't include folders, append these to the list
    for name in namelist:
        index = name.find('/')
        if index != -1:
            root = os.path.join(dest, name[:index])
            if root not in top_level_files:
                top_level_files.append(root)

    return top_level_files


def rmtree(dir):
    """Removes the specified directory tree

    This is a replacement for shutil.rmtree that works better under
    windows."""
    # (Thanks to Bear at the OSAF for the code.)
    if not os.path.exists(dir):
        return
    if os.path.islink(dir):
        os.remove(dir)
        return

    # Verify the directory is read/write/execute for the current user
    os.chmod(dir, 0700)

    # os.listdir below only returns a list of unicode filenames
    # if the parameter is unicode.
    # If a non-unicode-named dir contains a unicode filename,
    # that filename will get garbled.
    # So force dir to be unicode.
    if not isinstance(dir, unicode):
        try:
            dir = unicode(dir, "utf-8")
        except UnicodeDecodeError:
            if os.environ.get('DEBUG') == '1':
                print("rmtree: decoding from UTF-8 failed for directory: %s" %s)

    for name in os.listdir(dir):
        full_name = os.path.join(dir, name)
        # on Windows, if we don't have write permission we can't remove
        # the file/directory either, so turn that on
        if os.name == 'nt':
            if not os.access(full_name, os.W_OK):
                # I think this is now redundant, but I don't have an NT
                # machine to test on, so I'm going to leave it in place
                # -warner
                os.chmod(full_name, 0600)

        if os.path.islink(full_name):
            os.remove(full_name)
        elif os.path.isdir(full_name):
            rmtree(full_name)
        else:
            if os.path.isfile(full_name):
                os.chmod(full_name, 0700)
            os.remove(full_name)
    os.rmdir(dir)


class NamedTemporaryFile(object):
    """
    Like tempfile.NamedTemporaryFile except it works on Windows
    in the case where you open the created file a second time.

    This behaves very similarly to tempfile.NamedTemporaryFile but may
    not behave exactly the same. For example, this function does not
    prevent fd inheritance by children.

    Example usage:

    with NamedTemporaryFile() as fh:
        fh.write(b'foobar')

        print('Filename: %s' % fh.name)

    see https://bugzilla.mozilla.org/show_bug.cgi?id=821362
    """
    def __init__(self, mode='w+b', bufsize=-1, suffix='', prefix='tmp',
                 dir=None, delete=True):

        fd, path = tempfile.mkstemp(suffix, prefix, dir, 't' in mode)
        os.close(fd)

        self.file = open(path, mode)
        self._path = path
        self._delete = delete
        self._unlinked = False

    def __getattr__(self, k):
        return getattr(self.__dict__['file'], k)

    def __iter__(self):
        return self.__dict__['file']

    def __enter__(self):
        self.file.__enter__()
        return self

    def __exit__(self, exc, value, tb):
        self.file.__exit__(exc, value, tb)
        if self.__dict__['_delete']:
            os.unlink(self.__dict__['_path'])
            self._unlinked = True

    def __del__(self):
        if self.__dict__['_unlinked']:
            return
        self.file.__exit__(None, None, None)
        if self.__dict__['_delete']:
            os.unlink(self.__dict__['_path'])


### utilities dealing with URLs

def is_url(thing):
    """
    Return True if thing looks like a URL.
    """

    parsed = urlparse.urlparse(thing)
    if 'scheme' in parsed:
        return len(parsed.scheme) >= 2
    else:
        return len(parsed[0]) >= 2

def load(resource):
    """
    open a file or URL for reading.  If the passed resource string is not a URL,
    or begins with 'file://', return a ``file``.  Otherwise, return the
    result of urllib2.urlopen()
    """

    # handle file URLs separately due to python stdlib limitations
    if resource.startswith('file://'):
        resource = resource[len('file://'):]

    if not is_url(resource):
        # if no scheme is given, it is a file path
        return file(resource)

    return urllib2.urlopen(resource)

@contextmanager
def TemporaryDirectory():
    """
    create a temporary directory using tempfile.mkdtemp, and then clean it up.

    Example usage:
    with TemporaryDirectory() as tmp:
       open(os.path.join(tmp, "a_temp_file"), "w").write("data")

    """
    tempdir = tempfile.mkdtemp()
    try:
        yield tempdir
    finally:
        shutil.rmtree(tempdir)
