:mod:`mozinstall` --- Install and uninstall Gecko-based applications
====================================================================

mozinstall is a small python module with several convenience methods
useful for installing and uninstalling a gecko-based application
(e.g. Firefox) on the desktop.

Simple example
--------------

::

    import mozinstall
    import tempfile

    tempdir = tempfile.mkdtemp()
    firefox_dmg = 'firefox-38.0a1.en-US.mac.dmg'
    install_folder = mozinstall.install(src=firefox_dmg, dest=tempdir)
    binary = mozinstall.get_binary(install_folder, 'Firefox')
    # from here you can execute the binary directly
    # ...
    mozinstall.uninstall(install_folder)

API Documentation
-----------------

.. automodule:: mozinstall
   :members: is_installer, install, get_binary, uninstall,
             InstallError, InvalidBinary, InvalidSource
