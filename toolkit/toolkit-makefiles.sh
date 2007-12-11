#! /bin/sh
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Mozilla build system
#
# The Initial Developer of the Original Code is
# Ben Turner <mozilla@songbirdnest.com>
#
# Portions created by the Initial Developer are Copyright (C) 2007
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# This file contains makefiles that will be generated for every XUL app.

MAKEFILES_db="
  db/Makefile
  db/mdb/Makefile
  db/mdb/public/Makefile
  db/mork/Makefile
  db/mork/build/Makefile
  db/mork/src/Makefile
"

MAKEFILES_dom="
  dom/Makefile
  dom/public/Makefile
  dom/public/base/Makefile
  dom/public/coreEvents/Makefile
  dom/public/idl/Makefile
  dom/public/idl/base/Makefile
  dom/public/idl/canvas/Makefile
  dom/public/idl/core/Makefile
  dom/public/idl/css/Makefile
  dom/public/idl/events/Makefile
  dom/public/idl/html/Makefile
  dom/public/idl/range/Makefile
  dom/public/idl/stylesheets/Makefile
  dom/public/idl/views/Makefile
  dom/public/idl/xbl/Makefile
  dom/public/idl/xpath/Makefile
  dom/public/idl/xul/Makefile
  dom/src/Makefile
  dom/src/base/Makefile
  dom/src/events/Makefile
  dom/src/jsurl/Makefile
  dom/locales/Makefile
"

MAKEFILES_editor="
  editor/Makefile
  editor/public/Makefile
  editor/idl/Makefile
  editor/txmgr/Makefile
  editor/txmgr/idl/Makefile
  editor/txmgr/public/Makefile
  editor/txmgr/src/Makefile
  editor/txmgr/tests/Makefile
  editor/txtsvc/Makefile
  editor/txtsvc/public/Makefile
  editor/txtsvc/src/Makefile
"

MAKEFILES_expat="
  parser/expat/Makefile
  parser/expat/lib/Makefile
"

MAKEFILES_gfx="
  gfx/Makefile
  gfx/idl/Makefile
  gfx/public/Makefile
  gfx/src/Makefile
  gfx/src/beos/Makefile
  gfx/src/psshared/Makefile
  gfx/src/photon/Makefile
  gfx/src/mac/Makefile
  gfx/src/windows/Makefile
  gfx/src/thebes/Makefile
  gfx/tests/Makefile
"

MAKEFILES_htmlparser="
  parser/htmlparser/Makefile
  parser/htmlparser/robot/Makefile
  parser/htmlparser/robot/test/Makefile
  parser/htmlparser/public/Makefile
  parser/htmlparser/src/Makefile
  parser/htmlparser/tests/Makefile
  parser/htmlparser/tests/grabpage/Makefile
  parser/htmlparser/tests/logparse/Makefile
  parser/htmlparser/tests/html/Makefile
  parser/htmlparser/tests/outsinks/Makefile
"

MAKEFILES_intl="
  intl/Makefile
  intl/chardet/Makefile
  intl/chardet/public/Makefile
  intl/chardet/src/Makefile
  intl/uconv/Makefile
  intl/uconv/idl/Makefile
  intl/uconv/public/Makefile
  intl/uconv/src/Makefile
  intl/uconv/tests/Makefile
  intl/uconv/ucvja/Makefile
  intl/uconv/ucvlatin/Makefile
  intl/uconv/ucvcn/Makefile
  intl/uconv/ucvtw/Makefile
  intl/uconv/ucvtw2/Makefile
  intl/uconv/ucvko/Makefile
  intl/uconv/ucvibm/Makefile
  intl/uconv/native/Makefile
  intl/locale/Makefile
  intl/locale/public/Makefile
  intl/locale/idl/Makefile
  intl/locale/src/Makefile
  intl/locale/src/unix/Makefile
  intl/locale/src/os2/Makefile
  intl/locale/src/windows/Makefile
  intl/locale/tests/Makefile
  intl/lwbrk/Makefile
  intl/lwbrk/src/Makefile
  intl/lwbrk/public/Makefile
  intl/lwbrk/tests/Makefile
  intl/unicharutil/Makefile
  intl/unicharutil/util/Makefile
  intl/unicharutil/util/internal/Makefile
  intl/unicharutil/idl/Makefile
  intl/unicharutil/src/Makefile
  intl/unicharutil/public/Makefile
  intl/unicharutil/tables/Makefile
  intl/unicharutil/tests/Makefile
  intl/unicharutil/tools/Makefile
  intl/strres/Makefile
  intl/strres/public/Makefile
  intl/strres/src/Makefile
  intl/strres/tests/Makefile
"

MAKEFILES_js="
  js/src/Makefile
  js/src/fdlibm/Makefile
"

MAKEFILES_liveconnect="
  js/src/liveconnect/Makefile
  js/src/liveconnect/classes/Makefile
"

