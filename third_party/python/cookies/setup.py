from setuptools import setup, Command
from cookies import __version__

class Test(Command):
    user_options = []

    def initialize_options(self):
        pass

    def finalize_options(self):
        pass

    def run(self):
        try:
            import pytest
        except ImportError:
            raise AssertionError("Install py.test to run the tests")
        import sys, subprocess
        errno = subprocess.call([sys.executable, '-m', 'py.test'])
        raise SystemExit(errno)

setup(
    name="cookies",
    version=__version__,
    author="Sasha Hart",
    author_email="s@sashahart.net",
    url="https://github.com/sashahart/cookies",
    py_modules=['cookies', 'test_cookies'],
    description="Friendlier RFC 6265-compliant cookie parser/renderer",
    long_description=open('README').read(),
    classifiers=[
        "Development Status :: 4 - Beta",
        "Environment :: Other Environment",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: MIT License",
        "Programming Language :: Python :: 2.6",
        "Programming Language :: Python :: 2.7",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.2",
        "Programming Language :: Python :: 3.3",
        "Programming Language :: Python :: Implementation :: PyPy",
        "Topic :: Software Development :: Libraries :: Python Modules",
    ],
    cmdclass = {'test': Test},
)
