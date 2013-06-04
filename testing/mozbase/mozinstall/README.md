[Mozinstall](https://github.com/mozilla/mozbase/tree/master/mozinstall) is a
python package for installing and uninstalling Mozilla applications on
various platforms.

For example, depending on the platform, Firefox can be distributed as a
zip, tar.bz2, exe, or dmg file or cloned from a repository. Mozinstall takes
the hassle out of extracting and/or running these files and for convenience
returns the full path to the install directory. In the case that mozinstall
is invoked from the command line, the binary path will be printed to stdout.

To remove an installed application the uninstaller can be used. It requires
the installation path of the application and will remove all the installed
files. On Windows the uninstaller will be tried first.

# Usage
Mozinstall can be used as API or via the CLI commands.

## API
An application can be installed by running the commands below. The install
method will return the installation path of the application.

    import mozinstall
    path = mozinstall.install(%installer%, %install_folder%)

To retrieve the real binary call get_binary with the path and
the application name as arguments:

    mozinstall.get_binary(path, 'firefox')

If the application is not needed anymore the uninstaller will remove all
traces from the system:

    mozinstall.uninstall(path)

## CLI
The installer can also be used as a command line tool:

    $ mozinstall -d firefox %installer%

Whereby the directory option is optional and will default to the current
working directory. If the installation was successful the path to the
binary will be printed to stdout.

Also the uninstaller can be called via the command line:

    $ mozuninstall %install_path%

# Error Handling

Mozinstall throws different types of exceptions:

- mozinstall.InstallError is thrown when the installation fails for any reason. A traceback is provided.
- mozinstall.InvalidBinary is thrown when the binary cannot be found.
- mozinstall.InvalidSource is thrown when the source is not a recognized file type (zip, exe, tar.bz2, tar.gz, dmg).


# Dependencies

Mozinstall depends on the [mozinfo](https://github.com/mozilla/mozbase/tree/master/mozinfo) 
package which is also found in the mozbase repository.
