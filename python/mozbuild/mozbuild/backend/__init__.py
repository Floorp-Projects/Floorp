# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

backends = {
    'ChromeMap': 'mozbuild.codecoverage.chrome_map',
    'CompileDB': 'mozbuild.compilation.database',
    'CppEclipse': 'mozbuild.backend.cpp_eclipse',
    'FasterMake': 'mozbuild.backend.fastermake',
    'FasterMake+RecursiveMake': None,
    'GnConfigGen': 'mozbuild.gn_processor',
    'GnMozbuildWriter': 'mozbuild.gn_processor',
    'RecursiveMake': 'mozbuild.backend.recursivemake',
    'TestManifest': 'mozbuild.backend.test_manifest',
    'VisualStudio': 'mozbuild.backend.visualstudio',
}


def get_backend_class(name):
    if '+' in name:
        from mozbuild.backend.base import HybridBackend
        return HybridBackend(*(get_backend_class(name)
                               for name in name.split('+')))

    class_name = '%sBackend' % name
    module = __import__(backends[name], globals(), locals(), [class_name])
    return getattr(module, class_name)
