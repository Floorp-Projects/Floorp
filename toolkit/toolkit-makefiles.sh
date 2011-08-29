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
  db/sqlite3/src/Makefile
"

MAKEFILES_dom="
  ipc/Makefile
  ipc/chromium/Makefile
  ipc/glue/Makefile
  ipc/ipdl/Makefile
  dom/Makefile
  dom/interfaces/base/Makefile
  dom/interfaces/canvas/Makefile
  dom/interfaces/core/Makefile
  dom/interfaces/css/Makefile
  dom/interfaces/events/Makefile
  dom/interfaces/geolocation/Makefile
  dom/interfaces/html/Makefile
  dom/interfaces/json/Makefile
  dom/interfaces/load-save/Makefile
  dom/interfaces/offline/Makefile
  dom/interfaces/range/Makefile
  dom/interfaces/sidebar/Makefile
  dom/interfaces/storage/Makefile
  dom/interfaces/stylesheets/Makefile
  dom/interfaces/svg/Makefile
  dom/interfaces/traversal/Makefile
  dom/interfaces/xbl/Makefile
  dom/interfaces/xpath/Makefile
  dom/interfaces/xul/Makefile
  dom/base/Makefile
  dom/src/Makefile
  dom/src/events/Makefile
  dom/src/jsurl/Makefile
  dom/src/geolocation/Makefile
  dom/src/json/Makefile
  dom/src/offline/Makefile
  dom/src/storage/Makefile
  dom/locales/Makefile
  dom/plugins/base/Makefile
  dom/plugins/ipc/Makefile
  dom/plugins/test/Makefile
  dom/plugins/test/mochitest/Makefile
  dom/plugins/test/testplugin/Makefile
  js/jetpack/Makefile
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
  editor/composer/Makefile
  editor/composer/public/Makefile
  editor/composer/src/Makefile
  editor/composer/test/Makefile
  editor/libeditor/Makefile
  editor/libeditor/base/Makefile
  editor/libeditor/base/tests/Makefile
  editor/libeditor/html/Makefile
  editor/libeditor/text/Makefile
"

MAKEFILES_xmlparser="
  parser/expat/Makefile
  parser/expat/lib/Makefile
  parser/xml/Makefile
  parser/xml/public/Makefile
  parser/xml/src/Makefile
"

MAKEFILES_gfx="
  gfx/Makefile
  gfx/ycbcr/Makefile
  gfx/layers/Makefile
  gfx/src/Makefile
  gfx/tests/Makefile
  gfx/thebes/Makefile
  gfx/qcms/Makefile
  gfx/angle/Makefile
  gfx/angle/src/libGLESv2/Makefile
  gfx/angle/src/libEGL/Makefile
"

MAKEFILES_htmlparser="
  parser/htmlparser/Makefile
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
  intl/build/Makefile
  intl/chardet/Makefile
  intl/chardet/public/Makefile
  intl/chardet/src/Makefile
  intl/uconv/Makefile
  intl/uconv/idl/Makefile
  intl/uconv/util/Makefile
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
  intl/locale/Makefile
  intl/locale/public/Makefile
  intl/locale/idl/Makefile
  intl/locale/src/Makefile
  intl/locale/src/mac/Makefile
  intl/locale/src/unix/Makefile
  intl/locale/src/os2/Makefile
  intl/locale/src/windows/Makefile
  intl/locale/tests/Makefile
  intl/locales/Makefile
  intl/lwbrk/Makefile
  intl/lwbrk/idl/Makefile
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
"

