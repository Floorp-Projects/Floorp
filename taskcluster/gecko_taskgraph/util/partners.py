# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import json
import logging
import os
import requests
import xml.etree.ElementTree as ET
from copy import deepcopy
from urllib.parse import urlencode

import yaml
from redo import retry
from taskgraph.util.schema import resolve_keyed_by

from gecko_taskgraph.util.attributes import release_level

# Suppress chatty requests logging
logging.getLogger("requests").setLevel(logging.WARNING)

log = logging.getLogger(__name__)

GITHUB_API_ENDPOINT = "https://api.github.com/graphql"

"""
LOGIN_QUERY, MANIFEST_QUERY, and REPACK_CFG_QUERY are all written to the Github v4 API,
which users GraphQL. See https://developer.github.com/v4/
"""

LOGIN_QUERY = """query {
  viewer {
    login
    name
  }
}
"""

# Returns the contents of default.xml from a manifest repository
MANIFEST_QUERY = """query {
  repository(owner:"%(owner)s", name:"%(repo)s") {
    object(expression: "master:%(file)s") {
      ... on Blob {
        text
      }
    }
  }
}
"""
# Example response:
# {
#   "data": {
#     "repository": {
#       "object": {
#         "text": "<?xml version=\"1.0\" ?>\n<manifest>\n  " +
#           "<remote fetch=\"git@github.com:mozilla-partners/\" name=\"mozilla-partners\"/>\n  " +
#           "<remote fetch=\"git@github.com:mozilla/\" name=\"mozilla\"/>\n\n  " +
#           "<project name=\"repack-scripts\" path=\"scripts\" remote=\"mozilla-partners\" " +
#           "revision=\"master\"/>\n  <project name=\"build-tools\" path=\"scripts/tools\" " +
#           "remote=\"mozilla\" revision=\"master\"/>\n  <project name=\"mozilla-EME-free\" " +
#           "path=\"partners/mozilla-EME-free\" remote=\"mozilla-partners\" " +
#           "revision=\"master\"/>\n</manifest>\n"
#       }
#     }
#   }
# }

# Returns the contents of desktop/*/repack.cfg for a partner repository
REPACK_CFG_QUERY = """query{
  repository(owner:"%(owner)s", name:"%(repo)s") {
    object(expression: "%(revision)s:desktop/"){
      ... on Tree {
        entries {
          name
          object {
            ... on Tree {
              entries {
                name
                object {
                  ... on Blob {
                    text
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}
"""
# Example response:
# {
#   "data": {
#     "repository": {
#       "object": {
#         "entries": [
#           {
#             "name": "mozilla-EME-free",
#             "object": {
#               "entries": [
#                 {
#                   "name": "distribution",
#                   "object": {}
#                 },
#                 {
#                   "name": "repack.cfg",
#                   "object": {
#                     "text": "aus=\"mozilla-EMEfree\"\ndist_id=\"mozilla-EMEfree\"\n" +
#                             "dist_version=\"1.0\"\nlinux-i686=true\nlinux-x86_64=true\n" +
#                             " locales=\"ach af de en-US\"\nmac=true\nwin32=true\nwin64=true\n" +
#                             "output_dir=\"%(platform)s-EME-free/%(locale)s\"\n\n" +
#                             "# Upload params\nbucket=\"net-mozaws-prod-delivery-firefox\"\n" +
#                             "upload_to_candidates=true\n"
#                   }
#                 }
#               ]
#             }
#           }
#         ]
#       }
#     }
#   }
# }

# Map platforms in repack.cfg into their equivalents in taskcluster
TC_PLATFORM_PER_FTP = {
    "linux-i686": "linux-shippable",
    "linux-x86_64": "linux64-shippable",
    "mac": "macosx64-shippable",
    "win32": "win32-shippable",
    "win64": "win64-shippable",
    "win64-aarch64": "win64-aarch64-shippable",
}

TASKCLUSTER_PROXY_SECRET_ROOT = "http://taskcluster/secrets/v1/secret"

LOCALES_FILE = os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__)))),
    "browser",
    "locales",
    "l10n-changesets.json",
)

# cache data at the module level
partner_configs = {}


