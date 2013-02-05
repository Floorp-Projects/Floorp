#! /bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This file contains makefiles that will be generated for every XUL app.

MAKEFILES_dom="
  ipc/Makefile
  ipc/app/Makefile
  ipc/chromium/Makefile
  ipc/glue/Makefile
  ipc/ipdl/Makefile
  ipc/testshell/Makefile
  dom/Makefile
  dom/interfaces/apps/Makefile
  dom/interfaces/base/Makefile
  dom/interfaces/canvas/Makefile
  dom/interfaces/contacts/Makefile
  dom/interfaces/core/Makefile
  dom/interfaces/css/Makefile
  dom/interfaces/devicestorage/Makefile
  dom/interfaces/events/Makefile
  dom/interfaces/geolocation/Makefile
  dom/interfaces/html/Makefile
  dom/interfaces/json/Makefile
  dom/interfaces/load-save/Makefile
  dom/interfaces/offline/Makefile
  dom/interfaces/notification/Makefile
  dom/interfaces/range/Makefile
  dom/interfaces/settings/Makefile
  dom/interfaces/sidebar/Makefile
  dom/interfaces/storage/Makefile
  dom/interfaces/stylesheets/Makefile
  dom/interfaces/svg/Makefile
  dom/interfaces/traversal/Makefile
  dom/interfaces/xbl/Makefile
  dom/interfaces/xpath/Makefile
  dom/interfaces/xul/Makefile
  dom/activities/Makefile
  dom/activities/interfaces/Makefile
  dom/activities/src/Makefile
  dom/alarm/Makefile
  dom/apps/Makefile
  dom/apps/src/Makefile
  dom/base/Makefile
  dom/battery/Makefile
  dom/bindings/Makefile
  dom/browser-element/Makefile
  dom/contacts/Makefile
  dom/devicestorage/Makefile
  dom/devicestorage/ipc/Makefile
  dom/file/Makefile
  dom/identity/Makefile
  dom/indexedDB/Makefile
  dom/indexedDB/ipc/Makefile
  dom/ipc/Makefile
  dom/locales/Makefile
  dom/media/Makefile
  dom/messages/Makefile
  dom/messages/interfaces/Makefile
  dom/mms/Makefile
  dom/mms/interfaces/Makefile
  dom/mms/src/Makefile
  dom/network/Makefile
  dom/network/interfaces/Makefile
  dom/network/src/Makefile
  dom/plugins/base/Makefile
  dom/plugins/ipc/Makefile
  dom/power/Makefile
  dom/quota/Makefile
  dom/settings/Makefile
  dom/sms/Makefile
  dom/sms/interfaces/Makefile
  dom/sms/src/Makefile
  dom/src/Makefile
  dom/src/events/Makefile
  dom/src/jsurl/Makefile
  dom/src/geolocation/Makefile
  dom/src/json/Makefile
  dom/src/offline/Makefile
  dom/src/notification/Makefile
  dom/src/storage/Makefile
  dom/system/Makefile
  dom/workers/Makefile
  dom/time/Makefile
"

MAKEFILES_editor="
  editor/Makefile
  editor/public/Makefile
  editor/idl/Makefile
  editor/txmgr/Makefile
  editor/txmgr/idl/Makefile
  editor/txmgr/public/Makefile
  editor/txmgr/src/Makefile
  editor/txtsvc/Makefile
  editor/txtsvc/public/Makefile
  editor/txtsvc/src/Makefile
  editor/composer/Makefile
  editor/composer/public/Makefile
  editor/composer/src/Makefile
  editor/libeditor/Makefile
  editor/libeditor/base/Makefile
  editor/libeditor/html/Makefile
  editor/libeditor/text/Makefile
"

MAKEFILES_parser="
  parser/Makefile
  parser/html/Makefile
  parser/htmlparser/Makefile
  parser/htmlparser/public/Makefile
  parser/htmlparser/src/Makefile
  parser/expat/Makefile
  parser/expat/lib/Makefile
  parser/xml/Makefile
  parser/xml/public/Makefile
  parser/xml/src/Makefile
"

MAKEFILES_gfx="
  gfx/Makefile
  gfx/2d/Makefile
  gfx/angle/Makefile
  gfx/gl/Makefile
  gfx/harfbuzz/src/Makefile
  gfx/ipc/Makefile
  gfx/layers/Makefile
  gfx/ots/src/Makefile
  gfx/src/Makefile
  gfx/thebes/Makefile
  gfx/qcms/Makefile
  gfx/ycbcr/Makefile
"

MAKEFILES_intl="
  intl/Makefile
  intl/build/Makefile
  intl/chardet/Makefile
  intl/chardet/public/Makefile
  intl/chardet/src/Makefile
  intl/hyphenation/Makefile
  intl/hyphenation/public/Makefile
  intl/hyphenation/src/Makefile
  intl/uconv/Makefile
  intl/uconv/idl/Makefile
  intl/uconv/util/Makefile
  intl/uconv/public/Makefile
  intl/uconv/src/Makefile
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
  intl/locales/Makefile
  intl/lwbrk/Makefile
  intl/lwbrk/idl/Makefile
  intl/lwbrk/src/Makefile
  intl/lwbrk/public/Makefile
  intl/unicharutil/Makefile
  intl/unicharutil/util/Makefile
  intl/unicharutil/util/internal/Makefile
  intl/unicharutil/idl/Makefile
  intl/unicharutil/src/Makefile
  intl/unicharutil/public/Makefile
  intl/unicharutil/tables/Makefile
  intl/strres/Makefile
  intl/strres/public/Makefile
  intl/strres/src/Makefile
"

MAKEFILES_xpconnect="
  js/xpconnect/Makefile
  js/xpconnect/public/Makefile
  js/xpconnect/idl/Makefile
  js/xpconnect/shell/Makefile
  js/xpconnect/src/Makefile
  js/xpconnect/loader/Makefile
  js/xpconnect/wrappers/Makefile
"

MAKEFILES_jsipc="
  js/ipc/Makefile
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
  content/media/webrtc/Makefile
  content/svg/Makefile
  content/svg/document/src/Makefile
  content/svg/content/Makefile
  content/svg/content/src/Makefile
  content/xml/Makefile
  content/xml/content/src/Makefile
  content/xml/document/Makefile
  content/xml/document/public/Makefile
  content/xml/document/resources/Makefile
  content/xml/document/src/Makefile
  content/xul/Makefile
  content/xul/content/Makefile
  content/xul/content/public/Makefile
  content/xul/content/src/Makefile
  content/xul/document/Makefile
  content/xul/document/public/Makefile
  content/xul/document/src/Makefile
  content/xbl/Makefile
  content/xbl/src/Makefile
  content/xbl/builtin/Makefile
  content/xslt/Makefile
  content/xslt/public/Makefile
  content/xslt/src/Makefile
  content/xslt/src/base/Makefile
  content/xslt/src/xml/Makefile
  content/xslt/src/xpath/Makefile
  content/xslt/src/xslt/Makefile
