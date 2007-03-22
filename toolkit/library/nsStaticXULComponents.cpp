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

#include "nsIGenericFactory.h"
#include "nsXPCOM.h"
#include "nsStaticComponents.h"
#include "nsMemory.h"

#define NSGETMODULE(_name) _name##_NSGetModule

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

#ifdef MOZ_MATHML
#define MATHML_MODULES MODULE(nsUCvMathModule)
#else
#define MATHML_MODULES
#endif

#ifdef MOZ_IPCD
#define IPC_MODULE MODULE(ipcdclient)
#else
#define IPC_MODULE
#endif

#ifdef MOZ_CAIRO_GFX
#  define GFX_MODULES MODULE(nsGfxModule)
#else
#  if defined(MOZ_WIDGET_GTK) || defined(MOZ_WIDGET_GTK2)
#    define GFX_MODULES MODULE(nsGfxGTKModule)
#  elif defined(MOZ_WIDGET_QT)
#    define GFX_MODULES MODULE(nsGfxQtModule)
#  elif defined(MOZ_WIDGET_XLIB)
#    define GFX_MODULES MODULE(nsGfxXlibModule)
#  elif defined(MOZ_WIDGET_PHOTON)
#    define GFX_MODULES MODULE(nsGfxPhModule)
#  elif defined(XP_WIN)
#    define GFX_MODULES MODULE(nsGfxModule)
#  elif defined(XP_MACOSX)
#    define GFX_MODULES MODULE(nsGfxMacModule)
#  elif defined(XP_BEOS)
#    define GFX_MODULES MODULE(nsGfxBeOSModule)
#  elif defined(XP_OS2)
#    define GFX_MODULES MODULE(nsGfxOS2Module)
#  else
#    error Unknown GFX module.
#  endif
#endif

#ifdef XP_WIN
#  define WIDGET_MODULES MODULE(nsWidgetModule)
#elif defined(XP_MACOSX)
#  define WIDGET_MODULES MODULE(nsWidgetMacModule)
#elif defined(XP_BEOS)
#  define WIDGET_MODULES MODULE(nsWidgetBeOSModule)
#elif defined(XP_OS2)
#  define WIDGET_MODULES MODULE(nsWidgetOS2Module)
#elif defined(MOZ_WIDGET_GTK)
#  define WIDGET_MODULES MODULE(nsWidgetGTKModule)
#elif defined(MOZ_WIDGET_GTK2)
#  define WIDGET_MODULES MODULE(nsWidgetGtk2Module)
#elif defined(MOZ_WIDGET_XLIB)
#  define WIDGET_MODULES MODULE(nsWidgetXLIBModule)
#elif defined(MOZ_WIDGET_PHOTON)
#  define WIDGET_MODULES MODULE(nsWidgetPhModule)
#elif defined(MOZ_WIDGET_QT)
#  define WIDGET_MODULES MODULE(nsWidgetQtModule)
#else
#  error Unknown widget module.
#endif

#ifdef ICON_DECODER
#define ICON_MODULE MODULE(nsIconDecoderModule)
#else
#define ICON_MODULE
#endif

#ifdef MOZ_ENABLE_XPRINT
#define XPRINT_MODULES MODULE(nsGfxXprintModule)
#else
#define XPRINT_MODULES
#endif

#ifdef MOZ_RDF
#define RDF_MODULE MODULE(nsRDFModule)
#else
#define RDF_MODULE
#endif

#ifdef OJI
#define OJI_MODULES MODULE(nsCJVMManagerModule)
#else
#define OJI_MODULES
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

#ifdef MOZ_ENABLE_GTK2
#ifdef MOZ_PREF_EXTENSIONS
#define SYSTEMPREF_MODULES MODULE(system-pref) \
    MODULE(nsAutoConfigModule)
#else
#define SYSTEMPREF_MODULES
#endif
#else
#define SYSTEMPREF_MODULES
#endif

#ifdef MOZ_PLUGINS
#define PLUGINS_MODULES \
    MODULE(nsPluginModule)
#else
#define PLUGINS_MODULES
#endif

#ifdef MOZ_WEBSERVICES
#define WEBSERVICES_MODULES \
    MODULE(nsWebServicesModule)
#else
#define WEBSERVICES_MODULES
#endif

