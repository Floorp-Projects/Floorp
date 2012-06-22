/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "udp_socket_manager_posix.h"

#include <strings.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "trace.h"
#include "udp_socket_posix.h"

namespace webrtc {
UdpSocketManagerPosix::UdpSocketManagerPosix()
    : UdpSocketManager(),
      _id(-1),
      _critSect(CriticalSectionWrapper::CreateCriticalSection()),
      _numberOfSocketMgr(-1),
      _incSocketMgrNextTime(0),
      _nextSocketMgrToAssign(0),
      _socketMgr()
{
}

bool UdpSocketManagerPosix::Init(WebRtc_Word32 id,
                                 WebRtc_UWord8& numOfWorkThreads) {
    CriticalSectionScoped cs(_critSect);
    if ((_id != -1) || (_numOfWorkThreads != 0)) {
        assert(_id != -1);
        assert(_numOfWorkThreads != 0);
        return false;
    }

    _id = id;
    _numberOfSocketMgr = numOfWorkThreads;
    _numOfWorkThreads = numOfWorkThreads;

    if(MAX_NUMBER_OF_SOCKET_MANAGERS_LINUX < _numberOfSocketMgr)
    {
        _numberOfSocketMgr = MAX_NUMBER_OF_SOCKET_MANAGERS_LINUX;
    }
    for(int i = 0;i < _numberOfSocketMgr; i++)
    {
        _socketMgr[i] = new UdpSocketManagerPosixImpl();
    }
    return true;
}


UdpSocketManagerPosix::~UdpSocketManagerPosix()
{
    Stop();
    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "UdpSocketManagerPosix(%d)::UdpSocketManagerPosix()",
                 _numberOfSocketMgr);

    for(int i = 0;i < _numberOfSocketMgr; i++)
    {
        delete _socketMgr[i];
    }
    delete _critSect;
}

WebRtc_Word32 UdpSocketManagerPosix::ChangeUniqueId(const WebRtc_Word32 id)
{
    _id = id;
    return 0;
}

bool UdpSocketManagerPosix::Start()
{
    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "UdpSocketManagerPosix(%d)::Start()",
                 _numberOfSocketMgr);

    _critSect->Enter();
    bool retVal = true;
    for(int i = 0;i < _numberOfSocketMgr && retVal; i++)
    {
        retVal = _socketMgr[i]->Start();
    }
    if(!retVal)
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "UdpSocketManagerPosix(%d)::Start() error starting socket managers",
            _numberOfSocketMgr);
    }
    _critSect->Leave();
    return retVal;
}

bool UdpSocketManagerPosix::Stop()
{
    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "UdpSocketManagerPosix(%d)::Stop()",_numberOfSocketMgr);

    _critSect->Enter();
    bool retVal = true;
    for(int i = 0; i < _numberOfSocketMgr && retVal; i++)
    {
        retVal = _socketMgr[i]->Stop();
    }
    if(!retVal)
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "UdpSocketManagerPosix(%d)::Stop() there are still active socket "
            "managers",
            _numberOfSocketMgr);
    }
    _critSect->Leave();
    return retVal;
}

bool UdpSocketManagerPosix::AddSocket(UdpSocketWrapper* s)
{
    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "UdpSocketManagerPosix(%d)::AddSocket()",_numberOfSocketMgr);

    _critSect->Enter();
    bool retVal = _socketMgr[_nextSocketMgrToAssign]->AddSocket(s);
    if(!retVal)
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "UdpSocketManagerPosix(%d)::AddSocket() failed to add socket to\
 manager",
            _numberOfSocketMgr);
    }

    // Distribute sockets on UdpSocketManagerPosixImpls in a round-robin
    // fashion.
    if(_incSocketMgrNextTime == 0)
    {
        _incSocketMgrNextTime++;
    } else {
        _incSocketMgrNextTime = 0;
        _nextSocketMgrToAssign++;
        if(_nextSocketMgrToAssign >= _numberOfSocketMgr)
        {
            _nextSocketMgrToAssign = 0;
        }
    }
    _critSect->Leave();
    return retVal;
}

