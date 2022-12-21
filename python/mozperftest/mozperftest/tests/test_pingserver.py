#!/usr/bin/env python
import json
import sys
from pathlib import Path

import mozunit
import requests

from mozperftest.system.pingserver import PingServer
from mozperftest.tests.support import get_running_env
from mozperftest.utils import ON_TRY, temp_dir


def test_ping_server():
    if ON_TRY and sys.platform == "darwin":
        # macos slave in the CI are restricted
        return
    ping_data = {"some": "data"}
    with temp_dir() as output:
        args = {"verbose": True, "output": output}
        mach_cmd, metadata, env = get_running_env(**args)
        layer = PingServer(env, mach_cmd)
        layer.setup()
        try:
            metadata = layer.run(metadata)
            # simulates a ping
            requests.post(
                layer.endpoint + "/submit/something", data=json.dumps(ping_data)
            )
        finally:
            layer.teardown()

        with Path(output, "telemetry.json").open() as f:
            assert json.loads(f.read()) == [ping_data]


if __name__ == "__main__":
    mozunit.main()