MAKEFILES_xpconnect="
  js/src/xpconnect/Makefile
  js/src/xpconnect/public/Makefile
  js/src/xpconnect/idl/Makefile
  js/src/xpconnect/shell/Makefile
  js/src/xpconnect/src/Makefile
  js/src/xpconnect/loader/Makefile
  js/src/xpconnect/tests/Makefile
  js/src/xpconnect/tests/components/Makefile
  js/src/xpconnect/tests/idl/Makefile
  js/src/xpconnect/tools/Makefile
  js/src/xpconnect/tools/idl/Makefile
"

MAKEFILES_jsdebugger="
  js/jsd/Makefile
  js/jsd/idl/Makefile
"

MAKEFILES_content="
  content/Makefile
  content/base/Makefile
  content/base/public/Makefile
  content/base/src/Makefile
  content/canvas/Makefile
  content/canvas/public/Makefile
  content/canvas/src/Makefile
  content/events/Makefile
  content/events/public/Makefile
  content/events/src/Makefile
  content/html/Makefile
  content/html/content/Makefile
  content/html/content/public/Makefile
  content/html/content/src/Makefile
  content/html/document/Makefile
  content/html/document/public/Makefile
  content/html/document/src/Makefile
  content/xml/Makefile
  content/xml/content/Makefile
  content/xml/content/src/Makefile
  content/xml/document/Makefile
  content/xml/document/public/Makefile
  content/xml/document/src/Makefile
  content/xul/Makefile
  content/xul/content/Makefile
  content/xul/content/src/Makefile
  content/xul/document/Makefile
  content/xul/document/public/Makefile
  content/xul/document/src/Makefile
  content/xul/templates/public/Makefile
  content/xul/templates/src/Makefile
  content/xbl/Makefile
  content/xbl/public/Makefile
  content/xbl/src/Makefile
  content/xbl/builtin/Makefile
  content/xslt/Makefile
  content/xslt/public/Makefile
  content/xslt/src/Makefile
  content/xslt/src/base/Makefile
  content/xslt/src/xml/Makefile
  content/xslt/src/xpath/Makefile
  content/xslt/src/xslt/Makefile
  content/xslt/src/main/Makefile
"

MAKEFILES_layout="
  layout/Makefile
  layout/base/Makefile
  layout/base/tests/Makefile
  layout/build/Makefile
  layout/forms/Makefile
  layout/html/tests/Makefile
  layout/style/Makefile
  layout/printing/Makefile
  layout/tools/Makefile
  layout/xul/Makefile
  layout/xul/base/Makefile
  layout/xul/base/public/Makefile
  layout/xul/base/src/Makefile
  layout/xul/base/src/tree/Makefile
  layout/xul/base/src/tree/src/Makefile
  layout/xul/base/src/tree/public/Makefile
"

MAKEFILES_libimg="
  modules/libimg/Makefile
"

MAKEFILES_libjar="
  modules/libjar/Makefile
  modules/libjar/standalone/Makefile
  modules/libjar/test/Makefile
"

MAKEFILES_libreg="
  modules/libreg/Makefile
  modules/libreg/include/Makefile
  modules/libreg/src/Makefile
  modules/libreg/standalone/Makefile
"

MAKEFILES_libpref="
  modules/libpref/Makefile
  modules/libpref/public/Makefile
  modules/libpref/src/Makefile
"

MAKEFILES_libutil="
  modules/libutil/Makefile
  modules/libutil/public/Makefile
  modules/libutil/src/Makefile
"

MAKEFILES_oji="
  modules/oji/Makefile
  modules/oji/public/Makefile
  modules/oji/src/Makefile
  plugin/oji/JEP/Makefile
"

MAKEFILES_plugin="
  modules/plugin/Makefile
  modules/plugin/base/src/Makefile
  modules/plugin/base/public/Makefile
  modules/plugin/samples/simple/Makefile
  modules/plugin/samples/SanePlugin/Makefile
  modules/plugin/samples/default/unix/Makefile
  modules/plugin/tools/sdk/Makefile
  modules/plugin/tools/sdk/samples/Makefile
  modules/plugin/tools/sdk/samples/common/Makefile
  modules/plugin/tools/sdk/samples/basic/windows/Makefile
  modules/plugin/tools/sdk/samples/scriptable/windows/Makefile
  modules/plugin/tools/sdk/samples/simple/Makefile
  modules/plugin/tools/sdk/samples/winless/windows/Makefile
"