MAKEFILES_jsipc="
  js/ipc/Makefile
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
  content/base/test/Makefile
  content/base/test/chrome/Makefile
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
  content/svg/Makefile
  content/svg/document/Makefile
  content/svg/document/src/Makefile
  content/svg/content/Makefile
  content/svg/content/src/Makefile
  content/xml/Makefile
  content/xml/content/Makefile
  content/xml/content/src/Makefile
  content/xml/document/Makefile
  content/xml/document/public/Makefile
  content/xml/document/resources/Makefile
  content/xml/document/src/Makefile
  content/xul/Makefile
  content/xul/content/Makefile
  content/xul/content/src/Makefile
  content/xul/document/Makefile
  content/xul/document/public/Makefile
  content/xul/document/src/Makefile
  content/xul/templates/Makefile
  content/xul/templates/public/Makefile
  content/xul/templates/src/Makefile
  content/xul/templates/tests/Makefile
  content/xul/templates/tests/chrome/Makefile
  content/xbl/Makefile
  content/xbl/public/Makefile
  content/xbl/src/Makefile
  content/xbl/builtin/Makefile
  content/xbl/builtin/emacs/Makefile
  content/xbl/builtin/mac/Makefile
  content/xbl/builtin/unix/Makefile
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
  layout/generic/Makefile
  layout/inspector/public/Makefile
  layout/inspector/src/Makefile
  layout/printing/Makefile
  layout/style/Makefile
  layout/style/xbl-marquee/Makefile
  layout/tables/Makefile
  layout/svg/base/src/Makefile
  layout/xul/base/public/Makefile
  layout/xul/base/src/Makefile
  layout/xul/base/src/grid/Makefile
  layout/xul/base/src/tree/src/Makefile
  layout/xul/base/src/tree/public/Makefile
"

MAKEFILES_libimg="
  modules/libimg/Makefile
"

MAKEFILES_libjar="
  modules/libjar/Makefile
  modules/libjar/test/Makefile
"

MAKEFILES_libreg="
  modules/libreg/Makefile
  modules/libreg/include/Makefile
  modules/libreg/src/Makefile
"

MAKEFILES_libpref="
  modules/libpref/Makefile
  modules/libpref/public/Makefile
  modules/libpref/src/Makefile
"

MAKEFILES_libvorbis="
  media/libvorbis/Makefile
  media/libvorbis/lib/Makefile
  media/libvorbis/include/Makefile
  media/libvorbis/include/vorbis/Makefile
"

MAKEFILES_libtremor="
  media/libtremor/Makefile
  media/libtremor/lib/Makefile
  media/libtremor/include/tremor/Makefile
"

MAKEFILES_libvpx="
  media/libvpx/Makefile
"

MAKEFILES_libtheora="
  media/libtheora/Makefile
  media/libtheora/lib/Makefile
  media/libtheora/include/Makefile
  media/libtheora/include/theora/Makefile
"

MAKEFILES_libogg="
  media/libogg/Makefile
  media/libogg/src/Makefile
  media/libogg/include/Makefile
  media/libogg/include/ogg/Makefile
"

MAKEFILES_libsydneyaudio="
  media/libsydneyaudio/Makefile
  media/libsydneyaudio/include/Makefile
  media/libsydneyaudio/src/Makefile
"

MAKEFILES_libnestegg="
  media/libnestegg/Makefile
  media/libnestegg/include/Makefile
  media/libnestegg/src/Makefile
"

MAKEFILES_mathml="
  content/mathml/Makefile
  content/mathml/content/Makefile
  content/mathml/content/src/Makefile
  layout/mathml/Makefile
"

MAKEFILES_netwerk="
  netwerk/Makefile
  netwerk/base/Makefile
  netwerk/base/public/Makefile
  netwerk/base/src/Makefile
  netwerk/build/Makefile
  netwerk/cache/Makefile
  netwerk/cookie/Makefile
  netwerk/wifi/Makefile
  netwerk/dns/Makefile
  netwerk/protocol/Makefile
  netwerk/protocol/about/Makefile
  netwerk/protocol/data/Makefile
  netwerk/protocol/file/Makefile
  netwerk/protocol/ftp/Makefile
  netwerk/protocol/http/Makefile
  netwerk/protocol/res/Makefile
  netwerk/protocol/viewsource/Makefile
  netwerk/mime/Makefile
  netwerk/socket/Makefile
  netwerk/streamconv/Makefile
  netwerk/streamconv/converters/Makefile
  netwerk/streamconv/public/Makefile
  netwerk/streamconv/src/Makefile
  netwerk/streamconv/test/Makefile
  netwerk/test/Makefile
  netwerk/locales/Makefile
  netwerk/system/Makefile
  netwerk/system/mac/Makefile
  netwerk/system/win32/Makefile
"

MAKEFILES_storage="
  storage/Makefile
  storage/public/Makefile
  storage/src/Makefile
  storage/build/Makefile
  storage/test/Makefile
"

MAKEFILES_uriloader="
  uriloader/Makefile
  uriloader/base/Makefile
  uriloader/exthandler/Makefile
  uriloader/exthandler/tests/Makefile
  uriloader/prefetch/Makefile
