# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

class BaseBootstrapper(object):
    """Base class for system bootstrappers."""
    def __init__(self):
        pass

    def install_system_packages(self):
        raise NotImplemented('%s must implement install_system_packages()' %
            __name__)