bool UdpSocketManagerPosix::RemoveSocket(UdpSocketWrapper* s)
{
    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "UdpSocketManagerPosix(%d)::RemoveSocket()",
                 _numberOfSocketMgr);

    _critSect->Enter();
    bool retVal = false;
    for(int i = 0;i < _numberOfSocketMgr && (retVal == false); i++)
    {
        retVal = _socketMgr[i]->RemoveSocket(s);
    }
    if(!retVal)
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "UdpSocketManagerPosix(%d)::RemoveSocket() failed to remove socket\
 from manager",
            _numberOfSocketMgr);
    }
    _critSect->Leave();
    return retVal;
}


UdpSocketManagerPosixImpl::UdpSocketManagerPosixImpl()
{
    _critSectList = CriticalSectionWrapper::CreateCriticalSection();
    _thread = ThreadWrapper::CreateThread(UdpSocketManagerPosixImpl::Run, this,
                                          kRealtimePriority,
                                          "UdpSocketManagerPosixImplThread");
    FD_ZERO(&_readFds);
    WEBRTC_TRACE(kTraceMemory,  kTraceTransport, -1,
                 "UdpSocketManagerPosix created");
}

UdpSocketManagerPosixImpl::~UdpSocketManagerPosixImpl()
{
    if(_thread != NULL)
    {
        delete _thread;
    }

    if (_critSectList != NULL)
    {
        UpdateSocketMap();

        _critSectList->Enter();

        MapItem* item = _socketMap.First();
        while(item)
        {
            UdpSocketPosix* s = static_cast<UdpSocketPosix*>(item->GetItem());
            _socketMap.Erase(item);
            item = _socketMap.First();
            delete s;
        }
        _critSectList->Leave();

        delete _critSectList;
    }

    WEBRTC_TRACE(kTraceMemory,  kTraceTransport, -1,
                 "UdpSocketManagerPosix deleted");
}

bool UdpSocketManagerPosixImpl::Start()
{
    unsigned int id = 0;
    if (_thread == NULL)
    {
        return false;
    }

    WEBRTC_TRACE(kTraceStateInfo,  kTraceTransport, -1,
                 "Start UdpSocketManagerPosix");
    return _thread->Start(id);
}

bool UdpSocketManagerPosixImpl::Stop()
{
    if (_thread == NULL)
    {
        return true;
    }

    WEBRTC_TRACE(kTraceStateInfo,  kTraceTransport, -1,
                 "Stop UdpSocketManagerPosix");
    return _thread->Stop();
}

bool UdpSocketManagerPosixImpl::Process()
{
    bool doSelect = false;
    // Timeout = 1 second.
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 10000;
    MapItem* it;

    FD_ZERO(&_readFds);

    UpdateSocketMap();

    unsigned int maxFd = 0;
    for (it = _socketMap.First(); it != NULL; it=_socketMap.Next(it))
    {
        doSelect = true;
        maxFd = maxFd > it->GetUnsignedId() ? maxFd : it->GetUnsignedId();
        FD_SET(it->GetUnsignedId(), &_readFds);

        maxFd = maxFd > it->GetUnsignedId() ? maxFd : it->GetUnsignedId();
        doSelect = true;
    }

    int num = 0;
    if (doSelect)
    {
        num = select(maxFd+1, &_readFds, NULL, NULL, &timeout);

        if (num == SOCKET_ERROR)
        {
            // Timeout = 10 ms.
            timespec t;
            t.tv_sec = 0;
            t.tv_nsec = 10000*1000;
            nanosleep(&t, NULL);
            return true;
        }
    }else
    {
        // Timeout = 10 ms.
        timespec t;
        t.tv_sec = 0;
        t.tv_nsec = 10000*1000;
        nanosleep(&t, NULL);
        return true;
    }

    for (it = _socketMap.First(); it != NULL && num > 0;
         it = _socketMap.Next(it))
    {
        UdpSocketPosix* s = static_cast<UdpSocketPosix*>(it->GetItem());
        if (FD_ISSET(it->GetUnsignedId(), &_readFds))
        {
            s->HasIncoming();
            num--;
        }
    }
    return true;
}

