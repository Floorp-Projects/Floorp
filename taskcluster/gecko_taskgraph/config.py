# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os
import logging

from taskgraph.config import GraphConfig
from taskgraph.util.schema import Schema, optionally_keyed_by, validate_schema
from taskgraph.util.yaml import load_yaml
from voluptuous import Required, Optional, Any

logger = logging.getLogger(__name__)

graph_config_schema = Schema(
    {
        # The trust-domain for this graph.
        # (See https://firefox-source-docs.mozilla.org/taskcluster/taskcluster/taskgraph.html#taskgraph-trust-domain)  # noqa
        Required("trust-domain"): str,
        # This specifes the prefix for repo parameters that refer to the project being built.
        # This selects between `head_rev` and `comm_head_rev` and related paramters.
        # (See http://firefox-source-docs.mozilla.org/taskcluster/taskcluster/parameters.html#push-information  # noqa
        # and http://firefox-source-docs.mozilla.org/taskcluster/taskcluster/parameters.html#comm-push-information)  # noqa
        Required("project-repo-param-prefix"): str,
        # This specifies the top level directory of the application being built.
        # ie. "browser/" for Firefox, "comm/mail/" for Thunderbird.
        Required("product-dir"): str,
        Required("treeherder"): {
            # Mapping of treeherder group symbols to descriptive names
            Required("group-names"): {str: str}
        },
        Required("index"): {Required("products"): [str]},
        Required("try"): {
            # We have a few platforms for which we want to do some "extra" builds, or at
            # least build-ish things.  Sort of.  Anyway, these other things are implemented
            # as different "platforms".  These do *not* automatically ride along with "-p
            # all"
            Required("ridealong-builds"): {str: [str]},
        },
        Required("release-promotion"): {
            Required("products"): [str],
            Required("flavors"): {
                str: {
                    Required("product"): str,
                    Required("target-tasks-method"): str,
                    Optional("is-rc"): bool,
                    Optional("rebuild-kinds"): [str],
                    Optional("version-bump"): bool,
                    Optional("partial-updates"): bool,
                }
            },
        },
        Required("merge-automation"): {
            Required("behaviors"): {
                str: {
                    Optional("from-branch"): str,
                    Required("to-branch"): str,
                    Optional("from-repo"): str,
                    Required("to-repo"): str,
                    Required("version-files"): [
                        {
                            Required("filename"): str,
                            Optional("new-suffix"): str,
                            Optional("version-bump"): Any("major", "minor"),
                        }
                    ],
                    Required("replacements"): [[str]],
                    Required("merge-old-head"): bool,
                    Optional("base-tag"): str,
                    Optional("end-tag"): str,
                    Optional("fetch-version-from"): str,
                }
            },
        },
        Required("scriptworker"): {
            # Prefix to add to scopes controlling scriptworkers
            Required("scope-prefix"): str,
        },
        Required("task-priority"): optionally_keyed_by(
            "project",
            Any(
                "highest",
                "very-high",
                "high",
                "medium",
                "low",
                "very-low",
                "lowest",
            ),
        ),
        Required("partner-urls"): {
            Required("release-partner-repack"): optionally_keyed_by(
                "release-product", "release-level", "release-type", Any(str, None)
            ),
            Optional("release-partner-attribution"): optionally_keyed_by(
                "release-product", "release-level", "release-type", Any(str, None)
            ),
            Required("release-eme-free-repack"): optionally_keyed_by(
                "release-product", "release-level", "release-type", Any(str, None)
            ),
        },
        Required("workers"): {
            Required("aliases"): {
                str: {
                    Required("provisioner"): optionally_keyed_by("level", str),
                    Required("implementation"): str,
                    Required("os"): str,
                    Required("worker-type"): optionally_keyed_by(
                        "level", "release-level", str
                    ),
                }
            },
        },
        Required("mac-notarization"): {
            Required("mac-behavior"): optionally_keyed_by(
                "project",
                "shippable",
                Any("mac_notarize", "mac_geckodriver", "mac_sign", "mac_sign_and_pkg"),
            ),
            Required("mac-entitlements"): optionally_keyed_by(
                "platform", "release-level", str
            ),
            Required("mac-requirements"): optionally_keyed_by("platform", str),
        },
        Required("taskgraph"): {
            Optional(
                "register",
                description="Python function to call to register extensions.",
            ): str,
            Optional("decision-parameters"): str,
        },
    }
)


def validate_graph_config(config):
    validate_schema(graph_config_schema, config, "Invalid graph configuration:")


def load_graph_config(root_dir):
    config_yml = os.path.join(root_dir, "config.yml")
    if not os.path.exists(config_yml):
        raise Exception(f"Couldn't find taskgraph configuration: {config_yml}")

    logger.debug(f"loading config from `{config_yml}`")
    config = load_yaml(config_yml)
    logger.debug("validating the graph config.")
    validate_graph_config(config)
    return GraphConfig(config=config, root_dir=root_dir)
