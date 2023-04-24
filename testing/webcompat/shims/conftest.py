# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from ..fixtures import *  # noqa: F403


def pytest_generate_tests(metafunc):
    """Generate tests based on markers."""

    if "session" not in metafunc.fixturenames:
        return

    marks = [mark.name for mark in metafunc.function.pytestmark]

    otherargs = {}
    argvalues = []
    ids = []

    if "only_platforms" in marks:
        for mark in metafunc.function.pytestmark:
            if mark.name == "only_platforms":
                otherargs["only_platforms"] = mark.args

    if "skip_platforms" in marks:
        for mark in metafunc.function.pytestmark:
            if mark.name == "skip_platforms":
                otherargs["skip_platforms"] = mark.args

    if "with_private_browsing" in marks:
        otherargs["with_private_browsing"] = True
    if "with_strict_etp" in marks:
        otherargs["with_strict_etp"] = True
    if "without_storage_partitioning" in marks:
        otherargs["without_storage_partitioning"] = True
    if "without_tcp " in marks:
        otherargs["without_tcp "] = True

    if "with_shims" in marks:
        argvalues.append([dict({"shims": True}, **otherargs)])
        ids.append("with_shims")

    if "without_shims" in marks:
        argvalues.append([dict({"shims": False}, **otherargs)])
        ids.append("without_shims")

    metafunc.parametrize(["session"], argvalues, ids=ids, indirect=True)


@pytest.fixture(scope="function")  # noqa: F405
async def test_config(request, driver):
    params = request.node.callspec.params.get("session")

    use_shims = params.get("shims")
    if use_shims is None:
        raise ValueError(
            "Missing shims marker in %s:%s"
            % (request.fspath, request.function.__name__)
        )

    return {
        "aps": not params.get("without_storage_partitioning", False),
        "use_pbm": params.get("with_private_browsing", False),
        "use_shims": use_shims,
        "use_strict_etp": params.get("with_strict_etp", False),
        "without_tcp": params.get("without_tcp", False),
    }