"

MAKEFILES_profile="
  profile/Makefile
  profile/public/Makefile
  profile/dirserviceprovider/Makefile
  profile/dirserviceprovider/public/Makefile
  profile/dirserviceprovider/src/Makefile
  profile/dirserviceprovider/standalone/Makefile
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

MAKEFILES_caps="
  caps/Makefile
  caps/idl/Makefile
  caps/include/Makefile
  caps/src/Makefile
  caps/tests/Makefile
  caps/tests/mochitest/Makefile
"

MAKEFILES_chrome="
  chrome/Makefile
  chrome/public/Makefile
  chrome/src/Makefile
"

MAKEFILES_view="
  view/Makefile
  view/public/Makefile
  view/src/Makefile
"

MAKEFILES_docshell="
  docshell/Makefile
  docshell/base/Makefile
  docshell/resources/Makefile
  docshell/resources/content/Makefile
  docshell/shistory/Makefile
  docshell/shistory/public/Makefile
  docshell/shistory/src/Makefile
  docshell/build/Makefile
"

MAKEFILES_widget="
  widget/Makefile
  widget/public/Makefile
  widget/src/Makefile
  widget/src/build/Makefile
  widget/src/gtk2/Makefile
  widget/src/gtkxtbin/Makefile
  widget/src/cocoa/Makefile
  widget/src/os2/Makefile
  widget/src/windows/Makefile
  widget/src/xpwidgets/Makefile
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
  xpcom/stub/Makefile
  xpcom/windbgdlg/Makefile
  xpcom/system/Makefile
"

MAKEFILES_xpcom_tests="
  xpcom/tests/Makefile
  xpcom/tests/windows/Makefile
  xpcom/tests/static-checker/Makefile
"

MAKEFILES_xpfe="
  widget/src/xremoteclient/Makefile
  toolkit/components/remote/Makefile
  xpfe/components/Makefile
  xpfe/components/directory/Makefile
  xpfe/components/autocomplete/Makefile
  xpfe/components/autocomplete/public/Makefile
  xpfe/components/autocomplete/src/Makefile
  xpfe/components/windowds/Makefile
  xpfe/components/build/Makefile
  xpfe/appshell/Makefile
  xpfe/appshell/src/Makefile
  xpfe/appshell/public/Makefile
  extensions/spellcheck/Makefile
  extensions/spellcheck/hunspell/Makefile
  extensions/spellcheck/hunspell/src/Makefile
  extensions/spellcheck/idl/Makefile
  extensions/spellcheck/locales/Makefile
  extensions/spellcheck/src/Makefile
"

MAKEFILES_embedding="
  embedding/Makefile
  embedding/base/Makefile
  embedding/browser/Makefile
  embedding/browser/build/Makefile
  embedding/browser/webBrowser/Makefile
  embedding/components/Makefile
  embedding/components/appstartup/Makefile
  embedding/components/appstartup/src/Makefile
  embedding/components/build/Makefile
  embedding/components/commandhandler/Makefile
  embedding/components/commandhandler/public/Makefile
  embedding/components/commandhandler/src/Makefile
  embedding/components/find/Makefile
  embedding/components/find/public/Makefile
  embedding/components/find/src/Makefile
  embedding/components/printingui/Makefile
  embedding/components/printingui/src/Makefile
  embedding/components/printingui/src/mac/Makefile
  embedding/components/printingui/src/unixshared/Makefile
  embedding/components/printingui/src/win/Makefile
  embedding/components/webbrowserpersist/Makefile
  embedding/components/webbrowserpersist/public/Makefile
  embedding/components/webbrowserpersist/src/Makefile
  embedding/components/windowwatcher/Makefile
  embedding/components/windowwatcher/public/Makefile
  embedding/components/windowwatcher/src/Makefile
  embedding/tests/Makefile
  embedding/tests/winEmbed/Makefile
"

