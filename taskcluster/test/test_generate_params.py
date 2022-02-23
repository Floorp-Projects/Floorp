import json
import os
import subprocess

import pytest
from mozunit import main

from gecko_taskgraph import GECKO
from gecko_taskgraph.taskgraph import TaskGraph

pytestmark = pytest.mark.slow
PARAMS_DIR = os.path.join(GECKO, "taskcluster", "test", "params")


@pytest.fixture(scope="module")
def get_graph_from_spec(tmpdir_factory):
    outdir = tmpdir_factory.mktemp("graphs")

    # Use a mach subprocess to leverage the auto parallelization of
    # parameters when specifying a directory.
    cmd = [
        "./mach",
        "taskgraph",
        "morphed",
        "--json",
        f"--parameters={PARAMS_DIR}",
        f"--output-file={outdir}/graph.json",
    ]
    subprocess.run(cmd, cwd=GECKO)
    assert len(outdir.listdir()) > 0

    def inner(param_spec):
        outfile = f"{outdir}/graph_{param_spec}.json"
        with open(outfile) as fh:
            output = fh.read()
            try:
                return TaskGraph.from_json(json.loads(output))[1]
            except ValueError:
                return output

    return inner


@pytest.mark.parametrize(
    "param_spec", [os.path.splitext(p)[0] for p in os.listdir(PARAMS_DIR)]
)
def test_generate_graphs(get_graph_from_spec, param_spec):
    ret = get_graph_from_spec(param_spec)
    if isinstance(ret, str):
        print(ret)
        pytest.fail("An exception was raised during graph generation!")

    assert isinstance(ret, TaskGraph)
    assert len(ret.tasks) > 0


if __name__ == "__main__":
    main()
