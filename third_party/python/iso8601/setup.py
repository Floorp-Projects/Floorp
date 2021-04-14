import os

try:
    from setuptools import setup
except ImportError:
    from distutils import setup

long_description = (
    open(os.path.join(os.path.dirname(__file__), "README.rst"), "rb")
    .read()
    .decode("UTF-8")
)

setup(
    name="iso8601",
    version="0.1.14",
    description=long_description.split("\n")[0],
    long_description=long_description,
    long_description_content_type="text/x-rst",
    author="Michael Twomey",
    author_email="pyiso8601@mick.twomeylee.name",
    url="https://github.com/micktwomey/pyiso8601",
    project_urls={
        "Documentation": "https://pyiso8601.readthedocs.org/",
        "Issue Tracker": "https://github.com/micktwomey/pyiso8601/issues",
    },
    packages=["iso8601"],
    license="MIT",
    classifiers=[
        "License :: OSI Approved :: MIT License",
        "Programming Language :: Python :: 2",
        "Programming Language :: Python :: 3",
    ],
)