MAKEFILES_xulapp="
  toolkit/Makefile
  toolkit/library/Makefile
  toolkit/crashreporter/Makefile
  toolkit/crashreporter/client/Makefile
  toolkit/crashreporter/google-breakpad/src/client/Makefile
  toolkit/crashreporter/google-breakpad/src/client/linux/handler/Makefile
  toolkit/crashreporter/google-breakpad/src/client/linux/minidump_writer/Makefile
  toolkit/crashreporter/google-breakpad/src/client/mac/handler/Makefile
  toolkit/crashreporter/google-breakpad/src/client/solaris/handler/Makefile
  toolkit/crashreporter/google-breakpad/src/client/windows/crash_generation/Makefile
  toolkit/crashreporter/google-breakpad/src/client/windows/handler/Makefile
  toolkit/crashreporter/google-breakpad/src/client/windows/sender/Makefile
  toolkit/crashreporter/google-breakpad/src/common/Makefile
  toolkit/crashreporter/google-breakpad/src/common/linux/Makefile
  toolkit/crashreporter/google-breakpad/src/common/mac/Makefile
  toolkit/crashreporter/google-breakpad/src/common/dwarf/Makefile
  toolkit/crashreporter/google-breakpad/src/common/solaris/Makefile
  toolkit/crashreporter/google-breakpad/src/common/windows/Makefile
  toolkit/crashreporter/google-breakpad/src/tools/linux/dump_syms/Makefile
  toolkit/crashreporter/google-breakpad/src/tools/mac/dump_syms/Makefile
  toolkit/crashreporter/google-breakpad/src/tools/solaris/dump_syms/Makefile
  toolkit/content/Makefile
  toolkit/components/alerts/Makefile
  toolkit/components/alerts/mac/Makefile
  toolkit/components/alerts/mac/growl/Makefile
  toolkit/components/apppicker/Makefile
  toolkit/components/autocomplete/Makefile
  toolkit/components/Makefile
  toolkit/components/build/Makefile
  toolkit/components/commandlines/Makefile
  toolkit/components/console/Makefile
  toolkit/components/contentprefs/Makefile
  toolkit/components/cookie/Makefile
  toolkit/components/downloads/Makefile
  toolkit/components/exthelper/Makefile
  toolkit/components/feeds/Makefile
  toolkit/components/filepicker/Makefile
  toolkit/components/find/Makefile
  toolkit/components/help/Makefile
  toolkit/components/intl/Makefile
  toolkit/components/microformats/Makefile
  toolkit/components/parentalcontrols/Makefile
  toolkit/components/passwordmgr/Makefile
  toolkit/components/passwordmgr/content/Makefile
  toolkit/components/passwordmgr/test/Makefile
  toolkit/components/places/Makefile
  toolkit/components/printing/Makefile
  toolkit/components/satchel/Makefile
  toolkit/components/search/Makefile
  toolkit/components/startup/Makefile
  toolkit/components/startup/public/Makefile
  toolkit/components/statusfilter/Makefile
  toolkit/components/typeaheadfind/Makefile
  toolkit/components/url-classifier/Makefile
  toolkit/components/urlformatter/Makefile
  toolkit/components/viewconfig/Makefile
  toolkit/components/viewsource/Makefile
  toolkit/locales/Makefile
  toolkit/mozapps/downloads/Makefile
  toolkit/mozapps/extensions/Makefile
  toolkit/mozapps/handling/Makefile
  toolkit/mozapps/plugins/Makefile
  toolkit/mozapps/readstrings/Makefile
  toolkit/mozapps/update/Makefile
  toolkit/mozapps/update/updater/Makefile
  toolkit/mozapps/xpinstall/Makefile
  toolkit/profile/Makefile
  toolkit/system/dbus/Makefile
  toolkit/system/gnome/Makefile
  toolkit/system/unixproxy/Makefile
  toolkit/system/osxproxy/Makefile
  toolkit/system/windowsproxy/Makefile
  toolkit/themes/Makefile
  toolkit/themes/gnomestripe/global/Makefile
  toolkit/themes/gnomestripe/Makefile
  toolkit/themes/gnomestripe/mozapps/Makefile
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

MAKEFILES_jsctypes="
  toolkit/components/ctypes/Makefile
  toolkit/components/ctypes/tests/Makefile
"

MAKEFILES_jsreflect="
  toolkit/components/reflect/Makefile
"