MAKEFILES_netwerk="
  netwerk/Makefile
  netwerk/base/Makefile
  netwerk/base/public/Makefile
  netwerk/base/src/Makefile
  netwerk/build/Makefile
  netwerk/cache/Makefile
  netwerk/cache/public/Makefile
  netwerk/cache/src/Makefile
  netwerk/cookie/Makefile
  netwerk/cookie/public/Makefile
  netwerk/cookie/src/Makefile
  netwerk/dns/Makefile
  netwerk/dns/public/Makefile
  netwerk/dns/src/Makefile
  netwerk/protocol/Makefile
  netwerk/protocol/about/Makefile
  netwerk/protocol/about/public/Makefile
  netwerk/protocol/about/src/Makefile
  netwerk/protocol/data/Makefile
  netwerk/protocol/data/src/Makefile
  netwerk/protocol/file/Makefile
  netwerk/protocol/file/public/Makefile
  netwerk/protocol/file/src/Makefile
  netwerk/protocol/ftp/Makefile
  netwerk/protocol/ftp/public/Makefile
  netwerk/protocol/ftp/src/Makefile
  netwerk/protocol/gopher/Makefile
  netwerk/protocol/gopher/src/Makefile
  netwerk/protocol/http/Makefile
  netwerk/protocol/http/public/Makefile
  netwerk/protocol/http/src/Makefile
  netwerk/protocol/res/Makefile
  netwerk/protocol/res/public/Makefile
  netwerk/protocol/res/src/Makefile
  netwerk/mime/Makefile
  netwerk/mime/public/Makefile
  netwerk/mime/src/Makefile
  netwerk/socket/Makefile
  netwerk/socket/base/Makefile
  netwerk/streamconv/Makefile
  netwerk/streamconv/converters/Makefile
  netwerk/streamconv/public/Makefile
  netwerk/streamconv/src/Makefile
  netwerk/streamconv/test/Makefile
  netwerk/test/Makefile
  netwerk/testserver/Makefile
  netwerk/resources/Makefile
  netwerk/locales/Makefile
  netwerk/system/Makefile
  netwerk/system/win32/Makefile
"

MAKEFILES_uriloader="
  uriloader/Makefile
  uriloader/base/Makefile
  uriloader/exthandler/Makefile
  uriloader/exthandler/tests/Makefile
"

MAKEFILES_profile="
  profile/Makefile
  profile/public/Makefile
  profile/dirserviceprovider/Makefile
  profile/dirserviceprovider/public/Makefile
  profile/dirserviceprovider/src/Makefile
"

MAKEFILES_rdf="
  rdf/Makefile
  rdf/base/Makefile
  rdf/base/idl/Makefile
  rdf/base/public/Makefile
  rdf/base/src/Makefile
  rdf/util/Makefile
  rdf/util/public/Makefile
  rdf/util/src/Makefile
  rdf/util/src/internal/Makefile
  rdf/build/Makefile
  rdf/datasource/Makefile
  rdf/datasource/public/Makefile
  rdf/datasource/src/Makefile
  rdf/tests/Makefile
  rdf/tests/rdfcat/Makefile
  rdf/tests/rdfpoll/Makefile
"

MAKEFILES_sun_java="
  sun-java/Makefile
  sun-java/stubs/Makefile
  sun-java/stubs/include/Makefile
  sun-java/stubs/jri/Makefile
"

MAKEFILES_caps="
  caps/Makefile
  caps/idl/Makefile
  caps/include/Makefile
  caps/src/Makefile
"

MAKEFILES_chrome="
  chrome/Makefile
  chrome/public/Makefile
  chrome/src/Makefile
  embedding/minimo/chromelite/Makefile
  rdf/chrome/Makefile
  rdf/chrome/public/Makefile
  rdf/chrome/build/Makefile
  rdf/chrome/src/Makefile
  rdf/chrome/tools/Makefile
  rdf/chrome/tools/chromereg/Makefile
"

MAKEFILES_view="
  view/Makefile
  view/public/Makefile
  view/src/Makefile
"

MAKEFILES_docshell="
  docshell/Makefile
  docshell/base/Makefile
  docshell/shistory/Makefile
  docshell/shistory/public/Makefile
  docshell/shistory/src/Makefile
  docshell/build/Makefile
"

MAKEFILES_webshell="
  webshell/Makefile
  webshell/public/Makefile
"

MAKEFILES_widget="
  widget/Makefile
  widget/public/Makefile
  widget/src/Makefile
  widget/src/beos/Makefile
  widget/src/build/Makefile
  widget/src/gtkxtbin/Makefile
  widget/src/photon/Makefile
  widget/src/cocoa/Makefile
  widget/src/os2/Makefile
  widget/src/windows/Makefile
  widget/src/xpwidgets/Makefile
  widget/src/support/Makefile
"

