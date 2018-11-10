# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import

import json
import os
import subprocess
import sys

import mozunit
import pytest

from mozboot.bootstrap import update_or_create_build_telemetry_config


@pytest.fixture
def run_mach(tmpdir):
    """Return a function that runs mach with the provided arguments and then returns
    a list of the data contained within any telemetry entries generated during the command.
    """
    # Use tmpdir as the mozbuild state path, and enable telemetry in
    # a machrc there.
    update_or_create_build_telemetry_config(unicode(tmpdir.join('machrc')))
    env = dict(os.environ)
    env['MOZBUILD_STATE_PATH'] = str(tmpdir)
    mach = os.path.normpath(os.path.join(os.path.dirname(__file__),
                                         '../../../../mach'))

    def run(*args, **kwargs):
        # Run mach with the provided arguments
        subprocess.check_output([sys.executable, mach] + list(args),
                                stderr=subprocess.STDOUT,
                                env=env,
                                **kwargs)
        # Load any telemetry data that was written
        path = unicode(tmpdir.join('telemetry', 'outgoing'))
        try:
            return [json.load(open(os.path.join(path, f), 'rb')) for f in os.listdir(path)]
        except OSError:
            return []
    return run


def test_simple(run_mach, tmpdir):
    data = run_mach('python', '-c', 'pass')
    assert len(data) == 1
    d = data[0]
    assert d['command'] == 'python'
    assert d['argv'] == ['-c', 'pass']
    client_id_data = json.load(tmpdir.join('telemetry_client_id.json').open('rb'))
    assert 'client_id' in client_id_data
    assert client_id_data['client_id'] == d['client_id']


if __name__ == '__main__':
    mozunit.main()