"

MAKEFILES_smil="
  content/smil/Makefile
  dom/interfaces/smil/Makefile
"

MAKEFILES_layout="
  layout/Makefile
  layout/base/Makefile
  layout/build/Makefile
  layout/forms/Makefile
  layout/generic/Makefile
  layout/ipc/Makefile
  layout/inspector/public/Makefile
  layout/inspector/src/Makefile
  layout/media/Makefile
  layout/style/Makefile
  layout/style/xbl-marquee/Makefile
  layout/tables/Makefile
  layout/svg/Makefile
  layout/xul/base/public/Makefile
  layout/xul/base/src/Makefile
"

MAKEFILES_libjar="
  modules/libjar/Makefile
"

MAKEFILES_libpref="
  modules/libpref/Makefile
  modules/libpref/public/Makefile
  modules/libpref/src/Makefile
"

MAKEFILES_mathml="
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
  netwerk/dns/Makefile
  netwerk/ipc/Makefile
  netwerk/protocol/Makefile
  netwerk/protocol/about/Makefile
  netwerk/protocol/app/Makefile
  netwerk/protocol/data/Makefile
  netwerk/protocol/device/Makefile
  netwerk/protocol/file/Makefile
  netwerk/protocol/ftp/Makefile
  netwerk/protocol/http/Makefile
  netwerk/protocol/res/Makefile
  netwerk/protocol/viewsource/Makefile
  netwerk/protocol/websocket/Makefile
  netwerk/protocol/wyciwyg/Makefile
  netwerk/mime/Makefile
  netwerk/socket/Makefile
  netwerk/streamconv/Makefile
  netwerk/streamconv/converters/Makefile
  netwerk/streamconv/public/Makefile
  netwerk/streamconv/src/Makefile
  netwerk/locales/Makefile
  netwerk/system/Makefile
"

MAKEFILES_storage="
  storage/Makefile
  storage/public/Makefile
  storage/src/Makefile
  storage/build/Makefile
"

MAKEFILES_uriloader="
  uriloader/Makefile
  uriloader/base/Makefile
  uriloader/exthandler/Makefile
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
"

MAKEFILES_view="
  view/Makefile
  view/public/Makefile
  view/src/Makefile
"

MAKEFILES_docshell="
  docshell/Makefile
  docshell/base/Makefile
  docshell/resources/content/Makefile
  docshell/shistory/Makefile
  docshell/shistory/public/Makefile
  docshell/shistory/src/Makefile
  docshell/build/Makefile
"

MAKEFILES_widget="
  widget/Makefile
  widget/shared/Makefile
  widget/xpwidgets/Makefile
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
  xpcom/glue/nomozalloc/Makefile
  xpcom/glue/standalone/Makefile
  xpcom/io/Makefile
  xpcom/typelib/Makefile
  xpcom/reflect/Makefile
  xpcom/typelib/xpt/Makefile
  xpcom/typelib/xpt/public/Makefile
  xpcom/typelib/xpt/src/Makefile
  xpcom/typelib/xpt/tools/Makefile
  xpcom/typelib/xpidl/Makefile
  xpcom/reflect/xptcall/Makefile
  xpcom/reflect/xptcall/public/Makefile
  xpcom/reflect/xptcall/src/Makefile
  xpcom/reflect/xptcall/src/md/Makefile
  xpcom/reflect/xptinfo/Makefile
  xpcom/reflect/xptinfo/public/Makefile
  xpcom/reflect/xptinfo/src/Makefile
  xpcom/threads/Makefile
  xpcom/stub/Makefile
  xpcom/system/Makefile
  xpcom/idl-parser/Makefile
"

MAKEFILES_xpfe="
  xpfe/components/Makefile
  xpfe/components/directory/Makefile
  xpfe/components/windowds/Makefile
  xpfe/components/build/Makefile
  xpfe/appshell/Makefile
  xpfe/appshell/src/Makefile
  xpfe/appshell/public/Makefile
"

MAKEFILES_embedding="
  embedding/Makefile
  embedding/base/Makefile
  embedding/browser/Makefile
  embedding/browser/build/Makefile
  embedding/browser/webBrowser/Makefile
  embedding/components/Makefile
  embedding/components/appstartup/src/Makefile
  embedding/components/build/Makefile
  embedding/components/commandhandler/Makefile
  embedding/components/commandhandler/public/Makefile
  embedding/components/commandhandler/src/Makefile
  embedding/components/find/Makefile
  embedding/components/find/public/Makefile
  embedding/components/find/src/Makefile
  embedding/components/webbrowserpersist/Makefile
  embedding/components/webbrowserpersist/public/Makefile
  embedding/components/webbrowserpersist/src/Makefile
  embedding/components/windowwatcher/Makefile
  embedding/components/windowwatcher/public/Makefile
  embedding/components/windowwatcher/src/Makefile
"

MAKEFILES_xulapp="
  addon-sdk/Makefile
  toolkit/Makefile
  toolkit/library/Makefile
  toolkit/crashreporter/client/Makefile
  toolkit/content/Makefile
  toolkit/components/aboutmemory/Makefile
  toolkit/components/alerts/Makefile
  toolkit/components/apppicker/Makefile
  toolkit/components/Makefile
  toolkit/components/build/Makefile
  toolkit/components/commandlines/Makefile
  toolkit/components/console/Makefile
  toolkit/components/contentprefs/Makefile
  toolkit/components/cookie/Makefile
  toolkit/components/downloads/Makefile
  toolkit/components/exthelper/Makefile
  toolkit/components/filepicker/Makefile
  toolkit/components/find/Makefile
  toolkit/components/intl/Makefile
  toolkit/components/microformats/Makefile
  toolkit/components/osfile/Makefile
  toolkit/components/parentalcontrols/Makefile
  toolkit/components/passwordmgr/Makefile
  toolkit/components/perf/Makefile
  toolkit/components/places/Makefile
  toolkit/components/prompts/Makefile
  toolkit/components/prompts/src/Makefile
  toolkit/components/social/Makefile
  toolkit/components/startup/Makefile
  toolkit/components/startup/public/Makefile
  toolkit/components/statusfilter/Makefile
  toolkit/components/telemetry/Makefile
  toolkit/components/typeaheadfind/Makefile
  toolkit/components/urlformatter/Makefile
  toolkit/components/viewconfig/Makefile
  toolkit/components/viewsource/Makefile
  toolkit/devtools/Makefile
  toolkit/devtools/sourcemap/Makefile
  toolkit/forgetaboutsite/Makefile
  toolkit/forgetaboutsite/test/Makefile
  toolkit/forgetaboutsite/test/browser/Makefile
  toolkit/identity/Makefile
  toolkit/locales/Makefile
  toolkit/modules/Makefile
  toolkit/modules/tests/Makefile
  toolkit/mozapps/downloads/Makefile
  toolkit/mozapps/extensions/Makefile
  toolkit/mozapps/handling/Makefile
  toolkit/mozapps/plugins/Makefile
  toolkit/mozapps/preferences/Makefile
  toolkit/mozapps/shared/Makefile
  toolkit/mozapps/update/Makefile
  toolkit/mozapps/update/common/Makefile
  toolkit/obsolete/Makefile
  toolkit/profile/Makefile
  toolkit/themes/Makefile
  toolkit/webapps/Makefile
  toolkit/xre/Makefile
