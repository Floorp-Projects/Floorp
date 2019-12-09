from __future__ import absolute_import, unicode_literals

import os
import re
import subprocess
import sys
from pathlib import Path

from virtualenv import __version__

extensions = ["sphinx.ext.autodoc", "sphinx.ext.extlinks"]
source_suffix = ".rst"
master_doc = "index"
project = "virtualenv"
copyright = "2007-2018, Ian Bicking, The Open Planning Project, PyPA"

ROOT_SRC_TREE_DIR = Path(__file__).parents[1]


def generate_draft_news():
    home = "https://github.com"
    issue = "{}/issue".format(home)
    fragments_path = ROOT_SRC_TREE_DIR / "docs" / "changelog"
    for pattern, replacement in (
        (r"[^`]@([^,\s]+)", r"`@\1 <{}/\1>`_".format(home)),
        (r"[^`]#([\d]+)", r"`#pr\1 <{}/\1>`_".format(issue)),
    ):
        for path in fragments_path.glob("*.rst"):
            path.write_text(re.sub(pattern, replacement, path.read_text()))
    env = os.environ.copy()
    env["PATH"] += os.pathsep.join([os.path.dirname(sys.executable)] + env["PATH"].split(os.pathsep))
    changelog = subprocess.check_output(
        ["towncrier", "--draft", "--version", "DRAFT"], cwd=str(ROOT_SRC_TREE_DIR), env=env
    ).decode("utf-8")
    if "No significant changes" in changelog:
        content = ""
    else:
        note = "*Changes in master, but not released yet are under the draft section*."
        content = "{}\n\n{}".format(note, changelog)
    (ROOT_SRC_TREE_DIR / "docs" / "_draft.rst").write_text(content)


generate_draft_news()

version = ".".join(__version__.split(".")[:2])
release = __version__

today_fmt = "%B %d, %Y"
unused_docs = []
pygments_style = "sphinx"
exclude_patterns = ["changelog/*"]

extlinks = {
    "issue": ("https://github.com/pypa/virtualenv/issues/%s", "#"),
    "pull": ("https://github.com/pypa/virtualenv/pull/%s", "PR #"),
}

html_theme = "sphinx_rtd_theme"
html_theme_options = {
    "canonical_url": "https://virtualenv.pypa.io/en/latest/",
    "logo_only": False,
    "display_version": True,
    "prev_next_buttons_location": "bottom",
    "style_external_links": True,
    # Toc options
    "collapse_navigation": True,
    "sticky_navigation": True,
    "navigation_depth": 4,
    "includehidden": True,
    "titles_only": False,
}
html_last_updated_fmt = "%b %d, %Y"
htmlhelp_basename = "Pastedoc"
