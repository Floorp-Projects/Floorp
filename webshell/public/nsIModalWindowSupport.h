/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
#ifndef __nsIModalWindowSupport_h
#define __nsIModalWindowSupport_h

// {FFD41B80-4F95-11d3-9584-0060083A0BCF}
#define NS_IMODALWINDOWSUPPORT_IID \
  { 0xffd41b80, 0x4f95, 0x11d3,    \
    {0x95, 0x84, 0x0, 0x60, 0x8, 0x3a, 0xb, 0xcf } }

/**
 * nsIModalWindowSupport needs to do some skanky spinup and cleanup that
 * our system requires to support modal windows.
 */
class nsIModalWindowSupport: public nsISupports {
public:

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMODALWINDOWSUPPORT_IID)

  /**
   * Do preparatory work necessary for beginning a modal window. This must
   * be called before the window is actually created.
   * @return NS_OK if the spinup was successful. else, you probably won't
   *               be able to have your window modal.
   */
  NS_IMETHOD PrepareModality() = 0;

  /**
   * Clean up after PrepareModality. Implementations can expect this to
   * always be called, and always after PrepareModality wal called.
   * @return NS_OK if cleanup was successful. failure should be rare, and
   *               most likely of some bad, unexpected system kinkage.
   */
  NS_IMETHOD FinishModality() = 0;
};

#endif