"

MAKEFILES_debugger="
  toolkit/devtools/debugger/Makefile
"

MAKEFILES_jsreflect="
  toolkit/components/reflect/Makefile
"

MAKEFILES_jsductwork="
  js/ductwork/debugger/Makefile
"

MAKEFILES_imagelib="
  image/Makefile
  image/build/Makefile
  image/public/Makefile
  image/src/Makefile
  image/decoders/Makefile
  image/decoders/icon/Makefile
  image/encoders/Makefile
  image/encoders/bmp/Makefile
  image/encoders/ico/Makefile
  image/encoders/jpeg/Makefile
  image/encoders/png/Makefile
"

MAKEFILES_startupcache="
  startupcache/Makefile
"

MAKEFILES_hal="
  hal/Makefile
"

MAKEFILES_psm_public="
  security/manager/boot/public/Makefile
  security/manager/ssl/public/Makefile
"

MAKEFILES_profiler="
  tools/profiler/Makefile
"

MAKEFILES_snappy="
  other-licenses/snappy/Makefile
"

add_makefiles "
  $MAKEFILES_dom
  $MAKEFILES_editor
  $MAKEFILES_parser
  $MAKEFILES_gfx
  $MAKEFILES_intl
  $MAKEFILES_xpconnect
  $MAKEFILES_jsipc
  $MAKEFILES_debugger
  $MAKEFILES_jsreflect
  $MAKEFILES_jsductwork
  $MAKEFILES_content
  $MAKEFILES_smil
  $MAKEFILES_layout
  $MAKEFILES_libjar
  $MAKEFILES_libpref
  $MAKEFILES_mathml
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
  $MAKEFILES_xpfe
  $MAKEFILES_embedding
  $MAKEFILES_xulapp
  $MAKEFILES_imagelib
  $MAKEFILES_startupcache
  $MAKEFILES_hal
  $MAKEFILES_psm_public
  $MAKEFILES_profiler
  $MAKEFILES_snappy
"

#
# Platform specific makefiles
#

if [ "$MOZ_WIDGET_TOOLKIT" = "windows" ]; then
  add_makefiles "
    content/xbl/builtin/win/Makefile
    dom/system/windows/Makefile
    image/decoders/icon/win/Makefile
    intl/locale/src/windows/Makefile
    netwerk/system/win32/Makefile
    toolkit/system/windowsproxy/Makefile
    widget/windows/Makefile
    xpcom/reflect/xptcall/src/md/win32/Makefile
  "
elif [ "$MOZ_WIDGET_TOOLKIT" = "cocoa" ]; then
  add_makefiles "
    content/xbl/builtin/mac/Makefile
    dom/plugins/ipc/interpose/Makefile
    image/decoders/icon/mac/Makefile
    intl/locale/src/mac/Makefile
    netwerk/system/mac/Makefile
    toolkit/system/osxproxy/Makefile
    toolkit/themes/pinstripe/Makefile
    toolkit/themes/pinstripe/global/Makefile
    toolkit/themes/pinstripe/mozapps/Makefile
    toolkit/components/alerts/mac/Makefile
    toolkit/components/alerts/mac/growl/Makefile
    widget/cocoa/Makefile
  "
elif [ "$MOZ_WIDGET_TOOLKIT" = "gtk2" ]; then
  add_makefiles "
    image/decoders/icon/gtk/Makefile
    widget/gtk2/Makefile
  "
elif [ "$MOZ_WIDGET_TOOLKIT" = "android" ]; then
  add_makefiles "
    dom/plugins/base/android/Makefile
    dom/system/android/Makefile
    image/decoders/icon/android/Makefile
    netwerk/system/android/Makefile
    widget/android/Makefile
    toolkit/system/androidproxy/Makefile
  "
  if [ "$MOZ_BUILD_APP" = "mobile/xul" -o "$MOZ_BUILD_APP" = "b2g" ]; then
    add_makefiles "
      embedding/android/Makefile
      embedding/android/locales/Makefile
    "
  fi
elif [ "$MOZ_WIDGET_TOOLKIT" = "gonk" ]; then
  add_makefiles "
    widget/gonk/Makefile
  "
elif [ "$MOZ_WIDGET_TOOLKIT" = "qt" ]; then
  add_makefiles "
    image/decoders/icon/qt/Makefile
    image/decoders/icon/qt/public/Makefile
    widget/qt/Makefile
    widget/qt/faststartupqt/Makefile
  "
elif [ "$MOZ_WIDGET_TOOLKIT" = "os2" ]; then
  add_makefiles "
    image/decoders/icon/os2/Makefile
    intl/locale/src/os2/Makefile
    toolkit/themes/pmstripe/global/Makefile
    widget/os2/Makefile
    xpcom/reflect/xptcall/src/md/os2/Makefile
  "
fi

if [ "$MOZ_WIDGET_TOOLKIT" != "cocoa" ]; then
  add_makefiles "
    toolkit/themes/winstripe/Makefile
    toolkit/themes/winstripe/global/Makefile
    toolkit/themes/winstripe/mozapps/Makefile
  "
  if [ "$MOZ_THEME_FASTSTRIPE" ]; then
    add_makefiles "
      toolkit/themes/faststripe/global/Makefile
    "
  fi
fi

if [ "$MOZ_WIDGET_TOOLKIT" = "gtk2" -o "$MOZ_WIDGET_TOOLKIT" = "qt" ]; then
  add_makefiles "
    content/xbl/builtin/unix/Makefile
    dom/system/unix/Makefile
    toolkit/system/unixproxy/Makefile
    toolkit/themes/gnomestripe/Makefile
    toolkit/themes/gnomestripe/global/Makefile
    toolkit/themes/gnomestripe/mozapps/Makefile
  "
fi

if [ "$OS_ARCH" != "WINNT" -a "$OS_ARCH" != "OS2" ]; then
  add_makefiles "
    xpcom/reflect/xptcall/src/md/unix/Makefile
  "
  if [ "$MOZ_WIDGET_TOOLKIT" != "cocoa" \
    -a "$MOZ_WIDGET_TOOLKIT" != "gtk2" \
    -a "$MOZ_WIDGET_TOOLKIT" != "qt" ]
  then
    add_makefiles "
      content/xbl/builtin/emacs/Makefile
    "
  fi
