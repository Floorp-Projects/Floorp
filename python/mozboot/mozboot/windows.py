# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from mozboot.base import BaseBootstrapper

class WindowsBootstrapper(BaseBootstrapper):
    '''Bootstrapper for msys2 based environments for building in Windows.'''
    def __init__(self, **kwargs):
        if 'MOZ_WINDOWS_BOOTSTRAP' not in os.environ or os.environ['MOZ_WINDOWS_BOOTSTRAP'] != '1':
            raise NotImplementedError('Bootstrap support for Windows is under development. For now, use MozillaBuild '
                                      'to set up a build environment on Windows. If you are testing Windows Bootstrap support, '
                                      'try `export MOZ_WINDOWS_BOOTSTRAP=1`')
        BaseBootstrapper.__init__(self, **kwargs)
        raise NotImplementedError('Bootstrap support is not yet available for Windows. '
                                  'For now, use MozillaBuild to set up a build environment on Windows.')
