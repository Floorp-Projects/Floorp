// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSISocket.nw		-	The socket class                             
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSISocket.h,v $
// % Revision 1.1  2001/03/11 22:38:15  sgehani%netscape.com
// % First Checked In.
// %                                               
// % Revision 1.18  2000/10/16 04:34:23  neeri                             
// % Releasing 2.1.2                                                       
// %                                                                       
// % Revision 1.17  2000/05/23 07:19:34  neeri                             
// % Improve formatting, add close queue                                   
// %                                                                       
// % Revision 1.16  2000/03/15 07:20:53  neeri                             
// % Add GUSISocket::AddContextInScope                                     
// %                                                                       
// % Revision 1.15  1999/10/15 02:48:51  neeri                             
// % Make disconnects orderly                                              
// %                                                                       
// % Revision 1.14  1999/09/26 03:59:26  neeri                             
// % Releasing 2.0fc1                                                      
// %                                                                       
// % Revision 1.13  1999/08/26 05:45:09  neeri                             
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.12  1999/06/08 04:31:31  neeri                             
// % Getting ready for 2.0b2                                               
// %                                                                       
// % Revision 1.11  1999/05/29 06:26:45  neeri                             
// % Fixed header guards                                                   
// %                                                                       
// % Revision 1.10  1999/04/29 05:33:18  neeri                             
// % Fix fcntl prototype                                                   
// %                                                                       
// % Revision 1.9  1999/03/17 09:05:13  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// % Revision 1.8  1998/11/22 23:07:01  neeri                              
// % Releasing 2.0a4 in a hurry                                            
// %                                                                       
// % Revision 1.7  1998/10/11 16:45:23  neeri                              
// % Ready to release 2.0a2                                                
// %                                                                       
// % Revision 1.6  1998/08/01 21:29:53  neeri                              
// % Use context queues                                                    
// %                                                                       
// % Revision 1.5  1998/01/25 20:53:58  neeri                              
// % Engine implemented, except for signals & scheduling                   
// %                                                                       
// % Revision 1.4  1997/11/13 21:12:12  neeri                              
// % Fall 1997                                                             
// %                                                                       
// % Revision 1.3  1996/11/24  13:00:28  neeri                             
// % Fix comment leaders                                                   
// %                                                                       
// % Revision 1.2  1996/11/24  12:52:09  neeri                             
// % Added GUSIPipeSockets                                                 
// %                                                                       
// % Revision 1.1.1.1  1996/11/03  02:43:32  neeri                         
// % Imported into CVS                                                     
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{The GUSI Socket Class}                                         
//                                                                         
// GUSI is constructed around the [[GUSISocket]] class. This class is      
// mostly an abstract superclass, but all virtual procedures are implemented
// to return sensible error codes.                                         
//                                                                         
// <GUSISocket.h>=                                                         
#ifndef _GUSISocket_
#define _GUSISocket_

#ifdef GUSI_SOURCE

#include "GUSIBasics.h"
#include "GUSIContext.h"
#include "GUSIContextQueue.h"
#include "GUSIBuffer.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <stdarg.h>

#include <ConditionalMacros.h>
#include <LowMem.h>

#if PRAGMA_STRUCT_ALIGN
#pragma options align=native
#endif

// \section{Definition of [[GUSISocket]]}                                  
//                                                                         
// [[GUSISocket]] consists of a few maintenance functions and the socket operations.
// Each operation consists to a POSIX/BSD function with the file descriptor operand
// left out.                                                               
//                                                                         
// <Definition of class [[GUSISocket]]>=                                   
class GUSISocket {
	// Since a single [[GUSISocket]] may (through [[dup]]) be installed multiply
 // in a descriptor table or even in multiple descriptor tables, [[GUSISocket]]s
 // are not destroyed directly, but by manipulating a reference count. As soon
 // as the reference count hits zero, the destructor (which, of course, should 
 // probably be overridden) is called.                                      
 //                                                                         
 // Since destructors cannot call virtual functions, we call [[close]] which 
 // eventually calls the destructor. Some socket types can take quite long to close
 // under unfavorable circumstances. To speed up the process, we have the option of
 // queueing the socket up and regularly having [[Close]] called on it.     
 //                                                                         
 // <Reference counting for [[GUSISocket]]>=                                
 public:
 	void			AddReference();
 	void 			RemoveReference();
 	