def get_token(params):
    """We use a Personal Access Token from Github to lookup partner config. No extra scopes are
    needed on the token to read public repositories, but need the 'repo' scope to see private
    repositories. This is not fine grained and also grants r/w access, but is revoked at the repo
    level.
    """

    # Allow for local taskgraph debugging
    if os.environ.get("GITHUB_API_TOKEN"):
        return os.environ["GITHUB_API_TOKEN"]

    # The 'usual' method - via taskClusterProxy for decision tasks
    url = "{secret_root}/project/releng/gecko/build/level-{level}/partner-github-api".format(
        secret_root=TASKCLUSTER_PROXY_SECRET_ROOT, **params
    )
    try:
        resp = retry(
            requests.get,
            attempts=2,
            sleeptime=10,
            args=(url,),
            kwargs={"timeout": 60, "headers": ""},
        )
        j = resp.json()
        return j["secret"]["key"]
    except (requests.ConnectionError, ValueError, KeyError):
        raise RuntimeError("Could not get Github API token to lookup partner data")


def query_api(query, token):
    """Make a query with a Github auth header, returning the json"""
    headers = {"Authorization": "bearer %s" % token}
    r = requests.post(GITHUB_API_ENDPOINT, json={"query": query}, headers=headers)
    r.raise_for_status()

    j = r.json()
    if "errors" in j:
        raise RuntimeError("Github query error - %s", j["errors"])
    return j


def check_login(token):
    log.debug("Checking we have a valid login")
    query_api(LOGIN_QUERY, token)


def get_repo_params(repo):
    """Parse the organisation and repo name from an https or git url for a repo"""
    if repo.startswith("https"):
        # eg https://github.com/mozilla-partners/mozilla-EME-free
        return repo.rsplit("/", 2)[-2:]
    elif repo.startswith("git@"):
        # eg git@github.com:mozilla-partners/mailru.git
        repo = repo.replace(".git", "")
        return repo.split(":")[-1].split("/")


def get_partners(manifestRepo, token):
    """Given the url to a manifest repository, retrieve the default.xml and parse it into a
    list of partner repos.
    """
    log.debug("Querying for manifest default.xml in %s", manifestRepo)
    owner, repo = get_repo_params(manifestRepo)
    query = MANIFEST_QUERY % {"owner": owner, "repo": repo, "file": "default.xml"}
    raw_manifest = query_api(query, token)
    log.debug("Raw manifest: %s", raw_manifest)
    if not raw_manifest["data"]["repository"]:
        raise RuntimeError(
            "Couldn't load partner manifest at %s, insufficient permissions ?"
            % manifestRepo
        )
    e = ET.fromstring(raw_manifest["data"]["repository"]["object"]["text"])

    remotes = {}
    partners = {}
    for child in e:
        if child.tag == "remote":
            name = child.attrib["name"]
            url = child.attrib["fetch"]
            remotes[name] = url
            log.debug("Added remote %s at %s", name, url)
        elif child.tag == "project":
            # we don't need to check any code repos
            if "scripts" in child.attrib["path"]:
                continue
            owner, _ = get_repo_params(remotes[child.attrib["remote"]] + "_")
            partner_url = {
                "owner": owner,
                "repo": child.attrib["name"],
                "revision": child.attrib["revision"],
            }
            partners[child.attrib["name"]] = partner_url
            log.debug(
                "Added partner %s at revision %s"
                % (partner_url["repo"], partner_url["revision"])
            )
    return partners


def parse_config(data):
    """Parse a single repack.cfg file into a python dictionary.
    data is contents of the file, in "foo=bar\nbaz=buzz" style. We do some translation on
    locales and platforms data, otherwise passthrough
    """
    ALLOWED_KEYS = (
        "locales",
        "platforms",
        "upload_to_candidates",
        "repack_stub_installer",
        "publish_to_releases",
    )
    config = {"platforms": []}
    for l in data.splitlines():
        if "=" in l:
            l = str(l)
            key, value = l.split("=", 1)
            value = value.strip("'\"").rstrip("'\"")
            if key in TC_PLATFORM_PER_FTP.keys():
                if value.lower() == "true":
                    config["platforms"].append(TC_PLATFORM_PER_FTP[key])
                continue
            if key not in ALLOWED_KEYS:
                continue
            if key == "locales":
                # a list please
                value = value.split(" ")
            config[key] = value
    return config


def get_repack_configs(repackRepo, token):
    """For a partner repository, retrieve all the repack.cfg files and parse them into a dict"""
    log.debug("Querying for configs in %s", repackRepo)
    query = REPACK_CFG_QUERY % repackRepo
    raw_configs = query_api(query, token)
    raw_configs = raw_configs["data"]["repository"]["object"]["entries"]

    configs = {}
    for sub_config in raw_configs:
        name = sub_config["name"]
        for file in sub_config["object"].get("entries", []):
            if file["name"] != "repack.cfg":
                continue
            configs[name] = parse_config(file["object"]["text"])
    return configs


