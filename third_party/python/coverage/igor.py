# coding: utf-8
# Licensed under the Apache License: http://www.apache.org/licenses/LICENSE-2.0
# For details: https://github.com/nedbat/coveragepy/blob/master/NOTICE.txt

"""Helper for building, testing, and linting coverage.py.

To get portability, all these operations are written in Python here instead
of in shell scripts, batch files, or Makefiles.

"""

import contextlib
import fnmatch
import glob
import inspect
import os
import platform
import sys
import textwrap
import warnings
import zipfile

import pytest


@contextlib.contextmanager
def ignore_warnings():
    """Context manager to ignore warning within the with statement."""
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        yield


# Functions named do_* are executable from the command line: do_blah is run
# by "python igor.py blah".


def do_show_env():
    """Show the environment variables."""
    print("Environment:")
    for env in sorted(os.environ):
        print("  %s = %r" % (env, os.environ[env]))


def do_remove_extension():
    """Remove the compiled C extension, no matter what its name."""

    so_patterns = """
        tracer.so
        tracer.*.so
        tracer.pyd
        tracer.*.pyd
        """.split()

    for pattern in so_patterns:
        pattern = os.path.join("coverage", pattern)
        for filename in glob.glob(pattern):
            try:
                os.remove(filename)
            except OSError:
                pass


def label_for_tracer(tracer):
    """Get the label for these tests."""
    if tracer == "py":
        label = "with Python tracer"
    else:
        label = "with C tracer"

    return label


def should_skip(tracer):
    """Is there a reason to skip these tests?"""
    if tracer == "py":
        # $set_env.py: COVERAGE_NO_PYTRACER - Don't run the tests under the Python tracer.
        skipper = os.environ.get("COVERAGE_NO_PYTRACER")
    else:
        # $set_env.py: COVERAGE_NO_CTRACER - Don't run the tests under the C tracer.
        skipper = os.environ.get("COVERAGE_NO_CTRACER")

    if skipper:
        msg = "Skipping tests " + label_for_tracer(tracer)
        if len(skipper) > 1:
            msg += ": " + skipper
    else:
        msg = ""

    return msg


def make_env_id(tracer):
    """An environment id that will keep all the test runs distinct."""
    impl = platform.python_implementation().lower()
    version = "%s%s" % sys.version_info[:2]
    if '__pypy__' in sys.builtin_module_names:
        version += "_%s%s" % sys.pypy_version_info[:2]
    env_id = "%s%s_%s" % (impl, version, tracer)
    return env_id


def run_tests(tracer, *runner_args):
    """The actual running of tests."""
    if 'COVERAGE_TESTING' not in os.environ:
        os.environ['COVERAGE_TESTING'] = "True"
    # $set_env.py: COVERAGE_ENV_ID - Use environment-specific test directories.
    if 'COVERAGE_ENV_ID' in os.environ:
        os.environ['COVERAGE_ENV_ID'] = make_env_id(tracer)
    print_banner(label_for_tracer(tracer))
    return pytest.main(list(runner_args))


def run_tests_with_coverage(tracer, *runner_args):
    """Run tests, but with coverage."""
    # Need to define this early enough that the first import of env.py sees it.
    os.environ['COVERAGE_TESTING'] = "True"
    os.environ['COVERAGE_PROCESS_START'] = os.path.abspath('metacov.ini')
    os.environ['COVERAGE_HOME'] = os.getcwd()

    # Create the .pth file that will let us measure coverage in sub-processes.
    # The .pth file seems to have to be alphabetically after easy-install.pth
    # or the sys.path entries aren't created right?
    # There's an entry in "make clean" to get rid of this file.
    pth_dir = os.path.dirname(pytest.__file__)
    pth_path = os.path.join(pth_dir, "zzz_metacov.pth")
    with open(pth_path, "w") as pth_file:
        pth_file.write("import coverage; coverage.process_startup()\n")

    suffix = "%s_%s" % (make_env_id(tracer), platform.platform())
    os.environ['COVERAGE_METAFILE'] = os.path.abspath(".metacov."+suffix)

    import coverage
    cov = coverage.Coverage(config_file="metacov.ini")
    cov._warn_unimported_source = False
    cov._warn_preimported_source = False
    cov.start()

    try:
        # Re-import coverage to get it coverage tested!  I don't understand all
        # the mechanics here, but if I don't carry over the imported modules
        # (in covmods), then things go haywire (os == None, eventually).
        covmods = {}
        covdir = os.path.split(coverage.__file__)[0]
        # We have to make a list since we'll be deleting in the loop.
        modules = list(sys.modules.items())
        for name, mod in modules:
            if name.startswith('coverage'):
                if getattr(mod, '__file__', "??").startswith(covdir):
                    covmods[name] = mod
                    del sys.modules[name]
        import coverage                         # pylint: disable=reimported
        sys.modules.update(covmods)

        # Run tests, with the arguments from our command line.
        status = run_tests(tracer, *runner_args)

    finally:
        cov.stop()
        os.remove(pth_path)

    cov.combine()
    cov.save()

    return status