 	virtual void	close();
 	void 			CheckClose(UInt32 now = LMGetTicks());
 protected:
 	GUSISocket();
 	virtual			~GUSISocket();
 	virtual	bool	Close(UInt32 now = LMGetTicks());
 private:
 	u_long	fRefCount;
	// [[GUSIContext]]s are defined in {\tt GUSIBasics}. A context references all 
 // information you need in a completion procedure: The contents of [[A5]], 
 // the process ID, and thread information. [[Wakeup]] wakes up the threads 
 // and/or processes associated with the socket and is guaranteed to work even
 // at interrupt level. [[AddContext]] adds another context. [[RemoveContext]] 
 // indicates that this context no longer should be woken up when something happens.
 // To keep a context added inside a scope, declare an automatic object of class
 // [[AddContextInScope]].                                                  
 //                                                                         
 // <Context links for [[GUSISocket]]>=                                     
 public:
 	void				Wakeup();
 	void				AddContext(GUSIContext * context = nil);
 	void				RemoveContext(GUSIContext * context = nil);
 	
 	class AddContextInScope {
 	public:
 		AddContextInScope(GUSISocket * sock, GUSIContext *	context = nil) 
 			: fSocket(sock), fContext(context)	
 								{ fSocket->AddContext(fContext);	}
 		~AddContextInScope()	{ fSocket->RemoveContext(fContext);	}
 	private:
 		GUSISocket *	fSocket;
 		GUSIContext *	fContext;
 	};
 private:
 	GUSIContextQueue	fContexts;
	// There may be various reasons to keep sockets in queue. Currently the    
 // only reason is to queue up dying sockets.                               
 //                                                                         
 // <Queue management for [[GUSISocket]]>=                                  
 public:
 	void Enqueue(GUSISocket ** queue);
 	void Dequeue();
 private:
 	GUSISocket **	fQueue;
 	GUSISocket *	fNextSocket;
 	GUSISocket *	fPrevSocket;
	// Both read and write calls on sockets come in five different variants:   
 //                                                                         
 // \begin{enumerate}                                                       
 // \item [[read]] and [[write]]                                            
 // \item [[recv]] and [[send]]                                             
 // \item [[readv]] and [[writev]]                                          
 // \item [[recvfrom]] and [[sendto]]                                       
 // \item [[recvmsg]] and [[sendmsg]]                                       
 // \end{enumerate}                                                         
 //                                                                         
 // GUSI initially maps variants 3 and 5 of these calls to the [[recvmsg]] and 
 // [[sendmsg]] member functions, variants 2 and 4 to the [[recvfrom]] and  
 // [[sendto]] member functions, and variant 1 to the [[read]] and          
 // [[write]] member functions.                                             
 //                                                                         
 // The simpler member functions can always be translated into the complex member
 // functions, and under some circumstances, the opposite is also possible. 
 // To avoid translation loops, the translation routines (i.e., the default 
 // implementation of [[GUSISocket::read]] and [[GUSISocket::recvmsg]]      
 // check for the availablility of the other function by calling [[Supports]].
 // This member function must be overridden for any reasonable socket class.
 //                                                                         
 // <Configuration options for [[GUSISocket]]>=                             
 protected:
 	enum ConfigOption {
 		kSimpleCalls,		// [[read]], [[write]]
 		kSocketCalls,		// [[recvfrom]], [[sendto]]
 		kMsgCalls			// [[recvmsg]], [[sendmsg]]
 	};
 	virtual bool	Supports(ConfigOption config);
public:
	// Most sockets have names, which to [[GUSISocket]] are just opaque blocks of
 // memory. A name for a socket is established (before the socket is actually
 // used, of course) through [[bind]]. The name may be queried with         
 // [[getsockname]] and once the socket is connected, the name of the peer  
 // endpoint may be queried with [[getpeername]].                           
 //                                                                         
 // <Socket name management for [[GUSISocket]]>=                            
 virtual int	bind(void * name, socklen_t namelen);
 virtual int getsockname(void * name, socklen_t * namelen);
 virtual int getpeername(void * name, socklen_t * namelen);
	// Sockets follow either a virtual circuit model where all data is exchanged
 // with the same peer throughout the lifetime of the connection, or a datagram
 // model where potentially every message is exchanged with a different peer.
 //                                                                         
 // The vast majority of protocols follow the virtual circuit model. The server
 // end, typically after calling [[bind]] to attach the socket to a well known
 // address, calls [[listen]] to establish its willingness to accept connections.
 // [[listen]] takes a queue length parameter, which however is ignored for many
 // types of sockets.                                                       
 //                                                                         
 // Incoming connections are then accepted by calling [[accept]]. When [[accept]]
 // is successful, it always returns a new [[GUSISocket]], while the original socket 
 // remains available for further connections. To avoid blocking on [[accept]], you may poll for connections with an 
 // [[accept()] call in nonblocking mode or query the result of [[select]] whether 
 // the socket is ready for reading.                                        
 //                                                                         
 // The client end in the virtual circuit model connects itself to the well known
 // address by calling [[connect]]. To avoid blocking on [[connect]], you may
 // call it in nonblocking mode and then query the result of [[select]] whether 
 // the socket is ready for writing.                                        
 //                                                                         
 // In the datagram model, you don't need to establish connections. You may call
 // [[connect]] anyway to temporarily establish a virtual circuit.          
 //                                                                         
 // <Connection establishment for [[GUSISocket]]>=                          
 virtual int listen(int qlen);
 virtual GUSISocket * accept(void * address, socklen_t * addrlen);
 virtual int connect(void * address, socklen_t addrlen);
	// As mentioned before, there are three variants each for reading and writing. 
 // The socket variants provide a means to pass a peer address for the datagram 
 // model, while the msg variants also provides fields for passing access rights, 
 // which is, however not currently supported in GUSI. As syntactic sugar, the more
 // traditional flavors with [[buffer]]/[[length]] buffers are also supported.
 //                                                                         
 // <Sending and receiving data for [[GUSISocket]]>=                        
 virtual ssize_t	read(const GUSIScatterer & buffer);
 virtual ssize_t write(const GUSIGatherer & buffer);
 virtual ssize_t recvfrom(
 			const GUSIScatterer & buffer, int flags, void * from, socklen_t * fromlen);
 virtual ssize_t sendto(
 			const GUSIGatherer & buffer, int flags, const void * to, socklen_t tolen);
 virtual ssize_t	recvmsg(msghdr * msg, int flags);
 virtual ssize_t sendmsg(const msghdr * msg, int flags);

