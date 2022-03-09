#!/usr/bin/env python3

import re

RE_DEFAULT = re.compile(r"\[D=([TF])\] ")
RE_TRANSITION = re.compile(r"([a-zA-Z0-9 \[\]=#_-]+) *->(.*)")
RE_ASSERTION = re.compile(
    r"\[A S=([a-zA-Z01_]+) SS=([a-zA-Z01_]+) ES=([a-zA-Z01_]+) P=([a-zA-Z_]+) ESP=([a-zA-Z_]+)\]"
)
RE_ASSERTION_SHORTHAND = re.compile(r"\[A#([0-9T]+)\]")

# ======================================================================
# ======================================================================

testnum = 1


def start_test(line):
    global testnum
    output.write(
        """
    def test_{0}(self):
        # {1}...\n""".format(
            testnum, line[0:80]
        )
    )
    testnum += 1


def set_default(d):
    output.write(
        """
        if self.default_is is not {0}:
            return\n""".format(
            "True" if d == "T" else "False"
        )
    )


def enroll(e):
    if e.endswith("-C"):
        e = e[:-2]
        output.write("\n        # Re-set enrollment pref, like Normandy would do\n")
    output.write(
        "        self.set_enrollment_status(ExperimentStatus.ENROLLED_{0})\n".format(
            e.upper()
        )
    )


def set_pref(enabled):
    output.write(
        "\n        self.marionette.set_pref(Prefs.WIN32K, {0})\n".format(str(enabled))
    )


def set_e10s(enable):
    if enable:
        output.write(
            """
        app_version = self.execute_script("return Services.appinfo.version")
        self.restart(env={ENV_DISABLE_E10S: app_version})
        self.set_env(ENV_DISABLE_E10S, "null")\n"""
        )
    else:
        raise Exception("Not implemented")


def set_header(enable):
    if enable:
        output.write(
            """
        self.restart(env={ENV_DISABLE_WIN32K: "1"})\n"""
        )
    else:
        output.write(
            """
        self.set_env(ENV_DISABLE_WIN32K, "")\n"""
        )


def set_bad_requirements(enabled):
    output.write(
        """
        self.marionette.set_pref(Prefs.WEBGL, {0})\n""".format(
            "False" if enabled else "True"
        )
    )


def restart():
    output.write("\n        self.restart()\n")


def print_assertion(assertion):
    if not assertion:
        return
    output.write(
        """
        self.check_win32k_status(
            status=ContentWin32kLockdownState.{0},
            sessionStatus=ContentWin32kLockdownState.{1},
            experimentStatus=ExperimentStatus.{2},
            pref={3},
            enrollmentStatusPref=ExperimentStatus.{4},
        )\n""".format(
            *assertion
        )
    )


# ======================================================================
# ======================================================================

TESTS = open("win32k_tests.txt", "r").readlines()

output = open("test_win32k_enrollment.py", "w", newline="\n")
header = open("test_win32k_enrollment.template.py", "r")
for l in header:
    output.write(l)

mappings = {}
for line in TESTS:
    line = line.strip()
    if not line:
        continue

    if line.startswith("#"):
        continue
    elif line.startswith("> "):
        line = line[2:]
        key, value = line.split(":")
        mappings[key.strip()] = value.strip()
        continue
    elif line.startswith("-"):
        line = line[1:].strip()
    elif line.startswith("+"):
        line = line[1:].strip()
        import pdb

        pdb.set_trace()
    else:
        raise Exception("unknown line type: " + line)

    # We can't handle Safe Mode right now
    if "Safe Mode" in line:
        continue

    # If we have no assertions defined, skip the test entirely
    if "[A" not in line:
        continue

    if not RE_DEFAULT.match(line):
        raise Exception("'{0}' does not match the default regex".format(line))
    default = RE_DEFAULT.search(line).groups(1)[0]

    start_test(line)

    set_default(default)

    line = line[6:]

    while line:
        # this is a horrible hack for the line ending to avoid making the
        # regex more complicated and having to fix it
        if not line.endswith(" ->"):
            line += " ->"

        groups = RE_TRANSITION.search(line).groups()
        start = groups[0].strip()
        end = groups[1].strip()

        if RE_ASSERTION.search(start):
            assertion = RE_ASSERTION.search(start).groups()
            start = start[0 : start.index("[")].strip()
        elif RE_ASSERTION_SHORTHAND.search(start):
            key = RE_ASSERTION_SHORTHAND.search(start).groups()[0]
            assertion = RE_ASSERTION.search(mappings[key]).groups()
            start = start[0 : start.index("[")].strip()
        else:
            assertion = ""

        if start == "Nothing":
            pass
        elif start.startswith("Enrolled "):
            enroll(start[9:])
        elif start == "E10S":
            set_e10s(True)
        elif start == "Header-On":
            set_header(True)
        elif start == "Header-Off":
            set_header(False)
        elif start == "Bad Requirements":
            set_bad_requirements(True)
        elif start == "Restart":
            restart()
        elif start == "On":
            set_pref(True)
        elif start == "Off":
            set_pref(False)
        else:
            raise Exception("Unknown Action: " + start)

        print_assertion(assertion)

        line = end.strip()

    if RE_ASSERTION.match(line):
        print_assertion(RE_ASSERTION.search(line).groups())
    elif RE_ASSERTION_SHORTHAND.search(line):
        key = RE_ASSERTION_SHORTHAND.search(line).groups()[0]
        print_assertion(RE_ASSERTION.search(mappings[key]).groups())
