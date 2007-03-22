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
 * The Original Code is nsMime.cpp for the BeOS port of Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Paul Ashford.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Paul Ashford <arougthopher@lizardland.net>
 *   Fredrik Holmqvist <thesuckiestemail@yahoo.se>
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

#ifndef nsIDragSessionBeOS_h__
#define nsIDragSessionBeOS_h__


#include "nsISupports.h"

#include <Message.h>


#define NS_IDRAGSESSIONBEOS_IID      \
{ 0x36c4c381, 0x09e3, 0x11d4, { 0xb0, 0x33, 0xa4, 0x20, 0xf4, 0x2c, 0xfd, 0x7c } };

class nsIDragSessionBeOS : public nsISupports
{
  public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDRAGSESSIONBEOS_IID)
    NS_IMETHOD TransmitData( BMessage *aNegotiationReply) = 0;  
    NS_IMETHOD UpdateDragMessageIfNeeded(BMessage *aDragMessage) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDragSessionBeOS, NS_IDRAGSESSIONBEOS_IID)

#endif
