#!/usr/bin/env python
import mozunit

from mozperftest.tests.support import get_running_env
from mozperftest.environment import SYSTEM
from mozperftest.utils import silence


def test_proxy():
    mach_cmd, metadata, env = get_running_env(proxy=True)
    system = env.layers[SYSTEM]

    # XXX this will run for real, we need to mock HTTP calls
    with system as proxy, silence():
        proxy(metadata)


if __name__ == "__main__":
    mozunit.main()
