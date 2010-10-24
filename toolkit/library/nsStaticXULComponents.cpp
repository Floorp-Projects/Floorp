/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Seawood <cls@seawood.org>
 *   Chris Waterson <waterson@netscape.com>
 *   Benjamin Smedberg <bsmedberg@covad.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#define XPCOM_TRANSLATE_NSGM_ENTRY_POINT 1

#include "mozilla/Module.h"
#include "nsXPCOM.h"
#include "nsStaticComponents.h"
#include "nsMemory.h"

#ifdef MOZ_AUTH_EXTENSION
#define AUTH_MODULE    MODULE(nsAuthModule)
#else
#define AUTH_MODULE
#endif

#ifdef MOZ_PERMISSIONS
#define PERMISSIONS_MODULES                  \
    MODULE(nsCookieModule)                   \
    MODULE(nsPermissionsModule)
#else
#define PERMISSIONS_MODULES
#endif

#ifdef MOZ_UNIVERSALCHARDET
#define UNIVERSALCHARDET_MODULE MODULE(nsUniversalCharDetModule)
#else
#define UNIVERSALCHARDET_MODULE
#endif

#define GFX_MODULES MODULE(nsGfxModule)

#ifdef XP_WIN
#  define WIDGET_MODULES MODULE(nsWidgetModule)
#elif defined(XP_MACOSX)
#  define WIDGET_MODULES MODULE(nsWidgetMacModule)
#elif defined(XP_BEOS)
#  define WIDGET_MODULES MODULE(nsWidgetBeOSModule)
#elif defined(XP_OS2)
#  define WIDGET_MODULES MODULE(nsWidgetOS2Module)
#elif defined(MOZ_WIDGET_GTK2)
#  define WIDGET_MODULES MODULE(nsWidgetGtk2Module)
#elif defined(MOZ_WIDGET_QT)
#  define WIDGET_MODULES MODULE(nsWidgetQtModule)
#elif defined(MOZ_WIDGET_ANDROID)
#  define WIDGET_MODULES MODULE(nsWidgetAndroidModule)
#else
#  error Unknown widget module.
#endif

#ifdef ICON_DECODER
#define ICON_MODULE MODULE(nsIconDecoderModule)
#else
#define ICON_MODULE
#endif

#ifdef MOZ_RDF
#define RDF_MODULES \
    MODULE(nsRDFModule) \
    MODULE(nsWindowDataSourceModule)
#else
#define RDF_MODULES
#endif

#ifdef MOZ_PLAINTEXT_EDITOR_ONLY
#define COMPOSER_MODULE
#else
#define COMPOSER_MODULE MODULE(nsComposerModule)
#endif

#ifdef ACCESSIBILITY
#define ACCESS_MODULES MODULE(nsAccessibilityModule)
#else
#define ACCESS_MODULES
#endif

#ifdef MOZ_ENABLE_XREMOTE
#define XREMOTE_MODULES MODULE(RemoteServiceModule)
#else
#define XREMOTE_MODULES
#endif

#ifdef MOZ_PREF_EXTENSIONS
#ifdef MOZ_ENABLE_GTK2
#define SYSTEMPREF_MODULES \
    MODULE(nsSystemPrefModule) \
    MODULE(nsAutoConfigModule)
#else
#define SYSTEMPREF_MODULES MODULE(nsAutoConfigModule)
#endif
#else
#define SYSTEMPREF_MODULES
#endif

#ifdef ENABLE_LAYOUTDEBUG
#define LAYOUT_DEBUG_MODULE MODULE(nsLayoutDebugModule)
#else
#define LAYOUT_DEBUG_MODULE
#endif

#ifdef MOZ_IPC
#define JETPACK_MODULES \
    MODULE(jetpack)
#else
#define JETPACK_MODULES
#endif

#ifdef MOZ_PLUGINS
#define PLUGINS_MODULES \
    MODULE(nsPluginModule)
#else
#define PLUGINS_MODULES
#endif

#ifdef MOZ_JSDEBUGGER
#define JSDEBUGGER_MODULES \
    MODULE(JavaScript_Debugger)
#else
#define JSDEBUGGER_MODULES
#endif

#if defined(MOZ_FILEVIEW) && defined(MOZ_XUL)
#define FILEVIEW_MODULE MODULE(nsFileViewModule)
#else
#define FILEVIEW_MODULE
#endif

#ifdef MOZ_STORAGE
#define STORAGE_MODULE MODULE(mozStorageModule)
#else
#define STORAGE_MODULE
#endif

