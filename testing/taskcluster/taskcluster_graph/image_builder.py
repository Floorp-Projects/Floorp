import hashlib
import json
import os
import subprocess
import tarfile
import urllib2

import taskcluster_graph.transform.routes as routes_transform
import taskcluster_graph.transform.treeherder as treeherder_transform
from slugid import nice as slugid
from taskcluster_graph.templates import Templates

TASKCLUSTER_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.realpath(__file__)), '..'))
IMAGE_BUILD_TASK = os.path.join(TASKCLUSTER_ROOT, 'tasks', 'image.yml')
GECKO = os.path.realpath(os.path.join(TASKCLUSTER_ROOT, '..', '..'))
DOCKER_ROOT = os.path.join(GECKO, 'testing', 'docker')
REGISTRY = open(os.path.join(DOCKER_ROOT, 'REGISTRY')).read().strip()
INDEX_URL = 'https://index.taskcluster.net/v1/task/docker.images.v1.{}.{}.hash.{}'
ARTIFACT_URL = 'https://queue.taskcluster.net/v1/task/{}/artifacts/{}'
DEFINE_TASK = 'queue:define-task:aws-provisioner-v1/{}'

def is_docker_registry_image(registry_path):
    return os.path.isfile(registry_path)

# make a task label; in old decision tasks, this is a regular slugid, but when called
# from the taskgraph generator's legacy kind, this is monkey-patched to return a label
# (`TaskLabel==..`)
def mklabel():
    return slugid()

def docker_image(name):
    ''' Determine the docker tag/revision from an in tree docker file '''
    repository_path = os.path.join(DOCKER_ROOT, name, 'REGISTRY')
    repository = REGISTRY

    version = open(os.path.join(DOCKER_ROOT, name, 'VERSION')).read().strip()

    if os.path.isfile(repository_path):
        repository = open(repository_path).read().strip()

    return '{}/{}:{}'.format(repository, name, version)

def task_id_for_image(seen_images, project, name, create=True):
    if name in seen_images:
        return seen_images[name]['taskId']

    context_path = os.path.join('testing', 'docker', name)
    context_hash = generate_context_hash(context_path)
    task_id = get_task_id_for_namespace(project, name, context_hash)

    if task_id:
        seen_images[name] = {'taskId': task_id}
        return task_id

    if not create:
        return None

    task_id = mklabel()
    seen_images[name] = {
        'taskId': task_id,
        'path': context_path,
        'hash': context_hash
    }

    return task_id

def image_artifact_exists_for_task_id(task_id, path):
    ''' Verifies that the artifact exists for the task ID '''
    try:
        request = urllib2.Request(ARTIFACT_URL.format(task_id, path))
        request.get_method = lambda : 'HEAD'
        urllib2.urlopen(request)
        return True
    except urllib2.HTTPError,e:
        return False

def get_task_id_for_namespace(project, name, context_hash):
    '''
    Determine the Task ID for an indexed image.

    As an optimization, if the context hash exists for mozilla-central, that image
    task ID will be used.  The reasoning behind this is that eventually everything ends
    up on mozilla-central at some point if most tasks use this as a common image
    for a given context hash, a worker within Taskcluster does not need to contain
    the same image per branch.
    '''
    for p in ['mozilla-central', project]:
        image_index_url = INDEX_URL.format(p, name, context_hash)
        try:
            task = json.load(urllib2.urlopen(image_index_url))
            # Ensure that the artifact exists for the task and hasn't expired
            artifact_exists = image_artifact_exists_for_task_id(task['taskId'],
                                                                'public/image.tar')
            # Only return the task ID if the artifact exists for the indexed
            # task.  Otherwise, continue on looking at each of the branches.  Method
            # continues trying other branches in case mozilla-central has an expired
            # artifact, but 'project' might not. Only return no task ID if all
            # branches have been tried
            if artifact_exists:
                return task['taskId']
        except urllib2.HTTPError:
            pass

    return None

def generate_context_hash(image_path):
    '''
    Generates a sha256 hash for context directory used to build an image.

    Contents of the directory are sorted alphabetically, contents of each file is hashed,
    and then a hash is created for both the file hashs as well as their paths.

    This ensures that hashs are consistent and also change based on if file locations
    within the context directory change.
    '''
    context_hash = hashlib.sha256()
    files = []

    for dirpath, dirnames, filenames in os.walk(os.path.join(GECKO, image_path)):
        for filename in filenames:
            files.append(os.path.join(dirpath, filename))

    for filename in sorted(files):
        relative_filename = filename.replace(GECKO, '')
        with open(filename, 'rb') as f:
            file_hash = hashlib.sha256()
            while True:
                data = f.read()
                if not data:
                    break
                file_hash.update(data)
            context_hash.update(file_hash.hexdigest() + '\t' + relative_filename + '\n')

    return context_hash.hexdigest()

