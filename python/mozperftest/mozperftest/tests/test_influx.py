#!/usr/bin/env python
import json
import re
import sys

import mozunit
import pytest
import responses

from mozperftest.metrics.perfboard.influx import Influx
from mozperftest.tests.support import (
    BT_DATA,
    EXAMPLE_TEST,
    get_running_env,
    running_on_try,
)
from mozperftest.utils import ON_TRY, temp_dir


def mocks():
    # mocking the Influx service
    responses.add(
        responses.GET,
        "http://influxdb/ping",
        body=json.dumps({"version": "1"}),
        headers={"x-influxdb-version": "1"},
        status=204,
    )

    responses.add(
        responses.POST,
        "http://influxdb/write",
        body=json.dumps({"version": "1"}),
        headers={"x-influxdb-version": "1"},
        status=204,
    )

    responses.add(
        responses.GET,
        "http://grafana/api/search?tag=component",
        body=json.dumps([]),
        status=200,
    )

    responses.add(
        responses.POST,
        "http://grafana/api/dashboards/db",
        body=json.dumps({"uid": "id"}),
        status=200,
    )

    responses.add(
        responses.GET,
        "http://grafana/api/dashboards/uid/id",
        body=json.dumps({"dashboard": {"panels": []}}),
        status=200,
    )

    responses.add(
        responses.GET,
        re.compile(
            "https://firefox-ci-tc.services.mozilla.com/secrets/*|"
            "http://taskcluster/secrets/*"
        ),
        body=json.dumps(
            {
                "secret": {
                    "influx_host": "influxdb",
                    "influx_port": 0,
                    "influx_user": "admin",
                    "influx_password": "pass",
                    "influx_db": "db",
                    "grafana_key": "xxx",
                    "grafana_host": "grafana",
                    "grafana_port": 0,
                }
            }
        ),
        status=200,
    )


@responses.activate
@pytest.mark.parametrize("on_try", [True, False])
def test_influx_service(on_try):
    if ON_TRY and sys.platform == "darwin":
        # macos slave in the CI are restricted
        return

    mocks()
    with running_on_try(on_try), temp_dir() as output:
        args = {
            "verbose": True,
            "output": output,
            "perfboard-influx-password": "xxx",
            "perfboard-grafana-key": "xxx",
            "perfboard-grafana-host": "grafana",
            "perfboard-influx-port": 0,
            "perfboard-influx-host": "influxdb",
            "tests": [EXAMPLE_TEST],
        }

        mach_cmd, metadata, env = get_running_env(**args)
        metadata.add_result({"results": str(BT_DATA), "name": "browsertime"})
        layer = Influx(env, mach_cmd)
        layer.setup()
        try:
            metadata = layer.run(metadata)
        finally:
            layer.teardown()

    index = on_try and 2 or 1
    sent_data = responses.calls[index].request.body.split(b"\n")
    fields = [line.split(b",")[0].strip() for line in sent_data]
    assert b"rumspeedindex" in fields

    responses.reset()


if __name__ == "__main__":
    mozunit.main()
