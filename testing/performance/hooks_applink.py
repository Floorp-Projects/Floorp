# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import json
import tempfile
import pathlib

from mozperftest.test.browsertime import add_options
from mozperftest.system.android import _ROOT_URL
from mozperftest.utils import download_file, get_multi_tasks_url

URL = "'https://www.example.com'"

COMMON_OPTIONS = [("processStartTime", "true"),
                  ("firefox.disableBrowsertimeExtension", "true"),
                  ("firefox.android.intentArgument", "'-a'"),
                  ("firefox.android.intentArgument", "'android.intent.action.VIEW'"),
                  ("firefox.android.intentArgument", "'-d'"),
                  ("firefox.android.intentArgument", URL)]

NIGHTLY_SIM_ROUTE = "project.mobile.fenix.v2.nightly-simulation"
G5_SUFFIX = "artifacts/public/build/armeabi-v7a/geckoNightly/target.apk"
P2_SUFFIX = "artifacts/public/build/arm64-v8a/geckoNightly/target.apk"

build_generator = None


def before_iterations(kw):
    global build_generator

    if "fenix_nightlysim_multicommit" not in kw.get("android_install_apk")[0]:
        return

    # Get the builds to test
    build_url = get_multi_tasks_url(NIGHTLY_SIM_ROUTE)
    tmpfile = pathlib.Path(tempfile.mkdtemp(), "alltasks.json")
    download_file(build_url, tmpfile)

    # Set the number of test-iterations to the number of builds
    with tmpfile.open() as f:
        tasks = json.load(f)["tasks"]
    kw["test_iterations"] = len(tasks)

    # Finally, make an iterator for the builds (used in `before_runs`)
    route_suffix = G5_SUFFIX
    if "arm64_v8a" in kw.get("android_install_apk"):
        route_suffix = P2_SUFFIX

    def _build_iterator(route_suffix):
        for task in tasks:
            route = task["namespace"]
            revision = route.split(".")[-1]
            print("Testing revision %s" % revision)
            download_url = "%s%s/%s" % (_ROOT_URL, route, route_suffix)
            yield revision, [download_url]

    build_generator = _build_iterator(route_suffix)

    return kw


def before_runs(env, **kw):
    global build_generator

    add_options(env, COMMON_OPTIONS)
    if build_generator:
        revision, build = next(build_generator)
        env.set_arg("android-install-apk", build)
        env.set_arg("perfherder-prefix", revision)
