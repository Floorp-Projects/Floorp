/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsWidgetsCID.h"
#include "nsClipboardProxy.h"
#include "nsColorPickerProxy.h"
#include "nsDragServiceProxy.h"
#include "nsFilePickerProxy.h"
#include "nsSoundProxy.h"
#include "mozilla/widget/ScreenManager.h"

using namespace mozilla;
using namespace mozilla::widget;

NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboardProxy)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsColorPickerProxy)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDragServiceProxy)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFilePickerProxy)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSoundProxy)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(ScreenManager,
                                         ScreenManager::GetAddRefedSingleton)

NS_DEFINE_NAMED_CID(NS_CLIPBOARD_CID);
NS_DEFINE_NAMED_CID(NS_COLORPICKER_CID);
NS_DEFINE_NAMED_CID(NS_DRAGSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_FILEPICKER_CID);
NS_DEFINE_NAMED_CID(NS_SOUND_CID);
NS_DEFINE_NAMED_CID(NS_SCREENMANAGER_CID);

static const mozilla::Module::CIDEntry kWidgetCIDs[] = {
    {&kNS_CLIPBOARD_CID, false, nullptr, nsClipboardProxyConstructor,
     Module::CONTENT_PROCESS_ONLY},
    {&kNS_COLORPICKER_CID, false, nullptr, nsColorPickerProxyConstructor,
     Module::CONTENT_PROCESS_ONLY},
    {&kNS_DRAGSERVICE_CID, false, nullptr, nsDragServiceProxyConstructor,
     Module::CONTENT_PROCESS_ONLY},
    {&kNS_FILEPICKER_CID, false, nullptr, nsFilePickerProxyConstructor,
     Module::CONTENT_PROCESS_ONLY},
    {&kNS_SOUND_CID, false, nullptr, nsSoundProxyConstructor,
     Module::CONTENT_PROCESS_ONLY},
    {&kNS_SCREENMANAGER_CID, false, nullptr, ScreenManagerConstructor,
     Module::CONTENT_PROCESS_ONLY},
    {nullptr}};

static const mozilla::Module::ContractIDEntry kWidgetContracts[] = {
    {"@mozilla.org/widget/clipboard;1", &kNS_CLIPBOARD_CID,
     Module::CONTENT_PROCESS_ONLY},
    {"@mozilla.org/colorpicker;1", &kNS_COLORPICKER_CID,
     Module::CONTENT_PROCESS_ONLY},
    {"@mozilla.org/filepicker;1", &kNS_FILEPICKER_CID,
     Module::CONTENT_PROCESS_ONLY},
    {"@mozilla.org/gfx/screenmanager;1", &kNS_SCREENMANAGER_CID,
     Module::CONTENT_PROCESS_ONLY},
    {"@mozilla.org/sound;1", &kNS_SOUND_CID, Module::CONTENT_PROCESS_ONLY},
    {"@mozilla.org/widget/dragservice;1", &kNS_DRAGSERVICE_CID,
     Module::CONTENT_PROCESS_ONLY},
    {nullptr}};

extern const mozilla::Module kContentProcessWidgetModule = {
    mozilla::Module::kVersion, kWidgetCIDs, kWidgetContracts};
