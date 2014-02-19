import os
import shutil
import tempfile


# stub file paths
files = [('foo.txt',),
         ('foo', 'bar.txt',),
         ('foo', 'bar', 'fleem.txt',),
         ('foobar', 'fleem.txt',),
         ('bar.txt',),
         ('nested_tree', 'bar', 'fleem.txt',),
         ('readonly.txt',),
         ]


def create_stub():
    """create a stub directory"""

    tempdir = tempfile.mkdtemp()
    try:
        for path in files:
            fullpath = os.path.join(tempdir, *path)
            dirname = os.path.dirname(fullpath)
            if not os.path.exists(dirname):
                os.makedirs(dirname)
            contents = path[-1]
            f = file(fullpath, 'w')
            f.write(contents)
            f.close()
        return tempdir
    except Exception, e:
        try:
            shutil.rmtree(tempdir)
        except:
            pass
        raise e
