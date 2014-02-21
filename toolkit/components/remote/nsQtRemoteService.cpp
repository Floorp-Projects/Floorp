/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=8:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <QWindow>
#include "nsQtRemoteService.h"

#include "mozilla/ModuleUtils.h"
#include "mozilla/X11Util.h"
#include "nsIServiceManager.h"
#include "nsIAppShellService.h"

#include "nsCOMPtr.h"

/**
  Helper class which is used to receive notification about property changes
*/
class MozQRemoteEventHandlerWidget: public QWindow {
public:
  /**
    Constructor
    @param aRemoteService remote service, which is notified about atom change
  */
  MozQRemoteEventHandlerWidget(nsQtRemoteService &aRemoteService);
  virtual ~MozQRemoteEventHandlerWidget() {}

protected:
  /**
    Event filter, which receives all XEvents
    @return false which continues event handling
  */
  bool x11Event(XEvent *);

private:
  /**
    Service which is notified about property change
  */
  nsQtRemoteService &mRemoteService;
};

MozQRemoteEventHandlerWidget::MozQRemoteEventHandlerWidget(nsQtRemoteService &aRemoteService)
  :mRemoteService(aRemoteService)
{
}

bool
MozQRemoteEventHandlerWidget::x11Event(XEvent *aEvt)
{
  if (aEvt->type == PropertyNotify && aEvt->xproperty.state == PropertyNewValue)
    mRemoteService.PropertyNotifyEvent(aEvt);

  return false;
}

NS_IMPL_ISUPPORTS2(nsQtRemoteService,
                   nsIRemoteService,
                   nsIObserver)

nsQtRemoteService::nsQtRemoteService():
mServerWindow(0)
{
}

NS_IMETHODIMP
nsQtRemoteService::Startup(const char* aAppName, const char* aProfileName)
{
  if (mServerWindow) return NS_ERROR_ALREADY_INITIALIZED;
  NS_ASSERTION(aAppName, "Don't pass a null appname!");

  XRemoteBaseStartup(aAppName,aProfileName);

  //Create window, which is not shown.
  mServerWindow = new MozQRemoteEventHandlerWidget(*this);

  HandleCommandsFor(mServerWindow->winId());
  return NS_OK;
}

NS_IMETHODIMP
nsQtRemoteService::RegisterWindow(nsIDOMWindow* aWindow)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsQtRemoteService::Shutdown()
{
  if (!mServerWindow)
    return NS_ERROR_NOT_INITIALIZED;

  delete mServerWindow;
  mServerWindow = nullptr;

  return NS_OK;
}

void
nsQtRemoteService::PropertyNotifyEvent(XEvent *aEvt)
{
  HandleNewProperty(aEvt->xproperty.window,
                    mozilla::DefaultXDisplay(),
                    aEvt->xproperty.time,
                    aEvt->xproperty.atom,
                    0);
}

void
nsQtRemoteService::SetDesktopStartupIDOrTimestamp(const nsACString& aDesktopStartupID,
                                                  uint32_t aTimestamp)
{
}

// {C0773E90-5799-4eff-AD03-3EBCD85624AC}
#define NS_REMOTESERVICE_CID \
  { 0xc0773e90, 0x5799, 0x4eff, { 0xad, 0x3, 0x3e, 0xbc, 0xd8, 0x56, 0x24, 0xac } }

NS_GENERIC_FACTORY_CONSTRUCTOR(nsQtRemoteService)
NS_DEFINE_NAMED_CID(NS_REMOTESERVICE_CID);

static const mozilla::Module::CIDEntry kRemoteCIDs[] = {
  { &kNS_REMOTESERVICE_CID, false, nullptr, nsQtRemoteServiceConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kRemoteContracts[] = {
  { "@mozilla.org/toolkit/remote-service;1", &kNS_REMOTESERVICE_CID },
  { nullptr }
};

static const mozilla::Module kRemoteModule = {
  mozilla::Module::kVersion,
  kRemoteCIDs,
  kRemoteContracts
};

NSMODULE_DEFN(RemoteServiceModule) = &kRemoteModule;
