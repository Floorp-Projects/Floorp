import pytest
from mozunit import main

from gecko_taskgraph.transforms.build_schedules import set_build_schedules_optimization


@pytest.mark.parametrize(
    "kind,task,expected",
    (
        pytest.param("build", {"when": "foo"}, None, id="no-op"),
        pytest.param(
            "build",
            {"attributes": {"build_platform": "linux64/opt"}},
            {"build": ["linux", "firefox"]},
            id="build",
        ),
        pytest.param(
            "build-components",
            {},
            {"build": ["android", "fenix", "focus-android"]},
            id="build-components",
        ),
        pytest.param(
            "build-bundle",
            {"name": "build-bundle-fenix"},
            {"build": ["android", "fenix"]},
            id="build-bundle-fenix",
        ),
        pytest.param(
            "build-apk",
            {"name": "fenix"},
            {"build": ["android", "fenix"]},
            id="build-apk-fenix",
        ),
        pytest.param(
            "build-apk",
            {"name": "build-apk-focus"},
            {"build": ["android", "focus-android"]},
            id="build-apk-focus",
        ),
        pytest.param(
            "build-apk",
            {"name": "build-apk-klar"},
            {"build": ["android", "focus-android"]},
            id="build-apk-klar",
        ),
    ),
)
def test_build_schedules(run_transform, kind, task, expected):
    tasks = list(run_transform(set_build_schedules_optimization, [task], kind=kind))
    assert len(tasks) == 1
    assert tasks[0].get("optimization") == expected


if __name__ == "__main__":
    main()
