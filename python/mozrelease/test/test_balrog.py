# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import mozunit
import pytest

import json
from mozrelease.util import load as yaml_load
from pathlib import Path

from mozrelease.balrog import generate_update_properties
from mozilla_version.gecko import GeckoVersion

DATA_PATH = Path(__file__).parent.joinpath("data")


@pytest.mark.parametrize(
    "context,config_file,output_file",
    [
        (
            {
                "release-type": "release",
                "product": "firefox",
                "version": GeckoVersion.parse("62.0.3"),
            },
            "whatsnew-62.0.3.yml",
            "Firefox-62.0.3.update.json",
        ),
        (
            {
                "release-type": "beta",
                "product": "firefox",
                "version": GeckoVersion.parse("64.0"),
            },
            "whatsnew-62.0.3.yml",
            "Firefox-64.0b13.update.json",
        ),
    ],
)
def test_update_properties(context, config_file, output_file):
    with DATA_PATH.joinpath(config_file).open("r", encoding="utf-8") as f:
        config = yaml_load(f)

    update_line = generate_update_properties(context, config)

    assert update_line == json.load(
        DATA_PATH.joinpath(output_file).open("r", encoding="utf-8")
    )


if __name__ == "__main__":
    mozunit.main()
