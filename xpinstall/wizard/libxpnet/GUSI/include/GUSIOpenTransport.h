// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSIOpenTransport.nw-	OpenTransport sockets                   
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSIOpenTransport.h,v $
// % Revision 1.1  2001/03/11 22:37:24  sgehani%netscape.com
// % First Checked In.
// %                                        
// % Revision 1.18  2001/01/17 08:58:06  neeri                             
// % Releasing 2.1.4                                                       
// %                                                                       
// % Revision 1.17  2000/10/16 04:07:23  neeri                             
// % Fix accept code                                                       
// %                                                                       
// % Revision 1.16  2000/06/01 06:31:10  neeri                             
// % Reset shutdown flags on connect, refine test for data available, fix memory leak in UDP sendto
// %                                                                       
// % Revision 1.15  2000/05/23 07:13:19  neeri                             
// % Improve formatting, implement immediate close and sorrect linger handling
// %                                                                       
// % Revision 1.14  2000/03/15 07:19:53  neeri                             
// % Fix numerous race conditions                                          
// %                                                                       
// % Revision 1.13  2000/03/06 06:10:01  neeri                             
// % Reorganize Yield()                                                    
// %                                                                       
// % Revision 1.12  1999/12/14 06:28:29  neeri                             
// % Read pending data while closing                                       
// %                                                                       
// % Revision 1.11  1999/12/13 02:44:19  neeri                             
// % Fix SO_LINGER, read results for disconnected sockets                  
// %                                                                       
// % Revision 1.10  1999/10/15 02:48:50  neeri                             
// % Make disconnects orderly                                              
// %                                                                       
// % Revision 1.9  1999/09/09 07:20:29  neeri                              
// % Fix numerous bugs, add support for interface ioctls                   
// %                                                                       
// % Revision 1.8  1999/09/03 06:31:36  neeri                              
// % Needed more mopups                                                    
// %                                                                       
// % Revision 1.7  1999/08/26 05:43:09  neeri                              
// % Supply missing Unbind                                                 
// %                                                                       
// % Revision 1.6  1999/08/02 07:02:45  neeri                              
// % Support for asynchronous errors and other socket options              
// %                                                                       
// % Revision 1.5  1999/07/19 06:17:44  neeri                              
// % Fix nonblocking connect                                               
// %                                                                       
// % Revision 1.4  1999/06/28 06:04:59  neeri                              
// % Support interrupted calls                                             
// %                                                                       
// % Revision 1.3  1999/05/30 03:06:41  neeri                              
// % MPW compiler compatibility, fix select for datagram sockets           
// %                                                                       
// % Revision 1.2  1999/04/29 05:33:19  neeri                              
// % Fix fcntl prototype                                                   
// %                                                                       
// % Revision 1.1  1999/03/17 09:05:11  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{Open Transport socket infrastructure}                          
//                                                                         
// A [[GUSIOTSocket]] defines a class of Open Transport sockets. Since most 
// families differ only in a few details, like address representation, we  
// abstract the typical differences in a strategy class [[GUSIOTStrategy]].
//                                                                         
// <GUSIOpenTransport.h>=                                                  
#ifndef _GUSIOpenTransport_
#define _GUSIOpenTransport_

#ifdef GUSI_INTERNAL

#include "GUSISocket.h"
#include "GUSISocketMixins.h"
#include "GUSIFactory.h"

#include <netinet/in.h>
#include <netinet/tcp.h>

#undef O_ASYNC
#undef O_NDELAY
#undef O_NONBLOCK
#undef SIGHUP
#undef SIGURG
#undef AF_INET
#undef TCP_KEEPALIVE
#undef TCP_NODELAY
#undef TCP_MAXSEG

#include <OpenTransport.h>
#include <OpenTptInternet.h>

