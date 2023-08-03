#  This Source Code Form is subject to the terms of the Mozilla Public
#  License, v. 2.0. If a copy of the MPL was not distributed with this
#  file, You can obtain one at https://mozilla.org/MPL/2.0/.

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from contextlib import nullcontext as does_not_raise

import pytest
from mozunit import main

from mozbuild.vendor.vendor_manifest import _replace_in_file


@pytest.mark.parametrize(
    "pattern,replacement,input_data,expected,exception",
    [
        (
            "lookup.c",
            "vorbis_lookup.c",
            '#include "lookup.c"\n',
            '#include "vorbis_lookup.c"\n',
            does_not_raise(),
        ),
        (
            "lookup.c",
            "vorbis_lookup.c",
            '#include "vorbis_lookup.c"\n',
            '#include "vorbis_lookup.c"\n',
            pytest.raises(Exception),
        ),
        (
            "@VCS_TAG@",
            "616bfd1506a8a75c6a358e578cbec9ca11931502",
            '#define DAV1D_VERSION "@VCS_TAG@"\n',
            '#define DAV1D_VERSION "616bfd1506a8a75c6a358e578cbec9ca11931502"\n',
            does_not_raise(),
        ),
        (
            "@VCS_TAG@",
            "616bfd1506a8a75c6a358e578cbec9ca11931502",
            '#define DAV1D_VERSION "8a651f3a1f40fe73847423fb6a5776103cb42218"\n',
            '#define DAV1D_VERSION "616bfd1506a8a75c6a358e578cbec9ca11931502"\n',
            pytest.raises(Exception),
        ),
        (
            "[regexy pattern]",
            "replaced",
            "here\nis [regexy pattern]\n",
            "here\nis replaced\n",
            does_not_raise(),
        ),
        (
            "some (other) regex.",
            "replaced,",
            "This can be some (other) regex. Yo!\n",
            "This can be replaced, Yo!\n",
            does_not_raise(),
        ),
    ],
)
def test_replace_in_file(
    tmp_path, pattern, replacement, input_data, expected, exception
):
    file = tmp_path / "file.txt"
    file.write_text(input_data)

    with exception:
        _replace_in_file(file, pattern, replacement, regex=False)
        assert file.read_text() == expected


@pytest.mark.parametrize(
    "pattern,replacement,input_data,expected,exception",
    [
        (
            r"\[tag v[1-9\.]+\]",
            "[tag v1.2.13]",
            "Version:\n#[tag v1.2.12]\n",
            "Version:\n#[tag v1.2.13]\n",
            does_not_raise(),
        ),
        (
            r"\[tag v[1-9\.]+\]",
            "[tag v1.2.13]",
            "Version:\n#[tag v1.2.13]\n",
            "Version:\n#[tag v1.2.13]\n",
            does_not_raise(),
        ),
        (
            r"\[tag v[1-9\.]+\]",
            "[tag v0.17.0]",
            "Version:\n#[tag v0.16.3]\n",
            "Version:\n#[tag v0.17.0]\n",
            pytest.raises(Exception),
        ),
        (
            r"\[tag v[1-9\.]+\]",
            "[tag v0.17.0]",
            "Version:\n#[tag v0.16.3]\n",
            "Version:\n#[tag v0.17.0]\n",
            pytest.raises(Exception),
        ),
        (
            r'DEFINES\["OPUS_VERSION"\] = "(.+)"',
            'DEFINES["OPUS_VERSION"] = "5023249b5c935545fb02dbfe845cae996ecfc8bb"',
            'DEFINES["OPUS_BUILD"] = True\nDEFINES["OPUS_VERSION"] = "8a651f3a1f40fe73847423fb6a5776103cb42218"\nDEFINES["USE_ALLOCA"] = True\n',
            'DEFINES["OPUS_BUILD"] = True\nDEFINES["OPUS_VERSION"] = "5023249b5c935545fb02dbfe845cae996ecfc8bb"\nDEFINES["USE_ALLOCA"] = True\n',
            does_not_raise(),
        ),
        (
            r'DEFINES\["OPUS_VERSION"\] = "(.+)"',
            'DEFINES["OPUS_VERSION"] = "5023249b5c935545fb02dbfe845cae996ecfc8bb"',
            'DEFINES["OPUS_BUILD"] = True\nDEFINES["OPUS_TAG"] = "8a651f3a1f40fe73847423fb6a5776103cb42218"\nDEFINES["USE_ALLOCA"] = True\n',
            'DEFINES["OPUS_BUILD"] = True\nDEFINES["OPUS_VERSION"] = "5023249b5c935545fb02dbfe845cae996ecfc8bb"\nDEFINES["USE_ALLOCA"] = True\n',
            pytest.raises(Exception),
        ),
    ],
)
def test_replace_in_file_regex(
    tmp_path, pattern, replacement, input_data, expected, exception
):
    file = tmp_path / "file.txt"
    file.write_text(input_data)

    with exception:
        _replace_in_file(file, pattern, replacement, regex=True)
        assert file.read_text() == expected


if __name__ == "__main__":
    main()
