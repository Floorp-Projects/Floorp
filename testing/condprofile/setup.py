# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from setuptools import setup, find_packages


entry_points = """
      [console_scripts]
      cp-creator = condprof.main:main
      cp-client = condprof.client:main
      """

setup(
    name="conditioned-profile",
    version="0.2",
    packages=find_packages(),
    description="Firefox Heavy Profile creator",
    include_package_data=True,
    data_files=[
        (
            "condprof",
            [
                "condprof/customization/default.json",
                "condprof/customization/youtube.json",
            ],
        )
    ],
    zip_safe=False,
    install_requires=[],  # use requirements files
    entry_points=entry_points,
)
