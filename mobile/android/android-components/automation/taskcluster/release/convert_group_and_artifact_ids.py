#!/usr/bin/env python3
"""
Temporary script to massage some of the group/artifact ids. Does the following:
a. download upstreamZip files
b. determine what artifacts should change
c. open each zip, rename files if needed, and regenerate the pom.xml
(+ checksums). pom.xml must be regenerated because it list dependencies,
which might have changed (eg. if org.mozilla.components:support-base,
every package that depends on it must be updated)
"""

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import aiohttp
import asyncio
import hashlib
import logging
import os
import shutil
import sys
import tempfile
import zipfile

from xml.etree import ElementTree
import taskcluster.aio

from scriptworker.exceptions import DownloadError
from scriptworker.utils import download_file, retry_async, raise_future_exceptions


log = logging.getLogger(name=__name__)

SUPPORTED_HASHES = ('md5', 'sha1')

ElementTree.register_namespace('', 'http://maven.apache.org/POM/4.0.0')

XML_NAMESPACES = {
    'm': 'http://maven.apache.org/POM/4.0.0',
}


def craft_taskcluster_download_url(task_id, artifact_path):
    # Sadly, this is not part of the taskcluster python library
    return 'https://queue.taskcluster.net/v1/task/{task_id}/artifacts/{artifact_path}'.format(
        task_id=task_id, artifact_path=artifact_path
    )


def craft_abs_filename(artifact_path):
    return os.path.abspath(os.path.join(
        os.path.dirname(__file__), '../../../work_dir/{}'.format(artifact_path)
    ))


async def download_all_zip_artifacts(upstream_zip_definitions, session):
    async_tasks = tuple([
        asyncio.ensure_future(
            retry_async(
                download_file,
                kwargs={'context': None, 'url': definition['url'], 'abs_filename': definition['abs_filename'], 'session': session},
                retry_exceptions=(DownloadError, aiohttp.ClientError, asyncio.TimeoutError)
            )

        )
        for definition in upstream_zip_definitions
    ])

    await raise_future_exceptions(async_tasks)


def should_artifact_be_converted(zip_definition):
    if not zip_definition['newGroupId'] or not zip_definition['newArtifactId']:
        log.warning(
            'Artifact "{}" from task "{}" won\'t be modified. Missing either '
            'newGroupId or newArtifactId. newGroupId: {}. newArtifactId: {}'.format(
                zip_definition['path'], zip_definition['taskId'],
                zip_definition['newGroupId'], zip_definition['newArtifactId']
            )
        )
        return False
    return True


def convert_zip(zip_path, conversion_dict):
    log.info('Converting "{}"...'.format(zip_path))
    base_name = os.path.basename(zip_path)

    with tempfile.TemporaryDirectory() as d:
        out_path = os.path.join(d, base_name)
        with zipfile.ZipFile(zip_path) as zip_in:
            with zipfile.ZipFile(out_path, mode='w') as zip_out:
                zip_info_of_files = [zip_info for zip_info in zip_in.infolist() if not zip_info.is_dir()]
                for zip_info in zip_info_of_files:
                    new_file_name = get_new_name(zip_info.filename, conversion_dict)

                    if new_file_name.endswith('.pom'):
                        process_pom_and_checksums(zip_in, zip_out, zip_info, new_file_name, conversion_dict)
                    elif new_file_name.endswith('.pom.md5') or new_file_name.endswith('.pom.sha1'):
                        # these files are regenerated above.
                        continue
                    else:
                        copy_file_into_zip(zip_in, zip_out, zip_info, new_file_name)

        shutil.move(out_path, zip_path)

    log.info('"{}" converted!'.format(zip_path))


def get_new_name(file_name, conversion_dict):
    try:
        old_group_id = extract_group_id(file_name)
        old_artifact_id = extract_artifact_id(file_name)
        new = conversion_dict[get_conversion_key(old_group_id, old_artifact_id)]
        new_file_name = convert_file_name(
            file_name, old_group_id, old_artifact_id, new['new_group_id'], new['new_artifact_id']
        )
        log.debug('File "{}" renamed into "{}"'.format(file_name, new_file_name))
    except KeyError:
        new_file_name = file_name
        log.debug('Keeping old file name "{}"'.format(new_file_name))

    return new_file_name


def convert_file_name(file_name, old_group_id, old_artifact_id, new_group_id, new_artifact_id):
    return file_name.replace(
        old_group_id.replace('.', '/'), new_group_id.replace('.', '/')
    ).replace(
        old_artifact_id, new_artifact_id
    )


