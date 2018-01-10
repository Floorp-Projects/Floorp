# This software may be used and distributed according to the terms of the
# GNU General Public License version 2 or any later version.

OBSOLETE = """
ERROR: the eslintvalidate hook is obsolete. This commit went
through, but ESlint didn't run. You can lint your changes
after the fact by running:

    $ mach lint --outgoing

Please remove this hook and upgrade by following these
instructions:
https://firefox-source-docs.mozilla.org/tools/lint/usage.html#using-a-vcs-hook
""".lstrip()


def eslinthook(ui, repo, node=None, **opts):
    ui.warn(OBSOLETE)
    return False


def reposetup(ui, repo):
    ui.setconfig('hooks', 'commit.eslint', eslinthook)