MAKEFILES_libpr0n="
  modules/libpr0n/Makefile
  modules/libpr0n/build/Makefile
  modules/libpr0n/public/Makefile
  modules/libpr0n/src/Makefile
  modules/libpr0n/decoders/Makefile
  modules/libpr0n/decoders/icon/Makefile
  modules/libpr0n/decoders/icon/mac/Makefile
  modules/libpr0n/decoders/icon/win/Makefile
  modules/libpr0n/decoders/icon/gtk/Makefile
  modules/libpr0n/encoders/Makefile
  modules/libpr0n/encoders/png/Makefile
  modules/libpr0n/encoders/jpeg/Makefile
"

MAKEFILES_accessible="
  accessible/Makefile
  accessible/public/Makefile
  accessible/public/ia2/Makefile
  accessible/public/msaa/Makefile
  accessible/src/Makefile
  accessible/src/base/Makefile
  accessible/src/html/Makefile
  accessible/src/xforms/Makefile
  accessible/src/xul/Makefile
  accessible/src/msaa/Makefile
  accessible/src/atk/Makefile
  accessible/src/mac/Makefile
  accessible/build/Makefile
"

MAKEFILES_zlib="
  modules/zlib/standalone/Makefile
"

MAKEFILES_libmar="
  modules/libmar/Makefile
  modules/libmar/src/Makefile
  modules/libmar/tool/Makefile
"

MAKEFILES_extensions="
  extensions/cookie/Makefile
  extensions/permissions/Makefile
  extensions/pref/Makefile
  extensions/pref/autoconfig/Makefile
  extensions/pref/autoconfig/public/Makefile
  extensions/pref/autoconfig/src/Makefile
"

MAKEFILES_startupcache="
  startupcache/Makefile
"

add_makefiles "
  $MAKEFILES_db
  $MAKEFILES_dom
  $MAKEFILES_editor
  $MAKEFILES_xmlparser
  $MAKEFILES_gfx
  $MAKEFILES_htmlparser
  $MAKEFILES_intl
  $MAKEFILES_xpconnect
  $MAKEFILES_jsipc
  $MAKEFILES_jsdebugger
  $MAKEFILES_jsctypes
  $MAKEFILES_jsreflect
  $MAKEFILES_content
  $MAKEFILES_layout
  $MAKEFILES_libimg
  $MAKEFILES_libjar
  $MAKEFILES_libreg
  $MAKEFILES_libpref
  $MAKEFILES_mathml
  $MAKEFILES_plugin
  $MAKEFILES_netwerk
  $MAKEFILES_storage
  $MAKEFILES_uriloader
  $MAKEFILES_profile
  $MAKEFILES_rdf
  $MAKEFILES_caps
  $MAKEFILES_chrome
  $MAKEFILES_view
  $MAKEFILES_docshell
  $MAKEFILES_widget
  $MAKEFILES_xpcom
  $MAKEFILES_xpcom_tests
  $MAKEFILES_xpfe
  $MAKEFILES_embedding
  $MAKEFILES_xulapp
  $MAKEFILES_libpr0n
  $MAKEFILES_accessible
  $MAKEFILES_zlib
  $MAKEFILES_libmar
  $MAKEFILES_extensions
  $MAKEFILES_startupcache
"

#
# Conditional makefiles
#

