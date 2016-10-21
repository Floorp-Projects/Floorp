devtools-l10n-migration script
==============================

For devtools.html, devtools will no longer rely on DTD files. This migration
script is aimed at localizers to automate the migration of strings from DTD to
properties files.

How to run this script?

To migrate all configuration files:
  python migrate/main.py path/to/your/l10n/repo/ -c migrate/conf/

To migrate only one configuration file:
  python migrate/main.py path/to/your/l10n/repo/ -c migrate/conf/bug1294186

All configuration files should be named after the bug where specific devtools strings were migrated.