bool UdpSocketManagerPosixImpl::Run(ThreadObj obj)
{
    UdpSocketManagerPosixImpl* mgr =
        static_cast<UdpSocketManagerPosixImpl*>(obj);
    return mgr->Process();
}

bool UdpSocketManagerPosixImpl::AddSocket(UdpSocketWrapper* s)
{
    UdpSocketPosix* sl = static_cast<UdpSocketPosix*>(s);
    if(sl->GetFd() == INVALID_SOCKET || !(sl->GetFd() < FD_SETSIZE))
    {
        return false;
    }
    _critSectList->Enter();
    _addList.PushBack(s);
    _critSectList->Leave();
    return true;
}

bool UdpSocketManagerPosixImpl::RemoveSocket(UdpSocketWrapper* s)
{
    // Put in remove list if this is the correct UdpSocketManagerPosixImpl.
    _critSectList->Enter();

    // If the socket is in the add list it's safe to remove and delete it.
    ListItem* addListItem = _addList.First();
    while(addListItem)
    {
        UdpSocketPosix* addSocket = (UdpSocketPosix*)addListItem->GetItem();
        unsigned int addFD = addSocket->GetFd();
        unsigned int removeFD = static_cast<UdpSocketPosix*>(s)->GetFd();
        if(removeFD == addFD)
        {
            _removeList.PushBack(removeFD);
            _critSectList->Leave();
            return true;
        }
        addListItem = _addList.Next(addListItem);
    }

    // Checking the socket map is safe since all Erase and Insert calls to this
    // map are also protected by _critSectList.
    if(_socketMap.Find(static_cast<UdpSocketPosix*>(s)->GetFd()) != NULL)
    {
        _removeList.PushBack(static_cast<UdpSocketPosix*>(s)->GetFd());
        _critSectList->Leave();
         return true;
    }
    _critSectList->Leave();
    return false;
}

void UdpSocketManagerPosixImpl::UpdateSocketMap()
{
    // Remove items in remove list.
    _critSectList->Enter();
    while(!_removeList.Empty())
    {
        UdpSocketPosix* deleteSocket = NULL;
        unsigned int removeFD = _removeList.First()->GetUnsignedItem();

        // If the socket is in the add list it hasn't been added to the socket
        // map yet. Just remove the socket from the add list.
        ListItem* addListItem = _addList.First();
        while(addListItem)
        {
            UdpSocketPosix* addSocket = (UdpSocketPosix*)addListItem->GetItem();
            unsigned int addFD = addSocket->GetFd();
            if(removeFD == addFD)
            {
                deleteSocket = addSocket;
                _addList.Erase(addListItem);
                break;
            }
            addListItem = _addList.Next(addListItem);
        }

        // Find and remove socket from _socketMap.
        MapItem* it = _socketMap.Find(removeFD);
        if(it != NULL)
        {
            UdpSocketPosix* socket =
                static_cast<UdpSocketPosix*>(it->GetItem());
            if(socket)
            {
                deleteSocket = socket;
            }
            _socketMap.Erase(it);
        }
        if(deleteSocket)
        {
            deleteSocket->ReadyForDeletion();
            delete deleteSocket;
        }
        _removeList.PopFront();
    }

    // Add sockets from add list.
    while(!_addList.Empty())
    {
        UdpSocketPosix* s =
            static_cast<UdpSocketPosix*>(_addList.First()->GetItem());
        if(s)
        {
            _socketMap.Insert(s->GetFd(), s);
        }
        _addList.PopFront();
    }
    _critSectList->Leave();
}
} // namespace webrtc
