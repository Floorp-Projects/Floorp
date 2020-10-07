# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from rich.console import Console
from rich.logging import RichHandler

import argparse
import logging
import requests

console = Console()
log = logging.getLogger("webdriver-driver")

parser = argparse.ArgumentParser()
subparsers = parser.add_subparsers(help="sub-command help")

parser.add_argument(
    "--verbose", "-v", help="Be more verbose", action="count", default=0
)
parser.add_argument(
    "--url",
    default="http://localhost:8080/webauthn/authenticator",
    help="webdriver url",
)


def device_add(args):
    data = {
        "protocol": args.protocol,
        "transport": args.transport,
        "hasResidentKey": args.residentkey in ["true", "yes"],
        "isUserConsenting": args.consent in ["true", "yes"],
        "hasUserVerification": args.uv in ["available", "verified"],
        "isUserVerified": args.uv in ["verified"],
    }
    console.print("Adding new device: ", data)
    rsp = requests.post(args.url, json=data)
    console.print(rsp)
    console.print(rsp.json())


parser_add = subparsers.add_parser("add", help="Add a device")
parser_add.set_defaults(func=device_add)
parser_add.add_argument(
    "--consent",
    choices=["yes", "no", "true", "false"],
    default="true",
    help="consent automatically",
)
parser_add.add_argument(
    "--residentkey",
    choices=["yes", "no", "true", "false"],
    default="no",
    help="indicate a resident key",
)
parser_add.add_argument(
    "--uv",
    choices=["no", "available", "verified"],
    default="no",
    help="indicate user verification",
)
parser_add.add_argument(
    "--protocol", choices=["ctap1/u2f", "ctap2"], default="ctap1/u2f", help="protocol"
)
parser_add.add_argument("--transport", default="usb", help="transport type(s)")


def device_delete(args):
    rsp = requests.delete(f"{args.url}/{args.id}")
    console.print(rsp)
    console.print(rsp.json())


parser_delete = subparsers.add_parser("delete", help="Delete a device")
parser_delete.set_defaults(func=device_delete)
parser_delete.add_argument("id", type=int, help="device ID to delete")


def device_view(args):
    rsp = requests.get(f"{args.url}/{args.id}")
    console.print(rsp)
    console.print(rsp.json())


parser_view = subparsers.add_parser("view", help="View data about a device")
parser_view.set_defaults(func=device_view)
parser_view.add_argument("id", type=int, help="device ID to view")


def device_update_uv(args):
    data = {"isUserVerified": args.uv in ["verified", "yes"]}
    rsp = requests.post(f"{args.url}/{args.id}/uv", json=data)
    console.print(rsp)
    console.print(rsp.json())


parser_update_uv = subparsers.add_parser(
    "update-uv", help="Update the User Verified setting"
)
parser_update_uv.set_defaults(func=device_update_uv)
parser_update_uv.add_argument("id", type=int, help="device ID to update")
parser_update_uv.add_argument(
    "uv",
    choices=["no", "yes", "verified"],
    default="no",
    help="indicate user verification",
)


def credential_add(args):
    data = {
        "credentialId": args.credentialId,
        "isResidentCredential": args.isResidentCredential in ["true", "yes"],
        "rpId": args.rpId,
        "privateKey": args.privateKey,
        "signCount": args.signCount,
    }
    if args.userHandle:
        data["userHandle"] = (args.userHandle,)

    console.print(f"Adding new credential to device {args.id}: ", data)
    rsp = requests.post(f"{args.url}/{args.id}/credential", json=data)
    console.print(rsp)
    console.print(rsp.json())


parser_credential_add = subparsers.add_parser("addcred", help="Add a credential")
parser_credential_add.set_defaults(func=credential_add)
parser_credential_add.add_argument(
    "--id", required=True, type=int, help="device ID to use"
)
parser_credential_add.add_argument(
    "--credentialId", required=True, help="base64url-encoded credential ID"
)
parser_credential_add.add_argument(
    "--isResidentCredential",
    choices=["yes", "no", "true", "false"],
    default="no",
    help="indicate a resident key",
)
parser_credential_add.add_argument("--rpId", required=True, help="RP id (hostname)")
parser_credential_add.add_argument(
    "--privateKey", required=True, help="base64url-encoded private key per RFC 5958"
)
parser_credential_add.add_argument("--userHandle", help="base64url-encoded user handle")
parser_credential_add.add_argument(
    "--signCount", default=0, type=int, help="initial signature counter"
)


def credentials_get(args):
    rsp = requests.get(f"{args.url}/{args.id}/credentials")
    console.print(rsp)
    console.print(rsp.json())


parser_credentials_get = subparsers.add_parser("getcreds", help="Get credentials")
parser_credentials_get.set_defaults(func=credentials_get)
parser_credentials_get.add_argument("id", type=int, help="device ID to query")


def credential_delete(args):
    rsp = requests.delete(f"{args.url}/{args.id}/credentials/{args.credentialId}")
    console.print(rsp)
    console.print(rsp.json())


parser_credentials_get = subparsers.add_parser("delcred", help="Delete a credential")
parser_credentials_get.set_defaults(func=credential_delete)
parser_credentials_get.add_argument("id", type=int, help="device ID to affect")
parser_credentials_get.add_argument(
    "credentialId", help="base64url-encoded credential ID"
)


def credentials_clear(args):
    rsp = requests.delete(f"{args.url}/{args.id}/credentials")
    console.print(rsp)
    console.print(rsp.json())


parser_credentials_get = subparsers.add_parser(
    "clearcreds", help="Clear all credentials for a device"
)
parser_credentials_get.set_defaults(func=credentials_clear)
parser_credentials_get.add_argument("id", type=int, help="device ID to affect")


def main():
    args = parser.parse_args()

    loglevel = logging.INFO
    if args.verbose > 0:
        loglevel = logging.DEBUG
    logging.basicConfig(
        level=loglevel, format="%(message)s", datefmt="[%X]", handlers=[RichHandler()]
    )

    try:
        args.func(args)
    except requests.exceptions.ConnectionError as ce:
        log.error(f"Connection refused to {args.url}: {ce}")


if __name__ == "__main__":
    main()
