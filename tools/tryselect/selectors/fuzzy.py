# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os
import platform
import subprocess
import shutil
import six
import sys
from distutils.spawn import find_executable
from distutils.version import StrictVersion

from mozbuild.base import MozbuildObject
from mach.util import get_state_dir
from mozterm import Terminal

import mozfile
from mozboot.util import http_download_and_save

from ..cli import BaseTryParser
from ..tasks import generate_tasks, filter_tasks_by_paths
from ..push import check_working_directory, push_to_try, generate_try_task_config
from ..util.manage_estimates import (
    download_task_history_data,
    make_trimmed_taskgraph_cache,
)

from gecko_taskgraph.target_tasks import filter_by_uncommon_try_tasks

terminal = Terminal()

here = os.path.abspath(os.path.dirname(__file__))
build = MozbuildObject.from_environment(cwd=here)

PREVIEW_SCRIPT = os.path.join(build.topsrcdir, "tools/tryselect/selectors/preview.py")

FZF_MIN_VERSION = "0.20.0"
FZF_CURRENT_VERSION = "0.29.0"

# It would make more sense to have the full filename be the key; but that makes
# the line too long and ./mach lint and black can't agree about what to about that.
# You can get these from the github release, e.g.
#          https://github.com/junegunn/fzf/releases/download/0.24.1/fzf_0.24.1_checksums.txt
# However the darwin releases may not be included, so double check you have everything
FZF_CHECKSUMS = {
    "linux_armv5.tar.gz": "61d3c2aa77b977ba694836fd1134da9272bd97ee490ececaf87959b985820111",
    "linux_armv6.tar.gz": "db6b30fcbbd99ac4cf7e3ff6c5db1d3c0afcbe37d10ec3961bdc43e8c4f2e4f9",
    "linux_armv7.tar.gz": "ed86f0e91e41d2cea7960a78e3eb175dc2a5fc1510380c195d0c3559bfdc701c",
    "linux_arm64.tar.gz": "47988d8b68905541cbc26587db3ed1cfa8bc3aa8da535120abb4229b988f259e",
    "linux_amd64.tar.gz": "0106f458b933be65edb0e8f0edb9a16291a79167836fd26a77ff5496269b5e9a",
    "windows_armv5.zip": "08eaac45b3600d82608d292c23e7312696e7e11b6278b292feba25e8eb91c712",
    "windows_armv6.zip": "8b6618726a9d591a45120fddebc29f4164e01ce6639ed9aa8fc79ab03eefcfed",
    "windows_armv7.zip": "c167117b4c08f4f098446291115871ce5f14a8a8b22f0ca70e1b4342452ab5d7",
    "windows_arm64.zip": "0cda7bf68850a3e867224a05949612405e63a4421d52396c1a6c9427d4304d72",
    "windows_amd64.zip": "f0797ceee089017108c80b09086c71b8eec43d4af11ce939b78b1d5cfd202540",
    "darwin_arm64.zip": "2571b4d381f1fc691e7603bbc8113a67116da2404751ebb844818d512dd62b4b",
    "darwin_amd64.zip": "bc541e8ae0feb94efa96424bfe0b944f746db04e22f5cccfe00709925839a57f",
    "openbsd_amd64.tar.gz": "b62343827ff83949c09d5e2c8ca0c1198d05f733c9a779ec37edd840541ccdab",
    "freebsd_amd64.tar.gz": "f0367f2321c070d103589c7c7eb6a771bc7520820337a6c2fbb75be37ff783a9",
}

FZF_INSTALL_MANUALLY = """
The `mach try fuzzy` command depends on fzf. Please install it following the
appropriate instructions for your platform:

    https://github.com/junegunn/fzf#installation

Only the binary is required, if you do not wish to install the shell and
editor integrations, download the appropriate binary and put it on your $PATH:

    https://github.com/junegunn/fzf/releases
""".lstrip()

FZF_COULD_NOT_DETERMINE_PLATFORM = (
    """
Could not automatically obtain the `fzf` binary because we could not determine
your Operating System.

""".lstrip()
    + FZF_INSTALL_MANUALLY
)

FZF_COULD_NOT_DETERMINE_MACHINE = (
    """
Could not automatically obtain the `fzf` binary because we could not determine
your machine type.  It's reported as '%s' and we don't handle that case; but fzf
may still be available as a prebuilt binary.

""".lstrip()
    + FZF_INSTALL_MANUALLY
)

FZF_NOT_SUPPORTED_X86 = (
    """
We don't believe that a prebuilt binary for `fzf` if available on %s, but we
could be wrong.

""".lstrip()
    + FZF_INSTALL_MANUALLY
)