// \section{Definition of [[GUSIOTStrategy]]}                              
//                                                                         
// A [[GUSIOTStrategy]] contains all the tricky parts of each Open Transport
// family.                                                                 
//                                                                         
// <Definition of class [[GUSIOTStrategy]]>=                               
class GUSIOTStrategy {
public:
	// [[CreateConfiguration]] creates an appropriate [[OTConfiguration]]. This 
 // method is not virtual, as it relies on the strategy method              
 // [[ConfigPath]].                                                         
 //                                                                         
 // <Strategic interfaces for [[GUSIOTStrategy]]>=                          
 OTConfiguration *		CreateConfiguration();
 // [[PackAddress]] converts a socket address into an OT address, and       
 // [[UnpackAddress]] performs the reverse step. [[CopyAddress]] copies an address.
 //                                                                         
 // <Strategic interfaces for [[GUSIOTStrategy]]>=                          
 virtual	int	PackAddress(
 	const void * address, socklen_t len, TNetbuf & addr, bool non_null = false) = 0;
 virtual	int	UnpackAddress(const TNetbuf & addr, void * address, socklen_t * len) = 0;
 virtual int CopyAddress(const TNetbuf & from, TNetbuf & to);
 // [[EndpointInfo]] returns a data structure storing endpoint parameters. We only
 // need one copy per socket type.                                          
 //                                                                         
 // <Strategic interfaces for [[GUSIOTStrategy]]>=                          
 TEndpointInfo *	EndpointInfo()	{ return &fEndpointInfo; }
protected:
	// <Privatissima of [[GUSIOTStrategy]]>=                                   
 virtual const char *	ConfigPath() = 0;
 // <Privatissima of [[GUSIOTStrategy]]>=                                   
 TEndpointInfo	fEndpointInfo;
 // \section{Implementation of [[GUSIOTStrategy]]}                          
 //                                                                         
 // [[GUSIOTStrategy]] is mostly abstract, except for the [[CreateConfiguration]]
 // and [[CopyAddress]] methods.                                            
 //                                                                         
 // <Privatissima of [[GUSIOTStrategy]]>=                                   
 OTConfiguration *	fConfig;
 GUSIOTStrategy() 					: fConfig(nil) {}
 virtual ~GUSIOTStrategy();
};
// \section{Definition of [[GUSIOTFactory]] and descendants}               
//                                                                         
// A [[GUSIOTFactory]] is an abstract class combining a socket creation    
// mechanism with a strategy instance. To clarify our intent, we isolate   
// the latter in [[GUSIOTStrategy]].                                       
//                                                                         
// <Definition of class [[GUSIOTFactory]]>=                                
class GUSIOTFactory : public GUSISocketFactory {
public:
	static	bool			Initialize();
protected:
	virtual GUSIOTStrategy *Strategy(int domain, int type, int protocol) = 0;
private:
	// \section{Implementation of [[GUSIOTFactory]] and descendants}           
 //                                                                         
 //                                                                         
 // <Privatissima of [[GUSIOTFactory]]>=                                    
 static bool	sOK;
};
// <Definition of class [[GUSIOTStreamFactory]]>=                          
class GUSIOTStreamFactory : public GUSIOTFactory {
public:
	GUSISocket * socket(int domain, int type, int protocol);
};
// <Definition of class [[GUSIOTDatagramFactory]]>=                        
class GUSIOTDatagramFactory : public GUSIOTFactory {
public:
	GUSISocket * socket(int domain, int type, int protocol);
};
// \section{Definition of [[GUSIOT]]}                                      
//                                                                         
// Open Transport allocates and deallocates many data structures, which we 
// simplify with a smart template. Allocation works via class allocation   
// operators, which is a bit weird admittedly.                             
//                                                                         
// <Definition of template [[GUSIOT]]>=                                    
template <class T, int tag> class GUSIOT : public T {
public:
	void * operator new(size_t, EndpointRef ref)	
		{	OSStatus err; return OTAlloc(ref, tag, T_ALL, &err); 	}
	void * operator new(size_t, EndpointRef ref, int fields)	
		{	OSStatus err; return OTAlloc(ref, tag, fields, &err); 	}
	void operator delete(void * o)
		{ 	if (o) OTFree(o, tag);									}
};	
template <class T, int tag> class GUSIOTAddr : public GUSIOT<T, tag> {
public:
	int	Pack(GUSIOTStrategy * strategy, const void * address, socklen_t len, bool non_null=false)
		{	return strategy->PackAddress(address, len, addr, non_null);	}
	int	Unpack(GUSIOTStrategy * strategy, void * address, socklen_t * len)
		{	return strategy->UnpackAddress(addr, address, len);			}
	int Copy(GUSIOTStrategy * strategy, GUSIOTAddr<T, tag> * to)
		{	return strategy->CopyAddress(addr, to->addr);				}
};

