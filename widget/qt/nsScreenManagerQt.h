/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScreenManagerQt_h___
#define nsScreenManagerQt_h___

#include "nsIScreenManager.h"
#include "nsIScreen.h"
#include "nsCOMPtr.h"

//------------------------------------------------------------------------
class QDesktopWidget;

class nsScreenManagerQt : public nsIScreenManager
{
public:
  nsScreenManagerQt ( );
  virtual ~nsScreenManagerQt();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISCREENMANAGER

private:

  void init ();

  nsCOMPtr<nsIScreen> *screens;
  QDesktopWidget *desktop;
  int nScreens;
};

#endif  // nsScreenManagerQt_h___
