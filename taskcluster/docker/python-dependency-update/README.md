
==Python Dependency Updates==

This docker image contains the necessary dependencies and scripts to update
in-tree requirement.txt produced by `pip-compile --generate-hashes`, produce a
diff, and submit it to Phabricator.


==Quick Start==

```sh
docker build -t python-dependency-update --no-cache --rm .

docker run -e PYTHON3="1" -e BRANCH="mozilla-central" -e REQUIREMENTS_FILE="taskcluster/docker/funsize-update-generator/requirements.in" python-dependency-update
```
