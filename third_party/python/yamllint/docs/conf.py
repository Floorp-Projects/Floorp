# -*- coding: utf-8 -*-
# yamllint documentation build configuration file, created by
# sphinx-quickstart on Thu Jan 21 21:18:52 2016.

import sys
import os
from unittest.mock import MagicMock

sys.path.insert(0, os.path.abspath('..'))  # noqa

from yamllint import __copyright__, APP_NAME, APP_VERSION

# -- General configuration ------------------------------------------------

extensions = [
    'sphinx.ext.autodoc',
]

source_suffix = '.rst'

master_doc = 'index'

project = APP_NAME
copyright = __copyright__

version = APP_VERSION
release = APP_VERSION

pygments_style = 'sphinx'

# -- Options for HTML output ----------------------------------------------

html_theme = 'default'

htmlhelp_basename = 'yamllintdoc'

# -- Options for manual page output ---------------------------------------

# One entry per manual page. List of tuples
# (source start file, name, description, authors, manual section).
man_pages = [
    ('index', 'yamllint', '', [u'Adrien Vergé'], 1)
]

# -- Build with sphinx automodule without needing to install third-party libs


class Mock(MagicMock):
    @classmethod
    def __getattr__(cls, name):
        return MagicMock()


MOCK_MODULES = ['pathspec', 'yaml']
sys.modules.update((mod_name, Mock()) for mod_name in MOCK_MODULES)
