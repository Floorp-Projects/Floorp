"""Taskcluster module. Defines a few helper functions to call into the taskcluster
   client.
"""
import os
from datetime import datetime, timedelta
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
