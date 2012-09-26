# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals


class ArgumentProvider(object):
    """Base class for classes wishing to provide CLI arguments to mach."""

    @staticmethod
    def populate_argparse(parser):
        raise Exception("populate_argparse not implemented.")
