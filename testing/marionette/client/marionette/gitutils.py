# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from git import *
from optparse import OptionParser
import os
import sys

def updategaia(repopath):
    b2g = Repo(repopath)
    gaia = Repo(os.path.join(repopath, 'gaia'))

    gaia_submodule = None
    for submodule in b2g.submodules:
        if 'gaia' in submodule.name:
            gaia_submodule = submodule
    assert(gaia_submodule)
    gaia_submodule_commit = gaia_submodule.hexsha
    print 'gaia_submodule_commit', gaia_submodule_commit

    gaia.heads.master.checkout()
    print 'pulling from gaia origin/master'
    gaia.remotes.origin.pull('master')
    gaia_new_head = gaia.heads.master.commit.hexsha
    print 'gaia_new_head', gaia_new_head

    if gaia_submodule_commit == gaia_new_head:
        print 'no change, exiting with code 10'
        sys.exit(10)

def commitgaia(repopath):
    b2g = Repo(repopath)
    gaia = Repo(os.path.join(repopath, 'gaia'))

    gaia_submodule = None
    for submodule in b2g.submodules:
        if 'gaia' in submodule.name:
            gaia_submodule = submodule
    assert(gaia_submodule)

    gaia_submodule.binsha = gaia_submodule.module().head.commit.binsha
    b2g.index.add([gaia_submodule])
    commit = b2g.index.commit('Update gaia')
    print 'pushing to B2G origin/master'
    b2g.remotes.origin.push(b2g.head.reference)
    print 'done!'


if __name__ == '__main__':
    parser = OptionParser(usage='%prog [options]')
    parser.add_option("--repo",
                      action = "store", dest = "repo",
                      help = "path to B2G repo")
    parser.add_option("--updategaia",
                      action = "store_true", dest = "updategaia",
                      help = "update the Gaia submodule to HEAD")
    parser.add_option("--commitgaia",
                      action = "store_true", dest = "commitgaia",
                      help = "commit current Gaia submodule HEAD")
    options, tests = parser.parse_args()

    if not options.repo:
        raise 'must specify --repo /path/to/B2G'

    if options.updategaia:
        updategaia(options.repo)
    elif options.commitgaia:
        commitgaia(options.repo)
    else:
        raise 'No command specified'

