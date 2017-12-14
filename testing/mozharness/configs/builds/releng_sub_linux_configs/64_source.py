import os

config = {
    "tmpl": """The binaries in this directory are made available to you under the Mozilla Public License v.2
(MPL 2):
http://www.mozilla.org/MPL/2.0/

The source code used to build these binaries is specified by the following unique URL:
{repo}/rev/{revision}

    zip: {repo}/archive/{revision}.zip
    tar.gz: {repo}/archive/{revision}.tar.gz
    tar.bz2: {repo}/archive/{revision}.tar.bz2

Instructions for downloading and building this source code can be found here:
https://developer.mozilla.org/en/Mozilla_Source_Code_%28Mercurial%29
""",
    "out_path": "artifacts/SOURCE",
    "repo": os.environ["GECKO_HEAD_REPOSITORY"],
    "revision": os.environ["GECKO_HEAD_REV"],
}
