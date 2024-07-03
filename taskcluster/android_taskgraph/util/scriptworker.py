# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import itertools
import os
from copy import deepcopy
from datetime import datetime
from functools import lru_cache

import jsone
from taskgraph.util.schema import resolve_keyed_by
from taskgraph.util.taskcluster import get_artifact_prefix
from taskgraph.util.yaml import load_yaml

cached_load_yaml = lru_cache(maxsize=None)(load_yaml)


def generate_beetmover_upstream_artifacts(
    config, job, platform, locale=None, dependencies=None, **kwargs
):
    """Generate the upstream artifacts for beetmover, using the artifact map.

    Currently only applies to beetmover tasks.

    Args:
        job (dict): The current job being generated
        dependencies (list): A list of the job's dependency labels.
        platform (str): The current build platform
        locale (str): The current locale being beetmoved.

    Returns:
        list: A list of dictionaries conforming to the upstream_artifacts spec.
    """
    base_artifact_prefix = get_artifact_prefix(job)
    resolve_keyed_by(
        job,
        "attributes.artifact_map",
        "artifact map",
        **{
            "release-type": config.params["release_type"],
            "platform": platform,
        },
    )
    map_config = deepcopy(cached_load_yaml(job["attributes"]["artifact_map"]))
    upstream_artifacts = list()

    if not locale:
        locales = map_config["default_locales"]
    elif isinstance(locale, list):
        locales = locale
    else:
        locales = [locale]

    if not dependencies:
        if job.get("dependencies"):
            dependencies = job["dependencies"].keys()
        elif job.get("primary-dependency"):
            dependencies = [job["primary-dependency"].kind]
        else:
            raise Exception("Unsupported type of dependency. Got job: {}".format(job))

    for locale, dep in itertools.product(locales, dependencies):
        paths = list()

        for filename in map_config["mapping"]:
            if dep not in map_config["mapping"][filename]["from"]:
                continue
            if locale != "multi" and not map_config["mapping"][filename]["all_locales"]:
                continue
            if (
                "only_for_platforms" in map_config["mapping"][filename]
                and platform
                not in map_config["mapping"][filename]["only_for_platforms"]
            ):
                continue
            if (
                "not_for_platforms" in map_config["mapping"][filename]
                and platform in map_config["mapping"][filename]["not_for_platforms"]
            ):
                continue
            if "partials_only" in map_config["mapping"][filename]:
                continue
            # The next time we look at this file it might be a different locale.
            file_config = deepcopy(map_config["mapping"][filename])
            resolve_keyed_by(
                file_config,
                "source_path_modifier",
                "source path modifier",
                locale=locale,
            )

            kwargs["locale"] = locale

            paths.append(
                os.path.join(
                    base_artifact_prefix,
                    jsone.render(file_config["source_path_modifier"], kwargs),
                    jsone.render(filename, kwargs),
                )
            )

        if job.get("dependencies") and getattr(
            job["dependencies"][dep], "release_artifacts", None
        ):
            paths = [
                path
                for path in paths
                if path in job["dependencies"][dep].release_artifacts
            ]

        if not paths:
            continue

        upstream_artifacts.append(
            {
                "taskId": {"task-reference": "<{}>".format(dep)},
                "taskType": map_config["tasktype_map"].get(dep),
                "paths": sorted(paths),
                "locale": locale,
            }
        )

    upstream_artifacts.sort(key=lambda u: u["paths"])
    return upstream_artifacts