MAKEFILES_xpcom="
  xpcom/string/Makefile
  xpcom/string/public/Makefile
  xpcom/string/src/Makefile
  xpcom/Makefile
  xpcom/base/Makefile
  xpcom/build/Makefile
  xpcom/components/Makefile
  xpcom/ds/Makefile
  xpcom/glue/Makefile
  xpcom/glue/standalone/Makefile
  xpcom/io/Makefile
  xpcom/typelib/Makefile
  xpcom/reflect/Makefile
  xpcom/typelib/xpt/Makefile
  xpcom/typelib/xpt/public/Makefile
  xpcom/typelib/xpt/src/Makefile
  xpcom/typelib/xpt/tests/Makefile
  xpcom/typelib/xpt/tools/Makefile
  xpcom/typelib/xpidl/Makefile
  xpcom/reflect/xptcall/Makefile
  xpcom/reflect/xptcall/public/Makefile
  xpcom/reflect/xptcall/src/Makefile
  xpcom/reflect/xptcall/src/md/Makefile
  xpcom/reflect/xptcall/src/md/os2/Makefile
  xpcom/reflect/xptcall/src/md/test/Makefile
  xpcom/reflect/xptcall/src/md/unix/Makefile
  xpcom/reflect/xptcall/src/md/win32/Makefile
  xpcom/reflect/xptcall/tests/Makefile
  xpcom/reflect/xptinfo/Makefile
  xpcom/reflect/xptinfo/public/Makefile
  xpcom/reflect/xptinfo/src/Makefile
  xpcom/reflect/xptinfo/tests/Makefile
  xpcom/proxy/Makefile
  xpcom/proxy/public/Makefile
  xpcom/proxy/src/Makefile
  xpcom/proxy/tests/Makefile
  xpcom/sample/Makefile
  xpcom/threads/Makefile
  xpcom/tools/Makefile
  xpcom/tools/registry/Makefile
  xpcom/stub/Makefile
  xpcom/windbgdlg/Makefile
  xpcom/system/Makefile
"

MAKEFILES_xpcom_obsolete="
  xpcom/obsolete/Makefile
  xpcom/obsolete/component/Makefile
"

MAKEFILES_xpcom_tests="
  xpcom/tests/Makefile
  xpcom/tests/dynamic/Makefile
  xpcom/tests/services/Makefile
  xpcom/tests/windows/Makefile
"

MAKEFILES_xpinstall="
  xpinstall/Makefile
  xpinstall/public/Makefile
  xpinstall/res/Makefile
  xpinstall/src/Makefile
  xpinstall/stub/Makefile
"

MAKEFILES_xpfe="
  widget/src/xremoteclient/Makefile
  toolkit/components/remote/Makefile
  xpfe/Makefile
  xpfe/browser/Makefile
  xpfe/browser/public/Makefile
  xpfe/browser/src/Makefile
  xpfe/components/Makefile
  xpfe/components/directory/Makefile
  xpfe/components/download-manager/Makefile
  xpfe/components/download-manager/src/Makefile
  xpfe/components/download-manager/public/Makefile
  xpfe/components/download-manager/resources/Makefile
  xpfe/components/extensions/Makefile
  xpfe/components/extensions/src/Makefile
  xpfe/components/extensions/public/Makefile
  xpfe/components/find/Makefile
  xpfe/components/find/public/Makefile
  xpfe/components/find/src/Makefile
  xpfe/components/filepicker/Makefile
  xpfe/components/filepicker/public/Makefile
  xpfe/components/filepicker/src/Makefile
  xpfe/components/history/Makefile
  xpfe/components/history/src/Makefile
  xpfe/components/history/public/Makefile
  xpfe/components/intl/Makefile
  xpfe/components/related/Makefile
  xpfe/components/related/src/Makefile
  xpfe/components/related/public/Makefile
  xpfe/components/startup/Makefile
  xpfe/components/startup/public/Makefile
  xpfe/components/startup/src/Makefile
  xpfe/components/autocomplete/Makefile
  xpfe/components/autocomplete/public/Makefile
  xpfe/components/autocomplete/src/Makefile
  xpfe/components/updates/Makefile
  xpfe/components/updates/src/Makefile
  xpfe/components/winhooks/Makefile
  xpfe/components/windowds/Makefile
  xpfe/components/alerts/Makefile
  xpfe/components/alerts/public/Makefile
  xpfe/components/alerts/src/Makefile
  xpfe/components/console/Makefile
  xpfe/components/resetPref/Makefile
  xpfe/components/build/Makefile
  xpfe/components/xremote/Makefile
  xpfe/components/xremote/public/Makefile
  xpfe/components/xremote/src/Makefile
  xpfe/appshell/Makefile
  xpfe/appshell/src/Makefile
  xpfe/appshell/public/Makefile
  xpfe/bootstrap/appleevents/Makefile
  xpfe/global/Makefile
  xpfe/global/buildconfig.html
  xpfe/global/resources/Makefile
  xpfe/global/resources/content/Makefile
  xpfe/global/resources/content/os2/Makefile
  xpfe/global/resources/content/unix/Makefile
  xpfe/global/resources/locale/Makefile
  xpfe/global/resources/locale/en-US/Makefile
  xpfe/global/resources/locale/en-US/mac/Makefile
  xpfe/global/resources/locale/en-US/os2/Makefile
  xpfe/global/resources/locale/en-US/unix/Makefile
  xpfe/global/resources/locale/en-US/win/Makefile
  xpfe/communicator/Makefile
  extensions/spellcheck/Makefile
  extensions/spellcheck/hunspell/Makefile
  extensions/spellcheck/idl/Makefile
  extensions/spellcheck/locales/Makefile
  extensions/spellcheck/src/Makefile