def do_combine_html():
    """Combine data from a meta-coverage run, and make the HTML and XML reports."""
    import coverage
    os.environ['COVERAGE_HOME'] = os.getcwd()
    os.environ['COVERAGE_METAFILE'] = os.path.abspath(".metacov")
    cov = coverage.Coverage(config_file="metacov.ini")
    cov.load()
    cov.combine()
    cov.save()
    show_contexts = bool(os.environ.get('COVERAGE_CONTEXT'))
    cov.html_report(show_contexts=show_contexts)
    cov.xml_report()


def do_test_with_tracer(tracer, *runner_args):
    """Run tests with a particular tracer."""
    # If we should skip these tests, skip them.
    skip_msg = should_skip(tracer)
    if skip_msg:
        print(skip_msg)
        return None

    os.environ["COVERAGE_TEST_TRACER"] = tracer
    if os.environ.get("COVERAGE_COVERAGE", "no") == "yes":
        return run_tests_with_coverage(tracer, *runner_args)
    else:
        return run_tests(tracer, *runner_args)


def do_zip_mods():
    """Build the zipmods.zip file."""
    zf = zipfile.ZipFile("tests/zipmods.zip", "w")

    # Take one file from disk.
    zf.write("tests/covmodzip1.py", "covmodzip1.py")

    # The others will be various encodings.
    source = textwrap.dedent(u"""\
        # coding: {encoding}
        text = u"{text}"
        ords = {ords}
        assert [ord(c) for c in text] == ords
        print(u"All OK with {encoding}")
        """)
    # These encodings should match the list in tests/test_python.py
    details = [
        (u'utf8', u'ⓗⓔⓛⓛⓞ, ⓦⓞⓡⓛⓓ'),
        (u'gb2312', u'你好，世界'),
        (u'hebrew', u'שלום, עולם'),
        (u'shift_jis', u'こんにちは世界'),
        (u'cp1252', u'“hi”'),
    ]
    for encoding, text in details:
        filename = 'encoded_{}.py'.format(encoding)
        ords = [ord(c) for c in text]
        source_text = source.format(encoding=encoding, text=text, ords=ords)
        zf.writestr(filename, source_text.encode(encoding))

    zf.close()

    zf = zipfile.ZipFile("tests/covmain.zip", "w")
    zf.write("coverage/__main__.py", "__main__.py")
    zf.close()


def do_install_egg():
    """Install the egg1 egg for tests."""
    # I am pretty certain there are easier ways to install eggs...
    cur_dir = os.getcwd()
    os.chdir("tests/eggsrc")
    with ignore_warnings():
        import distutils.core
        distutils.core.run_setup("setup.py", ["--quiet", "bdist_egg"])
        egg = glob.glob("dist/*.egg")[0]
        distutils.core.run_setup(
            "setup.py", ["--quiet", "easy_install", "--no-deps", "--zip-ok", egg]
        )
    os.chdir(cur_dir)