def generate_beetmover_artifact_map(config, job, **kwargs):
    """Generate the beetmover artifact map.

    Currently only applies to beetmover tasks.

    Args:
        config (): Current taskgraph configuration.
        job (dict): The current job being generated
    Common kwargs:
        platform (str): The current build platform
        locale (str): The current locale being beetmoved.

    Returns:
        list: A list of dictionaries containing source->destination
            maps for beetmover.
    """
    platform = kwargs.get("platform", "")
    resolve_keyed_by(
        job,
        "attributes.artifact_map",
        job["label"],
        **{
            "release-type": config.params["release_type"],
            "platform": platform,
        },
    )
    map_config = deepcopy(cached_load_yaml(job["attributes"]["artifact_map"]))
    base_artifact_prefix = map_config.get(
        "base_artifact_prefix", get_artifact_prefix(job)
    )

    artifacts = list()

    dependencies = job["dependencies"].keys()

    if kwargs.get("locale"):
        if isinstance(kwargs["locale"], list):
            locales = kwargs["locale"]
        else:
            locales = [kwargs["locale"]]
    else:
        locales = map_config["default_locales"]

    resolve_keyed_by(
        map_config,
        "s3_bucket_paths",
        job["label"],
        **{"build-type": job["attributes"]["build-type"]},
    )

    for locale, dep in sorted(itertools.product(locales, dependencies)):
        paths = dict()
        for filename in map_config["mapping"]:
            # Relevancy checks
            if dep not in map_config["mapping"][filename]["from"]:
                # We don't get this file from this dependency.
                continue
            if locale != "multi" and not map_config["mapping"][filename]["all_locales"]:
                # This locale either doesn't produce or shouldn't upload this file.
                continue
            if (
                "only_for_platforms" in map_config["mapping"][filename]
                and platform
                not in map_config["mapping"][filename]["only_for_platforms"]
            ):
                # This platform either doesn't produce or shouldn't upload this file.
                continue
            if (
                "not_for_platforms" in map_config["mapping"][filename]
                and platform in map_config["mapping"][filename]["not_for_platforms"]
            ):
                # This platform either doesn't produce or shouldn't upload this file.
                continue
            if "partials_only" in map_config["mapping"][filename]:
                continue

            # deepcopy because the next time we look at this file the locale will differ.
            file_config = deepcopy(map_config["mapping"][filename])

            for field in [
                "destinations",
                "locale_prefix",
                "source_path_modifier",
                "update_balrog_manifest",
                "pretty_name",
                "checksums_path",
            ]:
                resolve_keyed_by(file_config, field, job["label"], locale=locale)

            # This format string should ideally be in the configuration file,
            # but this would mean keeping variable names in sync between code + config.
            destinations = [
                "{s3_bucket_path}/{dest_path}/{filename}".format(
                    s3_bucket_path=bucket_path,
                    dest_path=dest_path,
                    filename=file_config.get("pretty_name", filename),
                )
                for dest_path, bucket_path in itertools.product(
                    file_config["destinations"], map_config["s3_bucket_paths"]
                )
            ]
            # Creating map entries
            # Key must be artifact path, to avoid trampling duplicates, such
            # as public/build/target.apk and public/build/multi/target.apk
            key = os.path.join(
                base_artifact_prefix,
                file_config["source_path_modifier"],
                filename,
            )

            paths[key] = {
                "destinations": destinations,
            }
            if file_config.get("checksums_path"):
                paths[key]["checksums_path"] = file_config["checksums_path"]

            # optional flag: balrog manifest
            if file_config.get("update_balrog_manifest"):
                paths[key]["update_balrog_manifest"] = True
                if file_config.get("balrog_format"):
                    paths[key]["balrog_format"] = file_config["balrog_format"]

        if not paths:
            # No files for this dependency/locale combination.
            continue

        # Render all variables for the artifact map
        platforms = deepcopy(map_config.get("platform_names", {}))
        if platform:
            for key in platforms.keys():
                resolve_keyed_by(platforms, key, job["label"], platform=platform)

        version = config.params["version"]
        upload_date = datetime.fromtimestamp(config.params["build_date"])

        if "nightly" in job["attributes"].get("build-type", ""):
            folder_prefix = upload_date.strftime("%Y/%m/%Y-%m-%d-%H-%M-%S-")
            # TODO: Remove this when version.txt has versioning fixed
            version = version.split("-")[0]
        else:
            folder_prefix = f"{version}/android/"

        kwargs.update(
            {"locale": locale, "version": version, "folder_prefix": folder_prefix}
        )
        kwargs.update(**platforms)
        paths = jsone.render(paths, kwargs)
        artifacts.append(
            {
                "taskId": {"task-reference": "<{}>".format(dep)},
                "locale": locale,
                "paths": paths,
            }
        )

    return artifacts
