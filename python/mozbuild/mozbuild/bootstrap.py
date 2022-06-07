# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from mozbuild.configure import ConfigureSandbox
from pathlib import Path
import functools
import io
import logging
import os


@functools.lru_cache(maxsize=None)
def _bootstrap_sandbox():
    # Here, we don't want an existing mozconfig to interfere with what we
    # do, neither do we want the default for --enable-bootstrap (which is not
    # always on) to prevent this from doing something.
    out = io.StringIO()
    logger = logging.getLogger("moz.configure")
    handler = logging.StreamHandler(out)
    logger.addHandler(handler)
    logger.propagate = False
    sandbox = ConfigureSandbox(
        {},
        argv=["configure", "--enable-bootstrap", f"MOZCONFIG={os.devnull}"],
        logger=logger,
    )
    moz_configure = (
        Path(__file__).parent.parent.parent.parent / "build" / "moz.configure"
    )
    sandbox.include_file(str(moz_configure / "init.configure"))
    # bootstrap_search_path_order has a dependency on developer_options, which
    # is not defined in init.configure. Its value doesn't matter for us, though.
    sandbox["developer_options"] = sandbox["always"]
    sandbox.include_file(str(moz_configure / "bootstrap.configure"))
    return sandbox


def bootstrap_toolchain(toolchain_job):
    # Expand the `bootstrap_path` template for the given toolchain_job, and execute the
    # expanded function via `_value_for`, which will trigger autobootstrap.
    sandbox = _bootstrap_sandbox()
    sandbox._value_for(sandbox["bootstrap_path"](toolchain_job))
