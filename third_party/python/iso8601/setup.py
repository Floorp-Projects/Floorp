import os

try:
    from setuptools import setup
except ImportError:
    from distutils import setup

long_description = open(os.path.join(os.path.dirname(__file__), "README.rst")).read()

setup(
    name="iso8601",
    version="0.1.12",
    description=long_description.split("\n")[0],
    long_description=long_description,
    author="Michael Twomey",
    author_email="micktwomey+iso8601@gmail.com",
    url="https://bitbucket.org/micktwomey/pyiso8601",
    packages=["iso8601"],
    license="MIT",
    classifiers=[
        "License :: OSI Approved :: MIT License",
        "Programming Language :: Python :: 2",
        "Programming Language :: Python :: 3",
    ],
)
