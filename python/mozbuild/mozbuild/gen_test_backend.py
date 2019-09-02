# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import os
import sys

from mozbuild.backend.test_manifest import TestManifestBackend
from mozbuild.base import BuildEnvironmentNotFoundException, MozbuildObject
from mozbuild.frontend.emitter import TreeMetadataEmitter
from mozbuild.frontend.reader import BuildReader, EmptyConfig
import mozpack.path as mozpath


def gen_test_backend():
    build_obj = MozbuildObject.from_environment()
    try:
        config = build_obj.config_environment
    except BuildEnvironmentNotFoundException:
        # Create a stub config.status file, since the TestManifest backend needs
        # to be re-created if configure runs. If the file doesn't exist,
        # mozbuild continually thinks the TestManifest backend is out of date
        # and tries to regenerate it.

        if not os.path.isdir(build_obj.topobjdir):
            os.makedirs(build_obj.topobjdir)

        config_status = mozpath.join(build_obj.topobjdir, 'config.status')
        open(config_status, 'w').close()

        print("No build detected, test metadata may be incomplete.")

        # If 'JS_STANDALONE' is set, tests that don't require an objdir won't
        # be picked up due to bug 1345209.
        substs = EmptyConfig.default_substs
        if 'JS_STANDALONE' in substs:
            del substs['JS_STANDALONE']

        config = EmptyConfig(build_obj.topsrcdir, substs)
        config.topobjdir = build_obj.topobjdir

    reader = BuildReader(config)
    emitter = TreeMetadataEmitter(config)
    backend = TestManifestBackend(config)

    context = reader.read_topsrcdir()
    data = emitter.emit(context, emitfn=emitter._process_test_manifests)
    backend.consume(data)


if __name__ == '__main__':
    sys.exit(gen_test_backend())
