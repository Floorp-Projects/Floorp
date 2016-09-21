# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
This file contains a whitelist of gecko.v2 index route job names.  The intent
of this whitelist is to raise an alarm when new jobs are added.  If those jobs
already run in Buildbot, then it's important that the generated index routes
match (and that only one of Buildbot and TaskCluster be tier-1 at any time).
If the jobs are new and never ran in Buildbot, then their job name can be added
here without any further fuss.

Once all jobs have been ported from Buildbot, this file can be removed.
"""

from __future__ import absolute_import, print_function, unicode_literals

# please keep me in lexical order
JOB_NAME_WHITELIST = set([
    'android-api-15-debug',
    'android-api-15-gradle-dependencies-opt',
    'android-api-15-gradle-opt',
    'android-api-15-opt',
    'android-api-15-nightly-opt',
    'android-api-15-partner-sample1-opt',
    'android-l10n-opt',
    'android-x86-opt',
    'aries-debug',
    'aries-eng-opt',
    'browser-haz-debug',
    'linux32-l10n-opt',
    'linux64-artifact-opt',
    'linux64-asan-debug',
    'linux64-asan-opt',
    'linux64-debug',
    'linux64-l10n-opt',
    'linux64-opt',
    'linux64-pgo-opt',
    'linux64-st-an-opt',
    'linux64-valgrind-opt',
    'linux-debug',
    'linux-opt',
    'macosx64-debug',
    'macosx64-opt',
    'macosx64-st-an-opt',
    'mulet-dbg',
    'mulet-haz-debug',
    'mulet-opt',
    'nexus-5-l-eng-debug',
    'nexus-5-l-eng-opt',
    'shell-haz-debug',
    'sm-arm64-sim-debug',
    'sm-arm-sim-debug',
    'sm-asan-opt',
    'sm-compacting-debug',
    'sm-msan-opt',
    'sm-nonunified-debug',
    'sm-package-opt',
    'sm-plaindebug-debug',
    'sm-plain-opt',
    'sm-rootanalysis-debug',
    'sm-tsan-opt',
    'win32-debug',
    'win32-opt',
    'win64-debug',
    'win64-opt',
])

JOB_NAME_WHITELIST_ERROR = """\
The gecko-v2 job name {} is not in the whitelist in __file__.
If this job runs on Buildbot, please ensure that the job names match between
Buildbot and TaskCluster, then add the job name to the whitelist.  If this is a
new job, there is nothing to check -- just add the job to the whitelist.
"""