FZF_NOT_FOUND = (
    """
Could not find the `fzf` binary.

""".lstrip()
    + FZF_INSTALL_MANUALLY
)

FZF_VERSION_FAILED = (
    """
Could not obtain the 'fzf' version; we require version > 0.20.0 for some of
the features.

""".lstrip()
    + FZF_INSTALL_MANUALLY
)

FZF_INSTALL_FAILED = (
    """
Failed to install fzf.

""".lstrip()
    + FZF_INSTALL_MANUALLY
)

FZF_HEADER = """
For more shortcuts, see {t.italic_white}mach help try fuzzy{t.normal} and {t.italic_white}man fzf
{shortcuts}
""".strip()

fzf_shortcuts = {
    "ctrl-a": "select-all",
    "ctrl-d": "deselect-all",
    "ctrl-t": "toggle-all",
    "alt-bspace": "beginning-of-line+kill-line",
    "?": "toggle-preview",
}

fzf_header_shortcuts = [
    ("select", "tab"),
    ("accept", "enter"),
    ("cancel", "ctrl-c"),
    ("select-all", "ctrl-a"),
    ("cursor-up", "up"),
    ("cursor-down", "down"),
]


class FuzzyParser(BaseTryParser):
    name = "fuzzy"
    arguments = [
        [
            ["-q", "--query"],
            {
                "metavar": "STR",
                "action": "append",
                "default": [],
                "help": "Use the given query instead of entering the selection "
                "interface. Equivalent to typing <query><ctrl-a><enter> "
                "from the interface. Specifying multiple times schedules "
                "the union of computed tasks.",
            },
        ],
        [
            ["-i", "--interactive"],
            {
                "action": "store_true",
                "default": False,
                "help": "Force running fzf interactively even when using presets or "
                "queries with -q/--query.",
            },
        ],
        [
            ["-x", "--and"],
            {
                "dest": "intersection",
                "action": "store_true",
                "default": False,
                "help": "When specifying queries on the command line with -q/--query, "
                "use the intersection of tasks rather than the union. This is "
                "especially useful for post filtering presets.",
            },
        ],
        [
            ["-e", "--exact"],
            {
                "action": "store_true",
                "default": False,
                "help": "Enable exact match mode. Terms will use an exact match "
                "by default, and terms prefixed with ' will become fuzzy.",
            },
        ],
        [
            ["-u", "--update"],
            {
                "action": "store_true",
                "default": False,
                "help": "Update fzf before running.",
            },
        ],
        [
            ["-s", "--show-estimates"],
            {
                "action": "store_true",
                "default": False,
                "help": "Show task duration estimates.",
            },
        ],
        [
            ["--disable-target-task-filter"],
            {
                "action": "store_true",
                "default": False,
                "help": "Some tasks run on mozilla-central but are filtered out "
                "of the default list due to resource constraints. This flag "
                "disables this filtering.",
            },
        ],
    ]
    common_groups = ["push", "task", "preset"]
    task_configs = [
        "artifact",
        "browsertime",
        "chemspill-prio",
        "disable-pgo",
        "env",
        "gecko-profile",
        "path",
        "pernosco",
        "rebuild",
        "routes",
        "worker-overrides",
    ]


def run_cmd(cmd, cwd=None):
    is_win = platform.system() == "Windows"
    return subprocess.call(cmd, cwd=cwd, shell=True if is_win else False)


def get_fzf_platform():
    if platform.machine() in ["i386", "i686"]:
        print(FZF_NOT_SUPPORTED_X86 % platform.machine())
        sys.exit(1)

    if platform.system().lower() == "windows":
        if platform.machine().lower() in ["x86_64", "amd64"]:
            return "windows_amd64.zip"
        elif platform.machine().lower() == "arm64":
            return "windows_arm64.zip"
        else:
            print(FZF_COULD_NOT_DETERMINE_MACHINE % platform.machine())
            sys.exit(1)
    elif platform.system().lower() == "darwin":
        if platform.machine().lower() in ["x86_64", "amd64"]:
            return "darwin_amd64.zip"
        elif platform.machine().lower() == "arm64":
            return "darwin_arm64.zip"
        else:
            print(FZF_COULD_NOT_DETERMINE_MACHINE % platform.machine())
            sys.exit(1)
    elif platform.system().lower() == "linux":
        if platform.machine().lower() in ["x86_64", "amd64"]:
            return "linux_amd64.tar.gz"
        elif platform.machine().lower() == "arm64":
            return "linux_arm64.tar.gz"
        else:
            print(FZF_COULD_NOT_DETERMINE_MACHINE % platform.machine())
            sys.exit(1)
    else:
        print(FZF_COULD_NOT_DETERMINE_PLATFORM)
        sys.exit(1)


