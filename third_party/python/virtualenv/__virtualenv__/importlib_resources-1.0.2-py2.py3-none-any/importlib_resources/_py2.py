import os
import errno
import tempfile

from ._compat import FileNotFoundError
from contextlib import contextmanager
from importlib import import_module
from io import BytesIO, TextIOWrapper, open as io_open
from pathlib2 import Path
from zipfile import ZipFile


def _get_package(package):
    """Normalize a path by ensuring it is a string.

    If the resulting string contains path separators, an exception is raised.
    """
    if isinstance(package, basestring):                      # noqa: F821
        module = import_module(package)
    else:
        module = package
    if not hasattr(module, '__path__'):
        raise TypeError("{!r} is not a package".format(package))
    return module


def _normalize_path(path):
    """Normalize a path by ensuring it is a string.

    If the resulting string contains path separators, an exception is raised.
    """
    str_path = str(path)
    parent, file_name = os.path.split(str_path)
    if parent:
        raise ValueError("{!r} must be only a file name".format(path))
    else:
        return file_name


def open_binary(package, resource):
    """Return a file-like object opened for binary reading of the resource."""
    resource = _normalize_path(resource)
    package = _get_package(package)
    # Using pathlib doesn't work well here due to the lack of 'strict' argument
    # for pathlib.Path.resolve() prior to Python 3.6.
    package_path = os.path.dirname(package.__file__)
    relative_path = os.path.join(package_path, resource)
    full_path = os.path.abspath(relative_path)
    try:
        return io_open(full_path, 'rb')
    except IOError:
        # This might be a package in a zip file.  zipimport provides a loader
        # with a functioning get_data() method, however we have to strip the
        # archive (i.e. the .zip file's name) off the front of the path.  This
        # is because the zipimport loader in Python 2 doesn't actually follow
        # PEP 302.  It should allow the full path, but actually requires that
        # the path be relative to the zip file.
        try:
            loader = package.__loader__
            full_path = relative_path[len(loader.archive)+1:]
            data = loader.get_data(full_path)
        except (IOError, AttributeError):
            package_name = package.__name__
            message = '{!r} resource not found in {!r}'.format(
                resource, package_name)
            raise FileNotFoundError(message)
        else:
            return BytesIO(data)


def open_text(package, resource, encoding='utf-8', errors='strict'):
    """Return a file-like object opened for text reading of the resource."""
    resource = _normalize_path(resource)
    package = _get_package(package)
    # Using pathlib doesn't work well here due to the lack of 'strict' argument
    # for pathlib.Path.resolve() prior to Python 3.6.
    package_path = os.path.dirname(package.__file__)
    relative_path = os.path.join(package_path, resource)
    full_path = os.path.abspath(relative_path)
    try:
        return io_open(full_path, mode='r', encoding=encoding, errors=errors)
    except IOError:
        # This might be a package in a zip file.  zipimport provides a loader
        # with a functioning get_data() method, however we have to strip the
        # archive (i.e. the .zip file's name) off the front of the path.  This
        # is because the zipimport loader in Python 2 doesn't actually follow
        # PEP 302.  It should allow the full path, but actually requires that
        # the path be relative to the zip file.
        try:
            loader = package.__loader__
            full_path = relative_path[len(loader.archive)+1:]
            data = loader.get_data(full_path)
        except (IOError, AttributeError):
            package_name = package.__name__
            message = '{!r} resource not found in {!r}'.format(
                resource, package_name)
            raise FileNotFoundError(message)
        else:
            return TextIOWrapper(BytesIO(data), encoding, errors)


def read_binary(package, resource):
    """Return the binary contents of the resource."""
    resource = _normalize_path(resource)
    package = _get_package(package)
    with open_binary(package, resource) as fp:
        return fp.read()


def read_text(package, resource, encoding='utf-8', errors='strict'):
    """Return the decoded string of the resource.

    The decoding-related arguments have the same semantics as those of
    bytes.decode().
    """
    resource = _normalize_path(resource)
    package = _get_package(package)
    with open_text(package, resource, encoding, errors) as fp:
        return fp.read()


@contextmanager
def path(package, resource):
    """A context manager providing a file path object to the resource.

    If the resource does not already exist on its own on the file system,
    a temporary file will be created. If the file was created, the file
    will be deleted upon exiting the context manager (no exception is
    raised if the file was deleted prior to the context manager
    exiting).
    """
    resource = _normalize_path(resource)
    package = _get_package(package)
    package_directory = Path(package.__file__).parent
    file_path = package_directory / resource
    # If the file actually exists on the file system, just return it.
    # Otherwise, it's probably in a zip file, so we need to create a temporary
    # file and copy the contents into that file, hence the contextmanager to
    # clean up the temp file resource.
    if file_path.exists():
        yield file_path
    else:
        with open_binary(package, resource) as fp:
            data = fp.read()
        # Not using tempfile.NamedTemporaryFile as it leads to deeper 'try'
        # blocks due to the need to close the temporary file to work on Windows
        # properly.
        fd, raw_path = tempfile.mkstemp()
        try:
            os.write(fd, data)
            os.close(fd)
            yield Path(raw_path)
        finally:
            try:
                os.remove(raw_path)
            except FileNotFoundError:
                pass