#ifdef MOZ_XPFE_COMPONENTS
#ifdef MOZ_RDF
#define RDFAPP_MODULES \
    MODULE(nsXPIntlModule) \
    MODULE(nsWindowDataSourceModule)
#else
#define RDFAPP_MODULES
#endif
#define APPLICATION_MODULES \
    MODULE(application) \
    MODULE(nsFindComponent)
#else
#define APPLICATION_MODULES
#define RDFAPP_MODULES
#endif

#ifdef MOZ_XPINSTALL
#define XPINSTALL_MODULES \
    MODULE(nsSoftwareUpdate)
#else
#define XPINSTALL_MODULES
#endif

#ifdef MOZ_JSDEBUGGER
#define JSDEBUGGER_MODULES \
    MODULE(JavaScript_Debugger)
#else
#define JSDEBUGGER_MODULES
#endif

#if defined(MOZ_FILEVIEW) && defined(MOZ_XPFE_COMPONENTS)
#define FILEVIEW_MODULE MODULE(nsFileViewModule)
#else
#define FILEVIEW_MODULE
#endif

#ifdef MOZ_STORAGE
#define STORAGE_MODULE MODULE(mozStorageModule)
#else
#define STORAGE_MODULE
#endif

#ifdef MOZ_PLACES
#define PLACES_MODULES \
    MODULE(nsPlacesModule)
#else
#if (defined(MOZ_MORK) && defined(MOZ_XUL))
#define PLACES_MODULES \
    MODULE(nsMorkModule)                     \
    MODULE(nsToolkitHistory)
#else
#define PLACES_MODULES
#endif
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

#define XUL_MODULES                          \
    MODULE(xpconnect)                        \
    MATHML_MODULES                           \
    MODULE(nsUConvModule)                    \
    MODULE(nsI18nModule)                     \
    MODULE(nsChardetModule)                  \
    UNIVERSALCHARDET_MODULE                  \
    MODULE(necko)                            \
    PERMISSIONS_MODULES                      \
    AUTH_MODULE                              \
    IPC_MODULE                               \
    MODULE(nsJarModule)                      \
    MODULE(nsPrefModule)                     \
    MODULE(nsSecurityManagerModule)          \
    RDF_MODULE                               \
    RDFAPP_MODULES                           \
    MODULE(nsParserModule)                   \
    GFX_MODULES                              \
    WIDGET_MODULES                           \
    MODULE(nsImageLib2Module)                \
    ICON_MODULE                              \
    PLUGINS_MODULES                          \
    MODULE(nsLayoutModule)                   \
    MODULE(nsXMLExtrasModule)                \
    WEBSERVICES_MODULES                      \
    MODULE(docshell_provider)                \
    MODULE(embedcomponents)                  \
    MODULE(Browser_Embedding_Module)         \
    OJI_MODULES                              \
    ACCESS_MODULES                           \
    MODULE(appshell)                         \
    MODULE(nsTransactionManagerModule)       \
    COMPOSER_MODULE                          \
    MODULE(nsChromeModule)                   \
    APPLICATION_MODULES                      \
    MODULE(Apprunner)                        \
    MODULE(CommandLineModule)                \
    FILEVIEW_MODULE                          \
    STORAGE_MODULE                           \
    PLACES_MODULES                           \
    XULENABLED_MODULES                       \
    MODULE(nsToolkitCompsModule)             \
    XREMOTE_MODULES                          \
    XPINSTALL_MODULES                        \
    JSDEBUGGER_MODULES                       \
    MODULE(BOOT)                             \
    MODULE(NSS)                              \
    SYSTEMPREF_MODULES                       \
    SPELLCHECK_MODULE                        \
    /* end of list */

#define MODULE(_name) \
NSGETMODULE_ENTRY_POINT(_name) (nsIComponentManager*, nsIFile*, nsIModule**);

XUL_MODULES

#undef MODULE

#define MODULE(_name) { #_name, NSGETMODULE(_name) },

/**
 * The nsStaticModuleInfo
 */
static nsStaticModuleInfo const gStaticModuleInfo[] = {
    XUL_MODULES
};

nsStaticModuleInfo const *const kPStaticModules = gStaticModuleInfo;
PRUint32 const kStaticModuleCount = NS_ARRAY_LENGTH(gStaticModuleInfo);
