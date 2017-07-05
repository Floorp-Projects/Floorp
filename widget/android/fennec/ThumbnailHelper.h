/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ThumbnailHelper_h
#define ThumbnailHelper_h

#include "AndroidBridge.h"
#include "FennecJNINatives.h"
#include "gfxPlatform.h"
#include "mozIDOMWindow.h"
#include "nsAppShell.h"
#include "nsCOMPtr.h"
#include "nsIChannel.h"
#include "nsIDOMWindowUtils.h"
#include "nsIDOMClientRect.h"
#include "nsIDocShell.h"
#include "nsIHttpChannel.h"
#include "nsIPresShell.h"
#include "nsIURI.h"
#include "nsPIDOMWindow.h"
#include "nsPresContext.h"

#include "mozilla/Preferences.h"

namespace mozilla {

class ThumbnailHelper final
    : public java::ThumbnailHelper::Natives<ThumbnailHelper>
{
    ThumbnailHelper() = delete;

    static already_AddRefed<mozIDOMWindowProxy>
    GetWindowForTab(int32_t aTabId)
    {
        nsAppShell* const appShell = nsAppShell::Get();
        if (!appShell) {
            return nullptr;
        }

        nsCOMPtr<nsIAndroidBrowserApp> browserApp = appShell->GetBrowserApp();
        if (!browserApp) {
            return nullptr;
        }

        nsCOMPtr<mozIDOMWindowProxy> window;
        nsCOMPtr<nsIBrowserTab> tab;

        if (NS_FAILED(browserApp->GetBrowserTab(aTabId, getter_AddRefs(tab))) ||
                !tab ||
                NS_FAILED(tab->GetWindow(getter_AddRefs(window))) ||
                !window) {
            return nullptr;
        }

        return window.forget();
    }

    // Decides if we should store thumbnails for a given docshell based on the
    // presence of a Cache-Control: no-store header and the
    // "browser.cache.disk_cache_ssl" pref.
    static bool
    ShouldStoreThumbnail(nsIDocShell* docShell)
    {
        nsCOMPtr<nsIChannel> channel;
        if (NS_FAILED(docShell->GetCurrentDocumentChannel(
                getter_AddRefs(channel))) || !channel) {
            return false;
        }

        nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel);
        if (!httpChannel) {
            // Allow storing non-HTTP thumbnails.
            return true;
        }

        // Don't store thumbnails for sites that didn't load or have
        // Cache-Control: no-store.
        uint32_t responseStatus = 0;
        bool isNoStoreResponse = false;

        if (NS_FAILED(httpChannel->GetResponseStatus(&responseStatus)) ||
                (responseStatus / 100) != 2 ||
                NS_FAILED(httpChannel->IsNoStoreResponse(&isNoStoreResponse)) ||
                isNoStoreResponse) {
            return false;
        }

        // Deny storage if we're viewing a HTTPS page with a 'Cache-Control'
        // header having a value that is not 'public', unless enabled by user.
        nsCOMPtr<nsIURI> uri;
        bool isHttps = false;

        if (NS_FAILED(channel->GetURI(getter_AddRefs(uri))) ||
                !uri ||
                NS_FAILED(uri->SchemeIs("https", &isHttps))) {
            return false;
        }

        if (!isHttps ||
                Preferences::GetBool("browser.cache.disk_cache_ssl", false)) {
            // Allow storing non-HTTPS thumbnails, and HTTPS ones if enabled by
            // user.
            return true;
        }

        nsAutoCString cacheControl;
        if (NS_FAILED(httpChannel->GetResponseHeader(
                NS_LITERAL_CSTRING("Cache-Control"), cacheControl))) {
            return false;
        }

        if (cacheControl.IsEmpty() ||
                cacheControl.LowerCaseEqualsLiteral("public")) {
            // Allow no cache-control, or public cache-control.
            return true;
        }
        return false;
    }

