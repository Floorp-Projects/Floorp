/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "udp_socket_windows.h"

// Disable deprication warning from traffic.h
#pragma warning(disable : 4995)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <Qos.h>
// Don't change include order for these header files.
#include <Winsock2.h>
#include <Ntddndis.h>
#include <traffic.h>

#include "traffic_control_windows.h"
#include "udp_socket_manager_wrapper.h"

namespace webrtc {
typedef struct _QOS_DESTADDR
{
    QOS_OBJECT_HDR ObjectHdr;
    const struct sockaddr* SocketAddress;
    ULONG SocketAddressLength;
} QOS_DESTADDR, *LPQOS_DESTADDR;

typedef const QOS_DESTADDR* LPCQOS_DESTADDR;

#define QOS_GENERAL_ID_BASE 2000
#define QOS_OBJECT_DESTADDR (0x00000004 + QOS_GENERAL_ID_BASE)

#define MAX_PACKET_SIZE 2048

class UDPPacket
{
public:
    UDPPacket()
    {
        _length = 0;
    }
    WebRtc_Word32 Set(const WebRtc_Word8* buf, WebRtc_Word32 length)
    {
        if(length > MAX_PACKET_SIZE)
            return 0;

        _length = length;
        memcpy(_buffer,buf,length);
        return length;
    }
    WebRtc_Word32 Set(const WebRtc_Word8* buf, WebRtc_Word32 length,
                      const SocketAddress* addr)
    {
        if(length > MAX_PACKET_SIZE)
        {
            return 0;
        }

        _length = length;
        memcpy(&_remoteAddr,addr,sizeof(SocketAddress));
        memcpy(_buffer,buf,length);
        return length;
    }

