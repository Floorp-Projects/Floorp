/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 */

class nsIWidget;

// {9fe661fa-1dd1-11b2-bd72-ff439d2e8557}

#define NS_IXREMOTEWIDGETHELPER_IID \
  { 0x9fe661fa, 0x1dd1, 0x11b2, \
  { 0xbd, 0x72, 0xff, 0x43, 0x9d, 0x2e, 0x85, 0x57 } }

class nsIXRemoteWidgetHelper : public nsISupports {

 public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IXREMOTEWIDGETHELPER_IID)

  NS_IMETHOD EnableXRemoteCommands(nsIWidget *aWidget,
                                   const char *aProfile,
                                   const char *aProgram) = 0;

};

#define NS_IXREMOTEWIDGETHELPER_CONTRACTID "@mozilla.org/widgets/xremotehelper;1"
#define NS_IXREMOTEWIDGETHELPER_CLASSNAME  "Mozilla XRemote Widget Helper Service"