def get_attribution_config(manifestRepo, token):
    log.debug("Querying for manifest attribution_config.yml in %s", manifestRepo)
    owner, repo = get_repo_params(manifestRepo)
    query = MANIFEST_QUERY % {
        "owner": owner,
        "repo": repo,
        "file": "attribution_config.yml",
    }
    raw_manifest = query_api(query, token)
    if not raw_manifest["data"]["repository"]:
        raise RuntimeError(
            "Couldn't load partner manifest at %s, insufficient permissions ?"
            % manifestRepo
        )
    # no file has been set up, gracefully continue
    if raw_manifest["data"]["repository"]["object"] is None:
        log.debug("No attribution_config.yml file found")
        return {}

    return yaml.safe_load(raw_manifest["data"]["repository"]["object"]["text"])


def get_partner_config_by_url(manifest_url, kind, token, partner_subset=None):
    """Retrieve partner data starting from the manifest url, which points to a repository
    containing a default.xml that is intended to be drive the Google tool 'repo'. It
    descends into each partner repo to lookup and parse the repack.cfg file(s).

    If partner_subset is a list of sub_config names only return data for those.

    Supports caching data by kind to avoid repeated requests, relying on the related kinds for
    partner repacking, signing, repackage, repackage signing all having the same kind prefix.
    """
    if not manifest_url:
        raise RuntimeError(f"Manifest url for {kind} not defined")
    if kind not in partner_configs:
        log.info("Looking up data for %s from %s", kind, manifest_url)
        check_login(token)
        if kind == "release-partner-attribution":
            partner_configs[kind] = get_attribution_config(manifest_url, token)
        else:
            partners = get_partners(manifest_url, token)

            partner_configs[kind] = {}
            for partner, partner_url in partners.items():
                if partner_subset and partner not in partner_subset:
                    continue
                partner_configs[kind][partner] = get_repack_configs(partner_url, token)

    return partner_configs[kind]


def check_if_partners_enabled(config, tasks):
    if (
        (
            config.params["release_enable_partner_repack"]
            and config.kind.startswith("release-partner-repack")
        )
        or (
            config.params["release_enable_partner_attribution"]
            and config.kind.startswith("release-partner-attribution")
        )
        or (
            config.params["release_enable_emefree"]
            and config.kind.startswith("release-eme-free-")
        )
    ):
        yield from tasks


def get_partner_config_by_kind(config, kind):
    """Retrieve partner data starting from the manifest url, which points to a repository
    containing a default.xml that is intended to be drive the Google tool 'repo'. It
    descends into each partner repo to lookup and parse the repack.cfg file(s).

    Supports caching data by kind to avoid repeated requests, relying on the related kinds for
    partner repacking, signing, repackage, repackage signing all having the same kind prefix.
    """
    partner_subset = config.params["release_partners"]
    partner_configs = config.params["release_partner_config"] or {}

    # TODO eme-free should be a partner; we shouldn't care about per-kind
    for k in partner_configs:
        if kind.startswith(k):
            kind_config = partner_configs[k]
            break
    else:
        return {}
    # if we're only interested in a subset of partners we remove the rest
    if partner_subset:
        if kind.startswith("release-partner-repack"):
            # TODO - should be fatal to have an unknown partner in partner_subset
            for partner in [p for p in kind_config.keys() if p not in partner_subset]:
                del kind_config[partner]
        elif kind.startswith("release-partner-attribution") and isinstance(
            kind_config, dict
        ):
            all_configs = deepcopy(kind_config.get("configs", []))
            kind_config["configs"] = []
            for this_config in all_configs:
                if this_config["campaign"] in partner_subset:
                    kind_config["configs"].append(this_config)
    return kind_config


def _fix_subpartner_locales(orig_config, all_locales):
    subpartner_config = deepcopy(orig_config)
    # Get an ordered list of subpartner locales that is a subset of all_locales
    subpartner_config["locales"] = sorted(
        list(set(orig_config["locales"]) & set(all_locales))
    )
    return subpartner_config


