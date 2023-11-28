#!/usr/bin/env python3
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Uploads the bergamot wasm file to Remote Settings. This just uploads the build artifact,
approval and deployment will still need to be done through Remote Settings. You must
run ./build-bergamot.py first to generate the wasm artifact.

Log in to Remote Settings via LDAP to either dev or prod:

    Dev: https://remote-settings-dev.allizom.org/v1/admin
    Prod: https://remote-settings.mozilla.org/v1/admin

In the header click the little clipboard icon to get the authentication header.
Set the BEARER_TOKEN environment variable to use the bearer token. In zsh this can
be done privately via the command line with the `setopt HIST_IGNORE_SPACE` and
adding a space in front of the command, e.g.

` export BEARER_TOKEN="Bearer uLdb-Yafefe....2Hyl5_w"`
"""

import argparse
import asyncio
import json
import os
import pprint
import sys
import uuid
from collections import namedtuple
from textwrap import dedent

import requests
import yaml

# When running upload-bergamot.py, this number should be bumped for new uploads.
# A minor version bump means that there is no breaking change. A major version
# bump means that the upload is a breaking change. Firefox will only download
# records that match the TranslationsParent.BERGAMOT_MAJOR_VERSION.
REMOTE_SETTINGS_VERSION = "1.1"

COLLECTION_NAME = "translations-wasm"
DEV_SERVER = "https://remote-settings-dev.allizom.org/v1"
PROD_SERVER = "https://remote-settings.mozilla.org/v1"

DIR_PATH = os.path.realpath(os.path.dirname(__file__))
MOZ_YAML_PATH = os.path.join(DIR_PATH, "moz.yaml")
THIRD_PARTY_PATH = os.path.join(DIR_PATH, "thirdparty")
BUILD_PATH = os.path.join(THIRD_PARTY_PATH, "build-wasm")
WASM_PATH = os.path.join(BUILD_PATH, "bergamot-translator-worker.wasm")
ROOT_PATH = os.path.join(DIR_PATH, "../../../..")
BROWSER_VERSION_PATH = os.path.join(ROOT_PATH, "browser/config/version.txt")
RECORDS_PATH = "/admin/#/buckets/main-workspace/collections/translations-wasm/records"

parser = argparse.ArgumentParser(
    description=__doc__,
    # Preserves whitespace in the help text.
    formatter_class=argparse.RawTextHelpFormatter,
)
parser.add_argument("--server", help='Set to either "dev" or "prod"')
parser.add_argument(
    "--dry_run", action="store_true", help="Verify the login, but do not upload"
)
ArgNamespace = namedtuple("ArgNamespace", ["server", "dry_run"])

pp = pprint.PrettyPrinter(indent=2)


def print_error(message):
    """This is a simple util function."""
    red = "\033[91m"
    reset = "\033[0m"
    print(f"{red}Error:{reset} {message}\n", file=sys.stderr)


class RemoteSettings:
    """
    After validating the arguments, this class controls the REST operations for
    communicating with Remote Settings.
    """

    def __init__(self, server: str, bearer_token: str):
        self.server: str = server
        self.bearer_token: str = bearer_token

        with open(MOZ_YAML_PATH, "r", encoding="utf8") as file:
            moz_yaml_text = file.read()
        self.moz_yaml = yaml.safe_load(moz_yaml_text)

        self.version: str = REMOTE_SETTINGS_VERSION
        if not isinstance(self.version, str):
            print_error("The bergamot remote settings version must be a string.")
            sys.exit(1)

    async def fetch_json(self, path: str):
        """Perfrom a simple GET operation and return the JSON results"""
        url = self.server + path
        response = requests.get(url, headers=self.get_headers())

        if not response.ok:
            print_error(f"❌ Failed fetching {url}\nStatus: {response.status_code}")
            try:
                print(response.json())
            except Exception:
                print_error("Unable to read response")
            raise Exception()

        return response.json()

    async def verify_login(self):
        """Before performing any operations, verify the login credentials."""
        try:
            json = await self.fetch_json("/")
        except Exception:
            print_error("Your login information could not be verified")
            parser.print_help(sys.stderr)
            sys.exit(0)

        if "user" not in json:
            print_error("Your bearer token has expired or is not valid.")
            parser.print_help(sys.stderr)
            sys.exit(1)

        print(
            f"✅ Authorized to use {self.server} as user {json['user']['profile']['email']}"
        )

    def get_headers(self):
        return {"Authorization": self.bearer_token}

    async def verify_record_version(self):
        try:
            main_records = await self.fetch_json(
                f"/buckets/main/collections/{COLLECTION_NAME}/records",
            )
        except Exception:
            print_error("Failed to get the main records")
            sys.exit(1)

        for record in main_records["data"]:
            if (
                record["name"] == "bergamot-translator"
                and record["version"] == self.version
            ):
                print("Conflicting record in Remote Settings:", record)
                print_error(
                    dedent(
                        f"""
                    The version {self.version} already existed in the published records.
                    You need to bump the major or minor version number in the moz.yaml file.
                """
                    )
                )
                sys.exit(1)

        try:
            workspace_records = await self.fetch_json(
                f"/buckets/main-workspace/collections/{COLLECTION_NAME}/records",
            )
        except Exception:
            print_error("Failed to get the workspace records")
            sys.exit(1)

        for record in workspace_records["data"]:
            if (
                record["name"] == "bergamot-translator"
                and record["version"] == self.version
            ):
                print("Conflicting record in Remote Settings:", record)
                print_error(
                    dedent(
                        f"""
                    The version {self.version} already existed in the workspace records.
                    You need to delete the file in the workspace before uploading again.

                    {self.server + RECORDS_PATH}
                """
                    )
                )
                sys.exit(1)

        print("📦 Packages in the workspace:")
        for record in workspace_records["data"]:
            if record["name"] == "bergamot-translator":
                print(f'   - bergamot-translator@{record["version"]}')

        print(f"✅ Version {self.version} does not conflict, ready for uploading.")

    def create_record(self):
        name = self.moz_yaml["origin"]["name"]
        release = self.moz_yaml["origin"]["release"]
        revision = self.moz_yaml["origin"]["revision"]
        license = self.moz_yaml["origin"]["license"]
        version = REMOTE_SETTINGS_VERSION

        if not name or not release or not revision or not license or not version:
            print_error("Some of the required record data is not in the moz.yaml file.")
            sys.exit(1)

        with open(BROWSER_VERSION_PATH, "r", encoding="utf8") as file:
            fx_release = file.read().strip()

        files = [
            (
                "attachment",
                (
                    os.path.basename(WASM_PATH),  # filename
                    open(WASM_PATH, "rb"),  # file handle
                    "application/wasm",  # mimetype
                ),
            )
        ]

        data = {
            "name": name,
            "release": release,
            "revision": revision,
            "license": license,
            "version": version,
            "fx_release": fx_release,
            # Default to nightly and local builds.
            "filter_expression": "env.channel == 'nightly' || env.channel == 'default'",
        }

        print("📬 Posting record")
        print("✉️ Attachment: ", WASM_PATH)
        print("📀 Record: ", end="")
        pp.pprint(data)

        return files, data

    def upload_record(self, files, data):
        id = str(uuid.uuid4())
        url = f"{self.server}/buckets/main-workspace/collections/{COLLECTION_NAME}/records/{id}/attachment"

        print(f"\n⬆️ POSTing the record to: {url}\n")
        response = requests.post(
            url,
            headers=self.get_headers(),
            data={"data": json.dumps(data)},
            files=files,
        )

        if response.status_code >= 200 and response.status_code < 300:
            print("✅ Record created:", self.server + RECORDS_PATH)
            print("✉️ Attachment details: ", end="")
            pp.pprint(json.loads(response.text))
        else:
            print_error(
                f"Error creating record: (Error code {response.status_code})\n{response.text}"
            )
            raise Exception()


async def main():
    if len(sys.argv) == 1:
        parser.print_help(sys.stderr)
        sys.exit(1)

    args: ArgNamespace = parser.parse_args()

    bearer_token = os.environ.get("BEARER_TOKEN")
    if not bearer_token:
        print_error('A "BEARER_TOKEN" environment variable must be set.')
        parser.print_help(sys.stderr)
        sys.exit(1)

    if args.server == "prod":
        server = PROD_SERVER
    elif args.server == "dev":
        server = DEV_SERVER
    else:
        print_error('The server must either be "prod" or "dev"')
        parser.print_help(sys.stderr)
        sys.exit(1)

    remote_settings = RemoteSettings(server, bearer_token)

    await remote_settings.verify_login()
    await remote_settings.verify_record_version()

    files, data = remote_settings.create_record()
    if args.dry_run:
        return

    remote_settings.upload_record(files, data)


if __name__ == "__main__":
    loop = asyncio.get_event_loop()
    loop.run_until_complete(main())