typedef GUSIOTAddr<TBind, 		T_BIND>		GUSIOTTBind;
typedef GUSIOTAddr<TCall, 		T_CALL>		GUSIOTTCall;
typedef GUSIOTAddr<TUnitData,	T_UNITDATA>	GUSIOTTUnitData;
typedef GUSIOTAddr<TUDErr,		T_UDERROR>	GUSIOTTUDErr;
typedef GUSIOT<TDiscon, 		T_DIS>		GUSIOTTDiscon;
typedef GUSIOT<TOptMgmt,		T_OPTMGMT>	GUSIOTTOptMgmt;
// \section{Definition of [[GUSIOTSocket]] and descendants}                
//                                                                         
// Open Transport sockets are rather lightweight, since OT is rather similar 
// to sockets already.                                                     
//                                                                         
// <Definition of class [[GUSIOTSocket]]>=                                 
class GUSIOTSocket  : 
	public 		GUSISocket, 
	protected 	GUSISMBlocking, 
	protected 	GUSISMState,
	protected	GUSISMAsyncError
{
public:
	// <Overridden member functions for [[GUSIOTSocket]]>=                     
 virtual int	bind(void * name, socklen_t namelen);
 // [[getsockname]] and [[getpeername]] unpack the stored addresses.        
 // Note that the reaction to [[nil]] addresses is a bit different.         
 //                                                                         
 // <Overridden member functions for [[GUSIOTSocket]]>=                     
 virtual int getsockname(void * name, socklen_t * namelen);
 // [[shutdown]] just delegates to [[GUSISMState]].                         
 //                                                                         
 // <Overridden member functions for [[GUSIOTSocket]]>=                     
 virtual int shutdown(int how);
 // [[fcntl]] handles the blocking support.                                 
 //                                                                         
 // <Overridden member functions for [[GUSIOTSocket]]>=                     
 virtual int fcntl(int cmd, va_list arg);
 // [[ioctl]] deals with blocking support and with [[FIONREAD]].            
 //                                                                         
 // <Overridden member functions for [[GUSIOTSocket]]>=                     
 virtual int ioctl(unsigned int request, va_list arg);
 // [[getsockopt]] and [[setsockopt]] are available for a variety of options.
 //                                                                         
 // <Overridden member functions for [[GUSIOTSocket]]>=                     
 virtual int getsockopt(int level, int optname, void *optval, socklen_t * optlen);
 // <Overridden member functions for [[GUSIOTSocket]]>=                     
 virtual int setsockopt(int level, int optname, void *optval, socklen_t optlen);
 // Open Transport sockets implement socket style calls.                    
 //                                                                         
 // <Overridden member functions for [[GUSIOTSocket]]>=                     
 virtual bool Supports(ConfigOption config);
protected:
	GUSIOTSocket(GUSIOTStrategy * strategy);

	// \section{Implementation of [[GUSIOTSocket]]}                            
 //                                                                         
 // Open Transport may call this routine for dozens and dozens of different 
 // reasons. Pretty much every call results in a wakeup of the threads attached
 // to the socket. We save some of the more interesting events in bitsets.  
 // in [[MopupEvents]].                                                     
 //                                                                         
 // <Privatissima of [[GUSIOTSocket]]>=                                     
 uint16_t		fNewEvent;
 uint16_t		fCurEvent;
 uint16_t		fEvent;
 uint32_t		fNewCompletion;
 uint32_t		fCurCompletion;
 uint32_t		fCompletion;
 friend pascal void GUSIOTNotify(GUSIOTSocket *, OTEventCode, OTResult, void *);
 // To avoid race conditions with the notifier, we sometimes need a lock.   
 //                                                                         
 // <Privatissima of [[GUSIOTSocket]]>=                                     
 class Lock {
 public:
 	Lock(EndpointRef end)	: fEndpoint(end)	{ OTEnterNotifier(fEndpoint);	} 
 	~Lock()										{ OTLeaveNotifier(fEndpoint);	} 
 private:
 	EndpointRef	fEndpoint;
 };
 // For some events, we have to take a followup action at a more convenient time.
 //                                                                         
 // <Privatissima of [[GUSIOTSocket]]>=                                     
 virtual void MopupEvents();
 // [[GUSIOTSocket]] creates an asynchronous endpoint for the appropriate   
 // provider.                                                               
 //                                                                         
 // <Privatissima of [[GUSIOTSocket]]>=                                     
 GUSIOTStrategy *	fStrategy;
 EndpointRef			fEndpoint;
 linger				fLinger;
 UInt32				fDeadline;
 // The destructor tears down the connection as gracefully as possible. It also respects
 // the linger settings.                                                    
 //                                                                         
 // <Privatissima of [[GUSIOTSocket]]>=                                     
 virtual void close();
 virtual ~GUSIOTSocket();
 // [[Clone]] creates another socket of the same class.                     
 //                                                                         
 // <Privatissima of [[GUSIOTSocket]]>=                                     
 virtual GUSIOTSocket * Clone() = 0;
 // At the time the socket function [[bind]] is called, we are not really ready
 // yet to call [[OTBind]], but if we don't call it, we can't report whether the
 // address was free.                                                       
 //                                                                         
 // <Privatissima of [[GUSIOTSocket]]>=                                     
 GUSIOTTBind *	fSockName;
 int BindToAddress(GUSIOTTBind * addr);
 // Open Transport takes unbinding a lot more serious than MacTCP.          
 //                                                                         
 // <Privatissima of [[GUSIOTSocket]]>=                                     
 void Unbind();
	
	friend class GUSIOTStreamSocket;
	friend class GUSIOTDatagramSocket;
};
// <Definition of class [[GUSIOTStreamSocket]]>=                           
class GUSIOTStreamSocket  : public GUSIOTSocket {
public:
	// [[Clone]] creates another socket of the same class.                     
 //                                                                         
 // <Overridden member functions for [[GUSIOTStreamSocket]]>=               
 virtual GUSIOTSocket * Clone();
 // <Overridden member functions for [[GUSIOTStreamSocket]]>=               
 virtual void close();
 virtual bool Close(UInt32 now);
 ~GUSIOTStreamSocket();
 // Stream sockets include a mopup action for connect and disconnect.       
 //                                                                         
 // <Overridden member functions for [[GUSIOTStreamSocket]]>=               
 virtual void MopupEvents();
 // [[listen]] is a bit embarassing, because we already committed ourselves 
 // to a queue length of [[0]], so we have to unbind and rebind ourselves.  
 //                                                                         
 // <Overridden member functions for [[GUSIOTStreamSocket]]>=               
 virtual int listen(int qlen);
 // <Overridden member functions for [[GUSIOTStreamSocket]]>=               
 virtual int getpeername(void * name, socklen_t * namelen);
 // [[accept]] may become quite complex, because connections could nest. The
 // listening socket calls [[OTListen]], queues candidates by their         
 // [[fNextListener]] field, and then trys calling [[OTAccept]] on the first
 // candidate.                                                              
 //                                                                         
 // <Overridden member functions for [[GUSIOTStreamSocket]]>=               
 virtual GUSISocket * accept(void * address, socklen_t * addrlen);
 // [[connect]] is comparatively simple.                                    
 //                                                                         
 // <Overridden member functions for [[GUSIOTStreamSocket]]>=               
 virtual int connect(void * address, socklen_t addrlen);
 // Data transfer is simple as well. Here is the version for stream protocols.
 //                                                                         
 // <Overridden member functions for [[GUSIOTStreamSocket]]>=               
 virtual ssize_t recvfrom(const GUSIScatterer & buffer, int flags, void * from, socklen_t * fromlen);
 // <Overridden member functions for [[GUSIOTStreamSocket]]>=               
 virtual ssize_t sendto(const GUSIGatherer & buffer, int flags, const void * to, socklen_t tolen);
 // [[select]] for stream sockets intermingles data information and connection
 // information as usual.                                                   
 //                                                                         
 // <Overridden member functions for [[GUSIOTStreamSocket]]>=               
 virtual bool select(bool * canRead, bool * canWrite, bool * except);
 // [[shutdown]] for stream sockets has to send an orderly disconnect.      
 //                                                                         
 // <Overridden member functions for [[GUSIOTStreamSocket]]>=               
 virtual int shutdown(int how);
protected:
	GUSIOTStreamSocket(GUSIOTStrategy * strategy);