 ssize_t	read(void * buffer, size_t length);
 ssize_t write(const void * buffer, size_t length);
 ssize_t recvfrom(
 			void * buffer, size_t length, int flags, void * from, socklen_t * fromlen);
 ssize_t sendto(
 			const void * buffer, size_t length, int flags, const void * to, socklen_t tolen);
	// A multitude of parameters can be manipulated for a [[GUSISocket]] through
 // the socket oriented calls [[getsockopt]], [[setsockopt]], the file oriented 
 // call [[fcntl]], and the device oriented call [[ioctl]].                 
 //                                                                         
 // [[isatty]] returns whether the socket should be considered an interactive 
 // console.                                                                
 //                                                                         
 // <Maintaining properties for [[GUSISocket]]>=                            
 virtual int getsockopt(int level, int optname, void *optval, socklen_t * optlen);
 virtual int setsockopt(int level, int optname, void *optval, socklen_t optlen);
 virtual int	fcntl(int cmd, va_list arg);
 virtual int	ioctl(unsigned int request, va_list arg);
 virtual int	isatty();
	// Three of the operations make sense primarily for files, and most other socket
 // types accept the default implementations. [[fstat]] returns information about
 // an open file, [[lseek]] repositions the read/write pointer, and [[ftruncate]]
 // cuts off an open file at a certain point.                               
 //                                                                         
 // <File oriented operations for [[GUSISocket]]>=                          
 virtual int	fstat(struct stat * buf);
 virtual off_t lseek(off_t offset, int whence);
 virtual int ftruncate(off_t offset);
	// [[select]] polls or waits for one of a group of [[GUSISocket]] to become
 // ready for reading, writing, or for an exceptional condition to occur.   
 // First, [[pre_select]] is called once for all [[GUSISocket]]s in the group.
 // It returns [[true]] is the socket will wake up as soon as one of the events
 // occurs and [[false]] if GUSI needs to poll.                             
 // Next, [[select]] is called for all [[GUSISocket]]s once or multiple times,
 // until a condition becomes true or the call times out. Finally, [[post_select]]
 // is called for all members of the group.                                 
 //                                                                         
 // <Multiplexing for [[GUSISocket]]>=                                      
 virtual bool pre_select(bool wantRead, bool wantWrite, bool wantExcept);
 virtual bool select(bool * canRead, bool * canWrite, bool * exception);
 virtual void post_select(bool wantRead, bool wantWrite, bool wantExcept);
	// A socket connection is usually full duplex. By calling [[shutdown(1)]], you 
 // indicate that you won't write any more data on this socket. The values 0 (no
 // more reads) and 2 (no more read/write) are used less frequently.        
 //                                                                         
 // <Miscellaneous operations for [[GUSISocket]]>=                          
 virtual int shutdown(int how);
 // Some socket types do not write out data immediately. Calling [[fsync]] guarantees
 // that all data is written.                                               
 //                                                                         
 // <Miscellaneous operations for [[GUSISocket]]>=                          
 virtual int fsync();
};

