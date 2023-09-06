# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from geckoprocesstypes import process_types


def process_name(string_name):
    if string_name == "default":
        string_name = "main"
    if string_name == "tab":
        string_name = "content"
    return string_name


def gen_process_map():
    kIdentifier = "/* SUBST: CRASH_MANAGER_PROCESS_MAP */"
    crashManagerMap = """
  processTypes: {"""

    for p in process_types:
        crashManagerMap += """
    // A crash in the %(procname)s process.
    %(proctype)d: "%(procname)s",""" % {
            "proctype": p.enum_value,
            "procname": process_name(p.string_name),
        }
    crashManagerMap += """
  },"""

    return (kIdentifier, crashManagerMap)


def gen_process_pings():
    kIdentifier = "/* SUBST: CRASH_MANAGER_PROCESS_PINGS */"
    crashManagerPing = ""

    for p in process_types:
        crashManagerPing += """
      "%(proctype)s": %(crashping)s,""" % {
            "proctype": process_name(p.string_name),
            "crashping": "true" if p.crash_ping else "false",
        }

    return (kIdentifier, crashManagerPing)


def main(o, crashManager):
    subst = [gen_process_map(), gen_process_pings()]
    with open(crashManager, "r") as src:
        for l in src.readlines():
            for id, value in subst:
                if id in l:
                    l = l.replace(id, value)
            o.write(l)
