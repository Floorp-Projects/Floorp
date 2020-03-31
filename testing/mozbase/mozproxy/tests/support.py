from __future__ import absolute_import, print_function
import shutil
import contextlib
import tempfile


# This helper can be replaced by pytest tmpdir fixture
# once Bug 1536029 lands (@mock.patch disturbs pytest fixtures)
@contextlib.contextmanager
def tempdir():
    dest_dir = tempfile.mkdtemp()
    try:
        yield dest_dir
    finally:
        shutil.rmtree(dest_dir, ignore_errors=True)
