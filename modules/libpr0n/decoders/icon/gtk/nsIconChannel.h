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
 * The Original Code is the Mozilla icon channel for gnome.
 *
 * The Initial Developer of the Original Code is
 * Christian Biesinger <cbiesinger@web.de>.
 * Portions created by the Initial Developer are Copyright (C) 2004
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

#ifndef nsIconChannel_h_
#define nsIconChannel_h_

#include "nsIChannel.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"
#include "nsIIconURI.h"
#include "nsCOMPtr.h"

/**
 * This class is the gnome implementation of nsIconChannel. It basically asks
 * gnome for the filename to a given mime type, and creates a new channel for
 * that file to which all calls will be proxied.
 */
class nsIconChannel : public nsIChannel {
  public:
    NS_DECL_ISUPPORTS
    NS_FORWARD_NSIREQUEST(mRealChannel->)
    NS_FORWARD_NSICHANNEL(mRealChannel->)

    /**
     * Called by nsIconProtocolHandler after it creates this channel.
     * Must be called before calling any other function on this object.
     * If this method fails, no other function must be called on this object.
     */
    NS_HIDDEN_(nsresult) Init(nsIURI* aURI);
  private:
    /**
     * The channel to the real icon file (e.g. to
     * /usr/share/icons/gnome/16x16/mimetypes/gnome-mime-application-msword.png).
     * Will always be non-null after a successful Init.
     */
    nsCOMPtr<nsIChannel> mRealChannel;
    /**
     * The moz-icon URI we're loading. Always non-null after a successful Init.
     */
    nsCOMPtr<nsIMozIconURI> mURI;
};



#endif
