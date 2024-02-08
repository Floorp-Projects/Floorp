# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
from pathlib import Path

import pytest
from mozunit import main
from skipfails import (
    MOCK_BUG_DEFAULTS,
    MOCK_TASK_DEFAULTS,
    MOCK_TASK_INITS,
    Mock,
    Skipfails,
)

DATA_PATH = Path(__file__).with_name("data")


def test_get_revision():
    """Test get_revision"""

    sf = Skipfails()
    with pytest.raises(ValueError) as e_info:
        sf.get_revision("")
    assert str(e_info.value) == "try_url scheme not https"

    with pytest.raises(ValueError) as e_info:
        sf.get_revision("https://foo.bar")
    assert str(e_info.value) == "try_url server not treeherder.mozilla.org"

    with pytest.raises(ValueError) as e_info:
        sf.get_revision("https://treeherder.mozilla.org")
    assert str(e_info.value) == "try_url query missing"

    with pytest.raises(ValueError) as e_info:
        sf.get_revision("https://treeherder.mozilla.org?a=1")
    assert str(e_info.value) == "try_url query missing revision"

    revision, repo = sf.get_revision(
        "https://treeherder.mozilla.org/jobs?repo=try&revision=5b1738d0af571777199ff3c694b1590ff574946b"
    )
    assert revision == "5b1738d0af571777199ff3c694b1590ff574946b"
    assert repo == "try"


def test_get_tasks():
    """Test get_tasks import of mozci"""

    from mozci.push import Push

    revision = "5b1738d0af571777199ff3c694b1590ff574946b"
    repo = "try"
    push = Push(revision, repo)
    assert push is not None


def get_failures(tasks_name, exp_f_name, task_details=None):
    """Runs Skipfails.get_failures on tasks to compare with failures"""
    sf = Skipfails()
    if task_details is not None:  # preload task details cache, if needed
        for task_id in task_details:
            sf.tasks[task_id] = task_details[task_id]
    tasks_fp = DATA_PATH.joinpath(tasks_name).open("r", encoding="utf-8")
    tasks = json.load(tasks_fp)
    tasks = [Mock(task, MOCK_TASK_DEFAULTS, MOCK_TASK_INITS) for task in tasks]
    exp_f_fp = DATA_PATH.joinpath(exp_f_name).open("r", encoding="utf-8")
    expected_failures = exp_f_fp.read().strip()
    failures = sf.get_failures(tasks)
    actual_failures = json.dumps(failures, indent=2, sort_keys=True).strip()
    assert actual_failures == expected_failures


def test_get_failures_1():
    """Test get_failures 1"""
    task_details = {"dwOJ8M9ERSmk6oI2KXg6hg": {}}
    get_failures("wayland-tasks-1.json", "wayland-failures-1.json", task_details)


def test_get_failures_2():
    """Test get_failures 2"""
    task_details = {
        "Y7r1q2xWSu-2bRAofEfeBw": {},
        "Z7r1q2xWSu-2bRAofEfeBw": {},
        "X7r1q2xWSu-2bRAofEfeBw": {},
    }
    get_failures("wayland-tasks-2.json", "wayland-failures-2.json", task_details)


def test_get_failures_3():
    """Test get_failures 3"""
    task_details = {
        "b7_ahjGtQ_-ZMNBG_hUZUw": {},
        "WVczuxkuSRKZg_jMiGyQsA": {},
        "UOZUIVAaTZKmRwArq5WkDw": {},
    }
    get_failures("wayland-tasks-3.json", "wayland-failures-3.json", task_details)


def test_get_failures_4():
    """Test get_failures 4"""
    task_details = {
        "bxMVPbPMTru_bfAivc1sPA": {},
        "EDql3NKPR3W6OEU3mLeKbg": {},
        "FDql3NKPR3W6OEU3mLeKbg": {},
    }
    get_failures("wayland-tasks-4.json", "wayland-failures-4.json", task_details)


def test_get_failures_5():
    """Test get_failures 5"""

    task_details = {
        "Bgc6We1sSjakIo3V9crldw": {
            "expires": "2024-01-09T16:05:56.825Z",
            "extra": {
                "suite": "mochitest-browser",
                "test-setting": {
                    "build": {"type": "opt"},
                    "platform": {
                        "arch": "64",
                        "os": {"name": "linux", "version": "22.04"},
                        "display": "wayland",
                    },
                    "runtime": {},
                },
            },
        }
    }
    get_failures("wayland-tasks-5.json", "wayland-failures-5.json", task_details)


