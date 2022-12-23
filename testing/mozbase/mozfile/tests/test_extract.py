#!/usr/bin/env python

import os
import tarfile
import tempfile
import zipfile

import mozfile
import mozunit
import pytest
import stubs


@pytest.fixture
def ensure_directory_contents():
    """ensure the directory contents match"""

    def inner(directory):
        for f in stubs.files:
            path = os.path.join(directory, *f)
            exists = os.path.exists(path)
            if not exists:
                print("%s does not exist" % (os.path.join(f)))
            assert exists
            if exists:
                contents = open(path).read().strip()
                assert contents == f[-1]

    return inner


@pytest.fixture(scope="module")
def tarpath(tmpdir_factory):
    """create a stub tarball for testing"""
    tmpdir = tmpdir_factory.mktemp("test_extract")

    tempdir = tmpdir.join("stubs").strpath
    stubs.create_stub(tempdir)
    filename = tmpdir.join("bundle.tar").strpath
    archive = tarfile.TarFile(filename, mode="w")
    for path in stubs.files:
        archive.add(os.path.join(tempdir, *path), arcname=os.path.join(*path))
    archive.close()

    assert os.path.exists(filename)
    return filename


@pytest.fixture(scope="module")
def zippath(tmpdir_factory):
    """create a stub zipfile for testing"""
    tmpdir = tmpdir_factory.mktemp("test_extract")

    tempdir = tmpdir.join("stubs").strpath
    stubs.create_stub(tempdir)
    filename = tmpdir.join("bundle.zip").strpath
    archive = zipfile.ZipFile(filename, mode="w")
    for path in stubs.files:
        archive.write(os.path.join(tempdir, *path), arcname=os.path.join(*path))
    archive.close()

    assert os.path.exists(filename)
    return filename


@pytest.fixture(scope="module", params=["tar", "zip"])
def bundlepath(request, tarpath, zippath):
    if request.param == "tar":
        return tarpath
    else:
        return zippath


def test_extract(tmpdir, bundlepath, ensure_directory_contents):
    """test extracting a zipfile"""
    dest = tmpdir.mkdir("dest").strpath
    mozfile.extract(bundlepath, dest)
    ensure_directory_contents(dest)


def test_extract_zipfile_missing_file_attributes(tmpdir):
    """if files do not have attributes set the default permissions have to be inherited."""
    _zipfile = os.path.join(
        os.path.dirname(__file__), "files", "missing_file_attributes.zip"
    )
    assert os.path.exists(_zipfile)
    dest = tmpdir.mkdir("dest").strpath

    # Get the default file permissions for the user
    fname = os.path.join(dest, "foo")
    with open(fname, "w"):
        pass
    default_stmode = os.stat(fname).st_mode

    files = mozfile.extract_zip(_zipfile, dest)
    for filename in files:
        assert os.stat(os.path.join(dest, filename)).st_mode == default_stmode


def test_extract_non_archive(tarpath, zippath):
    """test the generalized extract function"""
    # test extracting some non-archive; this should fail
    fd, filename = tempfile.mkstemp()
    os.write(fd, b"This is not a zipfile or tarball")
    os.close(fd)
    exception = None

    try:
        dest = tempfile.mkdtemp()
        mozfile.extract(filename, dest)
    except Exception as exc:
        exception = exc
    finally:
        os.remove(filename)
        os.rmdir(dest)

    assert isinstance(exception, Exception)


def test_extract_ignore(tmpdir, bundlepath):
    dest = tmpdir.mkdir("dest").strpath
    ignore = ("foo", "**/fleem.txt", "read*.txt")
    mozfile.extract(bundlepath, dest, ignore=ignore)

    assert sorted(os.listdir(dest)) == ["bar.txt", "foo.txt"]


def test_tarball_escape(tmpdir):
    """Ensures that extracting a tarball can't write outside of the intended
    destination directory.
    """
    workdir = tmpdir.mkdir("workdir")
    os.chdir(workdir)

    # Generate a "malicious" bundle.
    with open("bad.txt", "w") as fh:
        fh.write("pwned!")

    def change_name(tarinfo):
        tarinfo.name = "../" + tarinfo.name
        return tarinfo

    with tarfile.open("evil.tar", "w:xz") as tar:
        tar.add("bad.txt", filter=change_name)

    with pytest.raises(RuntimeError):
        mozfile.extract_tarball("evil.tar", workdir)
    assert not os.path.exists(tmpdir.join("bad.txt"))


if __name__ == "__main__":
    mozunit.main()
