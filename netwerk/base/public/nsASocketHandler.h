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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
 *   Florian Queze <florian@queze.net>
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

#ifndef nsASocketHandler_h__
#define nsASocketHandler_h__

// socket handler used by nsISocketTransportService.
// methods are only called on the socket thread.

class nsASocketHandler : public nsISupports
{
public:
    nsASocketHandler()
        : mCondition(NS_OK)
        , mPollFlags(0)
        , mPollTimeout(PR_UINT16_MAX)
        {}

    //
    // this condition variable will be checked to determine if the socket
    // handler should be detached.  it must only be accessed on the socket
    // thread.
    //
    nsresult mCondition;

    //
    // these flags can only be modified on the socket transport thread.
    // the socket transport service will check these flags before calling
    // PR_Poll.
    //
    PRUint16 mPollFlags;

    //
    // this value specifies the maximum amount of time in seconds that may be
    // spent waiting for activity on this socket.  if this timeout is reached,
    // then OnSocketReady will be called with outFlags = -1.
    //
    // the default value for this member is PR_UINT16_MAX, which disables the
    // timeout error checking.  (i.e., a timeout value of PR_UINT16_MAX is
    // never reached.)
    //
    PRUint16 mPollTimeout;

    //
    // called to service a socket
    // 
    // params:
    //   socketRef - socket identifier
    //   fd        - socket file descriptor
    //   outFlags  - value of PR_PollDesc::out_flags after PR_Poll returns
    //               or -1 if a timeout occured
    //
    virtual void OnSocketReady(PRFileDesc *fd, PRInt16 outFlags) = 0;

    //
    // called when a socket is no longer under the control of the socket
    // transport service.  the socket handler may close the socket at this
    // point.  after this call returns, the handler will no longer be owned
    // by the socket transport service.
    //
    virtual void OnSocketDetached(PRFileDesc *fd) = 0;
};

#endif // !nsASocketHandler_h__