def extract_group_id(file_name):
    return '.'.join(file_name.split('/')[:-3])


def extract_artifact_id(file_name):
    return file_name.split('/')[-3]


def process_pom_and_checksums(zip_in, zip_out, zip_info, new_file_name, conversion_dict):
    raw_xml = zip_in.read(zip_info.filename)
    pom = translate_pom_xml(raw_xml, conversion_dict)
    zip_out.writestr(new_file_name, pom, compress_type=zipfile.ZIP_DEFLATED)

    for hash_alg in SUPPORTED_HASHES:
        sum = get_checksum(pom, hash_alg)
        zip_out.writestr('{}.{}'.format(new_file_name, hash_alg), sum, compress_type=zipfile.ZIP_DEFLATED)


def copy_file_into_zip(zip_in, zip_out, zip_info, new_file_name):
    with tempfile.NamedTemporaryFile() as temp:
        temp.write(zip_in.read(zip_info.filename))
        temp.seek(0)
        zip_out.write(
            temp.name, arcname=new_file_name,
            # TODO: Figure out why zip_info.compress_type is invalid
            compress_type=zipfile.ZIP_DEFLATED
        )


def get_checksum(data, hash_alg):
    h = hashlib.new(hash_alg)
    h.update(data)
    return h.hexdigest()


def translate_pom_xml(raw_xml, conversion_dict):
    root = ElementTree.fromstring(raw_xml)
    convert_xml_element(root, conversion_dict)

    dependencies = root.find('m:dependencies', XML_NAMESPACES)
    if dependencies is not None:    # Some packages may not have any dependencies
        for dependency in dependencies.findall('m:dependency', XML_NAMESPACES):
            convert_xml_element(dependency, conversion_dict)

    new_xml = ElementTree.tostring(root, encoding='UTF-8')
    log.debug('pom.xml modified. From: {}\n\nTo: {}'.format(raw_xml, new_xml))

    return new_xml


def convert_xml_element(element, conversion_dict):
    group_id_element = element.find('m:groupId', XML_NAMESPACES)
    old_group_id = group_id_element.text
    artifact_id_element = element.find('m:artifactId', XML_NAMESPACES)
    old_artifact_id = artifact_id_element.text

    try:
        new = conversion_dict[get_conversion_key(old_group_id, old_artifact_id)]
        group_id_element.text = new['new_group_id']
        artifact_id_element.text = new['new_artifact_id']
    except KeyError:
        pass


async def async_main(session, task_id):
    asyncQueue = taskcluster.aio.Queue(session=session)
    task_definition = await asyncQueue.task(task_id)
    upstream_zip_definitions = task_definition['payload']['upstreamZip']

    upstream_zip_definitions = [
        {
            **definition,
            'url': craft_taskcluster_download_url(definition['taskId'], definition['path']),
            'abs_filename': craft_abs_filename(definition['path']),
        }
        for definition in upstream_zip_definitions
    ]

    await download_all_zip_artifacts(upstream_zip_definitions, session)

    conversion_dict = build_conversion_dict(upstream_zip_definitions)

    log.debug('Files will be converted based on this dictionary: {}'.format(conversion_dict))

    for definition in upstream_zip_definitions:
        convert_zip(definition['abs_filename'], conversion_dict)


def build_conversion_dict(upstream_zip_definitions):
    dict_ = {}

    for definition in upstream_zip_definitions:
        with zipfile.ZipFile(definition['abs_filename']) as zip_in:
            file_names = [zip_info.filename for zip_info in zip_in.infolist() if not zip_info.is_dir()]

            old_group_id = extract_group_id(file_names[0])
            old_artifact_id = extract_artifact_id(file_names[0])

        if should_artifact_be_converted(definition):
            new_group_id = definition['newGroupId']
            new_artifact_id = definition['newArtifactId']

            key = get_conversion_key(old_group_id, old_artifact_id)
            dict_[key] = {
                'new_group_id': new_group_id,
                'new_artifact_id': new_artifact_id,
            }

    return dict_


def get_conversion_key(old_group_id, old_artifact_id):
    return '{}:{}'.format(old_group_id, old_artifact_id)


async def _handle_asyncio_loop(async_main, task_id):
    async with aiohttp.ClientSession() as session:
        await async_main(session, task_id)


if __name__ == "__main__":
    logging.basicConfig(
        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s', level=logging.DEBUG
    )
    logging.getLogger('taskcluster').setLevel(logging.WARNING)

    task_id = sys.argv[1]

    loop = asyncio.get_event_loop()
    loop.run_until_complete(_handle_asyncio_loop(async_main, task_id))
