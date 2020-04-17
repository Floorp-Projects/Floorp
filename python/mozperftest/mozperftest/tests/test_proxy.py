#!/usr/bin/env python
import mozunit

from mozperftest.tests.support import get_running_env
from mozperftest.environment import SYSTEM


def test_proxy():
    mach_cmd, metadata, env = get_running_env()
    system = env.layers[SYSTEM]

    # XXX this will run for real, we need to mock HTTP calls
    with system as proxy:
        proxy(metadata)


if __name__ == "__main__":
    mozunit.main()
