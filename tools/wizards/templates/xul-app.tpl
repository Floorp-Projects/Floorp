# XUL application template, a simple XUL app that puts itself in the tasks
# menu, and displays a simple, single instance window.

# include default values
include "${top_wizard_dir}templates/default.tpl"

# location of templatized files
template_dir = ${top_wizard_dir}templates/xul-app/

# description of this template
template_description = file("${top_wizard_dir}templates/xul-app.description")

# variables the user's .tpl file MUST declare
required_variables = ${app_name_short}, ${app_name_long}

# directory depths
# if your xul app isn't going to end up in extensions/${app_name_short}, you'll
# need to adjust these depths accordingly.
depth_0_nix = ../..
depth_0_win = ..\..
depth_1_nix = ../../..
depth_1_win = ..\..\..

# filename mappings
rename ("app-ui.xul",      "${app_name_short}.xul")
rename ("app-ui.css",      "${app_name_short}.css")
rename ("app-ui.dtd",      "${app_name_short}.dtd")
rename ("app-ui.js",       "${app_name_short}.js")
rename ("app-static.js",   "${app_name_short}-static.js")
rename ("app-handlers.js", "${app_name_short}-handlers.js")
rename ("app-utils.js",    "${app_name_short}-utils.js")
rename ("app-overlay.xul", "${app_name_short}-overlay.xul")
rename ("app-overlay.css", "${app_name_short}-overlay.css")
rename ("app-overlay.dtd", "${app_name_short}-overlay.dtd")
rename ("app-overlay.js",  "${app_name_short}-overlay.js")

# name of the jar file to create
jar_file_name = ${app_name_short}.jar

# name to use in the menu item
app_name_menu = ${app_name_long}

# original code portion of the license
license_original_code = ${app_name_long}

# skin that this package provides
skin_name_short = modern
skin_name_long = Modern
skin_version = 1.0

# locale that this package provides
locale_name_short = en-US
locale_name_long = English(US)
locale_preview_url = http://www.mozilla.org/locales/en-US.gif

# places to register chrome:// url types
content_reg_dir = content/${app_name_short}/
locale_reg_dir = locale/${locale_name_short}/${app_name_short}/
skin_reg_dir = skin/${skin_name_short}/${app_name_short}/

# chrome urls
chrome_content_url = chrome://${app_name_short}/content/
chrome_locale_url = chrome://${app_name_short}/locale/
chrome_skin_url = chrome://${app_name_short}/skin/

# files defining the main ui
chrome_main_xul_url = ${chrome_content_url}${filename:app-ui.xul}
chrome_main_css_url = ${chrome_skin_url}${filename:app-ui.css}
chrome_main_dtd_url = ${chrome_locale_url}${filename:app-ui.dtd}

# file we want to load our overlay on top of
overlay_trigger_url = chrome://communicator/content/tasksOverlay.xul

# node from the overlay_trigger_url that we want to overlay
overlay_trigger_node = taskPopup

# value of position attribute for the first overlayed node
overlay_trigger_position = 6

# file that contains the nodes we want to overlay
overlay_node_url = ${chrome_content_url}${filename:app-overlay.xul}

# file that contains the javascript we want to overlay
overlay_js_url = ${chrome_content_url}${filename:app-overlay.js}

# file that contains the dtd we want to overlay
overlay_dtd_url = ${chrome_locale_url}${filename:app-overlay.dtd}