def test_get_bug_by_id():
    """Test get_bug_by_id"""

    sf = Skipfails()
    id = 1682371
    # preload bug cache
    bugs_filename = f"bug-{id}.json"
    bugs_fp = DATA_PATH.joinpath(bugs_filename).open("r", encoding="utf-8")
    bugs_data = json.load(bugs_fp)
    preload_bugs = [Mock(b, MOCK_BUG_DEFAULTS) for b in bugs_data]
    sf.bugs = preload_bugs
    # function under test
    bug = sf.get_bug_by_id(id)
    assert bug.id == id
    assert bug.product == "Testing"
    assert bug.component == "General"
    assert (
        bug.summary
        == "create tool to quickly parse and identify all failures from a try push and ideally annotate manifests"
    )


def test_get_bugs_by_summary():
    """Test get_bugs_by_summary"""

    sf = Skipfails()
    id = 1682371
    # preload bug cache
    bugs_filename = f"bug-{id}.json"
    bugs_fp = DATA_PATH.joinpath(bugs_filename).open("r", encoding="utf-8")
    bugs_data = json.load(bugs_fp)
    preload_bugs = [Mock(b, MOCK_BUG_DEFAULTS) for b in bugs_data]
    sf.bugs = preload_bugs
    # function under test
    summary = "create tool to quickly parse and identify all failures from a try push and ideally annotate manifests"
    bugs = sf.get_bugs_by_summary(summary)
    assert len(bugs) == 1
    assert bugs[0].id == id
    assert bugs[0].product == "Testing"
    assert bugs[0].component == "General"
    assert bugs[0].summary == summary


def test_get_variants():
    """Test get_variants"""

    sf = Skipfails()
    variants = sf.get_variants()
    assert "1proc" in variants
    assert variants["1proc"] == "e10s"
    assert "webrender-sw" in variants
    assert variants["webrender-sw"] == "swgl"
    assert "aab" in variants
    assert variants["aab"] == "aab"


def test_task_to_skip_if():
    """Test task_to_skip_if"""

    # preload task cache
    task_id = "UP-t3xrGSDWvUNjFGIt_aQ"
    task_details = {
        "expires": "2024-01-09T16:05:56.825Z",
        "extra": {
            "suite": "mochitest-plain",
            "test-setting": {
                "build": {"type": "debug"},
                "platform": {
                    "arch": "32",
                    "os": {"build": "2009", "name": "windows", "version": "11"},
                },
                "runtime": {},
            },
        },
    }
    sf = Skipfails()
    sf.tasks[task_id] = task_details
    # function under test
    skip_if = sf.task_to_skip_if(task_id)
    assert skip_if == "os == 'win' && os_version == '11' && bits == '32' && debug"


def test_get_filename_in_manifest():
    """Test get_filename_in_manifest"""

    sf = Skipfails()

    assert (
        sf.get_filename_in_manifest(
            "browser/components/sessionstore/test/browser.toml",
            "browser/components/sessionstore/test/browser_closed_tabs_windows.js",
        )
        == "browser_closed_tabs_windows.js"
    )
    assert (
        sf.get_filename_in_manifest(
            "browser/base/content/test/webrtc/gracePeriod/browser.toml",
            "browser/base/content/test/webrtc/browser_devices_get_user_media_grace.js",
        )
        == "../browser_devices_get_user_media_grace.js"
    )
    assert (
        sf.get_filename_in_manifest(
            "dom/animation/test/mochitest.toml",
            "dom/animation/test/document-timeline/test_document-timeline.html",
        )
        == "document-timeline/test_document-timeline.html"
    )


def test_label_to_platform_testname():
    """Test label_to_platform_testname"""

    sf = Skipfails()
    label = "test-linux2204-64-wayland/opt-mochitest-browser-chrome-swr-13"
    platform, testname = sf.label_to_platform_testname(label)
    assert platform == "test-linux2204-64-wayland/opt"
    assert testname == "mochitest-browser-chrome"


if __name__ == "__main__":
    main()