"

MAKEFILES_embedding="
  embedding/Makefile
  embedding/base/Makefile
  embedding/browser/Makefile
  embedding/browser/activex/src/Makefile
  embedding/browser/activex/src/control/Makefile
  embedding/browser/activex/src/control_kicker/Makefile
  embedding/browser/build/Makefile
  embedding/browser/chrome/Makefile
  embedding/browser/webBrowser/Makefile
  embedding/browser/gtk/Makefile
  embedding/browser/gtk/src/Makefile
  embedding/browser/gtk/tests/Makefile
  embedding/browser/photon/Makefile
  embedding/browser/photon/src/Makefile
  embedding/browser/photon/tests/Makefile
  embedding/components/Makefile
  embedding/components/build/Makefile
  embedding/components/windowwatcher/Makefile
  embedding/components/windowwatcher/public/Makefile
  embedding/components/windowwatcher/src/Makefile
  embedding/components/ui/Makefile
  embedding/components/ui/helperAppDlg/Makefile
  embedding/components/ui/progressDlg/Makefile
  embedding/config/Makefile
  embedding/tests/Makefile
  embedding/tests/cocoaEmbed/Makefile
  embedding/tests/winEmbed/Makefile
"

MAKEFILES_xulapp="
  toolkit/Makefile
  toolkit/library/Makefile
  toolkit/crashreporter/Makefile
  toolkit/crashreporter/client/Makefile
  toolkit/crashreporter/google-breakpad/src/client/Makefile
  toolkit/crashreporter/google-breakpad/src/client/mac/handler/Makefile
  toolkit/crashreporter/google-breakpad/src/client/windows/Makefile
  toolkit/crashreporter/google-breakpad/src/client/windows/handler/Makefile
  toolkit/crashreporter/google-breakpad/src/client/windows/sender/Makefile
  toolkit/crashreporter/google-breakpad/src/common/Makefile
  toolkit/crashreporter/google-breakpad/src/common/mac/Makefile
  toolkit/crashreporter/google-breakpad/src/common/windows/Makefile
  toolkit/crashreporter/google-breakpad/src/tools/mac/dump_syms/Makefile
  toolkit/content/Makefile
  toolkit/content/buildconfig.html
  toolkit/obsolete/Makefile
  toolkit/components/alerts/Makefile
  toolkit/components/alerts/public/Makefile
  toolkit/components/alerts/src/Makefile
  toolkit/components/autocomplete/Makefile
  toolkit/components/autocomplete/public/Makefile
  toolkit/components/autocomplete/src/Makefile
  toolkit/components/Makefile
  toolkit/components/build/Makefile
  toolkit/components/commandlines/Makefile
  toolkit/components/commandlines/public/Makefile
  toolkit/components/commandlines/src/Makefile
  toolkit/components/console/Makefile
  toolkit/components/cookie/Makefile
  toolkit/components/downloads/public/Makefile
  toolkit/components/downloads/Makefile
  toolkit/components/downloads/src/Makefile
  toolkit/components/filepicker/Makefile
  toolkit/system/gnome/Makefile
  toolkit/components/help/Makefile
  toolkit/components/history/Makefile
  toolkit/components/history/public/Makefile
  toolkit/components/history/src/Makefile
  toolkit/components/passwordmgr/Makefile
  toolkit/components/passwordmgr/public/Makefile
  toolkit/components/passwordmgr/src/Makefile
  toolkit/components/passwordmgr/content/Makefile
  toolkit/components/passwordmgr/test/Makefile
  toolkit/components/places/Makefile
  toolkit/components/places/public/Makefile
  toolkit/components/places/src/Makefile
  toolkit/components/printing/Makefile
  toolkit/components/satchel/Makefile
  toolkit/components/satchel/public/Makefile
  toolkit/components/satchel/src/Makefile
  toolkit/components/startup/Makefile
  toolkit/components/startup/public/Makefile
  toolkit/components/startup/src/Makefile
  toolkit/components/typeaheadfind/Makefile
  toolkit/components/typeaheadfind/public/Makefile
  toolkit/components/typeaheadfind/src/Makefile
  toolkit/components/viewconfig/Makefile
  toolkit/components/viewsource/Makefile
  toolkit/locales/Makefile
  toolkit/mozapps/Makefile
  toolkit/mozapps/downloads/Makefile
  toolkit/mozapps/downloads/src/Makefile
  toolkit/mozapps/extensions/Makefile
  toolkit/mozapps/extensions/public/Makefile
  toolkit/mozapps/extensions/src/Makefile
  toolkit/mozapps/update/Makefile
  toolkit/mozapps/update/public/Makefile
  toolkit/mozapps/update/src/Makefile
  toolkit/mozapps/xpinstall/Makefile
  toolkit/profile/Makefile
  toolkit/profile/public/Makefile
  toolkit/profile/skin/Makefile
  toolkit/profile/src/Makefile
  toolkit/themes/Makefile
  toolkit/themes/gnomestripe/global/Makefile
  toolkit/themes/gnomestripe/Makefile
  toolkit/themes/pmstripe/global/Makefile
  toolkit/themes/pmstripe/Makefile
  toolkit/themes/pinstripe/Makefile
  toolkit/themes/pinstripe/global/Makefile
  toolkit/themes/pinstripe/help/Makefile
  toolkit/themes/pinstripe/mozapps/Makefile
  toolkit/themes/winstripe/Makefile
  toolkit/themes/winstripe/global/Makefile
  toolkit/themes/winstripe/help/Makefile
  toolkit/themes/winstripe/mozapps/Makefile
  toolkit/xre/Makefile