def get_fzf_state_dir():
    return os.path.join(get_state_dir(), "fzf")


def get_fzf_filename():
    return "fzf-%s-%s" % (FZF_CURRENT_VERSION, get_fzf_platform())


def get_fzf_download_link():
    return "https://github.com/junegunn/fzf/releases/download/%s/%s" % (
        FZF_CURRENT_VERSION,
        get_fzf_filename(),
    )


def clean_up_state_dir():
    """
    We used to have a checkout of fzf that we would update.
    Now we only download the bin and cpin the hash; so if
    we find the old git checkout, wipe it
    """

    fzf_path = os.path.join(get_state_dir(), "fzf")
    git_path = os.path.join(fzf_path, ".git")
    if os.path.isdir(git_path):
        shutil.rmtree(fzf_path, ignore_errors=True)

    # Also delete any existing fzf binary
    fzf_bin = find_executable("fzf", fzf_path)
    if fzf_bin:
        mozfile.remove(fzf_bin)

    # Make sure the state dir is present
    if not os.path.isdir(fzf_path):
        os.makedirs(fzf_path)


def download_and_install_fzf():
    clean_up_state_dir()
    download_link = get_fzf_download_link()
    download_file = get_fzf_filename()
    download_destination_path = get_fzf_state_dir()
    download_destination_file = os.path.join(download_destination_path, download_file)
    http_download_and_save(
        download_link, download_destination_file, FZF_CHECKSUMS[get_fzf_platform()]
    )

    mozfile.extract(download_destination_file, download_destination_path)
    mozfile.remove(download_destination_file)


def get_fzf_version(fzf_bin):
    cmd = [fzf_bin, "--version"]
    try:
        fzf_version = subprocess.check_output(cmd)
    except subprocess.CalledProcessError:
        print(FZF_VERSION_FAILED)
        sys.exit(1)

    # Some fzf versions have extra, e.g 0.18.0 (ff95134)
    fzf_version = six.ensure_text(fzf_version.split()[0])

    return fzf_version


def should_force_fzf_update(fzf_bin):
    fzf_version = get_fzf_version(fzf_bin)

    # 0.20.0 introduced passing selections through a temporary file,
    # which is good for large ctrl-a actions.
    if StrictVersion(fzf_version) < StrictVersion(FZF_MIN_VERSION):
        print("fzf version is old, you must update to use ./mach try fuzzy.")
        return True
    return False


def fzf_bootstrap(update=False):
    """
    Bootstrap fzf if necessary and return path to the executable.

    This function is a bit complicated. We fetch a new version of fzf if:
    1) an existing fzf is too outdated
    2) the user says --update and we are behind the recommended version
    3) no fzf can be found and
      3a) user passes --update
      3b) user agrees to a prompt

    """
    fzf_path = get_fzf_state_dir()

    fzf_bin = find_executable("fzf")
    if not fzf_bin:
        fzf_bin = find_executable("fzf", fzf_path)

    if fzf_bin and should_force_fzf_update(fzf_bin):  # Case (1)
        update = True

    if fzf_bin and not update:
        return fzf_bin

    elif fzf_bin and update:
        # Case 2
        fzf_version = get_fzf_version(fzf_bin)
        if StrictVersion(fzf_version) < StrictVersion(FZF_CURRENT_VERSION) and update:
            # Bug 1623197: We only want to run fzf's `install` if it's not in the $PATH
            # Swap to os.path.commonpath when we're not on Py2
            if fzf_bin and update and not fzf_bin.startswith(fzf_path):
                print(
                    "fzf installed somewhere other than {}, please update manually".format(
                        fzf_path
                    )
                )
                sys.exit(1)

            download_and_install_fzf()
            print("Updated fzf to {}".format(FZF_CURRENT_VERSION))
        else:
            print("fzf is the recommended version and does not need an update")

    else:  # not fzf_bin:
        if not update:
            # Case 3b
            install = input("Could not detect fzf, install it now? [y/n]: ")
            if install.lower() != "y":
                return

        # Case 3a and 3b-fall-through
        download_and_install_fzf()
        fzf_bin = find_executable("fzf", fzf_path)
        print("Installed fzf to {}".format(fzf_path))

    return fzf_bin


