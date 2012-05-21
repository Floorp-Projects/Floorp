/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsQtRemoteService_h__
#define __nsQtRemoteService_h__

#include "nsXRemoteService.h"
#include <X11/Xlib.h>

class RemoteEventHandlerWidget;

class nsQtRemoteService : public nsXRemoteService
{
public:
  // We will be a static singleton, so don't use the ordinary methods.
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREMOTESERVICE  

  nsQtRemoteService();

private:
  ~nsQtRemoteService() { };

  virtual void SetDesktopStartupIDOrTimestamp(const nsACString& aDesktopStartupID,
                                              PRUint32 aTimestamp);

  void PropertyNotifyEvent(XEvent *evt);
  friend class MozQRemoteEventHandlerWidget;

  QWidget *mServerWindow;
};

#endif // __nsQtRemoteService_h__