def create_context_tar(context_dir, destination, image_name):
    ''' Creates a tar file of a particular context directory '''
    if not os.path.exists(os.path.dirname(destination)):
        os.makedirs(os.path.dirname(destination))

    with tarfile.open(destination, 'w:gz') as tar:
        tar.add(context_dir, arcname=image_name)

def image_requires_building(details):
    ''' Returns true if an image task should be created for a particular image '''
    if 'path' in details and 'hash' in details:
        return True
    else:
        return False

def create_image_task_parameters(params, name, details):
    image_parameters = dict(params)
    image_parameters['context_hash'] = details['hash']
    image_parameters['context_path'] = details['path']
    image_parameters['artifact_path'] = 'public/image.tar'
    image_parameters['image_slugid'] =  details['taskId']
    image_parameters['image_name'] = name

    return image_parameters

def get_image_details(seen_images, task_id):
    '''
    Based on a collection of image details, return the details
    for an image matching the requested task_id.

    Image details can include a path and hash indicating that the image requires
    building.
    '''
    for name, details in seen_images.items():
        if details['taskId'] == task_id:
            return [name, details]
    return None

def get_json_routes():
    ''' Returns routes that should be included in the image task. '''
    routes_file = os.path.join(TASKCLUSTER_ROOT, 'routes.json')
    with open(routes_file) as f:
        contents = json.load(f)
        json_routes = contents['docker_images']
    return json_routes

def normalize_image_details(graph, task, seen_images, params, decision_task_id):
    '''
    This takes a task-image payload and creates an image task to build that
    image.

    task-image payload is then converted to use a specific task ID of that
    built image.  All tasks within the graph requiring this same image will have their
    image details normalized and require the same image build task.
    '''
    image = task['task']['payload']['image']
    if isinstance(image, str) or image.get('type', 'docker-image') == 'docker-image':
        return

    if 'requires' not in task:
        task['requires'] = []

    name, details = get_image_details(seen_images, image['taskId'])

    if details.get('required', False) is True or image_requires_building(details) is False:
        if 'required' in details:
            task['requires'].append(details['taskId'])
        return

    image_parameters = create_image_task_parameters(params, name, details)

    if decision_task_id:
        image_artifact_path = "public/decision_task/image_contexts/{}/context.tar.gz".format(name)
        destination = "/home/worker/artifacts/decision_task/image_contexts/{}/context.tar.gz".format(name)
        image_parameters['context_url'] = ARTIFACT_URL.format(decision_task_id, image_artifact_path)

        create_context_tar(image_parameters['context_path'], destination, name)

    templates = Templates(TASKCLUSTER_ROOT)
    image_task = templates.load(IMAGE_BUILD_TASK, image_parameters)
    if params['revision_hash']:
        treeherder_transform.add_treeherder_revision_info(
            image_task['task'],
            params['head_rev'],
            params['revision_hash']
        )
        routes_transform.decorate_task_treeherder_routes(
            image_task['task'],
            "{}.{}".format(params['project'], params['revision_hash'])
        )
        routes_transform.decorate_task_json_routes(image_task['task'],
                                                   get_json_routes(),
                                                   image_parameters)

    image_task['attributes'] = {
        'kind': 'legacy',
    }

    graph['tasks'].append(image_task);
    task['requires'].append(details['taskId'])

    define_task = DEFINE_TASK.format(
        image_task['task']['workerType']
    )

    graph['scopes'].add(define_task)
    graph['scopes'] |= set(image_task['task'].get('scopes', []))
    route_scopes = map(lambda route: 'queue:route:' + route, image_task['task'].get('routes', []))
    graph['scopes'] |= set(route_scopes)

    details['required'] = True

def docker_load_from_url(url):
    """Get a docker image from a `docker save` tarball at the given URL,
    loading it into the running daemon and returning the image name."""

    # because we need to read this file twice (and one read is not all the way
    # through), it is difficult to stream it.  So we downlaod to disk and then
    # read it back.
    filename = 'temp-docker-image.tar'

    print("Downloading {}".format(url))
    subprocess.check_call(['curl', '-#', '-L', '-o', filename, url])

    print("Determining image name")
    tf = tarfile.open(filename)
    repositories = json.load(tf.extractfile('repositories'))
    name = repositories.keys()[0]
    tag = repositories[name].keys()[0]
    name = '{}:{}'.format(name, tag)
    print("Image name: {}".format(name))

    print("Loading image into docker")
    try:
        subprocess.check_call(['docker', 'load', '-i', filename])
    except subprocess.CalledProcessError:
        print("*** `docker load` failed.  You may avoid re-downloading that tarball by fixing the")
        print("*** problem and running `docker load < {}`.".format(filename))
        raise

    print("Deleting temporary file")
    os.unlink(filename)

    return name
