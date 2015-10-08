"""Taskcluster module. Defines a few helper functions to call into the taskcluster
   client.
"""
import os
from datetime import datetime, timedelta
from urlparse import urljoin

from mozharness.base.log import LogMixin


# Taskcluster {{{1
class Taskcluster(LogMixin):
    """
    Helper functions to report data to Taskcluster
    """
    def __init__(self, branch, rank, client_id, access_token, log_obj):
        self.rank = rank
        self.log_obj = log_obj

        # Try builds use a different set of credentials which have access to the
        # buildbot-try scope.
        if branch == 'try':
            self.buildbot = 'buildbot-try'
        else:
            self.buildbot = 'buildbot'

        # We can't import taskcluster at the top of the script because it is
        # part of the virtualenv, so import it now. The virtualenv needs to be
        # activated before this point by the mozharness script, or else we won't
        # be able to find this module.
        import taskcluster
        taskcluster.config['credentials']['clientId'] = client_id
        taskcluster.config['credentials']['accessToken'] = access_token
        self.taskcluster_queue = taskcluster.Queue()
        self.task_id = taskcluster.slugId()
        self.put_file = taskcluster.utils.putFile

    def create_task(self, routes):
        curdate = datetime.utcnow()
        self.info("Taskcluster taskId: %s" % self.task_id)
        self.info("Routes: %s" % routes)
        task = self.taskcluster_queue.createTask({
            # The null-provisioner and buildbot worker type don't actually exist.
            # So this task doesn't actually run - we just need to create the task so
            # we have something to attach artifacts to.
            "provisionerId": "null-provisioner",
            "workerType": self.buildbot,
            "created": curdate,
            "deadline": curdate + timedelta(hours=1),
            "routes": routes,
            "payload": {
            },
            "extra": {
                "index": {
                    "rank": self.rank,
                },
            },
            "metadata": {
                "name": "Buildbot/mozharness S3 uploader",
                "description": "Upload outputs of buildbot/mozharness builds to S3",
                "owner": "mshal@mozilla.com",
                "source": "http://hg.mozilla.org/build/mozharness/",
            }
        }, taskId=self.task_id)
        return task

    def claim_task(self, task):
        self.taskcluster_queue.claimTask(
            task['status']['taskId'],
            task['status']['runs'][0]['runId'],
            {
                "workerGroup": self.buildbot,
                "workerId": self.buildbot,
            })

    def create_artifact(self, task, filename):
        mime_types = {
            ".asc": "text/plain",
            ".checksums": "text/plain",
            ".json": "application/json",
            ".log": "text/plain",
            ".tar.bz2": "application/x-gtar",
            ".txt": "text/plain",
            ".xpi": "application/x-xpinstall",
            ".zip": "application/zip",
        }
        mime_type = mime_types.get(os.path.splitext(filename)[1], 'application/octet-stream')
        content_length = os.path.getsize(filename)

        self.info("Uploading to S3: filename=%s mimetype=%s length=%s" % (filename, mime_type, content_length))

        expiration = datetime.utcnow() + timedelta(weeks=52)
        artifact = self.taskcluster_queue.createArtifact(
            task['status']['taskId'],
            task['status']['runs'][0]['runId'],
            'public/build/%s' % os.path.basename(filename),
            {
                "storageType": "s3",
                "expires": expiration,
                "contentType": mime_type,
            })
        self.put_file(filename, artifact['putUrl'], mime_type)

    def report_completed(self, task):
        self.taskcluster_queue.reportCompleted(
            task['status']['taskId'],
            task['status']['runs'][0]['runId'],
            {
                "success": True,
            })

    def get_taskcluster_url(self, filename):
        return 'https://queue.taskcluster.net/v1/task/%s/artifacts/public/build/%s' % (
            self.task_id,
            os.path.basename(filename)
        )


