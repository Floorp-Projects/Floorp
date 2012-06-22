/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UDP_TRANSPORT_SOURCE_UDP_SOCKET2_WINDOWS_H_
#define WEBRTC_MODULES_UDP_TRANSPORT_SOURCE_UDP_SOCKET2_WINDOWS_H_

// Disable deprication warning from traffic.h
#pragma warning(disable : 4995)

// Don't change include order for these header files.
#include <Winsock2.h>
#include <Ntddndis.h>
#include <traffic.h>

#include "atomic32_wrapper.h"
#include "condition_variable_wrapper.h"
#include "critical_section_wrapper.h"
#include "event_wrapper.h"
#include "list_wrapper.h"
#include "rw_lock_wrapper.h"
#include "trace.h"
#include "udp_socket_wrapper.h"
#include "udp_socket2_manager_windows.h"

namespace webrtc {
class UdpSocket2ManagerWindows;
class TrafficControlWindows;
struct PerIoContext;

class UdpSocket2Windows : public UdpSocketWrapper
{
public:
    UdpSocket2Windows(const WebRtc_Word32 id, UdpSocketManager* mgr,
                      bool ipV6Enable = false, bool disableGQOS = false);
    virtual ~UdpSocket2Windows();

    virtual WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id);

    virtual bool ValidHandle();

    virtual bool SetCallback(CallbackObj, IncomingSocketCallback);

    virtual bool Bind(const SocketAddress& name);
    virtual bool SetSockopt(WebRtc_Word32 level, WebRtc_Word32 optname,
                            const WebRtc_Word8* optval, WebRtc_Word32 optlen);

    virtual bool StartReceiving(const WebRtc_UWord32 receiveBuffers);
    virtual inline bool StartReceiving() {return StartReceiving(8);}
    virtual bool StopReceiving();

    virtual WebRtc_Word32 SendTo(const WebRtc_Word8* buf, WebRtc_Word32 len,
                                 const SocketAddress& to);

    virtual void CloseBlocking();

    virtual SOCKET GetFd() { return _socket;}
    virtual bool SetQos(WebRtc_Word32 serviceType, WebRtc_Word32 tokenRate,
                        WebRtc_Word32 bucketSize, WebRtc_Word32 peekBandwith,
                        WebRtc_Word32 minPolicedSize, WebRtc_Word32 maxSduSize,
                        const SocketAddress &stRemName,
                        WebRtc_Word32 overrideDSCP = 0);

    virtual WebRtc_Word32 SetTOS(const WebRtc_Word32 serviceType);
    virtual WebRtc_Word32 SetPCP(const WebRtc_Word32 pcp);

    virtual WebRtc_UWord32 ReceiveBuffers(){return _receiveBuffers.Value();}

protected:
    void IOCompleted(PerIoContext* pIOContext, WebRtc_UWord32 ioSize,
                     WebRtc_UWord32 error);

    WebRtc_Word32 PostRecv();
    // Use pIoContext to post a new WSARecvFrom(..).
    WebRtc_Word32 PostRecv(PerIoContext* pIoContext);

private:
    friend class UdpSocket2WorkerWindows;

    // Set traffic control (TC) flow adding it the interface that matches this
    // sockets address.
    // A filter is created and added to the flow.
    // The flow consists of:
    // (1) QoS send and receive information (flow specifications).
    // (2) A DS object (for specifying exact DSCP value).
    // (3) Possibly a traffic object (for specifying exact 802.1p priority (PCP)
    //     value).
    //
    // dscp values:
    // -1   don't change the current dscp value.
    // 0    don't add any flow to TC, unless pcp is specified.
    // 1-63 Add a flow to TC with the specified dscp value.
    // pcp values:
    // -2  Don't add pcp info to the flow, (3) will not be added.
    // -1  Don't change the current value.
    // 0-7 Add pcp info to the flow with the specified value,
    //     (3) will be added.
    //
    // If both dscp and pcp are -1 no flow will be created or added to TC.
    // If dscp is 0 and pcp is 0-7 (1), (2) and (3) will be created.
    // Note: input parameter values are assumed to be in valid range, checks
    // must be done by caller.
    WebRtc_Word32 SetTrafficControl(WebRtc_Word32 dscp, WebRtc_Word32 pcp,
                                    const struct sockaddr_in* name,
                                    FLOWSPEC* send = NULL,
                                    FLOWSPEC* recv = NULL);
    WebRtc_Word32 CreateFlowSpec(WebRtc_Word32 serviceType,
                                 WebRtc_Word32 tokenRate,
                                 WebRtc_Word32 bucketSize,
                                 WebRtc_Word32 peekBandwith,
                                 WebRtc_Word32 minPolicedSize,
                                 WebRtc_Word32 maxSduSize, FLOWSPEC *f);

    WebRtc_Word32 _id;
    RWLockWrapper* _ptrCbRWLock;
    IncomingSocketCallback _incomingCb;
    CallbackObj _obj;
    bool _qos;

    SocketAddress _remoteAddr;
    SOCKET _socket;
    WebRtc_Word32 _iProtocol;
    UdpSocket2ManagerWindows* _mgr;

    CriticalSectionWrapper* _pCrit;
    Atomic32Wrapper _outstandingCalls;
    Atomic32Wrapper _outstandingCallComplete;
    volatile bool _terminate;
    volatile bool _addedToMgr;

    CriticalSectionWrapper* _ptrDeleteCrit;
    ConditionVariableWrapper* _ptrDeleteCond;
    bool _safeTodelete;

    RWLockWrapper* _ptrDestRWLock;
    bool _outstandingCallsDisabled;
    bool NewOutstandingCall();
    void OutstandingCallCompleted();
    void DisableNewOutstandingCalls();
    void WaitForOutstandingCalls();

    void RemoveSocketFromManager();

    // RWLockWrapper is used as a reference counter for the socket. Write lock
    // is used for creating and deleting socket. Read lock is used for
    // accessing the socket.
    RWLockWrapper* _ptrSocketRWLock;
    bool AquireSocket();
    void ReleaseSocket();
    bool InvalidateSocket();

    // Traffic control handles and structure pointers.
    HANDLE _clientHandle;
    HANDLE _flowHandle;
    HANDLE _filterHandle;
    PTC_GEN_FLOW _flow;
    // TrafficControlWindows implements TOS and PCP.
    TrafficControlWindows* _gtc;
    // Holds the current pcp value. Can be -2 or 0 - 7.
    int _pcp;

    Atomic32Wrapper _receiveBuffers;
};
} // namespace webrtc
#endif // WEBRTC_MODULES_UDP_TRANSPORT_SOURCE_UDP_SOCKET2_WINDOWS_H_