if [ "$ENABLE_TESTS" ]; then
  add_makefiles "
    chrome/test/Makefile
    content/canvas/test/Makefile
    content/events/test/Makefile
    content/html/content/test/Makefile
    content/html/document/test/Makefile
    content/smil/test/Makefile
    content/svg/content/test/Makefile
    content/test/Makefile
    content/xbl/test/Makefile
    content/xml/document/test/Makefile
    content/xslt/tests/buster/Makefile
    content/xslt/tests/mochitest/Makefile
    content/xtf/test/Makefile
    content/xul/content/test/Makefile
    content/xul/document/test/Makefile
    docshell/test/Makefile
    docshell/test/browser/Makefile
    docshell/test/chrome/Makefile
    docshell/test/navigation/Makefile
    dom/src/json/test/Makefile
    dom/src/jsurl/test/Makefile
    dom/tests/Makefile
    dom/tests/mochitest/Makefile
    dom/tests/mochitest/ajax/Makefile
    dom/tests/mochitest/ajax/jquery/Makefile
    dom/tests/mochitest/ajax/jquery/dist/Makefile
    dom/tests/mochitest/ajax/jquery/test/Makefile
    dom/tests/mochitest/ajax/jquery/test/data/Makefile
    dom/tests/mochitest/ajax/jquery/test/data/offset/Makefile
    dom/tests/mochitest/ajax/jquery/test/unit/Makefile
    dom/tests/mochitest/ajax/lib/Makefile
    dom/tests/mochitest/ajax/mochikit/Makefile
    dom/tests/mochitest/ajax/mochikit/MochiKit/Makefile
    dom/tests/mochitest/ajax/mochikit/tests/Makefile
    dom/tests/mochitest/ajax/mochikit/tests/SimpleTest/Makefile
    dom/tests/mochitest/ajax/offline/Makefile
    dom/tests/mochitest/ajax/offline/namespace1/Makefile
    dom/tests/mochitest/ajax/offline/namespace1/sub/Makefile
    dom/tests/mochitest/ajax/offline/namespace1/sub2/Makefile
    dom/tests/mochitest/ajax/offline/namespace2/Makefile
    dom/tests/mochitest/ajax/prototype/Makefile
    dom/tests/mochitest/ajax/prototype/dist/Makefile
    dom/tests/mochitest/ajax/prototype/test/Makefile
    dom/tests/mochitest/ajax/prototype/test/functional/Makefile
    dom/tests/mochitest/ajax/prototype/test/lib/Makefile
    dom/tests/mochitest/ajax/prototype/test/unit/Makefile
    dom/tests/mochitest/ajax/prototype/test/unit/fixtures/Makefile
    dom/tests/mochitest/ajax/prototype/test/unit/tmp/Makefile
    dom/tests/mochitest/ajax/scriptaculous/Makefile
    dom/tests/mochitest/ajax/scriptaculous/lib/Makefile
    dom/tests/mochitest/ajax/scriptaculous/src/Makefile
    dom/tests/mochitest/ajax/scriptaculous/test/Makefile
    dom/tests/mochitest/ajax/scriptaculous/test/unit/Makefile
    dom/tests/mochitest/bugs/Makefile
    dom/tests/mochitest/chrome/Makefile
    dom/tests/mochitest/dom-level0/Makefile
    dom/tests/mochitest/dom-level1-core/Makefile
    dom/tests/mochitest/dom-level1-core/files/Makefile
    dom/tests/mochitest/dom-level2-core/Makefile
    dom/tests/mochitest/dom-level2-core/files/Makefile
    dom/tests/mochitest/dom-level2-html/Makefile
    dom/tests/mochitest/dom-level2-html/files/Makefile
    dom/tests/mochitest/general/Makefile
    dom/tests/mochitest/geolocation/Makefile
    dom/tests/mochitest/localstorage/Makefile
    dom/tests/mochitest/sessionstorage/Makefile
    dom/tests/mochitest/whatwg/Makefile
    editor/libeditor/html/tests/Makefile
    editor/libeditor/text/tests/Makefile
    embedding/test/Makefile
    extensions/cookie/test/Makefile
    extensions/pref/Makefile
    js/src/xpconnect/tests/mochitest/Makefile
    layout/forms/test/Makefile
    layout/generic/test/Makefile
    layout/inspector/tests/Makefile
    layout/reftests/fonts/Makefile
    layout/reftests/fonts/mplus/Makefile
    layout/style/test/Makefile
    layout/tables/test/Makefile
    layout/tools/pageloader/Makefile
    layout/tools/reftest/Makefile
    layout/xul/base/test/Makefile
    layout/xul/test/Makefile
    modules/libjar/test/chrome/Makefile
    modules/libjar/test/mochitest/Makefile
    modules/libpr0n/test/Makefile
    modules/libpr0n/test/mochitest/Makefile
    modules/libpref/test/Makefile
    netwerk/test/httpserver/Makefile
    parser/htmlparser/tests/mochitest/Makefile
    parser/xml/test/Makefile
    rdf/tests/triplescat/Makefile
    startupcache/test/Makefile
    testing/mochitest/Makefile
    testing/mochitest/MochiKit/Makefile
    testing/mochitest/chrome/Makefile
    testing/mochitest/ssltunnel/Makefile
    testing/mochitest/static/Makefile
    testing/mochitest/tests/Makefile
    testing/mochitest/tests/MochiKit-1.4.2/Makefile
    testing/mochitest/tests/MochiKit-1.4.2/MochiKit/Makefile
    testing/mochitest/tests/MochiKit-1.4.2/tests/Makefile
    testing/mochitest/tests/MochiKit-1.4.2/tests/SimpleTest/Makefile
    testing/mochitest/tests/SimpleTest/Makefile
    testing/mochitest/tests/browser/Makefile
    testing/tools/screenshot/Makefile
    testing/xpcshell/Makefile
    testing/xpcshell/example/Makefile
    testing/firebug/Makefile
    toolkit/components/alerts/test/Makefile
    toolkit/components/autocomplete/tests/Makefile
    toolkit/components/commandlines/test/Makefile
    toolkit/components/contentprefs/tests/Makefile
    toolkit/components/downloads/test/Makefile
    toolkit/components/downloads/test/browser/Makefile
    toolkit/components/microformats/tests/Makefile
    toolkit/components/passwordmgr/test/browser/Makefile
    toolkit/components/places/tests/Makefile
    toolkit/components/places/tests/chrome/Makefile
    toolkit/components/places/tests/mochitest/bug_411966/Makefile
    toolkit/components/places/tests/mochitest/bug_461710/Makefile
    toolkit/components/satchel/test/Makefile
    toolkit/components/url-classifier/tests/Makefile
    toolkit/components/url-classifier/tests/mochitest/Makefile
    toolkit/components/urlformatter/tests/Makefile
    toolkit/components/viewsource/test/Makefile
    toolkit/content/tests/Makefile
    toolkit/content/tests/browser/Makefile
    toolkit/content/tests/chrome/Makefile
    toolkit/content/tests/widgets/Makefile
    toolkit/crashreporter/test/Makefile
    toolkit/mozapps/downloads/tests/Makefile
    toolkit/mozapps/downloads/tests/chrome/Makefile
    toolkit/mozapps/extensions/test/Makefile
    toolkit/mozapps/plugins/tests/Makefile
    toolkit/mozapps/update/test/Makefile
    toolkit/xre/test/Makefile
    uriloader/exthandler/tests/mochitest/Makefile
    widget/tests/Makefile
    xpcom/sample/program/Makefile
    xpcom/tests/external/Makefile
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

