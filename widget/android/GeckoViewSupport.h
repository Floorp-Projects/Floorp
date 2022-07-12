/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_GeckoViewSupport_h
#define mozilla_widget_GeckoViewSupport_h

#include "mozilla/java/GeckoResultWrappers.h"
#include "mozilla/java/GeckoSessionNatives.h"
#include "mozilla/java/WebResponseWrappers.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/widget/WindowEvent.h"

class nsPIDOMWindowOuter;
class nsWindow;

namespace mozilla {
namespace widget {

class GeckoViewSupport final
    : public java::GeckoSession::Window::Natives<GeckoViewSupport> {
  RefPtr<nsWindow> mWindow;

  // We hold a WeakRef because we want to allow the
  // GeckoSession.Window to be garbage collected.
  // Callers need to create a LocalRef from this
  // before calling methods.
  java::GeckoSession::Window::WeakRef mGeckoViewWindow;

 public:
  typedef java::GeckoSession::Window::Natives<GeckoViewSupport> Base;

  template <typename Functor>
  static void OnNativeCall(Functor&& aCall) {
    NS_DispatchToMainThread(new WindowEvent<Functor>(std::move(aCall)));
  }

  GeckoViewSupport(nsWindow* aWindow,
                   const java::GeckoSession::Window::LocalRef& aInstance,
                   nsPIDOMWindowOuter* aDOMWindow)
      : mWindow(aWindow), mGeckoViewWindow(aInstance), mDOMWindow(aDOMWindow) {}

  ~GeckoViewSupport();

  nsWindow* GetNsWindow() const { return mWindow; }

  using Base::DisposeNative;

  /**
   * GeckoView methods
   */
 private:
  nsCOMPtr<nsPIDOMWindowOuter> mDOMWindow;
  bool mIsReady{false};
  RefPtr<dom::CanonicalBrowsingContext> GetContentCanonicalBrowsingContext();

 public:
  // Create and attach a window.
  static void Open(const jni::Class::LocalRef& aCls,
                   java::GeckoSession::Window::Param aWindow,
                   jni::Object::Param aQueue, jni::Object::Param aCompositor,
                   jni::Object::Param aDispatcher,
                   jni::Object::Param aSessionAccessibility,
                   jni::Object::Param aInitData, jni::String::Param aId,
                   jni::String::Param aChromeURI, bool aPrivateMode);

  // Close and destroy the nsWindow.
  void Close();

  // Transfer this nsWindow to new GeckoSession objects.
  void Transfer(const java::GeckoSession::Window::LocalRef& inst,
                jni::Object::Param aQueue, jni::Object::Param aCompositor,
                jni::Object::Param aDispatcher,
                jni::Object::Param aSessionAccessibility,
                jni::Object::Param aInitData);

  void AttachEditable(const java::GeckoSession::Window::LocalRef& inst,
                      jni::Object::Param aEditableParent);

  void AttachAccessibility(const java::GeckoSession::Window::LocalRef& inst,
                           jni::Object::Param aSessionAccessibility);

  void OnReady(jni::Object::Param aQueue = nullptr);

  auto OnLoadRequest(mozilla::jni::String::Param aUri, int32_t aWindowType,
                     int32_t aFlags, mozilla::jni::String::Param aTriggeringUri,
                     bool aHasUserGesture, bool aIsTopLevel) const
      -> java::GeckoResult::LocalRef;

  void OnShowDynamicToolbar() const;

  void PassExternalResponse(java::WebResponse::Param aResponse);

  void AttachMediaSessionController(
      const java::GeckoSession::Window::LocalRef& inst,
      jni::Object::Param aController, const int64_t aId);

  void DetachMediaSessionController(
      const java::GeckoSession::Window::LocalRef& inst,
      jni::Object::Param aController);

  void OnWeakNonIntrusiveDetach(already_AddRefed<Runnable> aDisposer) {
    RefPtr<Runnable> disposer(aDisposer);
    disposer->Run();
  }

  void PrintToPdf(const java::GeckoSession::Window::LocalRef& inst,
                  jni::Object::Param aStream);
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_GeckoViewSupport_h