fi

if [ "$MOZ_WIDGET_TOOLKIT" != "windows" \
  -a "$MOZ_WIDGET_TOOLKIT" != "cocoa" \
  -a "$MOZ_WIDGET_TOOLKIT" != "os2" ]
then
  add_makefiles "
    intl/locale/src/unix/Makefile
  "
fi


#
# Tests
#

if [ "$ENABLE_TESTS" ]; then
  add_makefiles "
    addon-sdk/test/Makefile
    caps/tests/mochitest/Makefile
    chrome/test/Makefile
    content/base/test/Makefile
    content/base/test/chrome/Makefile
    content/base/test/websocket_hybi/Makefile
    content/canvas/test/Makefile
    content/canvas/test/crossorigin/Makefile
    content/canvas/test/webgl/Makefile
    content/events/test/Makefile
    content/html/content/test/Makefile
    content/html/content/test/bug649134/Makefile
    content/html/content/test/forms/Makefile
    content/html/document/test/Makefile
    content/smil/test/Makefile
    content/svg/content/test/Makefile
    content/test/Makefile
    content/xbl/test/Makefile
    content/xml/document/test/Makefile
    content/xslt/tests/buster/Makefile
    content/xslt/tests/mochitest/Makefile
    content/xul/content/test/Makefile
    content/xul/document/test/Makefile
    docshell/test/Makefile
    docshell/test/chrome/Makefile
    docshell/test/navigation/Makefile
    dom/alarm/test/Makefile
    dom/apps/tests/Makefile
    dom/base/test/Makefile
    dom/battery/test/Makefile
    dom/bindings/test/Makefile
    dom/browser-element/mochitest/Makefile
    dom/contacts/tests/Makefile
    dom/devicestorage/test/Makefile
    dom/file/test/Makefile
    dom/identity/tests/Makefile
    dom/imptests/Makefile
    dom/imptests/editing/Makefile
    dom/imptests/editing/conformancetest/Makefile
    dom/imptests/editing/css/Makefile
    dom/imptests/editing/selecttest/Makefile
    dom/imptests/failures/editing/conformancetest/Makefile
    dom/imptests/failures/editing/selecttest/Makefile
    dom/imptests/failures/html/tests/submission/Opera/microdata/Makefile
    dom/imptests/failures/webapps/DOMCore/tests/approved/Makefile
    dom/imptests/failures/webapps/DOMCore/tests/submissions/Opera/Makefile
    dom/imptests/failures/webapps/WebStorage/tests/submissions/Infraware/Makefile
    dom/imptests/failures/webapps/WebStorage/tests/submissions/Ms2ger/Makefile
    dom/imptests/html/tests/submission/Mozilla/Makefile
    dom/imptests/html/tests/submission/Opera/microdata/Makefile
    dom/imptests/webapps/DOMCore/tests/approved/Makefile
    dom/imptests/webapps/DOMCore/tests/submissions/Opera/Makefile
    dom/imptests/webapps/WebStorage/tests/submissions/Infraware/Makefile
    dom/imptests/webapps/WebStorage/tests/submissions/Infraware/iframe/Makefile
    dom/imptests/webapps/WebStorage/tests/submissions/Makefile
    dom/imptests/webapps/WebStorage/tests/submissions/Ms2ger/Makefile
    dom/imptests/webapps/XMLHttpRequest/tests/submissions/Ms2ger/Makefile
    dom/indexedDB/test/Makefile
    dom/indexedDB/test/unit/Makefile
    dom/network/tests/Makefile
    dom/plugins/test/Makefile
    dom/plugins/test/testplugin/Makefile
    dom/power/test/Makefile
    dom/settings/tests/Makefile
    dom/sms/tests/Makefile
    dom/src/foo/Makefile
    dom/src/json/test/Makefile
    dom/src/jsurl/test/Makefile
    dom/system/tests/Makefile
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
    dom/tests/mochitest/orientation/Makefile
    dom/tests/mochitest/pointerlock/Makefile
    dom/tests/mochitest/sessionstorage/Makefile
    dom/tests/mochitest/storageevent/Makefile
    dom/tests/mochitest/webapps/Makefile
    dom/tests/mochitest/webapps/apps/Makefile
    dom/tests/mochitest/whatwg/Makefile
    dom/workers/test/Makefile
    dom/workers/test/extensions/Makefile
    dom/workers/test/extensions/bootstrap/Makefile
    dom/workers/test/extensions/traditional/Makefile
    editor/composer/test/Makefile
    editor/libeditor/base/tests/Makefile
    editor/libeditor/html/tests/Makefile
    editor/libeditor/text/tests/Makefile
    editor/txmgr/tests/Makefile
    embedding/test/Makefile
    gfx/tests/Makefile
    image/test/Makefile
    image/test/mochitest/Makefile
    intl/locale/tests/Makefile
    intl/lwbrk/tests/Makefile
    intl/strres/tests/Makefile
    intl/uconv/tests/Makefile
    intl/unicharutil/tests/Makefile
    js/xpconnect/tests/Makefile
    js/xpconnect/tests/chrome/Makefile
    js/xpconnect/tests/components/js/Makefile
    js/xpconnect/tests/components/native/Makefile
    js/xpconnect/tests/idl/Makefile
    js/xpconnect/tests/mochitest/Makefile
    testing/specialpowers/Makefile
    layout/base/tests/Makefile
    layout/base/tests/chrome/Makefile
    layout/base/tests/cpp-tests/Makefile
    layout/forms/test/Makefile
    layout/generic/test/Makefile
    layout/inspector/tests/Makefile
    layout/inspector/tests/chrome/Makefile
    layout/mathml/tests/Makefile
    layout/reftests/fonts/Makefile
    layout/reftests/fonts/mplus/Makefile
    layout/style/test/Makefile
    layout/style/test/chrome/Makefile
    layout/tables/test/Makefile
    layout/tools/reftest/Makefile
    layout/xul/base/test/Makefile
    layout/xul/test/Makefile
    media/webrtc/signaling/test/Makefile
    modules/libjar/test/Makefile
    modules/libjar/test/chrome/Makefile
    modules/libjar/test/mochitest/Makefile
    modules/libpref/test/Makefile
    netwerk/streamconv/test/Makefile
    netwerk/test/Makefile
    netwerk/test/browser/Makefile
    netwerk/test/httpserver/Makefile
    parser/htmlparser/tests/Makefile
    parser/htmlparser/tests/mochitest/Makefile
    parser/htmlparser/tests/mochitest/dir_bug534293/Makefile
    parser/htmlparser/tests/mochitest/html5lib_tree_construction/Makefile
    parser/htmlparser/tests/mochitest/html5lib_tree_construction/scripted/Makefile
    parser/xml/test/Makefile
    rdf/tests/Makefile
    rdf/tests/rdfcat/Makefile
    rdf/tests/rdfpoll/Makefile
    rdf/tests/triplescat/Makefile
    services/crypto/component/tests/Makefile
    startupcache/test/Makefile
    storage/test/Makefile
    testing/mochitest/Makefile
    testing/mochitest/MochiKit/Makefile
    testing/mochitest/chrome/Makefile
    testing/mochitest/dynamic/Makefile
    testing/mochitest/ssltunnel/Makefile
    testing/mochitest/static/Makefile
    testing/mochitest/tests/Makefile
    testing/mochitest/tests/MochiKit-1.4.2/Makefile
    testing/mochitest/tests/MochiKit-1.4.2/MochiKit/Makefile
    testing/mochitest/tests/MochiKit-1.4.2/tests/Makefile
    testing/mochitest/tests/MochiKit-1.4.2/tests/SimpleTest/Makefile
    testing/mochitest/tests/SimpleTest/Makefile
    testing/mochitest/tests/browser/Makefile
    testing/mozbase/Makefile
    testing/peptest/Makefile
    testing/tools/screenshot/Makefile
    testing/xpcshell/Makefile
    testing/xpcshell/example/Makefile
    toolkit/components/aboutmemory/tests/Makefile
    toolkit/components/alerts/test/Makefile
    toolkit/components/commandlines/test/Makefile
    toolkit/components/contentprefs/tests/Makefile
    toolkit/components/downloads/test/Makefile
    toolkit/components/microformats/tests/Makefile
    toolkit/components/osfile/tests/Makefile
    toolkit/components/osfile/tests/mochi/Makefile
    toolkit/components/passwordmgr/test/Makefile
    toolkit/components/places/tests/Makefile
    toolkit/components/places/tests/chrome/Makefile
    toolkit/components/places/tests/cpp/Makefile
    toolkit/components/places/tests/mochitest/bug_411966/Makefile
    toolkit/components/places/tests/mochitest/bug_461710/Makefile
    toolkit/components/prompts/test/Makefile
    toolkit/components/satchel/test/Makefile
    toolkit/components/social/test/Makefile
    toolkit/components/social/test/browser/Makefile
    toolkit/components/telemetry/tests/Makefile
    toolkit/components/urlformatter/tests/Makefile
    toolkit/components/viewsource/test/Makefile
    toolkit/components/viewsource/test/browser/Makefile
    toolkit/content/tests/Makefile
    toolkit/content/tests/chrome/Makefile
    toolkit/content/tests/chrome/rtlchrome/Makefile
    toolkit/content/tests/chrome/rtltest/Makefile
    toolkit/content/tests/widgets/Makefile
    toolkit/devtools/debugger/tests/Makefile
    toolkit/devtools/sourcemap/tests/Makefile
    toolkit/identity/tests/Makefile
    toolkit/identity/tests/chrome/Makefile
    toolkit/identity/tests/mochitest/Makefile
    toolkit/mozapps/downloads/tests/Makefile
    toolkit/mozapps/downloads/tests/chrome/Makefile
    toolkit/mozapps/extensions/test/Makefile
    toolkit/mozapps/plugins/tests/Makefile
    toolkit/mozapps/shared/test/chrome/Makefile
    toolkit/mozapps/update/test_timermanager/Makefile
    toolkit/profile/test/Makefile
    toolkit/xre/test/Makefile
    uriloader/exthandler/tests/Makefile
    uriloader/exthandler/tests/mochitest/Makefile
    widget/tests/Makefile
    xpcom/sample/Makefile
    xpcom/sample/program/Makefile
    xpcom/tests/Makefile
    xpcom/tests/bug656331_component/Makefile
    xpcom/tests/component/Makefile
    xpcom/tests/component_no_aslr/Makefile
    xpcom/tests/external/Makefile
    xpcom/typelib/xpt/tests/Makefile
  "
  if [ "$ACCESSIBILITY" ]; then
    add_makefiles "
      accessible/tests/Makefile
      accessible/tests/mochitest/Makefile
      accessible/tests/mochitest/actions/Makefile
      accessible/tests/mochitest/attributes/Makefile
      accessible/tests/mochitest/bounds/Makefile
      accessible/tests/mochitest/editabletext/Makefile
      accessible/tests/mochitest/elm/Makefile
      accessible/tests/mochitest/events/Makefile
      accessible/tests/mochitest/focus/Makefile
      accessible/tests/mochitest/hittest/Makefile
      accessible/tests/mochitest/hyperlink/Makefile
      accessible/tests/mochitest/hypertext/Makefile
      accessible/tests/mochitest/name/Makefile
      accessible/tests/mochitest/pivot/Makefile
      accessible/tests/mochitest/relations/Makefile
      accessible/tests/mochitest/role/Makefile
      accessible/tests/mochitest/selectable/Makefile
      accessible/tests/mochitest/states/Makefile
      accessible/tests/mochitest/table/Makefile
      accessible/tests/mochitest/text/Makefile
      accessible/tests/mochitest/textcaret/Makefile
      accessible/tests/mochitest/textselection/Makefile
      accessible/tests/mochitest/tree/Makefile
      accessible/tests/mochitest/treeupdate/Makefile
      accessible/tests/mochitest/value/Makefile
    "
  fi
  if [ "$BUILD_CTYPES" ]; then
    add_makefiles "
      toolkit/components/ctypes/tests/Makefile
    "
  fi
  if [ "$DEHYDRA_PATH" ]; then
    add_makefiles "
      xpcom/tests/static-checker/Makefile
    "
  fi
  if [ "$MOZ_CRASHREPORTER" ]; then
    add_makefiles "
      toolkit/crashreporter/test/Makefile
    "
  fi
  if [ "$MOZ_FEEDS" ]; then
    add_makefiles "
      toolkit/components/feeds/test/Makefile
    "
  fi
  if [ "$MOZ_JSDEBUGGER" ]; then
    add_makefiles "
      js/jsd/test/Makefile
    "
  fi
  if [ "$MOZ_MEDIA" ]; then
   add_makefiles "
     content/media/test/Makefile
   "
  fi
  if [ "$MOZ_PERMISSIONS" ]; then
    add_makefiles "
      extensions/cookie/test/Makefile
    "
  fi
  if [ "$MOZ_PSM" ]; then
    add_makefiles "
      security/manager/ssl/tests/Makefile
      security/manager/ssl/tests/mochitest/Makefile
      security/manager/ssl/tests/mochitest/bugs/Makefile
      security/manager/ssl/tests/mochitest/mixedcontent/Makefile
      security/manager/ssl/tests/mochitest/stricttransportsecurity/Makefile
    "
  fi
  if [ "$MOZ_SPELLCHECK" ]; then
    add_makefiles "
      extensions/spellcheck/tests/chrome/Makefile
      extensions/spellcheck/tests/chrome/base/Makefile
      extensions/spellcheck/tests/chrome/map/Makefile
    "
  fi
  if [ "$MOZ_TOOLKIT_SEARCH" ]; then
    add_makefiles "
      toolkit/components/search/tests/Makefile
    "
  fi
  if [ "$MOZ_UNIVERSALCHARDET" ]; then
    add_makefiles "
      extensions/universalchardet/tests/Makefile
    "
  fi
  if [ "$MOZ_UPDATER" ]; then
    add_makefiles "
      toolkit/mozapps/update/test/Makefile
    "
    if [ "$OS_TARGET" != "Android" ]; then
      add_makefiles "
        toolkit/mozapps/update/test/chrome/Makefile
      "
    fi
    if [ "$MOZ_MAINTENANCE_SERVICE" ]; then
      add_makefiles "
        toolkit/mozapps/update/test_svc/Makefile
      "
    fi
  fi
  if [ "$MOZ_URL_CLASSIFIER" ]; then
    add_makefiles "
      toolkit/components/url-classifier/tests/Makefile
      toolkit/components/url-classifier/tests/mochitest/Makefile
    "
  fi
  if [ "$MOZ_XUL" ]; then
    add_makefiles "
      content/xul/templates/tests/chrome/Makefile
      toolkit/components/autocomplete/tests/Makefile
    "
  fi
  if [ "$MOZ_ZIPWRITER" ]; then
    add_makefiles "
      modules/libjar/zipwriter/test/Makefile
    "
  fi
  if [ "$MOZ_BUILD_APP" != "mobile" ]; then
    add_makefiles "
      docshell/test/browser/Makefile
      dom/tests/browser/Makefile
      image/test/browser/Makefile
      toolkit/components/downloads/test/browser/Makefile
      toolkit/components/passwordmgr/test/browser/Makefile
      toolkit/components/places/tests/browser/Makefile
      toolkit/components/startup/tests/browser/Makefile
      toolkit/content/tests/browser/Makefile
      toolkit/content/tests/browser/common/Makefile
      toolkit/content/tests/browser/data/Makefile
      toolkit/mozapps/extensions/test/browser/Makefile
      toolkit/mozapps/extensions/test/mochitest/Makefile
      toolkit/mozapps/extensions/test/xpinstall/Makefile
    "
  fi
  if [ "$MOZ_WIDGET_TOOLKIT" = "cocoa" ]; then
    add_makefiles "
      toolkit/themes/pinstripe/mochitests/Makefile
    "
  else
    add_makefiles "
      dom/ipc/tests/Makefile
    "
  fi
  if [ "$MOZ_WIDGET_TOOLKIT" = "windows" \
    -o "$MOZ_WIDGET_TOOLKIT" = "cocoa" \
    -o "$MOZ_WIDGET_TOOLKIT" = "gtk2" ]
  then
    add_makefiles "
      dom/plugins/test/mochitest/Makefile
    "
  fi
  if [ "$OS_ARCH" = "WINNT" ]; then
    add_makefiles "
      toolkit/xre/test/win/Makefile
      widget/windows/tests/Makefile
      xpcom/tests/windows/Makefile
    "
  fi
  if [ "$MOZ_BUILD_APP" = "mobile/android" ]; then
    add_makefiles "
      testing/mochitest/roboextender/Makefile
    "
  fi
