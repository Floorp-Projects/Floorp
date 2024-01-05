# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
from argparse import Namespace

import mozinfo
import pytest
import six
from manifestparser import TestManifest, expression
from moztest.selftest.fixtures import binary_fixture, setup_test_harness  # noqa

here = os.path.abspath(os.path.dirname(__file__))
setup_args = [os.path.join(here, "files"), "mochitest", "testing/mochitest"]


@pytest.fixture
def create_manifest(tmpdir, build_obj):
    def inner(string, name="manifest.ini"):
        manifest = tmpdir.join(name)
        manifest.write(string, ensure=True)
        # pylint --py3k: W1612
        path = six.text_type(manifest)
        return TestManifest(manifests=(path,), strict=False, rootdir=tmpdir.strpath)

    return inner


@pytest.fixture(scope="function")
def parser(request):
    parser = pytest.importorskip("mochitest_options")

    app = getattr(request.module, "APP", "generic")
    return parser.MochitestArgumentParser(app=app)


@pytest.fixture(scope="function")
def runtests(setup_test_harness, binary, parser, request):
    """Creates an easy to use entry point into the mochitest harness.

    :returns: A function with the signature `*tests, **opts`. Each test is a file name
              (relative to the `files` dir). At least one is required. The opts are
              used to override the default mochitest options, they are optional.
    """
    flavor = "plain"
    if "flavor" in request.fixturenames:
        flavor = request.getfixturevalue("flavor")

    runFailures = ""
    if "runFailures" in request.fixturenames:
        runFailures = request.getfixturevalue("runFailures")

    restartAfterFailure = False
    if "restartAfterFailure" in request.fixturenames:
        restartAfterFailure = request.getfixturevalue("restartAfterFailure")

    setup_test_harness(*setup_args, flavor=flavor)

    runtests = pytest.importorskip("runtests")

    mochitest_root = runtests.SCRIPT_DIR
    if flavor == "plain":
        test_root = os.path.join(mochitest_root, "tests", "selftests")
        manifest_name = "mochitest.ini"
    elif flavor == "browser-chrome":
        test_root = os.path.join(mochitest_root, "browser", "tests", "selftests")
        manifest_name = "browser.ini"
    else:
        raise Exception(f"Invalid flavor {flavor}!")

    # pylint --py3k: W1648
    buf = six.StringIO()
    options = vars(parser.parse_args([]))
    options.update(
        {
            "app": binary,
            "flavor": flavor,
            "runFailures": runFailures,
            "restartAfterFailure": restartAfterFailure,
            "keep_open": False,
            "log_raw": [buf],
        }
    )

    if runFailures == "selftest":
        options["crashAsPass"] = True
        options["timeoutAsPass"] = True
        runtests.mozinfo.update({"selftest": True})

    if not os.path.isdir(runtests.build_obj.bindir):
        package_root = os.path.dirname(mochitest_root)
        options.update(
            {
                "certPath": os.path.join(package_root, "certs"),
                "utilityPath": os.path.join(package_root, "bin"),
            }
        )
        options["extraProfileFiles"].append(
            os.path.join(package_root, "bin", "plugins")
        )

    options.update(getattr(request.module, "OPTIONS", {}))

    def normalize(test):
        if isinstance(test, str):
            test = [test]
        return [
            {
                "name": t,
                "relpath": t,
                "path": os.path.join(test_root, t),
                # add a dummy manifest file because mochitest expects it
                "manifest": os.path.join(test_root, manifest_name),
                "manifest_relpath": manifest_name,
                "skip-if": runFailures,
            }
            for t in test
        ]

    def inner(*tests, **opts):
        assert len(tests) > 0

        # Inject a TestManifest in the runtests option if one
        # has not been already included by the caller.
        if not isinstance(options["manifestFile"], TestManifest):
            manifest = TestManifest()
            options["manifestFile"] = manifest
            # pylint --py3k: W1636
            manifest.tests.extend(list(map(normalize, tests))[0])
            options.update(opts)

        result = runtests.run_test_harness(parser, Namespace(**options))
        out = json.loads("[" + ",".join(buf.getvalue().splitlines()) + "]")
        buf.close()
        return result, out

    return inner


@pytest.fixture
def build_obj(setup_test_harness):
    setup_test_harness(*setup_args)
    mochitest_options = pytest.importorskip("mochitest_options")
    return mochitest_options.build_obj


@pytest.fixture(autouse=True)
def skip_using_mozinfo(request, setup_test_harness):
    """Gives tests the ability to skip based on values from mozinfo.

    Example:
        @pytest.mark.skip_mozinfo("!e10s || os == 'linux'")
        def test_foo():
            pass
    """

    setup_test_harness(*setup_args)
    runtests = pytest.importorskip("runtests")
    runtests.update_mozinfo()

    skip_mozinfo = request.node.get_closest_marker("skip_mozinfo")
    if skip_mozinfo:
        value = skip_mozinfo.args[0]
        if expression.parse(value, **mozinfo.info):
            pytest.skip("skipped due to mozinfo match: \n{}".format(value))
