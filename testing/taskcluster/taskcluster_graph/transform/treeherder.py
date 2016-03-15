def add_treeherder_revision_info(task, revision, revision_hash):
    # Only add treeherder information if task.extra.treeherder is present
    if 'extra' not in task and 'treeherder' not in task.extra:
        return

    task['extra']['treeherder']['revision'] = revision
    task['extra']['treeherder']['revision_hash'] = revision_hash
