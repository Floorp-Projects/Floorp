def does_task_match_release_type(task, release_type):
    return (
        # TODO: only use a single attribute to compare to `release_type`
        task.attributes.get("build-type") == release_type
        or task.attributes.get("release-type") == release_type
    )
