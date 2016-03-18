TREEHERDER_ROUTE_PREFIX = 'tc-treeherder-stage'
TREEHERDER_ROUTES = {
    'staging': 'tc-treeherder-stage',
    'production': 'tc-treeherder'
}

def decorate_task_treeherder_routes(task, suffix):
    """
    Decorate the given task with treeherder routes.

    Uses task.extra.treeherderEnv if available otherwise defaults to only
    staging.

    :param dict task: task definition.
    :param str suffix: The project/revision_hash portion of the route.
    """

    if 'extra' not in task:
        return

    if 'routes' not in task:
        task['routes'] = []

    treeheder_env = task['extra'].get('treeherderEnv', ['staging'])

    for env in treeheder_env:
        task['routes'].append('{}.{}'.format(TREEHERDER_ROUTES[env], suffix))

def decorate_task_json_routes(task, json_routes, parameters):
    """
    Decorate the given task with routes.json routes.

    :param dict task: task definition.
    :param json_routes: the list of routes to use from routes.json
    :param parameters: dictionary of parameters to use in route templates
    """
    routes = task.get('routes', [])
    for route in json_routes:
        routes.append(route.format(**parameters))

    task['routes'] = routes