def fix_partner_config(orig_config):
    pc = {}
    with open(LOCALES_FILE) as fh:
        all_locales = list(json.load(fh).keys())
    # l10n-changesets.json doesn't include en-US, but the repack list does
    if "en-US" not in all_locales:
        all_locales.append("en-US")
    for kind, kind_config in orig_config.items():
        if kind == "release-partner-attribution":
            pc[kind] = {}
            if kind_config:
                pc[kind] = {"defaults": kind_config["defaults"]}
                for config in kind_config["configs"]:
                    # Make sure our locale list is a subset of all_locales
                    pc[kind].setdefault("configs", []).append(
                        _fix_subpartner_locales(config, all_locales)
                    )
        else:
            for partner, partner_config in kind_config.items():
                for subpartner, subpartner_config in partner_config.items():
                    # get rid of empty subpartner configs
                    if not subpartner_config:
                        continue
                    # Make sure our locale list is a subset of all_locales
                    pc.setdefault(kind, {}).setdefault(partner, {})[
                        subpartner
                    ] = _fix_subpartner_locales(subpartner_config, all_locales)
    return pc


# seems likely this exists elsewhere already
def get_ftp_platform(platform):
    if platform.startswith("win32"):
        return "win32"
    elif platform.startswith("win64-aarch64"):
        return "win64-aarch64"
    elif platform.startswith("win64"):
        return "win64"
    elif platform.startswith("macosx"):
        return "mac"
    elif platform.startswith("linux-"):
        return "linux-i686"
    elif platform.startswith("linux64"):
        return "linux-x86_64"
    else:
        raise ValueError(f"Unimplemented platform {platform}")


# Ugh
def locales_per_build_platform(build_platform, locales):
    if build_platform.startswith("mac"):
        exclude = ["ja"]
    else:
        exclude = ["ja-JP-mac"]
    return [locale for locale in locales if locale not in exclude]


def get_partner_url_config(parameters, graph_config):
    partner_url_config = deepcopy(graph_config["partner-urls"])
    substitutions = {
        "release-product": parameters["release_product"],
        "release-level": release_level(parameters["project"]),
        "release-type": parameters["release_type"],
    }
    resolve_keyed_by(
        partner_url_config,
        "release-eme-free-repack",
        "eme-free manifest_url",
        **substitutions,
    )
    resolve_keyed_by(
        partner_url_config,
        "release-partner-repack",
        "partner manifest url",
        **substitutions,
    )
    resolve_keyed_by(
        partner_url_config,
        "release-partner-attribution",
        "partner attribution url",
        **substitutions,
    )
    return partner_url_config


def get_repack_ids_by_platform(config, build_platform):
    partner_config = get_partner_config_by_kind(config, config.kind)
    combinations = []
    for partner, subconfigs in partner_config.items():
        for sub_config_name, sub_config in subconfigs.items():
            if build_platform not in sub_config.get("platforms", []):
                continue
            locales = locales_per_build_platform(
                build_platform, sub_config.get("locales", [])
            )
            for locale in locales:
                combinations.append(f"{partner}/{sub_config_name}/{locale}")
    return sorted(combinations)


def get_partners_to_be_published(config):
    # hardcoded kind because release-bouncer-aliases doesn't match otherwise
    partner_config = get_partner_config_by_kind(config, "release-partner-repack")
    partners = []
    for partner, subconfigs in partner_config.items():
        for sub_config_name, sub_config in subconfigs.items():
            if sub_config.get("publish_to_releases"):
                partners.append((partner, sub_config_name, sub_config["platforms"]))
    return partners


def apply_partner_priority(config, jobs):
    priority = None
    # Reduce the priority of the partner repack jobs because they don't block QE. Meanwhile
    # leave EME-free jobs alone because they do, and they'll get the branch priority like the rest
    # of the release. Only bother with this in production, not on staging releases on try.
    # medium is the same as mozilla-central, see taskcluster/ci/config.yml. ie higher than
    # integration branches because we don't want to wait a lot for the graph to be done, but
    # for multiple releases the partner tasks always wait for non-partner.
    if (
        config.kind.startswith(
            ("release-partner-repack", "release-partner-attribution")
        )
        and release_level(config.params["project"]) == "production"
    ):
        priority = "medium"
    for job in jobs:
        if priority:
            job["priority"] = priority
        yield job


def generate_attribution_code(defaults, partner):
    params = {
        "medium": defaults["medium"],
        "source": defaults["source"],
        "campaign": partner["campaign"],
        "content": partner["content"],
    }
    if partner.get("variation"):
        params["variation"] = partner["variation"]
    if partner.get("experiment"):
        params["experiment"] = partner["experiment"]

    code = urlencode(params)
    return code
