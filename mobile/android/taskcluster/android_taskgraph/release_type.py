def does_task_match_release_type(task, release_type):
    return (
        # TODO: only use a single attribute to compare to `release_type`
        task.attributes.get("build-type") == release_type
        or task.attributes.get("release-type") == release_type
        # TODO: remove the following hack. Android-Components beta builds have
        # historically been labeled "release" because we very introduced
        # beta numbers in the middle of the Android monorepo migration (bug 1800611)
        or (
            task.attributes.get("build-type") == "release"
            and release_type == "beta"
        )
    )