def do_check_eol():
    """Check files for incorrect newlines and trailing whitespace."""

    ignore_dirs = [
        '.svn', '.hg', '.git',
        '.tox*',
        '*.egg-info',
        '_build',
        '_spell',
    ]
    checked = set()

    def check_file(fname, crlf=True, trail_white=True):
        """Check a single file for whitespace abuse."""
        fname = os.path.relpath(fname)
        if fname in checked:
            return
        checked.add(fname)

        line = None
        with open(fname, "rb") as f:
            for n, line in enumerate(f, start=1):
                if crlf:
                    if b"\r" in line:
                        print("%s@%d: CR found" % (fname, n))
                        return
                if trail_white:
                    line = line[:-1]
                    if not crlf:
                        line = line.rstrip(b'\r')
                    if line.rstrip() != line:
                        print("%s@%d: trailing whitespace found" % (fname, n))
                        return

        if line is not None and not line.strip():
            print("%s: final blank line" % (fname,))

    def check_files(root, patterns, **kwargs):
        """Check a number of files for whitespace abuse."""
        for where, dirs, files in os.walk(root):
            for f in files:
                fname = os.path.join(where, f)
                for p in patterns:
                    if fnmatch.fnmatch(fname, p):
                        check_file(fname, **kwargs)
                        break
            for ignore_dir in ignore_dirs:
                ignored = []
                for dir_name in dirs:
                    if fnmatch.fnmatch(dir_name, ignore_dir):
                        ignored.append(dir_name)
                for dir_name in ignored:
                    dirs.remove(dir_name)

    check_files("coverage", ["*.py"])
    check_files("coverage/ctracer", ["*.c", "*.h"])
    check_files("coverage/htmlfiles", ["*.html", "*.scss", "*.css", "*.js"])
    check_files("tests", ["*.py"])
    check_files("tests", ["*,cover"], trail_white=False)
    check_files("tests/js", ["*.js", "*.html"])
    check_file("setup.py")
    check_file("igor.py")
    check_file("Makefile")
    check_file(".travis.yml")
    check_files(".", ["*.rst", "*.txt"])
    check_files(".", ["*.pip"])


def print_banner(label):
    """Print the version of Python."""
    try:
        impl = platform.python_implementation()
    except AttributeError:
        impl = "Python"

    version = platform.python_version()

    if '__pypy__' in sys.builtin_module_names:
        version += " (pypy %s)" % ".".join(str(v) for v in sys.pypy_version_info)

    try:
        which_python = os.path.relpath(sys.executable)
    except ValueError:
        # On Windows having a python executable on a different drive
        # than the sources cannot be relative.
        which_python = sys.executable
    print('=== %s %s %s (%s) ===' % (impl, version, label, which_python))
    sys.stdout.flush()


def do_help():
    """List the available commands"""
    items = list(globals().items())
    items.sort()
    for name, value in items:
        if name.startswith('do_'):
            print("%-20s%s" % (name[3:], value.__doc__))


def analyze_args(function):
    """What kind of args does `function` expect?

    Returns:
        star, num_pos:
            star(boolean): Does `function` accept *args?
            num_args(int): How many positional arguments does `function` have?
    """
    try:
        getargspec = inspect.getfullargspec
    except AttributeError:
        getargspec = inspect.getargspec
    with ignore_warnings():
        # DeprecationWarning: Use inspect.signature() instead of inspect.getfullargspec()
        argspec = getargspec(function)
    return bool(argspec[1]), len(argspec[0])


def main(args):
    """Main command-line execution for igor.

    Verbs are taken from the command line, and extra words taken as directed
    by the arguments needed by the handler.

    """
    while args:
        verb = args.pop(0)
        handler = globals().get('do_'+verb)
        if handler is None:
            print("*** No handler for %r" % verb)
            return 1
        star, num_args = analyze_args(handler)
        if star:
            # Handler has *args, give it all the rest of the command line.
            handler_args = args
            args = []
        else:
            # Handler has specific arguments, give it only what it needs.
            handler_args = args[:num_args]
            args = args[num_args:]
        ret = handler(*handler_args)
        # If a handler returns a failure-like value, stop.
        if ret:
            return ret
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