def format_header():
    shortcuts = []
    for action, key in fzf_header_shortcuts:
        shortcuts.append(
            "{t.white}{action}{t.normal}: {t.yellow}<{key}>{t.normal}".format(
                t=terminal, action=action, key=key
            )
        )
    return FZF_HEADER.format(shortcuts=", ".join(shortcuts), t=terminal)


def run_fzf(cmd, tasks):
    env = dict(os.environ)
    env.update(
        {"PYTHONPATH": os.pathsep.join([p for p in sys.path if "requests" in p])}
    )
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stdin=subprocess.PIPE,
        env=env,
        universal_newlines=True,
    )
    out = proc.communicate("\n".join(tasks))[0].splitlines()

    selected = []
    query = None
    if out:
        query = out[0]
        selected = out[1:]
    return query, selected


def run(
    update=False,
    query=None,
    intersect_query=None,
    try_config=None,
    full=False,
    parameters=None,
    save_query=False,
    stage_changes=False,
    dry_run=False,
    message="{msg}",
    test_paths=None,
    exact=False,
    closed_tree=False,
    show_estimates=False,
    disable_target_task_filter=False,
):
    fzf = fzf_bootstrap(update)

    if not fzf:
        print(FZF_NOT_FOUND)
        return 1

    push = not stage_changes and not dry_run
    check_working_directory(push)
    tg = generate_tasks(
        parameters, full=full, disable_target_task_filter=disable_target_task_filter
    )
    all_tasks = sorted(tg.tasks.keys())

    # graph_Cache created by generate_tasks, recreate the path to that file.
    cache_dir = os.path.join(
        get_state_dir(specific_to_topsrcdir=True), "cache", "taskgraph"
    )
    if full:
        graph_cache = os.path.join(cache_dir, "full_task_graph")
        dep_cache = os.path.join(cache_dir, "full_task_dependencies")
        target_set = os.path.join(cache_dir, "full_task_set")
    else:
        graph_cache = os.path.join(cache_dir, "target_task_graph")
        dep_cache = os.path.join(cache_dir, "target_task_dependencies")
        target_set = os.path.join(cache_dir, "target_task_set")

    if show_estimates:
        download_task_history_data(cache_dir=cache_dir)
        make_trimmed_taskgraph_cache(graph_cache, dep_cache, target_file=target_set)

    if not full and not disable_target_task_filter:
        # Put all_tasks into a list because it's used multiple times, and "filter()"
        # returns a consumable iterator.
        all_tasks = list(filter(filter_by_uncommon_try_tasks, all_tasks))

    if test_paths:
        all_tasks = filter_tasks_by_paths(all_tasks, test_paths)
        if not all_tasks:
            return 1

    key_shortcuts = [k + ":" + v for k, v in fzf_shortcuts.items()]
    base_cmd = [
        fzf,
        "-m",
        "--bind",
        ",".join(key_shortcuts),
        "--header",
        format_header(),
        "--preview-window=right:30%",
        "--print-query",
    ]

    if show_estimates:
        base_cmd.extend(
            [
                "--preview",
                '{} {} -g {} -s -c {} -t "{{+f}}"'.format(
                    sys.executable, PREVIEW_SCRIPT, dep_cache, cache_dir
                ),
            ]
        )
    else:
        base_cmd.extend(
            [
                "--preview",
                '{} {} -t "{{+f}}"'.format(sys.executable, PREVIEW_SCRIPT),
            ]
        )

    if exact:
        base_cmd.append("--exact")

    selected = set()
    queries = []

    def get_tasks(query_arg=None, candidate_tasks=all_tasks):
        cmd = base_cmd[:]
        if query_arg and query_arg != "INTERACTIVE":
            cmd.extend(["-f", query_arg])

        query_str, tasks = run_fzf(cmd, sorted(candidate_tasks))
        queries.append(query_str)
        return set(tasks)

    for q in query or []:
        selected |= get_tasks(q)

    for q in intersect_query or []:
        if not selected:
            tasks = get_tasks(q)
            selected |= tasks
        else:
            tasks = get_tasks(q, selected)
            selected &= tasks

    if not queries:
        selected = get_tasks()

    if not selected:
        print("no tasks selected")
        return

    if save_query:
        return queries

    # build commit message
    msg = "Fuzzy"
    args = ["query={}".format(q) for q in queries]
    if test_paths:
        args.append("paths={}".format(":".join(test_paths)))
    if args:
        msg = "{} {}".format(msg, "&".join(args))
    return push_to_try(
        "fuzzy",
        message.format(msg=msg),
        try_task_config=generate_try_task_config("fuzzy", selected, try_config),
        stage_changes=stage_changes,
        dry_run=dry_run,
        closed_tree=closed_tree,
    )
