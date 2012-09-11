mozboot - Bootstrap your system to build Mozilla projects
=========================================================

This package contains code used for bootstrapping a system to build
mozilla-central.

This code is not part of the build system per se. Instead, it is related
to everything up to invoking the actual build system.

If you have a copy of the source tree, you run:

    python bin/bootstrap.py

If you don't have a copy of the source tree, you can run:

    curl https://hg.mozilla.org/mozilla-central/raw-file/default/python/mozboot/bin/bootstrap.py | python -

The bootstrap script will download everything it needs from hg.mozilla.org
automatically!
