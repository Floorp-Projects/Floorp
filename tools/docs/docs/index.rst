Managing Documentation
======================

Documentation is hard. It's difficult to write, difficult to find and always out
of date. That's why we implemented our in-tree documentation system that
underpins firefox-source-docs.mozilla.org. The documentation lives next to the
code that it documents, so it can be updated within the same commit that makes
the underlying changes.

This documentation is generated via the
`Sphinx <http://sphinx-doc.org/>`_ tool from sources in the tree.

To build the documentation, run ``mach doc``. Run
``mach help doc`` to see configurable options.

This is the preferred location for all Firefox development process and
source code documentation.

.. toctree::
  :caption: Documentation
  :maxdepth: 2
  :glob:

  *