def is_resource(package, name):
    """True if name is a resource inside package.

    Directories are *not* resources.
    """
    package = _get_package(package)
    _normalize_path(name)
    try:
        package_contents = set(contents(package))
    except OSError as error:
        if error.errno not in (errno.ENOENT, errno.ENOTDIR):
            # We won't hit this in the Python 2 tests, so it'll appear
            # uncovered.  We could mock os.listdir() to return a non-ENOENT or
            # ENOTDIR, but then we'd have to depend on another external
            # library since Python 2 doesn't have unittest.mock.  It's not
            # worth it.
            raise                     # pragma: nocover
        return False
    if name not in package_contents:
        return False
    # Just because the given file_name lives as an entry in the package's
    # contents doesn't necessarily mean it's a resource.  Directories are not
    # resources, so let's try to find out if it's a directory or not.
    path = Path(package.__file__).parent / name
    if path.is_file():
        return True
    if path.is_dir():
        return False
    # If it's not a file and it's not a directory, what is it?  Well, this
    # means the file doesn't exist on the file system, so it probably lives
    # inside a zip file.  We have to crack open the zip, look at its table of
    # contents, and make sure that this entry doesn't have sub-entries.
    archive_path = package.__loader__.archive   # type: ignore
    package_directory = Path(package.__file__).parent
    with ZipFile(archive_path) as zf:
        toc = zf.namelist()
    relpath = package_directory.relative_to(archive_path)
    candidate_path = relpath / name
    for entry in toc:                               # pragma: nobranch
        try:
            relative_to_candidate = Path(entry).relative_to(candidate_path)
        except ValueError:
            # The two paths aren't relative to each other so we can ignore it.
            continue
        # Since directories aren't explicitly listed in the zip file, we must
        # infer their 'directory-ness' by looking at the number of path
        # components in the path relative to the package resource we're
        # looking up.  If there are zero additional parts, it's a file, i.e. a
        # resource.  If there are more than zero it's a directory, i.e. not a
        # resource.  It has to be one of these two cases.
        return len(relative_to_candidate.parts) == 0
    # I think it's impossible to get here.  It would mean that we are looking
    # for a resource in a zip file, there's an entry matching it in the return
    # value of contents(), but we never actually found it in the zip's table of
    # contents.
    raise AssertionError('Impossible situation')


def contents(package):
    """Return an iterable of entries in `package`.

    Note that not all entries are resources.  Specifically, directories are
    not considered resources.  Use `is_resource()` on each entry returned here
    to check if it is a resource or not.
    """
    package = _get_package(package)
    package_directory = Path(package.__file__).parent
    try:
        return os.listdir(str(package_directory))
    except OSError as error:
        if error.errno not in (errno.ENOENT, errno.ENOTDIR):
            # We won't hit this in the Python 2 tests, so it'll appear
            # uncovered.  We could mock os.listdir() to return a non-ENOENT or
            # ENOTDIR, but then we'd have to depend on another external
            # library since Python 2 doesn't have unittest.mock.  It's not
            # worth it.
            raise                     # pragma: nocover
        # The package is probably in a zip file.
        archive_path = getattr(package.__loader__, 'archive', None)
        if archive_path is None:
            raise
        relpath = package_directory.relative_to(archive_path)
        with ZipFile(archive_path) as zf:
            toc = zf.namelist()
        subdirs_seen = set()                        # type: Set
        subdirs_returned = []
        for filename in toc:
            path = Path(filename)
            # Strip off any path component parts that are in common with the
            # package directory, relative to the zip archive's file system
            # path.  This gives us all the parts that live under the named
            # package inside the zip file.  If the length of these subparts is
            # exactly 1, then it is situated inside the package.  The resulting
            # length will be 0 if it's above the package, and it will be
            # greater than 1 if it lives in a subdirectory of the package
            # directory.
            #
            # However, since directories themselves don't appear in the zip
            # archive as a separate entry, we need to return the first path
            # component for any case that has > 1 subparts -- but only once!
            if path.parts[:len(relpath.parts)] != relpath.parts:
                continue
            subparts = path.parts[len(relpath.parts):]
            if len(subparts) == 1:
                subdirs_returned.append(subparts[0])
            elif len(subparts) > 1:                 # pragma: nobranch
                subdir = subparts[0]
                if subdir not in subdirs_seen:
                    subdirs_seen.add(subdir)
                    subdirs_returned.append(subdir)
        return subdirs_returned
