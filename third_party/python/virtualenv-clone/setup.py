import sys
from setuptools.command.test import test as TestCommand
from setuptools import setup


if __name__ == '__main__' and sys.version_info < (2, 5):
    raise SystemExit("Python >= 2.5 required for virtualenv-clone")

test_requirements = [
    'virtualenv',
    'tox',
    'pytest'
]


class ToxTest(TestCommand):
    def finalize_options(self):
        TestCommand.finalize_options(self)
        self.test_args = []
        self.test_suite = True

    def run_tests(self):
        import tox
        tox.cmdline()


setup(name="virtualenv-clone",
    version='0.3.0',
    description='script to clone virtualenvs.',
    author='Edward George',
    author_email='edwardgeorge@gmail.com',
    url='http://github.com/edwardgeorge/virtualenv-clone',
    license="MIT",
    py_modules=["clonevirtualenv"],
    entry_points={
        'console_scripts': [
            'virtualenv-clone=clonevirtualenv:main',
    ]},
    classifiers=[
        "License :: OSI Approved :: MIT License",
        "Programming Language :: Python",
        "Intended Audience :: Developers",
        "Development Status :: 3 - Alpha",
        "Programming Language :: Python :: 2.6",
        "Programming Language :: Python :: 2.7",
        "Programming Language :: Python :: 3.3",
        "Programming Language :: Python :: 3.4",
        "Programming Language :: Python :: 3.5",
        "Programming Language :: Python :: 3.6",
    ],
    tests_require=test_requirements,
    cmdclass={'test': ToxTest}
)
