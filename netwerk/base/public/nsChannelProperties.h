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
 * The Original Code is mozilla.org networking code.
 *
 * The Initial Developer of the Original Code is
 * Christian Biesinger <cbiesinger@web.de>.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsChannelProperties_h__
#define nsChannelProperties_h__

#include "nsStringGlue.h"
#ifdef IMPL_NS_NET
#include "nsNetStrings.h"
#endif

/**
 * @file
 * This file contains constants for properties channels can expose.
 * They can be accessed by using QueryInterface to access the nsIPropertyBag
 * or nsIPropertyBag2 interface on a channel and reading the value.
 */


/**
 * Content-Length of a channel. Used instead of the nsIChannel.contentLength
 * property.
 * Not available before onStartRequest has been called.
 * Type: PRUint64
 */
#define NS_CHANNEL_PROP_CONTENT_LENGTH_STR "content-length"


/**
 * Exists to allow content policy mechanism to function properly during channel
 * redirects.  Contains security contextual information about the load.
 * Type: nsIChannelPolicy
 */
#define NS_CHANNEL_PROP_CHANNEL_POLICY_STR "channel-policy"

#ifdef IMPL_NS_NET
#define NS_CHANNEL_PROP_CONTENT_LENGTH gNetStrings->kContentLength
#define NS_CHANNEL_PROP_CHANNEL_POLICY gNetStrings->kChannelPolicy
#else
#define NS_CHANNEL_PROP_CONTENT_LENGTH \
  NS_LITERAL_STRING(NS_CHANNEL_PROP_CONTENT_LENGTH_STR)
#define NS_CHANNEL_PROP_CHANNEL_POLICY \
  NS_LITERAL_STRING(NS_CHANNEL_PROP_CHANNEL_POLICY_STR)
#endif

#endif