#ifdef MOZ_ZIPWRITER
#define ZIPWRITER_MODULE MODULE(ZipWriterModule)
#else
#define ZIPWRITER_MODULE
#endif

#ifdef MOZ_PLACES
#define PLACES_MODULES \
    MODULE(nsPlacesModule)
#else
#define PLACES_MODULES
#endif

#if (defined(MOZ_MORK) && defined(MOZ_XUL))
#define MORK_MODULES \
    MODULE(nsMorkModule)
#else
#define MORK_MODULES
#endif

#ifdef MOZ_XUL
#define XULENABLED_MODULES                   \
    MODULE(tkAutoCompleteModule)             \
    MODULE(satchel)                          \
    MODULE(PKI)
#else
#define XULENABLED_MODULES
#endif

#ifdef MOZ_SPELLCHECK
#define SPELLCHECK_MODULE MODULE(mozSpellCheckerModule)
#else
#define SPELLCHECK_MODULE
#endif

#ifdef MOZ_XUL
#ifdef MOZ_ENABLE_GTK2
#define UNIXPROXY_MODULE MODULE(nsUnixProxyModule)
#endif
#if defined(MOZ_WIDGET_QT)
#define UNIXPROXY_MODULE MODULE(nsUnixProxyModule)
#endif
#endif
#ifndef UNIXPROXY_MODULE
#define UNIXPROXY_MODULE
#endif

#if defined(XP_MACOSX)
#define OSXPROXY_MODULE MODULE(nsOSXProxyModule)
#else
#define OSXPROXY_MODULE
#endif

#if defined(XP_WIN)
#define WINDOWSPROXY_MODULE MODULE(nsWindowsProxyModule)
#else
#define WINDOWSPROXY_MODULE
#endif

#if defined(BUILD_CTYPES)
#define JSCTYPES_MODULE MODULE(jsctypes)
#else
#define JSCTYPES_MODULE
#endif

#if defined(MOZ_APP_COMPONENT_INCLUDE)
#include MOZ_APP_COMPONENT_INCLUDE
#else
#define APP_COMPONENT_MODULES
#endif

#define XUL_MODULES                          \
    MODULE(nsUConvModule)                    \
    MODULE(nsI18nModule)                     \
    MODULE(nsChardetModule)                  \
    UNIVERSALCHARDET_MODULE                  \
    MODULE(necko)                            \
    PERMISSIONS_MODULES                      \
    AUTH_MODULE                              \
    MODULE(nsJarModule)                      \
    ZIPWRITER_MODULE                         \
    MODULE(StartupCacheModule)               \
    MODULE(nsPrefModule)                     \
    RDF_MODULES                              \
    MODULE(nsParserModule)                   \
    GFX_MODULES                              \
    WIDGET_MODULES                           \
    MODULE(nsImageLib2Module)                \
    ICON_MODULE                              \
    JETPACK_MODULES                          \
    PLUGINS_MODULES                          \
    MODULE(nsLayoutModule)                   \
    MODULE(docshell_provider)                \
    MODULE(embedcomponents)                  \
    MODULE(Browser_Embedding_Module)         \
    ACCESS_MODULES                           \
    MODULE(appshell)                         \
    MODULE(nsTransactionManagerModule)       \
    COMPOSER_MODULE                          \
    MODULE(application)                      \
    MODULE(Apprunner)                        \
    MODULE(CommandLineModule)                \
    FILEVIEW_MODULE                          \
    STORAGE_MODULE                           \
    PLACES_MODULES                           \
    MORK_MODULES                             \
    XULENABLED_MODULES                       \
    MODULE(nsToolkitCompsModule)             \
    XREMOTE_MODULES                          \
    JSDEBUGGER_MODULES                       \
    MODULE(BOOT)                             \
    MODULE(NSS)                              \
    SYSTEMPREF_MODULES                       \
    SPELLCHECK_MODULE                        \
    LAYOUT_DEBUG_MODULE                      \
    UNIXPROXY_MODULE                         \
    OSXPROXY_MODULE                          \
    WINDOWSPROXY_MODULE                      \
    JSCTYPES_MODULE                          \
    MODULE(jsperf)                           \
    APP_COMPONENT_MODULES                    \
    /* end of list */

#define MODULE(_name) \
  NSMODULE_DECL(_name);

XUL_MODULES

#undef MODULE

#define MODULE(_name) \
    NSMODULE_NAME(_name),

static const mozilla::Module *const kStaticModules[] = {
  XUL_MODULES
  NULL
};

#undef MODULE

mozilla::Module const *const *const kPStaticModules = kStaticModules;
