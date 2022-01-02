# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

import datetime
import logging
import requests
from taskcluster.generated import _client_importer
from taskcluster.generated.aio import _client_importer as _async_client_importer
from taskcluster.utils import stringDate
import urllib.parse

logger = logging.getLogger(__name__)


class TaskclusterConfig(object):
    """
    Local configuration used to access Taskcluster service and objects
    """

    def __init__(self, url=None):
        self.options = None
        self.secrets = None
        self.default_url = url if url is not None else os.environ.get("TASKCLUSTER_ROOT_URL")
        assert self.default_url is not None, "You must specify a Taskcluster deployment url"

    def auth(self, client_id=None, access_token=None, max_retries=12):
        """
        Build Taskcluster credentials options
        Supports, by order of preference:
         * directly provided credentials
         * credentials from environment variables
         * taskclusterProxy
         * no authentication
        """
        self.options = {"maxRetries": max_retries}

        if client_id is None and access_token is None:
            # Credentials preference: Use env. variables
            client_id = os.environ.get("TASKCLUSTER_CLIENT_ID")
            access_token = os.environ.get("TASKCLUSTER_ACCESS_TOKEN")
            logger.info("Using taskcluster credentials from environment")
        else:
            logger.info("Using taskcluster credentials from cli")

        if client_id is not None and access_token is not None:
            # Use provided credentials
            self.options["credentials"] = {
                "clientId": client_id,
                "accessToken": access_token,
            }
            self.options["rootUrl"] = self.default_url

        elif "TASK_ID" in os.environ:
            # Use Taskcluster Proxy when running in a task
            logger.info("Taskcluster Proxy enabled")
            self.options["rootUrl"] = os.environ.get("TASKCLUSTER_PROXY_URL", "http://taskcluster")

        else:
            logger.info("No Taskcluster authentication.")
            self.options["rootUrl"] = self.default_url

    def get_service(self, service_name, use_async=False):
        """
        Build a Taskcluster service instance using current authentication
        """
        if self.options is None:
            self.auth()

        client_importer = _async_client_importer if use_async else _client_importer
        service = getattr(client_importer, service_name.capitalize(), None)
        assert service is not None, "Invalid Taskcluster service {}".format(
            service_name
        )
        return service(self.options)

    def load_secrets(
        self, secret_name, prefixes=[], required=[], existing={}, local_secrets=None
    ):
        """Shortcut to use load_secrets helper with current authentication"""
        self.secrets = load_secrets(
            self.get_service('secrets'),
            secret_name,
            prefixes,
            required,
            existing,
            local_secrets,
        )
        return self.secrets

    def upload_artifact(self, artifact_path, content, content_type, ttl):
        """Shortcut to use upload_artifact helper with current authentication"""
        path = upload_artifact(
            self.get_service('queue'),
            artifact_path,
            content,
            content_type,
            ttl,
        )

        return urllib.parse.urljoin(self.default_url, path)


def load_secrets(
    secrets_service, secret_name, prefixes=[], required=[], existing={}, local_secrets=None
):
    """
    Fetch a specific set of secrets by name and verify that the required
    secrets exist.
    Also supports providing local secrets to avoid using remote Taskcluster service
    for local development (or contributor onboarding)
    A user can specify prefixes to limit the part of secrets used (useful when a secret
    is shared amongst several services)
    """
    secrets = {}
    if existing:
        secrets.update(existing)

    if isinstance(local_secrets, dict):
        # Use local secrets file to avoid using Taskcluster secrets
        logger.info("Using provided local secrets")
        all_secrets = local_secrets
    else:
        # Use Taskcluster secret service
        assert secret_name is not None, "Missing Taskcluster secret secret_name"
        all_secrets = secrets_service.get(secret_name).get("secret", dict())
        logger.info("Loaded Taskcluster secret {}".format(secret_name))

    if prefixes:
        # Use secrets behind supported prefixes
        for prefix in prefixes:
            secrets.update(all_secrets.get(prefix, dict()))

    else:
        # Use all secrets available
        secrets.update(all_secrets)

    # Check required secrets
    for required_secret in required:
        if required_secret not in secrets:
            raise Exception("Missing value {} in secrets.".format(required_secret))

    return secrets


def upload_artifact(queue_service, artifact_path, content, content_type, ttl):
    """
    DEPRECATED. Do not use.
    """
    task_id = os.environ.get("TASK_ID")
    run_id = os.environ.get("RUN_ID")
    proxy = os.environ.get("TASKCLUSTER_PROXY_URL")
    assert task_id and run_id and proxy, "Can only run in Taskcluster tasks with proxy"
    assert isinstance(content, str)
    assert isinstance(ttl, datetime.timedelta)

    # Create S3 artifact on Taskcluster
    resp = queue_service.createArtifact(
        task_id,
        run_id,
        artifact_path,
        {
            "storageType": "s3",
            "expires": stringDate(datetime.datetime.utcnow() + ttl),
            "contentType": content_type,
        },
    )
    assert resp["storageType"] == "s3", "Not an s3 storage"
    assert "putUrl" in resp, "Missing putUrl"
    assert "contentType" in resp, "Missing contentType"

    # Push the artifact on storage service
    headers = {"Content-Type": resp["contentType"]}
    push = requests.put(url=resp["putUrl"], headers=headers, data=content)
    push.raise_for_status()

    # Build the absolute url
    return "/api/queue/v1/task/{task_id}/runs/{run_id}/artifacts/{path}".format(
        task_id=task_id,
        run_id=run_id,
        path=artifact_path,
    )