if [ "$MOZ_TREE_CAIRO" ] ; then
  add_makefiles "
    gfx/cairo/Makefile
    gfx/cairo/cairo/src/Makefile
    gfx/cairo/cairo/src/cairo-features.h
  "
  if [ "$MOZ_TREE_PIXMAN" ] ; then
    add_makefiles "
      gfx/cairo/libpixman/src/Makefile
    "
  fi

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
    security/manager/ssl/public/Makefile
    security/manager/pki/Makefile
    security/manager/pki/resources/Makefile
    security/manager/pki/src/Makefile
    security/manager/pki/public/Makefile
    security/manager/locales/Makefile
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

if [ ! "$SYSTEM_BZ2" ]; then
  add_makefiles "
    modules/libbz2/Makefile
    modules/libbz2/src/Makefile
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

if [ "$MOZ_SMIL" ]; then
  add_makefiles "
    content/smil/Makefile
    dom/interfaces/smil/Makefile
  "
fi

if [ "$MOZ_XTF" ]; then
  add_makefiles "
    content/xtf/Makefile
    content/xtf/public/Makefile
    content/xtf/src/Makefile
  "
fi

if [ "$MOZ_MEDIA" ]; then
 add_makefiles "
   content/media/Makefile
   content/media/test/Makefile
 "
fi

if [ "$MOZ_VORBIS" ]; then
 add_makefiles "
   $MAKEFILES_libvorbis
   $MAKEFILES_libogg
 "
fi

if [ "$MOZ_TREMOR" ]; then
 add_makefiles "
   $MAKEFILES_libtremor
   $MAKEFILES_libogg
 "
fi

if [ "$MOZ_OGG" ]; then
 add_makefiles "
   $MAKEFILES_libtheora
   content/media/ogg/Makefile
 "
fi

if [ "$MOZ_WEBM" ]; then
 add_makefiles "
   $MAKEFILES_libvpx
   $MAKEFILES_libnestegg
   content/media/webm/Makefile
 "
fi

if [ "$MOZ_WAVE" ]; then
 add_makefiles "
   content/media/wave/Makefile
 "
fi

if [ "$MOZ_SYDNEYAUDIO" ]; then
 add_makefiles "
   $MAKEFILES_libsydneyaudio
 "
fi