"

MAKEFILES_libpr0n="
  modules/libpr0n/Makefile
  modules/libpr0n/build/Makefile
  modules/libpr0n/public/Makefile
  modules/libpr0n/src/Makefile
  modules/libpr0n/decoders/Makefile
  modules/libpr0n/decoders/gif/Makefile
  modules/libpr0n/decoders/png/Makefile
  modules/libpr0n/decoders/jpeg/Makefile
  modules/libpr0n/decoders/bmp/Makefile
  modules/libpr0n/decoders/icon/Makefile
  modules/libpr0n/decoders/icon/win/Makefile
  modules/libpr0n/decoders/icon/gtk/Makefile
  modules/libpr0n/decoders/icon/beos/Makefile
  modules/libpr0n/decoders/xbm/Makefile
  modules/libpr0n/encoders/Makefile
  modules/libpr0n/encoders/png/Makefile
  modules/libpr0n/encoders/jpeg/Makefile
"

MAKEFILES_accessible="
  accessible/Makefile
  accessible/public/Makefile
  accessible/public/msaa/Makefile
  accessible/src/Makefile
  accessible/src/base/Makefile
  accessible/src/html/Makefile
  accessible/src/xul/Makefile
  accessible/src/msaa/Makefile
  accessible/src/atk/Makefile
  accessible/src/mac/Makefile
  accessible/build/Makefile
"
MAKEFILES_zlib="
  modules/zlib/standalone/Makefile
"

MAKEFILES_libbz2="
  modules/libbz2/Makefile
  modules/libbz2/src/Makefile
"

MAKEFILES_libmar="
  modules/libmar/Makefile
  modules/libmar/src/Makefile
  modules/libmar/tool/Makefile
"

add_makefiles "
  $MAKEFILES_db
  $MAKEFILES_dom
  $MAKEFILES_editor
  $MAKEFILES_expat
  $MAKEFILES_gfx
  $MAKEFILES_htmlparser
  $MAKEFILES_intl
  $MAKEFILES_js
  $MAKEFILES_liveconnect
  $MAKEFILES_xpconnect
  $MAKEFILES_jsdebugger
  $MAKEFILES_content
  $MAKEFILES_layout
  $MAKEFILES_libimg
  $MAKEFILES_libjar
  $MAKEFILES_libreg
  $MAKEFILES_libpref
  $MAKEFILES_libutil
  $MAKEFILES_oji
  $MAKEFILES_plugin
  $MAKEFILES_netwerk
  $MAKEFILES_uriloader
  $MAKEFILES_profile
  $MAKEFILES_rdf
  $MAKEFILES_sun_java
  $MAKEFILES_caps
  $MAKEFILES_chrome
  $MAKEFILES_view
  $MAKEFILES_docshell
  $MAKEFILES_webshell
  $MAKEFILES_widget
  $MAKEFILES_xpcom
  $MAKEFILES_xpcom_obsolete
  $MAKEFILES_xpcom_tests
  $MAKEFILES_xpinstall
  $MAKEFILES_xpfe
  $MAKEFILES_embedding
  $MAKEFILES_xulapp
  $MAKEFILES_libpr0n
  $MAKEFILES_accessible
  $MAKEFILES_zlib
  $MAKEFILES_libbz2
  $MAKEFILES_libmar
"

#
# Conditional makefiles
#

if [ "$MOZ_COMPOSER" ]; then
  add_makefiles "
    editor/composer/Makefile
    editor/ui/Makefile
    editor/ui/locales/Makefile
  "
fi

if [ "$MOZ_ZIPWRITER" ]; then
  add_makefiles "
    modules/libjar/zipwriter/Makefile
    modules/libjar/zipwriter/public/Makefile
    modules/libjar/zipwriter/src/Makefile
    modules/libjar/zipwriter/test/Makefile
  "
fi

if [ "$MOZ_STORAGE" ]; then
  add_makefiles "
    db/sqlite3/src/Makefile
    db/morkreader/Makefile
    storage/Makefile
    storage/public/Makefile
    storage/src/Makefile
    storage/build/Makefile
    storage/test/Makefile
  "
fi

