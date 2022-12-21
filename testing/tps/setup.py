# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from setuptools import find_packages, setup

version = "0.6"

deps = [
    "httplib2 == 0.9.2",
    "mozfile >= 1.2",
    "wptserve >= 3.0",
    "mozinfo >= 1.2",
    "mozinstall == 2.0.1",
    "mozprocess == 1.3",
    "mozprofile ~= 2.1",
    "mozrunner ~= 8.2",
    "mozversion == 2.3",
    "PyYAML >= 4.0",
]

setup(
    name="tps",
    version=version,
    description="run automated multi-profile sync tests",
    long_description="""\
""",
    classifiers=[
        "Programming Language :: Python :: 2.7",
        "Programming Language :: Python :: 2 :: Only",
    ],  # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
    keywords="",
    author="Mozilla Automation and Tools team",
    author_email="tools@lists.mozilla.org",
    url="https://developer.mozilla.org/en-US/docs/TPS",
    license="MPL 2.0",
    packages=find_packages(exclude=["ez_setup", "examples", "tests"]),
    include_package_data=True,
    zip_safe=False,
    install_requires=deps,
    entry_points="""
      # -*- Entry points: -*-
      [console_scripts]
      runtps = tps.cli:main
      """,
    data_files=[
        ("tps", ["config/config.json.in"]),
    ],
)
