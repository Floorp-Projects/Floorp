# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import logging
import os
from pathlib import Path
import subprocess
import time

import pytest
import requests

from experimentintegration.gradlewbuild import GradlewBuild
from experimentintegration.models.models import TelemetryModel

KLAATU_SERVER_URL = "http://localhost:1378"
KLAATU_LOCAL_SERVER_URL = "http://localhost:1378"

here = Path()


def pytest_addoption(parser):
    parser.addoption(
        "--experiment", action="store", help="The experiments experimenter URL"
    )
    parser.addoption(
        "--stage", action="store_true", default=None, help="Use the stage server"
    )


@pytest.fixture(name="load_branches")
def fixture_load_branches(experiment_url):
    branches = []

    if experiment_url:
        data = experiment_url
    else:
        try:
            data = requests.get(f"{KLAATU_SERVER_URL}/experiment").json()
        except ConnectionRefusedError:
            logging.warn("No URL or experiment slug provided, exiting.")
            exit()
        else:
            for item in reversed(data):
                data = item
                break
    experiment = requests.get(data).json()
    for item in experiment["branches"]:
        branches.append(item["slug"])
    return branches


@pytest.fixture
def gradlewbuild_log(pytestconfig, tmpdir):
    gradlewbuild_log = f"{tmpdir.join('gradlewbuild.log')}"
    pytestconfig._gradlewbuild_log = gradlewbuild_log
    yield gradlewbuild_log


@pytest.fixture
def gradlewbuild(gradlewbuild_log):
    yield GradlewBuild(gradlewbuild_log)


@pytest.fixture(name="experiment_data")
def fixture_experiment_data(experiment_url):
    data = requests.get(experiment_url).json()
    branches = next(iter(data.get("branches")), None)
    features = next(iter(branches.get("features")), None)
    if features.get("messages"):
        for item in features["value"]["messages"].values():
            item["surface"] = "homescreen"
            item["style"] = "URGENT"
            for count, trigger in enumerate(item["trigger"]):
                if "USER_EN_SPEAKER" not in trigger:
                    del item["trigger"][count]
    return [data]


@pytest.fixture(name="experiment_url", scope="module")
def fixture_experiment_url(request, variables):
    url = None

    if slug := request.config.getoption("--experiment"):
        # Build URL from slug
        if request.config.getoption("--stage"):
            url = f"{variables['urls']['stage_server']}/api/v6/experiments/{slug}"
        else:
            url = f"{variables['urls']['prod_server']}/api/v6/experiments/{slug}"
    else:
        try:
            data = requests.get(f"{KLAATU_SERVER_URL}/experiment").json()
        except requests.exceptions.ConnectionError:
            logging.error("No URL or experiment slug provided, exiting.")
            exit()
        else:
            for item in data:
                if isinstance(item, dict):
                    continue
                else:
                    url = item
    yield url
    return_data = {"url": url}
    try:
        requests.put(f"{KLAATU_SERVER_URL}/experiment", json=return_data)
    except requests.exceptions.ConnectionError:
        pass


@pytest.fixture(name="json_data")
def fixture_json_data(tmp_path, experiment_data):
    path = tmp_path / "data"
    path.mkdir()
    json_path = path / "data.json"
    with open(json_path, "w", encoding="utf-8") as f:
        # URL of experiment/klaatu server
        data = {"data": experiment_data}
        json.dump(data, f)
    return json_path


@pytest.fixture(name="experiment_slug")
def fixture_experiment_slug(experiment_data):
    return experiment_data[0]["slug"]


@pytest.fixture(name="start_app")
def fixture_start_app():
    def _():
        command = f"nimbus-cli --app fenix --channel developer open"
        try:
            out = subprocess.check_output(
                command,
                cwd=os.path.join(here, os.pardir),
                stderr=subprocess.STDOUT,
                universal_newlines=True,
                shell=True,
            )
        except subprocess.CalledProcessError as e:
            out = e.output
            raise
        finally:
            with open(gradlewbuild_log, "w") as f:
                f.write(out)
        time.sleep(
            15
        )  # Wait a while as there's no real way to know when the app has started

    return _


@pytest.fixture(name="send_test_results", autouse=True)
def fixture_send_test_results():
    yield
    here = Path()

    with open(f"{here.resolve()}/results/index.html", "rb") as f:
        files = {"file": f}
        try:
            requests.post(f"{KLAATU_SERVER_URL}/test_results", files=files)
        except requests.exceptions.ConnectionError:
            pass


@pytest.fixture(name="check_ping_for_experiment")
def fixture_check_ping_for_experiment(experiment_slug, variables):
    def _check_ping_for_experiment(
        branch=None, experiment=experiment_slug, reason=None
    ):
        model = TelemetryModel(branch=branch, experiment=experiment)

        timeout = time.time() + 60 * 5
        while time.time() < timeout:
            data = requests.get(f"{variables['urls']['telemetry_server']}/pings").json()
            events = []
            for item in data:
                event_items = item.get("events")
                if event_items:
                    for event in event_items:
                        if (
                            "category" in event
                            and "nimbus_events" in event["category"]
                            and "extra" in event
                            and "branch" in event["extra"]
                        ):
                            events.append(event)
            for event in events:
                event_name = event.get("name")
                if (reason == "enrollment" and event_name == "enrollment") or (
                    reason == "unenrollment"
                    and event_name in ["unenrollment", "disqualification"]
                ):
                    telemetry_model = TelemetryModel(
                        branch=event["extra"]["branch"],
                        experiment=event["extra"]["experiment"],
                    )
                    if model == telemetry_model:
                        return True
            time.sleep(5)
        return False

    return _check_ping_for_experiment


@pytest.fixture(name="setup_experiment")
def fixture_setup_experiment(experiment_slug, json_data, gradlewbuild_log, variables):
    def _(branch):
        requests.delete(f"{variables['urls']['telemetry_server']}/pings")
        logging.info(f"Testing experiment {experiment_slug}, BRANCH: {branch[0]}")
        command = f"nimbus-cli --app fenix --channel developer enroll {experiment_slug} --branch {branch[0]} --file {json_data} --reset-app"
        logging.info(f"Running command {command}")
        try:
            out = subprocess.check_output(command, shell=True, stderr=subprocess.STDOUT)
        except subprocess.CalledProcessError as e:
            out = e.output
            raise
        finally:
            with open(gradlewbuild_log, "w") as f:
                f.write(f"{out}")
        time.sleep(
            15
        )  # Wait a while as there's no real way to know when the app has started

    return _
