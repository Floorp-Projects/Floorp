# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from random import randint
from zipfile import ZipFile
import os
import shutil


def gen_binary_file(path, size):
    with open(path, 'wb') as f:
        for i in xrange(size):
            byte = '%c' % randint(0, 255)
            f.write(byte)


def gen_zip(path, files, stripped_prefix=''):
    with ZipFile(path, 'w') as z:
        for f in files:
            new_name = f.replace(stripped_prefix, '')
            z.write(f, new_name)


def mkdir(path, *args):
    try:
        os.mkdir(path, *args)
    except OSError:
        pass


def gen_folder_structure():
    root = 'test-files'
    prefix = os.path.join(root, 'push2')
    mkdir(prefix)

    gen_binary_file(os.path.join(prefix, 'file4.bin'), 59036)
    mkdir(os.path.join(prefix, 'sub1'))
    shutil.copyfile(os.path.join(root, 'mytext.txt'),
                    os.path.join(prefix, 'sub1', 'file1.txt'))
    mkdir(os.path.join(prefix, 'sub1', 'sub1.1'))
    shutil.copyfile(os.path.join(root, 'mytext.txt'),
                    os.path.join(prefix, 'sub1', 'sub1.1', 'file2.txt'))
    mkdir(os.path.join(prefix, 'sub2'))
    shutil.copyfile(os.path.join(root, 'mytext.txt'),
                    os.path.join(prefix, 'sub2', 'file3.txt'))


def gen_test_files():
    gen_folder_structure()
    flist = [
        os.path.join('test-files', 'push2'),
        os.path.join('test-files', 'push2', 'file4.bin'),
        os.path.join('test-files', 'push2', 'sub1'),
        os.path.join('test-files', 'push2', 'sub1', 'file1.txt'),
        os.path.join('test-files', 'push2', 'sub1', 'sub1.1'),
        os.path.join('test-files', 'push2', 'sub1', 'sub1.1', 'file2.txt'),
        os.path.join('test-files', 'push2', 'sub2'),
        os.path.join('test-files', 'push2', 'sub2', 'file3.txt')
    ]
    gen_zip(os.path.join('test-files', 'mybinary.zip'),
            flist, stripped_prefix=('test-files' + os.path.sep))
    gen_zip(os.path.join('test-files', 'mytext.zip'),
            [os.path.join('test-files', 'mytext.txt')])


def clean_test_files():
    ds = [os.path.join('test-files', d) for d in ('push1', 'push2')]
    for d in ds:
        try:
            shutil.rmtree(d)
        except OSError:
            pass

    fs = [os.path.join('test-files', f) for f in ('mybinary.zip', 'mytext.zip')]
    for f in fs:
        try:
            os.remove(f)
        except OSError:
            pass


if __name__ == '__main__':
    gen_test_files()