    // Return a non-null nsIDocShell to indicate success.
    static already_AddRefed<nsIDocShell>
    GetThumbnailAndDocShell(mozIDOMWindowProxy* aWindow,
                            jni::ByteBuffer::Param aData,
                            int32_t aThumbWidth, int32_t aThumbHeight,
                            const CSSRect& aPageRect, float aZoomFactor)
    {
        nsCOMPtr<nsPIDOMWindowOuter> win = nsPIDOMWindowOuter::From(aWindow);
        nsCOMPtr<nsIDocShell> docShell = win->GetDocShell();
        RefPtr<nsPresContext> presContext;

        if (!docShell || NS_FAILED(docShell->GetPresContext(
                getter_AddRefs(presContext))) || !presContext) {
            return nullptr;
        }

        uint8_t* const data = static_cast<uint8_t*>(aData->Address());
        if (!data) {
            return nullptr;
        }

        const bool is24bit = !AndroidBridge::Bridge() ||
                AndroidBridge::Bridge()->GetScreenDepth() == 24;
        const uint32_t stride = aThumbWidth * (is24bit ? 4 : 2);

        RefPtr<DrawTarget> dt = gfxPlatform::GetPlatform()->CreateDrawTargetForData(
                data,
                IntSize(aThumbWidth, aThumbHeight),
                stride,
                is24bit ? SurfaceFormat::B8G8R8A8
                        : SurfaceFormat::R5G6B5_UINT16);

        if (!dt || !dt->IsValid()) {
            return nullptr;
        }

        nsCOMPtr<nsIPresShell> presShell = presContext->PresShell();
        RefPtr<gfxContext> context = gfxContext::CreateOrNull(dt);
        MOZ_ASSERT(context); // checked the draw target above

        context->SetMatrix(context->CurrentMatrix().PreScale(
                aZoomFactor * float(aThumbWidth) / aPageRect.width,
                aZoomFactor * float(aThumbHeight) / aPageRect.height));

        const nsRect drawRect(
                nsPresContext::CSSPixelsToAppUnits(aPageRect.x),
                nsPresContext::CSSPixelsToAppUnits(aPageRect.y),
                nsPresContext::CSSPixelsToAppUnits(aPageRect.width),
                nsPresContext::CSSPixelsToAppUnits(aPageRect.height));
        const uint32_t renderDocFlags =
                nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING |
                nsIPresShell::RENDER_DOCUMENT_RELATIVE;
        const nscolor bgColor = NS_RGB(255, 255, 255);

        if (NS_FAILED(presShell->RenderDocument(
                drawRect, renderDocFlags, bgColor, context))) {
            return nullptr;
        }

        if (is24bit) {
            gfxUtils::ConvertBGRAtoRGBA(data, stride * aThumbHeight);
        }

        return docShell.forget();
    }

public:
    static void Init()
    {
        java::ThumbnailHelper::Natives<ThumbnailHelper>::Init();
    }

    template<class Functor>
    static void OnNativeCall(Functor&& aCall)
    {
        class IdleEvent : public nsAppShell::LambdaEvent<Functor>
        {
            using Base = nsAppShell::LambdaEvent<Functor>;

        public:
            IdleEvent(Functor&& aCall)
                : Base(Forward<Functor>(aCall))
            {}

            void Run() override
            {
                MessageLoop::current()->PostIdleTask(
                    NS_NewRunnableFunction("OnNativeCall", Move(Base::lambda)));
            }
        };

        // Invoke RequestThumbnail on the main thread when the thread is idle.
        nsAppShell::PostEvent(MakeUnique<IdleEvent>(Forward<Functor>(aCall)));
    }

    static void
    RequestThumbnail(jni::ByteBuffer::Param aData, jni::Object::Param aTab,
                     int32_t aTabId, int32_t aWidth, int32_t aHeight)
    {
        nsCOMPtr<mozIDOMWindowProxy> window = GetWindowForTab(aTabId);
        if (!window || !aData) {
            java::ThumbnailHelper::NotifyThumbnail(
                    aData, aTab, /* success */ false, /* store */ false);
            return;
        }

        // take a screenshot, as wide as possible, proportional to the destination size
        nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(window);
        nsCOMPtr<nsIDOMClientRect> rect;
        float pageLeft = 0.0f, pageTop = 0.0f, pageWidth = 0.0f, pageHeight = 0.0f;

        if (!utils ||
                NS_FAILED(utils->GetRootBounds(getter_AddRefs(rect))) ||
                !rect ||
                NS_FAILED(rect->GetLeft(&pageLeft)) ||
                NS_FAILED(rect->GetTop(&pageTop)) ||
                NS_FAILED(rect->GetWidth(&pageWidth)) ||
                NS_FAILED(rect->GetHeight(&pageHeight)) ||
                int32_t(pageWidth) == 0 || int32_t(pageHeight) == 0) {
            java::ThumbnailHelper::NotifyThumbnail(
                    aData, aTab, /* success */ false, /* store */ false);
            return;
        }

        const float aspectRatio = float(aWidth) / float(aHeight);
        if (pageWidth / aspectRatio < pageHeight) {
            pageHeight = pageWidth / aspectRatio;
        } else {
            pageWidth = pageHeight * aspectRatio;
        }

        nsCOMPtr<nsIDocShell> docShell = GetThumbnailAndDocShell(
                window, aData, aWidth, aHeight,
                CSSRect(pageLeft, pageTop, pageWidth, pageHeight),
                /* aZoomFactor */ 1.0f);
        const bool success = !!docShell;
        const bool store = success ? ShouldStoreThumbnail(docShell) : false;

        java::ThumbnailHelper::NotifyThumbnail(aData, aTab, success, store);
    }
};

} // namespace mozilla

#endif // ThumbnailHelper_h