    SocketAddress _remoteAddr;
    char _buffer[MAX_PACKET_SIZE];
    WebRtc_Word32 _length;
};

UdpSocketWindows::UdpSocketWindows(const WebRtc_Word32 id,
                                   UdpSocketManager* mgr, bool ipV6Enable)
    : _id(id),
      _qos(true)
{
    _wantsIncoming = false;
    _error = 0;
    _mgr = mgr;
    _addedToMgr = false;

    _obj = NULL;
    _incomingCb = NULL;
    _socket = INVALID_SOCKET;
    _terminate=false;

    _clientHandle = INVALID_HANDLE_VALUE;
    _flowHandle = INVALID_HANDLE_VALUE;
    _filterHandle = INVALID_HANDLE_VALUE;

    WEBRTC_TRACE(kTraceMemory, kTraceTransport, _id,
                 "UdpSocketWindows::UdpSocketWindows()");

    _gtc = NULL;

    // Check if QoS is supported.
    WSAPROTOCOL_INFO  pProtocolInfo;
    DWORD dwBufLen = 0;
    BOOL bProtocolFound = FALSE;
    WSAPROTOCOL_INFO* lpProtocolBuf = NULL;

    // Set dwBufLen to the size needed to retreive all the requested information
    // from WSAEnumProtocols.
    WebRtc_Word32 nRet = WSAEnumProtocols(NULL, lpProtocolBuf, &dwBufLen);
    lpProtocolBuf = (WSAPROTOCOL_INFO*)malloc(dwBufLen);
    nRet = WSAEnumProtocols(NULL, lpProtocolBuf, &dwBufLen);

    WebRtc_Word32 iProtocol;
    if (ipV6Enable)
    {
        iProtocol = AF_INET6;
    } else {
        iProtocol = AF_INET;
    }

    for (WebRtc_Word32 i = 0; i < nRet; i++)
    {
        if (iProtocol == lpProtocolBuf[i].iAddressFamily && IPPROTO_UDP ==
            lpProtocolBuf[i].iProtocol)
        {
            if ((XP1_QOS_SUPPORTED ==
                 (XP1_QOS_SUPPORTED & lpProtocolBuf[i].dwServiceFlags1)))
            {
                pProtocolInfo = lpProtocolBuf[i];
                bProtocolFound = TRUE;
                break;
            }
        }
     }

    if(!bProtocolFound)
    {
        _socket = INVALID_SOCKET;
        _qos = false;
        free(lpProtocolBuf);
        _error = SOCKET_ERROR_NO_QOS;
    }else {
        _socket = WSASocket(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO,
                            FROM_PROTOCOL_INFO,&pProtocolInfo, 0,
                            WSA_FLAG_OVERLAPPED);
        free(lpProtocolBuf);
        if (_socket != INVALID_SOCKET)
        {
            return;
        }else
        {
            _qos = false;
            _error = SOCKET_ERROR_NO_QOS;
        }
    }
    // QoS not supported.
    if(ipV6Enable)
    {
        _socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    }else
    {
        _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    }
    // Non-blocking mode.
    WebRtc_Word32 iMode = 1;
    ioctlsocket(_socket, FIONBIO, (u_long FAR*) &iMode);
}

UdpSocketWindows::~UdpSocketWindows()
{
    WEBRTC_TRACE(kTraceMemory, kTraceTransport, _id,
                 "UdpSocketWindows::~UdpSocketWindows()");
    if (_gtc)
    {
        TrafficControlWindows::Release(_gtc);
    }
}

WebRtc_Word32 UdpSocketWindows::ChangeUniqueId(const WebRtc_Word32 id)
{
    _id = id;
    if (_gtc)
    {
        _gtc->ChangeUniqueId(id);
    }
    return 0;
}

bool UdpSocketWindows::ValidHandle()
{
    return GetFd() != INVALID_SOCKET;
}

bool UdpSocketWindows::SetCallback(CallbackObj obj, IncomingSocketCallback cb)
{
    _obj = obj;
    _incomingCb = cb;

    if (_mgr->AddSocket(this))
    {
        _addedToMgr = true;
        return true;
    }
    return false;
}

bool UdpSocketWindows::SetSockopt(WebRtc_Word32 level, WebRtc_Word32 optname,
                                  const WebRtc_Word8* optval,
                                  WebRtc_Word32 optlen)
{
    if(0 == setsockopt(_socket, level, optname,
                       reinterpret_cast<const char*>(optval), optlen))
    {
        return true;
    }
    _error = WSAGetLastError();
    return false;
}

bool UdpSocketWindows::Bind(const SocketAddress& name)
{
    const struct sockaddr* socketName =
        reinterpret_cast<const struct sockaddr*>(&name);

    if (0 == bind(_socket, socketName, sizeof(SocketAddress)))
    {
        _localAddr = name;
        return true;
    }
    _error = WSAGetLastError();
    return false;
}

WebRtc_Word32 UdpSocketWindows::SendTo(const WebRtc_Word8* buf,
                                       WebRtc_Word32 len,
                                       const SocketAddress& to)
{
    // Don't try to send this packet if there are older packets queued up.
    if(!_notSentPackets.Empty())
    {
        UDPPacket* packet = new UDPPacket();
        packet->Set(buf, len, &to);
        if(!_notSentPackets.Empty())
        {
            _notSentPackets.PushBack(packet);
            return len;
        }else {
            // No old packets queued up. Free to try to send.
            delete packet;
        }
    }

    WebRtc_Word32 retVal;
    retVal = sendto(_socket, reinterpret_cast<const char*>(buf), len, 0,
                    reinterpret_cast<const struct sockaddr*>(&to),
                    sizeof(SocketAddress));

    if(retVal == SOCKET_ERROR)
    {
        _error = WSAGetLastError();
        if (_error == WSAEWOULDBLOCK)
        {
            UDPPacket* packet = new UDPPacket();
            packet->Set(buf,len, &to);
            _notSentPackets.PushBack(packet);
            return len;
        }
    }
    return retVal;
}

void UdpSocketWindows::HasIncoming()
{
    char buf[MAX_PACKET_SIZE];
    SocketAddress from;
    int fromlen = sizeof(from);
    WebRtc_Word32 retval = recvfrom(_socket, buf,
                                    sizeof(buf), 0,
                                    reinterpret_cast<struct sockaddr*>(&from),
                                    &fromlen);

    switch(retval)
    {
    case 0:
        // The connection has been gracefully closed.
        break;
    case SOCKET_ERROR:
        _error = WSAGetLastError();
        break;
    default:
        if(_wantsIncoming && _incomingCb)
            _incomingCb(_obj, reinterpret_cast<WebRtc_Word8*>(buf),
                        retval, &from);
        break;
    }
}

void UdpSocketWindows::CleanUp()
{
    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "UdpSocketWindows::CleanUp()");
    _wantsIncoming = false;

