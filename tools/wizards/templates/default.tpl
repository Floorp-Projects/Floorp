# default values for use by other templates.

# description of this template
template_description = file("${top_wizard_dir}templates/default.description")

# license information
license_original_code = mozilla.org code
license_author_short  = Netscape
license_author_long   = Netscape Communications Corporation
license_copyright     = Copyright (C) 1998 Netscape Communications Corporation.
license_contrib       =

# type of license
license_type = MPL-GPL-LGPL

# license files
license_dir = ${top_wizard_dir}templates/licenses/${license_type}
license_mak = file("${license_dir}/lic.mak")
license_pl  = file("${license_dir}/lic.pl")
license_c   = file("${license_dir}/lic.c")
license_cpp = file("${license_dir}/lic.cpp")
license_xml = file("${license_dir}/lic.xml")
license_css = file("${license_dir}/lic.css")
