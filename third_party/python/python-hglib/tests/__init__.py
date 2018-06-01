import os, tempfile, sys, shutil

def setUp():
    os.environ['LANG'] = os.environ['LC_ALL'] = os.environ['LANGUAGE'] = 'C'
    os.environ["EMAIL"] = "Foo Bar <foo.bar@example.com>"
    os.environ['CDPATH'] = ''
    os.environ['COLUMNS'] = '80'
    os.environ['GREP_OPTIONS'] = ''
    os.environ['http_proxy'] = ''

    os.environ["HGEDITOR"] = sys.executable + ' -c "import sys; sys.exit(0)"'
    os.environ["HGMERGE"] = "internal:merge"
    os.environ["HGUSER"]   = "test"
    os.environ["HGENCODING"] = "ascii"
    os.environ["HGENCODINGMODE"] = "strict"
    tmpdir = tempfile.mkdtemp('', 'python-hglib.')
    os.environ["HGTMP"] = os.path.realpath(tmpdir)
    os.environ["HGRCPATH"] = os.pathsep

def tearDown(self):
    os.chdir('..')
    shutil.rmtree(os.environ["HGTMP"])
