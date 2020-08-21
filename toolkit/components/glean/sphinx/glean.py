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
    from metrics_index import METRICS, PINGS
    input_files = [Path(os.path.join(manager.topsrcdir, x)) for x in METRICS]
    input_files += [Path(os.path.join(manager.topsrcdir, x)) for x in PINGS]

    # Generate the autodocs.
    from glean_parser import translate
    out_path = Path(os.path.join(manager.staging_dir, 'metrics'))
    translate.translate(input_files, 'markdown', out_path, {'project_title': 'Firefox'})

    # Rename the generated docfile to index so Sphinx finds it
    os.rename(os.path.join(out_path, 'metrics.md'), os.path.join(out_path, 'index.md'))
