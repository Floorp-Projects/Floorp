# load default template for a XUL app
include "${top_wizard_dir}templates/xul-app.tpl"

# short app name (can not contain spaces.)
# until http://bugzilla.mozilla.org/show_bug.cgi?id=75670 is fixed, this needs
# to be all lowercase.
app_name_short=xulsample

# long app name (spaces are OK.)
app_name_long=Sample XUL Application (generated from sample.xul-app.tpl)

# name as it should appear in the menu
app_name_menu=Sample XUL App

# version to tell the .xpi installer
app_version=1.0

# author, used in various chrome and app registration calls
app_author=mozilla.org

# size of the package when installed, in kilobytes.
# this number is used by the install.js script to check for enough disk space
# before the .xpi is installed.  You can just guess for now, or put 1, and fix it
# in install.js before you make your .xpi file.
install_size_kilobytes=1