    if(_clientHandle != INVALID_HANDLE_VALUE)
    {
        assert(_filterHandle != INVALID_HANDLE_VALUE);
        assert(_flowHandle != INVALID_HANDLE_VALUE);

        if (_gtc)
        {
            _gtc->TcDeleteFilter(_filterHandle);
            _gtc->TcDeleteFlow(_flowHandle);
            _gtc->TcDeregisterClient(_clientHandle);
        }

        _clientHandle = INVALID_HANDLE_VALUE;
        _filterHandle = INVALID_HANDLE_VALUE;
        _flowHandle = INVALID_HANDLE_VALUE;
    }

    while(!_notSentPackets.Empty())
    {
        UDPPacket* packet = (UDPPacket*)_notSentPackets.First()->GetItem();
        if(!packet)
        {
            break;
        }
        delete packet;
        _notSentPackets.PopFront();
    }

    if (_socket != INVALID_SOCKET)
    {
        if (closesocket(_socket) == SOCKET_ERROR)
        {
            WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                         "closesocket() => error = %d", WSAGetLastError());
        }
        WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                     "WinSock::closesocket() done");

        if(_addedToMgr)
        {
            WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                         "calling UdpSocketManager::RemoveSocket()");
            _mgr->RemoveSocket(this);
        }
    }
}

void UdpSocketWindows::SetWritable()
{
    // Try to send packets that have been queued up.
    while(!_notSentPackets.Empty())
    {
        UDPPacket* packet = (UDPPacket*)_notSentPackets.First()->GetItem();
        if(!packet)
        {
            break;
        }
        if(sendto(
               _socket, packet->_buffer,
               packet->_length,
               0,
               reinterpret_cast<const struct sockaddr*>(
                   &(packet->_remoteAddr)),
               sizeof(SocketAddress)) == SOCKET_ERROR)
        {
            _error = WSAGetLastError();
            if (_error == WSAEWOULDBLOCK)
            {
                return;
            }
        } else {
            delete packet;
            _notSentPackets.PopFront();
        }
    }
}