# TasckClusterArtifactFinderMixin {{{1
class TaskClusterArtifactFinderMixin(object):
    # This class depends that you have extended from the base script
    QUEUE_URL = 'https://queue.taskcluster.net/v1/task/'
    SCHEDULER_URL = 'https://scheduler.taskcluster.net/v1/task-graph/'

    def get_task(self, task_id):
        """ Get Task Definition """
        # Signature: task(taskId) : result
        return self.load_json_url(urljoin(self.QUEUE_URL, task_id))

    def get_list_latest_artifacts(self, task_id):
        """ Get Artifacts from Latest Run """
        # Signature: listLatestArtifacts(taskId) : result

        # Notice that this grabs the most recent run of a task since we don't
        # know the run_id. This slightly slower, however, it is more convenient
        return self.load_json_url(urljoin(self.QUEUE_URL, '{}/artifacts'.format(task_id)))

    def url_to_artifact(self, task_id, full_path):
        """ Return a URL for an artifact. """
        return urljoin(self.QUEUE_URL, '{}/artifacts/{}'.format(task_id, full_path))

    def get_inspect_graph(self, task_group_id):
        """ Inspect Task Graph """
        # Signature: inspect(taskGraphId) : result
        return self.load_json_url(urljoin(self.SCHEDULER_URL, '{}/inspect'.format(task_group_id)))

    def find_parent_task_id(self, task_id):
        """ Returns the task_id of the parent task associated to the given task_id."""
        # Find group id to associated to all related tasks
        task_group_id = self.get_task(task_id)['taskGroupId']

        # Find child task and determine on which task it depends on
        for task in self.get_inspect_graph(task_group_id)['tasks']:
            if task['taskId'] == task_id:
                parent_task_id = task['requires'][0]

        return parent_task_id

    def set_bbb_artifacts(self, task_id):
        """ Find BBB artifacts through properties.json and set them. """
        # The tasks which represent a buildbot job only uploads one artifact:
        # the properties.json file
        p = self.load_json_url(
            self.url_to_artifact(task_id, 'public/properties.json'))

        # Set importants artifacts for test jobs
        self.set_artifacts(
            p['packageUrl'][0] if p.get('packageUrl') else None,
            p['testPackagesUrl'][0] if p.get('testPackagesUrl') else None,
            p['symbolsUrl'][0] if p.get('symbolsUrl') else None
        )

    def set_artifacts(self, installer, tests, symbols):
        """ Sets installer, test and symbols URLs from the artifacts of BBB based task."""
        self.installer_url, self.test_url, self.symbols_url = installer, tests, symbols
        self.info('Set installer_url: %s' % self.installer_url)
        self.info('Set test_url: %s' % self.test_url)
        self.info('Set symbols_url: %s' % self.symbols_url)

    def set_parent_artifacts(self, child_task_id):
        """ Find and set installer_url, test_url and symbols_url by querying TaskCluster.

        In Buildbot Bridge's normal behaviour we can find the artifacts by inspecting
        a child's taskId, determine the task in which it depends on and find the uploaded
        artifacts.

        In order to support multi-tiered task graph scheduling for BBB triggered tasks,
        we remove the assumption that the task which depends on is the one from which we
        find the artifacts we need. Instead, we can set a parent_task_id which points to the
        tasks from which to retrieve the artifacts. This decouples task dependency from task
        from which to grab the artifacts.

        In-tree triggered BBB tasks do not use parent_task_id, once there is efforts to move
        the scheduling into tree we can make parent_task_id as the only method.

        """
        # Task definition
        child_task = self.get_task(child_task_id)

        if child_task['payload']['properties'].get('parent_task_id'):
            # parent_task_id is used to point to the task from which to grab artifacts
            # rather than the one we depend on
            parent_id = child_task['payload']['properties']['parent_task_id']

            # Find out where the parent task uploaded the build
            parent_task = self.get_task(parent_id)

            if parent_task['extra'].get('locations'):
                # Build tasks generated under TC specify where they upload their builds
                installer_path = parent_task['extra']['locations']['build']

                self.set_artifacts(
                    self.url_to_artifact(parent_id, installer_path),
                    self.url_to_artifact(parent_id, 'public/build/test_packages.json'),
                    self.url_to_artifact(parent_id, 'public/build/target.crashreporter-symbols.zip')
                )
            else:
                self.set_bbb_artifacts(parent_id)
        else:
            parent_id = self.find_parent_task_id(child_task_id)
            self.set_bbb_artifacts(parent_id)
