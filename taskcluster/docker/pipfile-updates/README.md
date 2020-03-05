
==Pipfile Updates==

This docker image contains the necessary dependencies and scripts to update in-tree Pipfile.lock's,
produce a diff, and submit it to Phabricator.


==Quick Start==

```sh
docker build -t pipfile-update --no-cache --rm .

docker run -e PYTHON3="1" -e BRANCH="mozilla-central" -e PIPFILE_DIRECTORY="taskcluster/docker/funsize-update-generator" pipfile-update
```
