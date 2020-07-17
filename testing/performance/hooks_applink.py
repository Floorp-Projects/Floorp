# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import json
import tempfile
import pathlib

from mozperftest.test.browsertime import add_options
from mozperftest.system.android import _ROOT_URL
from mozperftest.utils import download_file, get_multi_tasks_url, get_revision_namespace_url

URL = "'https://www.example.com'"

COMMON_OPTIONS = [("processStartTime", "true"),
                  ("firefox.disableBrowsertimeExtension", "true"),
                  ("firefox.android.intentArgument", "'-a'"),
                  ("firefox.android.intentArgument", "'android.intent.action.VIEW'"),
                  ("firefox.android.intentArgument", "'-d'"),
                  ("firefox.android.intentArgument", URL)]

NIGHTLY_SIM_ROUTE = "mobile.v2.fenix.nightly-simulation"
ROUTE_SUFFIX = "artifacts/public/build/{architecture}/geckoNightly/target.apk"

build_generator = None


def before_iterations(kw):
    global build_generator

    if "fenix_nightlysim_multicommit" not in kw.get("android_install_apk")[0]:
        return

    # Get the builds to test
    architecture = "arm64-v8a" if "arm64_v8a" in kw.get("android_install_apk") else "armeabi-v7a"
    json_ = _fetch_json(get_revision_namespace_url, NIGHTLY_SIM_ROUTE, day=kw["test_date"])
    namespaces = json_["namespaces"]
    revisions = [namespace["name"] for namespace in namespaces]

    tasks = []
    for revision in revisions:
        json_ = _fetch_json(get_multi_tasks_url, NIGHTLY_SIM_ROUTE, revision, day=kw["test_date"])
        for task in json_["tasks"]:
            route = task["namespace"]
            task_architecture = route.split(".")[-1]
            if task_architecture == architecture:
                tasks.append({
                    "revision": revision,
                    "route": route,
                    "route_suffix": ROUTE_SUFFIX.format(architecture=task_architecture),
                })

    kw["test_iterations"] = len(tasks)

    def _build_iterator():
        for task in tasks:
            revision = task["revision"]
            print(f"Testing revision {revision}")
            download_url = f'{_ROOT_URL}{task["route"]}/{task["route_suffix"]}'
            yield revision, [download_url]

    build_generator = _build_iterator()

    return kw


def _fetch_json(get_url_function, *args, **kwargs):
    build_url = get_url_function(*args, **kwargs)
    tmpfile = pathlib.Path(tempfile.mkdtemp(), "temp.json")
    download_file(build_url, tmpfile)

    # Set the number of test-iterations to the number of builds
    with tmpfile.open() as f:
        return json.load(f)


def before_runs(env, **kw):
    global build_generator

    add_options(env, COMMON_OPTIONS)
    if build_generator:
        revision, build = next(build_generator)
        env.set_arg("android-install-apk", build)
        env.set_arg("perfherder-prefix", revision)
