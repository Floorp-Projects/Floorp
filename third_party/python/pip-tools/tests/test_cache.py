from contextlib import contextmanager
from os import remove
from shutil import rmtree
from tempfile import NamedTemporaryFile

from pytest import raises

from piptools.cache import read_cache_file, CorruptCacheError, DependencyCache


@contextmanager
def _read_cache_file_helper(to_write):
    """
    On enter, create the file with the given string, and then yield its path.
    On exit, delete that file.

    :param str to_write: the content to write to the file
    :yield: the path to the temporary file
    """
    try:
        # Create the file and write to it
        cache_file = NamedTemporaryFile(mode="w", delete=False)
        cache_file.write(to_write)
        cache_file.close()

        # Yield the path to the file
        yield cache_file.name

    finally:
        # Delete the file on exit
        remove(cache_file.name)


def test_read_cache_file_not_json():
    """
    A cache file that's not JSON should throw a corrupt cache error.
    """
    with _read_cache_file_helper("not json") as cache_file_name:
        with raises(CorruptCacheError):
            read_cache_file(cache_file_name)


def test_read_cache_file_wrong_format():
    """
    A cache file with a wrong "__format__" value should throw an assertion error.
    """
    with _read_cache_file_helper('{"__format__": 2}') as cache_file_name:
        with raises(AssertionError):
            read_cache_file(cache_file_name)


def test_read_cache_file_successful():
    """
    A good cache file.
    """
    with _read_cache_file_helper('{"__format__": 1, "dependencies": "success"}') as cache_file_name:
        assert "success" == read_cache_file(cache_file_name)


def test_reverse_dependencies(from_line, tmpdir):
    # Since this is a test, make a temporary directory. Converting to str from py.path.
    tmp_dir_path = str(tmpdir)

    # Create a cache object. The keys are packages, and the values are lists of packages on which the keys depend.
    cache = DependencyCache(cache_dir=tmp_dir_path)
    cache[from_line("top==1.2")] = ["middle>=0.3", "bottom>=5.1.2"]
    cache[from_line("top[xtra]==1.2")] = ["middle>=0.3", "bottom>=5.1.2", "bonus==0.4"]
    cache[from_line("middle==0.4")] = ["bottom<6"]
    cache[from_line("bottom==5.3.5")] = []
    cache[from_line("bonus==0.4")] = []

    # In this case, we're using top 1.2 without an extra, so the "bonus" package is not depended upon.
    reversed_no_extra = cache.reverse_dependencies([
        from_line("top==1.2"),
        from_line("middle==0.4"),
        from_line("bottom==5.3.5"),
        from_line("bonus==0.4")
    ])
    assert reversed_no_extra == {
        'middle': {'top'},
        'bottom': {'middle', 'top'}
    }

    # Now we're using top 1.2 with the "xtra" extra, so it depends on the "bonus" package.
    reversed_extra = cache.reverse_dependencies([
        from_line("top[xtra]==1.2"),
        from_line("middle==0.4"),
        from_line("bottom==5.3.5"),
        from_line("bonus==0.4")
    ])
    assert reversed_extra == {
        'middle': {'top'},
        'bottom': {'middle', 'top'},
        'bonus': {'top'}
    }

    # Clean up our temp directory
    rmtree(tmp_dir_path)