bool UdpSocketWindows::SetQos(WebRtc_Word32 serviceType,
                              WebRtc_Word32 tokenRate,
                              WebRtc_Word32 bucketSize,
                              WebRtc_Word32 peekBandwith,
                              WebRtc_Word32 minPolicedSize,
                              WebRtc_Word32 maxSduSize,
                              const SocketAddress &stRemName,
                              WebRtc_Word32 overrideDSCP)
{
    if(_qos == false)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "UdpSocket2Windows::SetQos(), socket not capable of QOS");
        return false;
    }
    QOS Qos;
    WebRtc_Word32 result;
    DWORD BytesRet;

    if(overrideDSCP != 0)
    {
        FLOWSPEC f;
        WebRtc_Word32 err = CreateFlowSpec(serviceType, tokenRate, bucketSize,
                                           peekBandwith, minPolicedSize,
                                           maxSduSize, &f);
        if(err == -1)
        {
            return false;
        }
        return SetTOSByte(overrideDSCP, &f, &f) == 0;
    }
    memset(&Qos, QOS_NOT_SPECIFIED, sizeof(QOS));

    Qos.SendingFlowspec.ServiceType        = serviceType;
    Qos.SendingFlowspec.TokenRate          = tokenRate;
    Qos.SendingFlowspec.TokenBucketSize    = bucketSize;
    Qos.SendingFlowspec.PeakBandwidth      = peekBandwith;
    Qos.SendingFlowspec.DelayVariation     = QOS_NOT_SPECIFIED;
    Qos.SendingFlowspec.Latency            = QOS_NOT_SPECIFIED;
    Qos.SendingFlowspec.MinimumPolicedSize = minPolicedSize;
    Qos.SendingFlowspec.MaxSduSize         = maxSduSize;

    // Only ServiceType is needed for receiving.
    Qos.ReceivingFlowspec.ServiceType        = serviceType;
    Qos.ReceivingFlowspec.TokenRate          = QOS_NOT_SPECIFIED;
    Qos.ReceivingFlowspec.TokenBucketSize    = QOS_NOT_SPECIFIED;
    Qos.ReceivingFlowspec.PeakBandwidth      = QOS_NOT_SPECIFIED;
    Qos.ReceivingFlowspec.Latency            = QOS_NOT_SPECIFIED;
    Qos.ReceivingFlowspec.DelayVariation     = QOS_NOT_SPECIFIED;
    Qos.ReceivingFlowspec.MinimumPolicedSize = QOS_NOT_SPECIFIED;
    Qos.ReceivingFlowspec.MaxSduSize         = QOS_NOT_SPECIFIED;

    Qos.ProviderSpecific.len = 0;
    Qos.ProviderSpecific.buf = NULL;
    WebRtc_Word8* p = (WebRtc_Word8*)malloc(sizeof(QOS_DESTADDR) +
                                            sizeof(QOS_DS_CLASS));

    QOS_DESTADDR* QosDestaddr = (QOS_DESTADDR*)p;
    ZeroMemory((WebRtc_Word8 *)QosDestaddr, sizeof(QOS_DESTADDR));
    QosDestaddr->ObjectHdr.ObjectType = QOS_OBJECT_DESTADDR;
    QosDestaddr->ObjectHdr.ObjectLength = sizeof(QOS_DESTADDR);
    QosDestaddr->SocketAddress = (SOCKADDR*)&stRemName;
    QosDestaddr->SocketAddressLength = sizeof(SocketAddress);
    Qos.ProviderSpecific.len = QosDestaddr->ObjectHdr.ObjectLength;
    Qos.ProviderSpecific.buf = (char*)p;

    // Socket must be bound for this call to be successfull. If socket is not
    // bound WSAGetLastError() will return 10022.
    result = WSAIoctl(GetFd(),SIO_SET_QOS, &Qos,sizeof(QOS),NULL, 0, &BytesRet,
                      NULL,NULL);
    if (result == SOCKET_ERROR)
    {
        _error = WSAGetLastError();
        free(p);
        return false;
    }
    free(p);
    return true;
}

WebRtc_Word32 UdpSocketWindows::SetTOS(WebRtc_Word32 serviceType)
{
    WebRtc_Word32 res = SetTOSByte(serviceType, NULL, NULL);

    if (res == -1)
    {
        OSVERSIONINFO OsVersion;
        OsVersion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        GetVersionEx (&OsVersion);

        if ((OsVersion.dwMajorVersion == 4))
        {
            return -1;
        }
    }
    return res;
}

WebRtc_Word32 UdpSocketWindows::CreateFlowSpec(WebRtc_Word32 serviceType,
                                               WebRtc_Word32 tokenRate,
                                               WebRtc_Word32 bucketSize,
                                               WebRtc_Word32 peekBandwith,
                                               WebRtc_Word32 minPolicedSize,
                                               WebRtc_Word32 maxSduSize,
                                               FLOWSPEC *f)
{
    if(!f)
    {
        return -1;
    }

    f->ServiceType        = serviceType;
    f->TokenRate          = tokenRate;
    f->TokenBucketSize    = bucketSize;
    f->PeakBandwidth      = peekBandwith;
    f->DelayVariation     = QOS_NOT_SPECIFIED;
    f->Latency            = QOS_NOT_SPECIFIED;
    f->MinimumPolicedSize = minPolicedSize;
    f->MaxSduSize         = maxSduSize;
    return 0;
}