fi


#
# Feature specific makefiles
#

if [ "$ACCESSIBILITY" ]; then
  add_makefiles "
    accessible/Makefile
    accessible/build/Makefile
    accessible/public/Makefile
    accessible/src/Makefile
    accessible/src/base/Makefile
    accessible/src/generic/Makefile
    accessible/src/html/Makefile
    accessible/src/jsat/Makefile
    accessible/src/xpcom/Makefile
  "
  if [ "$MOZ_XUL" ]; then
    add_makefiles "
      accessible/src/xul/Makefile
    "
  fi
  if [ "$MOZ_WIDGET_TOOLKIT" = "windows" ]; then
    add_makefiles "
      accessible/public/ia2/Makefile
      accessible/public/msaa/Makefile
      accessible/src/msaa/Makefile
    "
  elif [ "$MOZ_WIDGET_TOOLKIT" = "cocoa" ]; then
    add_makefiles "
      accessible/src/mac/Makefile
    "
  elif [ "$MOZ_WIDGET_TOOLKIT" = "gtk2" ]; then
    add_makefiles "
      accessible/src/atk/Makefile
    "
  else
    add_makefiles "
      accessible/src/other/Makefile
    "
  fi
fi

if [ "$BUILD_CTYPES" ]; then
  add_makefiles "
    toolkit/components/ctypes/Makefile
  "
