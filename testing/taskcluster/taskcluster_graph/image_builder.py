import hashlib
import json
import os
import subprocess
import tarfile
import urllib2

from slugid import nice as slugid
from taskgraph.util.templates import Templates

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
