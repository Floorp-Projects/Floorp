# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import io
import mimetypes
import os
import sys

import botocore
import boto3
import concurrent.futures as futures
import requests


def s3_upload(files, key_prefix=None):
    """Upload files to an S3 bucket.

    ``files`` is an iterable of ``(path, BaseFile)`` (typically from a
    mozpack Finder).

    Keys in the bucket correspond to source filenames. If ``key_prefix`` is
    defined, key names will be ``<key_prefix>/<path>``.
    """
    region = 'us-west-2'
    level = os.environ.get('MOZ_SCM_LEVEL', '1')
    bucket = {
        '1': 'gecko-docs.mozilla.org-l1',
        '2': 'gecko-docs.mozilla.org-l2',
        '3': 'gecko-docs.mozilla.org',
    }[level]
    secrets_url = 'http://taskcluster/secrets/v1/secret/'
    secrets_url += 'project/releng/gecko/build/level-{}/gecko-docs-upload'.format(
        level)

    # Get the credentials from the TC secrets service.  Note that these
    # differ per SCM level
    if 'TASK_ID' in os.environ:
        print("Using AWS credentials from the secrets service")
        session = requests.Session()
        res = session.get(secrets_url)
        res.raise_for_status()
        secret = res.json()['secret']
        session = boto3.session.Session(
            aws_access_key_id=secret['AWS_ACCESS_KEY_ID'],
            aws_secret_access_key=secret['AWS_SECRET_ACCESS_KEY'],
            region_name=region)
    else:
        print("Trying to use your AWS credentials..")
        session = boto3.session.Session(region_name=region)

    s3 = session.client('s3',
                        config=botocore.client.Config(max_pool_connections=20))

    def upload(f, path, bucket, key, extra_args):
        # Need to flush to avoid buffering/interleaving from multiple threads.
        sys.stdout.write('uploading %s to %s\n' % (path, key))
        sys.stdout.flush()
        s3.upload_fileobj(f, bucket, key, ExtraArgs=extra_args)

    fs = []
    with futures.ThreadPoolExecutor(20) as e:
        for path, f in files:
            content_type, content_encoding = mimetypes.guess_type(path)
            extra_args = {}
            if content_type:
                extra_args['ContentType'] = content_type
            if content_encoding:
                extra_args['ContentEncoding'] = content_encoding

            if key_prefix:
                key = '%s/%s' % (key_prefix, path)
            else:
                key = path

            # The file types returned by mozpack behave like file objects. But
            # they don't accept an argument to read(). So we wrap in a BytesIO.
            fs.append(e.submit(upload, io.BytesIO(f.read()), path, bucket, key,
                               extra_args))

    # Need to do this to catch any exceptions.
    for f in fs:
        f.result()
