from taskgraph.util.taskcluster import get_session


def get_files_changed_pr(base_repository, pull_request_number):
    url = base_repository.replace("github.com", "api.github.com/repos")
    url += "/pulls/%s/files" % pull_request_number

    r = get_session().get(url, timeout=60)
    r.raise_for_status()
    files = [f["filename"] for f in r.json()]
    return files


def get_files_changed_push(base_repository, base_rev, head_rev):
    url = base_repository.replace("github.com", "api.github.com/repos")
    url += "/compare/"
    url += f"{base_rev}...{head_rev}"

    r = get_session().get(url, timeout=60)
    r.raise_for_status()
    files = [f["filename"] for f in r.json().get("files")]
    return files
