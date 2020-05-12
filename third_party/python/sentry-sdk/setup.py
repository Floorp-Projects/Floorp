#!/usr/bin/env python

"""
Sentry-Python - Sentry SDK for Python
=====================================

**Sentry-Python is an SDK for Sentry.** Check out `GitHub
<https://github.com/getsentry/sentry-python>`_ to find out more.
"""

from setuptools import setup, find_packages

setup(
    name="sentry-sdk",
    version="0.14.3",
    author="Sentry Team and Contributors",
    author_email="hello@getsentry.com",
    url="https://github.com/getsentry/sentry-python",
    description="Python client for Sentry (https://getsentry.com)",
    long_description=__doc__,
    packages=find_packages(exclude=("tests", "tests.*")),
    # PEP 561
    package_data={"sentry_sdk": ["py.typed"]},
    zip_safe=False,
    license="BSD",
    install_requires=["urllib3>=1.10.0", "certifi"],
    extras_require={
        "flask": ["flask>=0.11", "blinker>=1.1"],
        "bottle": ["bottle>=0.12.13"],
        "falcon": ["falcon>=1.4"],
        "django": ["django>=1.8"],
        "sanic": ["sanic>=0.8"],
        "celery": ["celery>=3"],
        "beam": ["beam>=2.12"],
        "rq": ["0.6"],
        "aiohttp": ["aiohttp>=3.5"],
        "tornado": ["tornado>=5"],
        "sqlalchemy": ["sqlalchemy>=1.2"],
        "pyspark": ["pyspark>=2.4.4"],
    },
    classifiers=[
        "Development Status :: 5 - Production/Stable",
        "Environment :: Web Environment",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: BSD License",
        "Operating System :: OS Independent",
        "Programming Language :: Python",
        "Programming Language :: Python :: 2",
        "Programming Language :: Python :: 2.7",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.4",
        "Programming Language :: Python :: 3.5",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Topic :: Software Development :: Libraries :: Python Modules",
    ],
)