fi

if [ "$DEHYDRA_PATH" ]; then
  add_makefiles "
    xpcom/analysis/Makefile
  "
fi

if [ "$MOZ_ANGLE_RENDERER" ]; then
  add_makefiles "
    gfx/angle/src/libGLESv2/Makefile
    gfx/angle/src/libEGL/Makefile
  "
fi

if [ "$MOZ_B2G_RIL" ]; then
  add_makefiles "
    dom/telephony/Makefile
    dom/wifi/Makefile
    ipc/ril/Makefile
  "
fi

if [ "$MOZ_PAY" ]; then
  add_makefiles "
    dom/payment/Makefile
  "
fi

if [ "$MOZ_B2G_FM" ]; then
  add_makefiles "
    dom/fm/Makefile
  "
fi

if [ "$MOZ_CRASHREPORTER" ]; then
  add_makefiles "
    toolkit/crashreporter/Makefile
  "
  MAKEFILES_crashreporter_shared="
    toolkit/crashreporter/google-breakpad/src/client/Makefile
    toolkit/crashreporter/google-breakpad/src/common/Makefile
    toolkit/crashreporter/google-breakpad/src/common/dwarf/Makefile
  "
  if [ "$OS_ARCH" = "WINNT" ]; then
    add_makefiles "
      toolkit/crashreporter/breakpad-windows-libxul/Makefile
      toolkit/crashreporter/breakpad-windows-standalone/Makefile
      toolkit/crashreporter/injector/Makefile
    "
  elif [ "$OS_ARCH" = "Darwin" ]; then
    add_makefiles "
      $MAKEFILES_crashreporter_shared
      toolkit/crashreporter/google-breakpad/src/client/mac/crash_generation/Makefile
      toolkit/crashreporter/google-breakpad/src/client/mac/handler/Makefile
      toolkit/crashreporter/google-breakpad/src/common/mac/Makefile
      toolkit/crashreporter/google-breakpad/src/tools/mac/dump_syms/Makefile
    "
  elif [ "$OS_ARCH" = "Linux" ]; then
    add_makefiles "
      $MAKEFILES_crashreporter_shared
      toolkit/crashreporter/google-breakpad/src/client/linux/crash_generation/Makefile
      toolkit/crashreporter/google-breakpad/src/client/linux/handler/Makefile
      toolkit/crashreporter/google-breakpad/src/client/linux/minidump_writer/Makefile
      toolkit/crashreporter/google-breakpad/src/common/linux/Makefile
      toolkit/crashreporter/google-breakpad/src/tools/linux/dump_syms/Makefile
    "
    if [ "$OS_TARGET" = "Android" ]; then
      add_makefiles "
        toolkit/crashreporter/fileid/Makefile
      "
    fi
  elif [ "$OS_ARCH" = "SunOS" ]; then
    add_makefiles "
      $MAKEFILES_crashreporter_shared
      toolkit/crashreporter/google-breakpad/src/client/solaris/handler/Makefile
      toolkit/crashreporter/google-breakpad/src/common/solaris/Makefile
      toolkit/crashreporter/google-breakpad/src/tools/solaris/dump_syms/Makefile
    "
  fi
