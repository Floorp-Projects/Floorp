/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef __nsApplicationManager_h
#define __nsApplicationManager_h


#include "nscore.h"
#include "plstr.h"
#include "prtypes.h"
#include "prmon.h"
#include "plstr.h"
#include "nsCom.h"
#include "nsshell.h"
#include "nsHashtable.h"
#include "nsIShellInstance.h"
#include "nsIApplicationShell.h"


/**
 * ModalMessage enums
 */

enum nsModalMessageType {   
                  ///OK Modal Message
                eModalMessage_ok,
                  ///OK/CANCEL Modal Message
                eModalMessage_ok_cancel,
              }; 


class NS_SHELL nsApplicationManager {
private:
  static PRMonitor *monitor;
  static nsHashtable * applications;

  static nsresult checkInitialized();

public:
  static nsresult Initialize();

  static nsresult GetShellInstance(nsIApplicationShell * aApplicationShell,
                                nsIShellInstance **aShellInstance);

  static nsresult SetShellAssociation(nsIApplicationShell * aApplicationShell,
                                      nsIShellInstance *aShellInstance);

  static nsresult DeleteShellAssociation(nsIApplicationShell * aApplicationShell,
                                         nsIShellInstance *aShellInstance);

  static nsresult ModalMessage(const nsString &aMessage, const nsString &aTitle, nsModalMessageType aModalMessageType);
};

#endif