#if PRAGMA_STRUCT_ALIGN
#pragma options align=reset
#endif

// \section{Implementation of [[GUSISocket]]}                              
//                                                                         
// \subsection{General socket management}                                  
//                                                                         
//                                                                         
// <Inline member functions for class [[GUSISocket]]>=                     
inline void GUSISocket::AddReference() 
{
	++fRefCount;
}

inline void GUSISocket::RemoveReference()
{
	if (!--fRefCount)
		close();
}

// \subsection{Context management}                                         
//                                                                         
//                                                                         
// <Inline member functions for class [[GUSISocket]]>=                     
inline void GUSISocket::Wakeup()
{
	fContexts.Wakeup();
}

// The traditional flavors of the I/O calls are translated to the scatterer/gatherer
// variants.                                                               
//                                                                         
// <Inline member functions for class [[GUSISocket]]>=                     
inline ssize_t	GUSISocket::read(void * buffer, size_t length)
{	
	return read(GUSIScatterer(buffer, length));	
}

inline ssize_t GUSISocket::write(const void * buffer, size_t length)
{	
	return write(GUSIGatherer(buffer, length));	
}

inline ssize_t GUSISocket::recvfrom(
	void * buffer, size_t length, int flags, void * from, socklen_t * fromlen)
{	
	return recvfrom(GUSIScatterer(buffer, length), flags, from, fromlen);	
}

inline ssize_t GUSISocket::sendto(
	const void * buffer, size_t length, int flags, const void * to, socklen_t tolen)
{	
	return sendto(GUSIGatherer(buffer, length), flags, to, tolen);	
}

#endif /* GUSI_SOURCE */

#endif /* _GUSISocket_ */