fi

if [ "$MOZ_DEBUG" ]; then
  add_makefiles "
    layout/tools/layout-debug/Makefile
    layout/tools/layout-debug/src/Makefile
    layout/tools/layout-debug/tests/Makefile
    layout/tools/layout-debug/ui/Makefile
  "
  if [ "$MOZ_WIDGET_TOOLKIT" = "windows" ]; then
    add_makefiles "
      xpcom/windbgdlg/Makefile
    "
  fi
fi

if [ "$MOZ_ENABLE_GNOME_COMPONENT" ]; then
  add_makefiles "
    toolkit/system/gnome/Makefile
  "
fi

if [ "$MOZ_ENABLE_LIBCONIC" ]; then
  add_makefiles "
    netwerk/system/maemo/Makefile
  "
  if [ "$MOZ_ENABLE_DBUS" ]; then
    add_makefiles "
      toolkit/system/dbus/Makefile
    "
  fi
fi

if [ "$MOZ_ENABLE_QTNETWORK" ]; then
  add_makefiles "
    netwerk/system/qt/Makefile
  "
fi

if [ "$MOZ_ENABLE_SKIA" ]; then
  add_makefiles "
    gfx/skia/Makefile
  "
fi

if [ "$MOZ_ENABLE_XREMOTE" ]; then
  add_makefiles "
    toolkit/components/remote/Makefile
    widget/xremoteclient/Makefile
  "
fi

if [ "$MOZ_FEEDS" ]; then
  add_makefiles "
    toolkit/components/feeds/Makefile
  "
fi

if [ "$MOZ_GRAPHITE" ]; then
  add_makefiles "
    gfx/graphite2/src/Makefile
  "
fi

if [ "$MOZ_HELP_VIEWER" ]; then
  add_makefiles "
    toolkit/components/help/Makefile
  "
  if [ "$MOZ_WIDGET_TOOLKIT" = "cocoa" ]; then
    add_makefiles "
      toolkit/themes/pinstripe/help/Makefile
    "
  else
    add_makefiles "
      toolkit/themes/winstripe/help/Makefile
    "
    if [ "$MOZ_WIDGET_TOOLKIT" = "gtk2" -o "$MOZ_WIDGET_TOOLKIT" = "qt" ]; then
      add_makefiles "
        toolkit/themes/gnomestripe/help/Makefile
      "
    fi
  fi
fi

if [ "$MOZ_IPDL_TESTS" ]; then
  add_makefiles "
    ipc/ipdl/test/Makefile
    ipc/ipdl/test/cxx/Makefile
    ipc/ipdl/test/cxx/app/Makefile
    ipc/ipdl/test/ipdl/Makefile
  "
fi

if [ "$MOZ_JSDEBUGGER" ]; then
  add_makefiles "
    js/jsd/Makefile
    js/jsd/idl/Makefile
  "
fi

if [ "$MOZ_MAINTENANCE_SERVICE" ]; then
  add_makefiles "
    toolkit/components/maintenanceservice/Makefile
  "
fi

if [ ! "$MOZ_NATIVE_SQLITE" ]; then
  add_makefiles "
    db/sqlite3/src/Makefile
  "
fi

if [ "$MOZ_PERMISSIONS" ]; then
  add_makefiles "
    extensions/cookie/Makefile
    extensions/permissions/Makefile
  "
fi

if [ "$MOZ_PREF_EXTENSIONS" ]; then
  add_makefiles "
    extensions/pref/Makefile
    extensions/pref/autoconfig/Makefile
    extensions/pref/autoconfig/public/Makefile
    extensions/pref/autoconfig/src/Makefile
  "
fi

if [ "$MOZ_SPELLCHECK" ]; then
  add_makefiles "
    extensions/spellcheck/Makefile
    extensions/spellcheck/hunspell/Makefile
    extensions/spellcheck/hunspell/src/Makefile
    extensions/spellcheck/idl/Makefile
    extensions/spellcheck/locales/Makefile
    extensions/spellcheck/src/Makefile
  "
fi

if [ "$MOZ_TOOLKIT_SEARCH" ]; then
  add_makefiles "
    toolkit/components/search/Makefile
  "
fi

if [ "$MOZ_UPDATER" ]; then
  add_makefiles "
    modules/libmar/Makefile
    modules/libmar/src/Makefile
    modules/libmar/tool/Makefile
  "
  if [ ! "$MOZ_NATIVE_BZ2" ]; then
    add_makefiles "
      modules/libbz2/Makefile
      modules/libbz2/src/Makefile
    "
  fi
  if [ "$MOZ_WIDGET_TOOLKIT" != "android" ]; then
    add_makefiles "
      toolkit/mozapps/update/updater/Makefile
    "
  fi
fi

if [ "$MOZ_UPDATER" -o "$MOZ_UPDATE_PACKAGING" ]; then
  add_makefiles "
    other-licenses/bsdiff/Makefile
  "
fi

if [ "$MOZ_URL_CLASSIFIER" ]; then
  add_makefiles "
    toolkit/components/url-classifier/Makefile
  "
fi

if [ "$MOZ_X11" ]; then
  add_makefiles "
    widget/shared/x11/Makefile
  "
  if [ "$MOZ_WIDGET_TOOLKIT" = "gtk2" ]; then
    add_makefiles "
      widget/gtkxtbin/Makefile
    "
  fi
fi

if [ "$MOZ_XUL" ]; then
  add_makefiles "
    content/xul/templates/Makefile
    content/xul/templates/public/Makefile
    content/xul/templates/src/Makefile
    layout/xul/grid/Makefile
    layout/xul/tree/Makefile
    toolkit/components/autocomplete/Makefile
    toolkit/components/satchel/Makefile
  "
fi

if [ "$NECKO_WIFI" ]; then
  add_makefiles "
    netwerk/wifi/Makefile
  "
fi