WebRtc_Word32 UdpSocketWindows::SetTOSByte(WebRtc_Word32 serviceType,
                                           FLOWSPEC* send, FLOWSPEC* recv)
{
    if(_socket == INVALID_SOCKET)
    {
        return -1;
    }
    if (!_gtc)
    {
        _gtc = TrafficControlWindows::GetInstance(_id);
    }
    if (!_gtc)
    {
        return -1;
    }

    TCI_CLIENT_FUNC_LIST QoSFunctions;
    QoSFunctions.ClAddFlowCompleteHandler = NULL;
    QoSFunctions.ClDeleteFlowCompleteHandler = NULL;
    QoSFunctions.ClModifyFlowCompleteHandler = NULL;
    QoSFunctions.ClNotifyHandler = (TCI_NOTIFY_HANDLER)MyClNotifyHandler;
    // Register the client with Traffic control interface.
    HANDLE ClientHandle;
    ULONG result = _gtc->TcRegisterClient(CURRENT_TCI_VERSION, NULL,
                                          &QoSFunctions,&ClientHandle);
    if(result != NO_ERROR)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "TcRegisterClient returned %d", result);
        return result;
    }

    // Find traffic control-enabled network interfaces.
    ULONG BufferSize = 0;
    result = _gtc->TcEnumerateInterfaces(ClientHandle, &BufferSize, NULL);

    if(result != NO_ERROR && result != ERROR_INSUFFICIENT_BUFFER)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "Error enumerating interfaces, %d", result);
        _gtc->TcDeregisterClient(ClientHandle);
        return result;
    }

    if(result != ERROR_INSUFFICIENT_BUFFER)
    {
        // Empty buffer contains all control-enabled network interfaces. I.e.
        // ToS is not enabled.
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "Error enumerating interfaces: passed in 0 and received\
 NO_ERROR when expecting INSUFFICIENT_BUFFER, %d");
        _gtc->TcDeregisterClient(ClientHandle);
        return -1;
    }

    PTC_IFC_DESCRIPTOR pInterfaceBuffer =
        (PTC_IFC_DESCRIPTOR)malloc(BufferSize);
    if(pInterfaceBuffer == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "Out ot memory failure");
        _gtc->TcDeregisterClient(ClientHandle);
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    result = _gtc->TcEnumerateInterfaces(ClientHandle, &BufferSize,
                                         pInterfaceBuffer);

    if(result != NO_ERROR)
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "Critical: error enumerating interfaces when passing in correct\
 buffer size: %d", result);
        _gtc->TcDeregisterClient(ClientHandle);
        free(pInterfaceBuffer);
        return result;
    }


    PTC_IFC_DESCRIPTOR oneinterface;
    HANDLE ifcHandle, iFilterHandle, iflowHandle;
    bool addrFound = false;
    ULONG filterSourceAddress = ULONG_MAX;

    const struct sockaddr_in* name;
    name = reinterpret_cast<const struct sockaddr_in*>(&_localAddr);

    // Find the interface corresponding to the local address.
    for(oneinterface = pInterfaceBuffer;
        oneinterface != (PTC_IFC_DESCRIPTOR)
            (((WebRtc_Word8*)pInterfaceBuffer) + BufferSize);
        oneinterface = (PTC_IFC_DESCRIPTOR)
            ((WebRtc_Word8*)oneinterface + oneinterface->Length))
    {

        char interfaceName[500];
        WideCharToMultiByte(CP_ACP, 0, oneinterface->pInterfaceName, -1,
                            interfaceName, sizeof(interfaceName), 0, 0);

        PNETWORK_ADDRESS_LIST addresses =
            &(oneinterface->AddressListDesc.AddressList);
        for(LONG i = 0; i < addresses->AddressCount; i++)
        {
            // Only look at TCP/IP addresses.
            if(addresses->Address[i].AddressType != NDIS_PROTOCOL_ID_TCP_IP)
            {
                continue;
            }

            NETWORK_ADDRESS_IP* pIpAddr =
                (NETWORK_ADDRESS_IP*)&(addresses->Address[i].Address);

            WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                         "Examining Interface %s", interfaceName);
            if(pIpAddr->in_addr == name->sin_addr.S_un.S_addr)
            {
                filterSourceAddress = pIpAddr->in_addr;
                WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                             "Found ip addr: %s", inet_ntoa(name->sin_addr));
                addrFound = true;
            }
        }
        if(!addrFound)
        {
            continue;
        }
        else
        {
            break;
        }
    }

    if(!addrFound)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id, "IP Address not found");
        _gtc->TcDeregisterClient(ClientHandle);
        free(pInterfaceBuffer);
        return -1;
    }


    result = _gtc->TcOpenInterfaceW(oneinterface->pInterfaceName, ClientHandle,
                                    NULL, &ifcHandle);

    if(result != NO_ERROR)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "Error opening interface: %d", result);
        _gtc->TcDeregisterClient(ClientHandle);
        free(pInterfaceBuffer);
        return result;
    }

    FLOWSPEC defaultSend, defaultRecv;
    if(send == NULL)
    {
        defaultSend.DelayVariation = QOS_NOT_SPECIFIED;
        defaultSend.Latency = QOS_NOT_SPECIFIED;
        defaultSend.MaxSduSize = QOS_NOT_SPECIFIED;
        defaultSend.MinimumPolicedSize = QOS_NOT_SPECIFIED;
        defaultSend.PeakBandwidth = QOS_NOT_SPECIFIED;
        defaultSend.ServiceType = SERVICETYPE_BESTEFFORT;
        defaultSend.TokenBucketSize = QOS_NOT_SPECIFIED;
        defaultSend.TokenRate = 10000;
    }
    else
    {
        defaultSend = *send;
    }
     if(recv == NULL)
    {
        defaultRecv = defaultSend;
        defaultRecv.ServiceType = SERVICETYPE_CONTROLLEDLOAD;
    }
    else
    {
        defaultRecv = *recv;
    }


    PTC_GEN_FLOW flow =
        (PTC_GEN_FLOW)malloc(sizeof(TC_GEN_FLOW) + sizeof(QOS_DS_CLASS));

    flow->ReceivingFlowspec = defaultRecv;
    flow->SendingFlowspec = defaultSend;

    QOS_DS_CLASS dsClass;

    ZeroMemory((WebRtc_Word8*)&dsClass, sizeof(QOS_DS_CLASS));

    dsClass.DSField = serviceType;

    dsClass.ObjectHdr.ObjectType = QOS_OBJECT_DS_CLASS;
    dsClass.ObjectHdr.ObjectLength = sizeof(dsClass);

    memcpy(flow->TcObjects, (void*)&dsClass, sizeof(QOS_DS_CLASS));
    flow->TcObjectsLength = sizeof(dsClass);

    result = _gtc->TcAddFlow(ifcHandle, NULL, 0, flow, &iflowHandle);
    if(result != NO_ERROR)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "Error adding flow: %d", result);
        _gtc->TcCloseInterface(ifcHandle);
        _gtc->TcDeregisterClient(ClientHandle);
        free(pInterfaceBuffer);
        return -1;
    }

    free(flow);
    IP_PATTERN filterPattern, mask;

    ZeroMemory((WebRtc_Word8*)&filterPattern, sizeof(IP_PATTERN));
    ZeroMemory((WebRtc_Word8*)&mask, sizeof(IP_PATTERN));

    filterPattern.ProtocolId = IPPROTO_UDP;
    // "name" fields are in network order.
    filterPattern.S_un.S_un_ports.s_srcport = name->sin_port;
    filterPattern.SrcAddr = filterSourceAddress;

    // Unsigned max of a type corresponds to a bitmask with all bits set to 1.
    // I.e. the filter should allow all ProtocolIds, any source port and any
    // IP address.
    mask.ProtocolId = UCHAR_MAX;
    mask.S_un.S_un_ports.s_srcport = USHRT_MAX;
    mask.SrcAddr = ULONG_MAX;

    TC_GEN_FILTER filter;
    filter.AddressType = NDIS_PROTOCOL_ID_TCP_IP;
    filter.Mask = (LPVOID)&mask;
    filter.Pattern = (LPVOID)&filterPattern;
    filter.PatternSize = sizeof(IP_PATTERN);
    if(_filterHandle != INVALID_HANDLE_VALUE)
    {
        _gtc->TcDeleteFilter(_filterHandle);
    }

    result = _gtc->TcAddFilter(iflowHandle, &filter, &iFilterHandle);
    if(result != NO_ERROR)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "Error adding filter: %d", result);
        _gtc->TcDeleteFlow(iflowHandle);
        _gtc->TcCloseInterface(ifcHandle);
        _gtc->TcDeregisterClient(ClientHandle);
        free(pInterfaceBuffer);
        return result;
    }

    _flowHandle = iflowHandle;
    _filterHandle = iFilterHandle;
    _clientHandle = ClientHandle;

    _gtc->TcCloseInterface(ifcHandle);
    free(pInterfaceBuffer);

    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "Successfully created flow and filter.");
    return 0;
}
} // namespace webrtc