if [ "$MOZ_TREE_CAIRO" ] ; then
  add_makefiles "
    gfx/cairo/Makefile
    gfx/cairo/libpixman/src/Makefile
    gfx/cairo/cairo/src/Makefile
    gfx/cairo/cairo/src/cairo-features.h
    gfx/cairo/glitz/src/Makefile
    gfx/cairo/glitz/src/glx/Makefile
    gfx/cairo/glitz/src/wgl/Makefile
  "
fi

if [ ! "$MOZ_NATIVE_LCMS" ] ; then
  add_makefiles "
    modules/lcms/Makefile
    modules/lcms/include/Makefile
    modules/lcms/src/Makefile
  "
fi

if [ "$SUNCTL" ] ; then
  add_makefiles "
    intl/ctl/Makefile
    intl/ctl/public/Makefile
    intl/ctl/src/Makefile
    intl/ctl/src/pangoLite/Makefile
    intl/ctl/src/thaiShaper/Makefile
    intl/ctl/src/hindiShaper/Makefile
  "
fi

if [ "$MOZ_UNIVERSALCHARDET" ] ; then
  add_makefiles "
    extensions/universalchardet/Makefile
    extensions/universalchardet/src/Makefile
    extensions/universalchardet/src/base/Makefile
    extensions/universalchardet/src/xpcom/Makefile
    extensions/universalchardet/tests/Makefile
  "
fi

if [ "$MOZ_AUTH_EXTENSION" ]; then
  add_makefiles "
    extensions/auth/Makefile
  "
fi

if test -n "$MOZ_PSM"; then
  add_makefiles "
    security/manager/Makefile
    security/manager/boot/Makefile
    security/manager/boot/src/Makefile
    security/manager/boot/public/Makefile
    security/manager/ssl/Makefile
    security/manager/ssl/src/Makefile
    security/manager/ssl/resources/Makefile
    security/manager/ssl/public/Makefile
    security/manager/pki/Makefile
    security/manager/pki/resources/Makefile
    security/manager/pki/src/Makefile
    security/manager/pki/public/Makefile
    security/manager/locales/Makefile
  "
fi

if test -n "$MOZ_CALENDAR"; then
  add_makefiles "
    calendar/Makefile
    calendar/resources/Makefile
    calendar/libical/Makefile
    calendar/libical/src/Makefile
    calendar/libical/src/libical/Makefile
    calendar/libical/src/libicalss/Makefile
    calendar/base/Makefile
    calendar/base/public/Makefile
    calendar/base/src/Makefile
    calendar/base/build/Makefile
    calendar/providers/Makefile
    calendar/providers/memory/Makefile
    calendar/providers/storage/Makefile
    calendar/providers/composite/Makefile
  "
fi

if [ "$MOZ_MAIL_NEWS" ]; then
  . "${srcdir}/mailnews/makefiles.sh"
fi

if test -n "$MOZ_IPCD"; then
  add_makefiles "
    ipc/ipcd/Makefile
    ipc/ipcd/daemon/public/Makefile
    ipc/ipcd/daemon/src/Makefile
    ipc/ipcd/client/public/Makefile
    ipc/ipcd/client/src/Makefile
    ipc/ipcd/shared/src/Makefile
    ipc/ipcd/test/Makefile
    ipc/ipcd/test/module/Makefile
    ipc/ipcd/extensions/Makefile
    ipc/ipcd/extensions/lock/Makefile
    ipc/ipcd/extensions/lock/public/Makefile
    ipc/ipcd/extensions/lock/src/Makefile
    ipc/ipcd/extensions/lock/src/module/Makefile
    ipc/ipcd/util/Makefile
    ipc/ipcd/util/public/Makefile
    ipc/ipcd/util/src/Makefile
  "
fi

if test -n "$MOZ_PROFILESHARING"; then
  add_makefiles "
    ipc/ipcd/extensions/transmngr/Makefile
    ipc/ipcd/extensions/transmngr/public/Makefile
    ipc/ipcd/extensions/transmngr/src/Makefile
    ipc/ipcd/extensions/transmngr/build/Makefile
    ipc/ipcd/extensions/transmngr/test/Makefile
    ipc/ipcd/extensions/transmngr/common/Makefile
    ipc/ipcd/extensions/transmngr/module/Makefile
    embedding/components/profilesharingsetup/Makefile
    embedding/components/profilesharingsetup/public/Makefile
    embedding/components/profilesharingsetup/src/Makefile
  "
fi

if [ ! "$SYSTEM_JPEG" ]; then
  add_makefiles "
    jpeg/Makefile
  "
fi

if [ ! "$SYSTEM_ZLIB" ]; then
  add_makefiles "
    modules/zlib/Makefile
    modules/zlib/src/Makefile
  "
fi

if test -n "$MOZ_UPDATE_PACKAGING"; then
  add_makefiles "
    tools/update-packaging/Makefile
    other-licenses/bsdiff/Makefile
  "
fi

if [ ! "$SYSTEM_PNG" ]; then
  add_makefiles "
    modules/libimg/png/Makefile
  "
fi

