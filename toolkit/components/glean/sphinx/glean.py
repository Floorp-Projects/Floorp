# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from pathlib import Path
import sys


def setup(app):
    from moztreedocs import manager

    # Import the list of metrics and ping files
    glean_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), os.path.pardir))
    sys.path.append(glean_dir)
    from metrics_index import metrics_yamls, pings_yamls

    # Import the custom version expiry code.
    glean_parser_ext_dir = os.path.abspath(
        Path(glean_dir) / "build_scripts" / "glean_parser_ext"
    )
    sys.path.append(glean_parser_ext_dir)
    from run_glean_parser import get_parser_options

    firefox_version = "4.0a1"  # TODO: bug 1676416 - Get the real app version.
    parser_config = get_parser_options(firefox_version)

    input_files = [Path(os.path.join(manager.topsrcdir, x)) for x in metrics_yamls]
    input_files += [Path(os.path.join(manager.topsrcdir, x)) for x in pings_yamls]

    # Generate the autodocs.
    from glean_parser import translate

    out_path = Path(os.path.join(manager.staging_dir, "metrics"))
    translate.translate(
        input_files, "markdown", out_path, {"project_title": "Firefox"}, parser_config
    )

    # Rename the generated docfile to index so Sphinx finds it
    os.rename(os.path.join(out_path, "metrics.md"), os.path.join(out_path, "index.md"))
