# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

"""module for http authentication operations"""
import getpass
import os

CREDENTIALS_PATH = os.path.expanduser("~/.mozilla/credentials.cfg")
DIRNAME = os.path.dirname(CREDENTIALS_PATH)
LDAP_PASSWORD = None

def get_credentials():
    """ Returns http credentials.

    The user's email address is stored on disk (for convenience in the future)
    while the password is requested from the user on first invocation.
    """
    global LDAP_PASSWORD
    if not os.path.exists(DIRNAME):
        os.makedirs(DIRNAME)

    if os.path.isfile(CREDENTIALS_PATH):
        with open(CREDENTIALS_PATH, 'r') as file_handler:
            content = file_handler.read().splitlines()

        https_username = content[0].strip()

        if len(content) > 1:
            # We want to remove files which contain the password
            os.remove(CREDENTIALS_PATH)
    else:
        https_username = \
                raw_input("Please enter your full LDAP email address: ")

        with open(CREDENTIALS_PATH, "w+") as file_handler:
            file_handler.write("%s\n" % https_username)

        os.chmod(CREDENTIALS_PATH, 0600)

    if not LDAP_PASSWORD:
        print "Please enter your LDAP password (we won't store it):"
        LDAP_PASSWORD = getpass.getpass()

    return https_username, LDAP_PASSWORD

def get_credentials_path():
    if os.path.isfile(CREDENTIALS_PATH):
        get_credentials()

    return CREDENTIALS_PATH
