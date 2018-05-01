import io
from setuptools import setup


def readfile(filename):
    with io.open(filename, encoding="utf-8") as stream:
        return stream.read().split("\n")

readme = readfile("README.rst")[5:]  # skip title and badges
requires = readfile("requirements.txt")
version = readfile("VERSION")[0].strip()

setup(
    name='pathlib2',
    version=version,
    py_modules=['pathlib2'],
    license='MIT',
    description='Object-oriented filesystem paths',
    long_description="\n".join(readme[2:]),
    author='Matthias C. M. Troffaes',
    author_email='matthias.troffaes@gmail.com',
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Operating System :: OS Independent',
        'Programming Language :: Python',
        'Programming Language :: Python :: 2',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 2.6',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3.2',
        'Programming Language :: Python :: 3.3',
        'Programming Language :: Python :: 3.4',
        'Topic :: Software Development :: Libraries',
        'Topic :: System :: Filesystems',
        ],
    download_url='https://pypi.python.org/pypi/pathlib2/',
    url='https://pypi.python.org/pypi/pathlib2/',
    install_requires=requires,
)
