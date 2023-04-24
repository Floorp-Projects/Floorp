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

    if "with_interventions" in marks:
        argvalues.append([dict({"interventions": True}, **otherargs)])
        ids.append("with_interventions")

    if "without_interventions" in marks:
        argvalues.append([dict({"interventions": False}, **otherargs)])
        ids.append("without_interventions")

    metafunc.parametrize(["session"], argvalues, ids=ids, indirect=True)


@pytest.fixture(scope="function")  # noqa: F405
async def test_config(request, driver):
    params = request.node.callspec.params.get("session")

    use_interventions = params.get("interventions")
    print(f"use_interventions {use_interventions}")
    if use_interventions is None:
        raise ValueError(
            "Missing intervention marker in %s:%s"
            % (request.fspath, request.function.__name__)
        )

    return {
        "use_interventions": use_interventions,
    }