	// Since all we need to know is in the [[GUSIOTStrategy]], it often suffices 
 // simply to create a [[GUSIOTSocket]]. Stream and datagram sockets differ 
 // merely in the descendant they create.                                   
 //                                                                         
 // <Privatissima of [[GUSIOTStreamSocket]]>=                               
 friend class GUSIOTStreamFactory;
 // <Privatissima of [[GUSIOTStreamSocket]]>=                               
 friend pascal void GUSIOTNotify(GUSIOTSocket *, OTEventCode, OTResult, void *);
 // The peer address for a [[GUSIOTStreamSocket]] is stored in a [[GUSIOTTCall]]
 // structure.                                                              
 //                                                                         
 // <Privatissima of [[GUSIOTStreamSocket]]>=                               
 GUSIOTTCall *	fPeerName;
 // <Privatissima of [[GUSIOTStreamSocket]]>=                               
 GUSIOTStreamSocket *	fNextListener;
};
// <Definition of class [[GUSIOTDatagramSocket]]>=                         
class GUSIOTDatagramSocket  : public GUSIOTSocket {
public:
	// [[Clone]] creates another socket of the same class.                     
 //                                                                         
 // <Overridden member functions for [[GUSIOTDatagramSocket]]>=             
 virtual GUSIOTSocket * Clone();
 // <Overridden member functions for [[GUSIOTDatagramSocket]]>=             
 ~GUSIOTDatagramSocket();
 // <Overridden member functions for [[GUSIOTDatagramSocket]]>=             
 virtual int getpeername(void * name, socklen_t * namelen);
 // A datagram socket can [[connect]] as many times as it wants.            
 //                                                                         
 // <Overridden member functions for [[GUSIOTDatagramSocket]]>=             
 virtual int connect(void * address, socklen_t addrlen);
 // Datagram protocols use slightly different calls for data transfers.     
 //                                                                         
 // <Overridden member functions for [[GUSIOTDatagramSocket]]>=             
 virtual ssize_t recvfrom(const GUSIScatterer & buffer, int flags, void * from, socklen_t * fromlen);
 // [[sendto]] needs either a valid [[to]] address or a stored peer address set by
 // [[connect]].                                                            
 //                                                                         
 // <Overridden member functions for [[GUSIOTDatagramSocket]]>=             
 virtual ssize_t sendto(const GUSIGatherer & buffer, int flags, const void * to, socklen_t tolen);
 // [[select]] for datagram sockets returns data information only.          
 //                                                                         
 // <Overridden member functions for [[GUSIOTDatagramSocket]]>=             
 virtual bool select(bool * canRead, bool * canWrite, bool * except);
protected:
	GUSIOTDatagramSocket(GUSIOTStrategy * strategy);

	// <Privatissima of [[GUSIOTDatagramSocket]]>=                             
 friend class GUSIOTDatagramFactory;
 // Datagram sockets might be bound at rather arbitrary times.              
 //                                                                         
 // <Privatissima of [[GUSIOTDatagramSocket]]>=                             
 int BindIfUnbound();
 // The peer address for a [[GUSIOTDatagramSocket]] is stored in a [[GUSIOTTBind]]
 // structure.                                                              
 //                                                                         
 // <Privatissima of [[GUSIOTDatagramSocket]]>=                             
 GUSIOTTBind *	fPeerName;
};

#endif /* GUSI_INTERNAL */

#endif /* _GUSIOpenTransport_ */