if [ -f ${srcdir}/l10n/makefiles.all ]; then
  MAKEFILES_langpacks=`cat ${srcdir}/l10n/makefiles.all`
  add_makefiles "
    $MAKEFILES_langpacks
  "
fi

if [ "$MOZ_L10N" ]; then
  add_makefiles "
    l10n/Makefile
  "

  if [ "$MOZ_L10N_LANG" ]; then
    add_makefiles "
      l10n/lang/Makefile
      l10n/lang/addressbook/Makefile
      l10n/lang/bookmarks/Makefile
      l10n/lang/directory/Makefile
      l10n/lang/editor/Makefile
      l10n/lang/global/Makefile
      l10n/lang/history/Makefile
      l10n/lang/messenger/Makefile
      l10n/lang/messengercompose/Makefile
      l10n/lang/navigator/Makefile
      l10n/lang/pref/Makefile
      l10n/lang/related/Makefile
      l10n/lang/sidebar/Makefile
      l10n/lang/addressbook/locale/Makefile
      l10n/lang/bookmarks/locale/Makefile
      l10n/lang/directory/locale/Makefile
      l10n/lang/editor/locale/Makefile
      l10n/lang/global/locale/Makefile
      l10n/lang/history/locale/Makefile
      l10n/lang/messenger/locale/Makefile
      l10n/lang/messengercompose/locale/Makefile
      l10n/lang/navigator/locale/Makefile
      l10n/lang/pref/locale/Makefile
      l10n/lang/related/locale/Makefile
      l10n/lang/sidebar/locale/Makefile
    "
  fi # MOZ_L10N_LANG

fi # MOZ_L10N

if [ "$MOZ_JPROF" ]; then
  add_makefiles "
    tools/jprof/Makefile
    tools/jprof/stub/Makefile
  "
fi

if [ "$MOZ_LEAKY" ]; then
  add_makefiles "
    tools/leaky/Makefile
  "
fi

if [ "$NS_TRACE_MALLOC" ]; then
  add_makefiles "
    tools/trace-malloc/Makefile
    tools/trace-malloc/lib/Makefile
  "
fi

if [ "$MOZ_MAPINFO" ]; then
  add_makefiles "
    tools/codesighs/Makefile
  "
fi

if [ "$MOZ_MATHML" ]; then
  add_makefiles "
    intl/uconv/ucvmath/Makefile
    layout/mathml/Makefile
    layout/mathml/base/Makefile
    layout/mathml/base/src/Makefile
    layout/mathml/content/Makefile
    layout/mathml/content/src/Makefile
  "
fi

if [ "$MOZ_SVG" ]; then
  add_makefiles "
    content/svg/Makefile
    content/svg/document/Makefile
    content/svg/document/src/Makefile
    content/svg/content/Makefile
    content/svg/content/src/Makefile
    dom/public/idl/svg/Makefile
    layout/svg/Makefile
    layout/svg/base/Makefile
    layout/svg/base/src/Makefile
  "
fi

if [ "$MOZ_XTF" ]; then
  add_makefiles "
    content/xtf/Makefile
    content/xtf/public/Makefile
    content/xtf/src/Makefile
  "
fi

if [ "$MOZ_XMLEXTRAS" ]; then
  add_makefiles "
    extensions/xmlextras/Makefile
    extensions/xmlextras/pointers/Makefile
    extensions/xmlextras/pointers/src/Makefile
    extensions/xmlextras/build/Makefile
    extensions/xmlextras/build/src/Makefile
  "
fi

if [ "$MOZ_WEBSERVICES" ]; then
  add_makefiles "
    extensions/webservices/Makefile
    extensions/webservices/build/Makefile
    extensions/webservices/build/src/Makefile
    extensions/webservices/interfaceinfo/Makefile
    extensions/webservices/interfaceinfo/src/Makefile
    extensions/webservices/proxy/Makefile
    extensions/webservices/proxy/src/Makefile
    extensions/webservices/public/Makefile
    extensions/webservices/security/Makefile
    extensions/webservices/security/src/Makefile
    extensions/webservices/schema/Makefile
    extensions/webservices/schema/src/Makefile
    extensions/webservices/soap/Makefile
    extensions/webservices/soap/src/Makefile
    extensions/webservices/wsdl/Makefile
    extensions/webservices/wsdl/src/Makefile
  "
fi

if [ "$MOZ_JAVAXPCOM" ]; then
  add_makefiles "
    extensions/java/Makefile
    extensions/java/xpcom/Makefile
    extensions/java/xpcom/interfaces/Makefile
    extensions/java/xpcom/src/Makefile
    extensions/java/xpcom/glue/Makefile
  "
fi

if [ "$MOZ_COMPONENTLIB" ]; then
  add_makefiles "
    embedding/componentlib/Makefile
  "
else
  if [ "$MOZ_STATIC_COMPONENTS" -o "$MOZ_META_COMPONENTS" ]; then
    add_makefiles "
      modules/staticmod/Makefile
    "
  fi
fi # MOZ_COMPONENTLIB
