# coding: utf-8
#!/usr/bin/env python
import pkg_resources


extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.intersphinx'
]

templates_path = ['_templates']
source_suffix = '.rst'
master_doc = 'index'
project = 'cbor2'
author = u'Alex Grönholm'
copyright = u'2016, ' + author

v = pkg_resources.get_distribution(project).parsed_version
version = v.base_version
release = v.public

language = None

exclude_patterns = ['_build']
pygments_style = 'sphinx'
highlight_language = 'python'
todo_include_todos = False

html_theme = 'sphinx_rtd_theme'
html_static_path = ['_static']
htmlhelp_basename = project.replace('-', '') + 'doc'

intersphinx_mapping = {'python': ('http://docs.python.org/', None)}