if [ "$NS_PRINTING" ]; then
  add_makefiles "
    layout/printing/Makefile
    toolkit/components/printing/Makefile
  "
  if [ "$MOZ_XUL" ]; then
    add_makefiles "
      embedding/components/printingui/src/Makefile
    "
    if [ "$MOZ_WIDGET_TOOLKIT" = "windows" ]; then
      add_makefiles "
        embedding/components/printingui/src/win/Makefile
      "
    elif [ "$MOZ_WIDGET_TOOLKIT" = "cocoa" ]; then
      add_makefiles "
        embedding/components/printingui/src/mac/Makefile
      "
    elif [ "$MOZ_WIDGET_TOOLKIT" = "os2" ]; then
      add_makefiles "
        embedding/components/printingui/src/os2/Makefile
      "
    elif [ "$MOZ_PDF_PRINTING" ]; then
      add_makefiles "
        embedding/components/printingui/src/unixshared/Makefile
      "
    fi
  fi
fi

if [ "$MOZ_ZIPWRITER" ]; then
  add_makefiles "
    modules/libjar/zipwriter/Makefile
    modules/libjar/zipwriter/public/Makefile
    modules/libjar/zipwriter/src/Makefile
  "
fi

if [ "$MOZ_TREE_CAIRO" ]; then
  add_makefiles "
    gfx/cairo/Makefile
    gfx/cairo/cairo/src/Makefile
    gfx/cairo/cairo/src/cairo-features.h
  "
  if [ "$MOZ_TREE_PIXMAN" ]; then
    add_makefiles "
      gfx/cairo/libpixman/src/Makefile
    "
  fi
fi

if [ "$MOZ_UNIVERSALCHARDET" ]; then
  add_makefiles "
    extensions/universalchardet/Makefile
    extensions/universalchardet/src/Makefile
    extensions/universalchardet/src/base/Makefile
    extensions/universalchardet/src/xpcom/Makefile
  "
fi

if [ "$MOZ_AUTH_EXTENSION" ]; then
  add_makefiles "
    extensions/auth/Makefile
  "
fi

if [ "$MOZ_PSM" ]; then
  add_makefiles "
    security/build/Makefile
    security/manager/Makefile
    security/manager/boot/Makefile
    security/manager/boot/src/Makefile
    security/manager/ssl/Makefile
    security/manager/ssl/src/Makefile
    security/manager/pki/Makefile
    security/manager/pki/resources/Makefile
    security/manager/pki/src/Makefile
    security/manager/pki/public/Makefile
    security/manager/locales/Makefile
  "
fi

if [ ! "$MOZ_NATIVE_JPEG" ]; then
  add_makefiles "
    media/libjpeg/Makefile
    media/libjpeg/simd/Makefile
  "
fi

if [ ! "$MOZ_NATIVE_ZLIB" ]; then
  add_makefiles "
    modules/zlib/Makefile
    modules/zlib/src/Makefile
  "
fi

if [ "$MOZ_UPDATE_PACKAGING" ]; then
  add_makefiles "
    tools/update-packaging/Makefile
  "
fi

if [ ! "$MOZ_NATIVE_PNG" ]; then
  add_makefiles "
    media/libpng/Makefile
  "
fi

if [ "$MOZ_JPROF" ]; then
  add_makefiles "
    tools/jprof/Makefile
    tools/jprof/stub/Makefile
  "
fi

if [ "$NS_TRACE_MALLOC" ]; then
  add_makefiles "
    tools/trace-malloc/Makefile
    tools/trace-malloc/lib/Makefile
  "
fi

if [ "$MOZ_DMD" ]; then
  add_makefiles "
    memory/replace/dmd/Makefile
  "
fi

if [ "$MOZ_MAPINFO" ]; then
  add_makefiles "
    tools/codesighs/Makefile
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
 "
fi

if [ "$MOZ_VORBIS" ]; then
  add_makefiles "
    media/libvorbis/Makefile
    media/libvorbis/lib/Makefile
    media/libvorbis/include/Makefile
    media/libvorbis/include/vorbis/Makefile
  "
fi

if [ "$MOZ_TREMOR" ]; then
  add_makefiles "
    media/libtremor/Makefile
    media/libtremor/lib/Makefile
    media/libtremor/include/tremor/Makefile
  "
fi

if [ "$MOZ_OPUS" ]; then
 add_makefiles "
   media/libopus/Makefile
 "
fi

if [ "$MOZ_OGG" ]; then
  add_makefiles "
    content/media/ogg/Makefile
    media/libogg/Makefile
    media/libogg/src/Makefile
    media/libogg/include/Makefile
    media/libogg/include/ogg/Makefile
    media/libtheora/Makefile
    media/libtheora/lib/Makefile
    media/libtheora/include/Makefile
    media/libtheora/include/theora/Makefile
  "
fi

if [ "$MOZ_RAW" ]; then
 add_makefiles "
   content/media/raw/Makefile
 "
fi

if [ "$MOZ_WEBM" ]; then
  add_makefiles "
    content/media/webm/Makefile
    media/libnestegg/Makefile
    media/libnestegg/include/Makefile
    media/libnestegg/src/Makefile
  "
fi
if [ "$MOZ_VP8" ]; then
  if [ ! "$MOZ_NATIVE_LIBVPX" ]; then
    add_makefiles "
      media/libvpx/Makefile
    "
  fi
fi

if [ "$MOZ_MEDIA_PLUGINS" ]; then
  add_makefiles "
    content/media/plugins/Makefile
  "
fi

if [ "$MOZ_OMX_PLUGIN" ]; then
  add_makefiles "
    media/omx-plugin/Makefile
    media/omx-plugin/lib/ics/libutils/Makefile
    media/omx-plugin/lib/ics/libstagefright/Makefile
  "
fi

if [ "$MOZ_WAVE" ]; then
 add_makefiles "
   content/media/wave/Makefile
 "
fi

if [ "$MOZ_CUBEB" ]; then
  add_makefiles "
    media/libcubeb/Makefile
    media/libcubeb/include/Makefile
    media/libcubeb/src/Makefile
  "
fi

if [ "$MOZ_SYDNEYAUDIO" ]; then
  add_makefiles "
    media/libsydneyaudio/Makefile
    media/libsydneyaudio/include/Makefile
    media/libsydneyaudio/src/Makefile
  "
fi

if [ "$MOZ_WEBRTC" ]; then
 add_makefiles "
   media/webrtc/Makefile
   media/mtransport/test/Makefile 
   media/mtransport/build/Makefile
   media/mtransport/standalone/Makefile
   media/webrtc/signaling/test/Makefile
 "
fi

if [ "$MOZ_SPEEX_RESAMPLER" ]; then
  add_makefiles "
    media/libspeex_resampler/Makefile
    media/libspeex_resampler/src/Makefile
  "
fi

if [ "$MOZ_SOUNDTOUCH" ]; then
  add_makefiles "
    media/libsoundtouch/Makefile
    media/libsoundtouch/src/Makefile
  "
fi

