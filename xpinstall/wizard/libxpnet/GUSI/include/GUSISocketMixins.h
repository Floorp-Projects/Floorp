// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSISocketMixins.nw	-	Useful building blocks                  
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSISocketMixins.h,v $
// % Revision 1.1  2001/03/11 22:38:21  sgehani%netscape.com
// % First Checked In.
// %                                         
// % Revision 1.11  2000/10/16 04:10:12  neeri                             
// % Add GUSISMProcess                                                     
// %                                                                       
// % Revision 1.10  2000/05/23 07:24:58  neeri                             
// % Improve formatting                                                    
// %                                                                       
// % Revision 1.9  1999/08/26 05:45:09  neeri                              
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.8  1999/08/02 07:02:46  neeri                              
// % Support for asynchronous errors and other socket options              
// %                                                                       
// % Revision 1.7  1999/05/29 06:26:45  neeri                              
// % Fixed header guards                                                   
// %                                                                       
// % Revision 1.6  1999/04/29 05:33:18  neeri                              
// % Fix fcntl prototype                                                   
// %                                                                       
// % Revision 1.5  1999/03/17 09:05:13  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// % Revision 1.4  1998/10/11 16:45:24  neeri                              
// % Ready to release 2.0a2                                                
// %                                                                       
// % Revision 1.3  1998/01/25 20:53:59  neeri                              
// % Engine implemented, except for signals & scheduling                   
// %                                                                       
// % Revision 1.2  1997/11/13 21:12:13  neeri                              
// % Fall 1997                                                             
// %                                                                       
// % Revision 1.1  1996/12/16 02:12:42  neeri                              
// % TCP Sockets sort of work                                              
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{Mixin Classes for Sockets}                                     
//                                                                         
// This section contains some building block classes for sockets:          
//                                                                         
// \begin{itemize}                                                         
// \item [[GUSISMBlocking]] implements the blocking/nonblocking flag.      
// \item [[GUSISMState]] implements a state variable.                      
// \item [[GUSISMInputBuffer]] provides a [[GUSIBuffer]] for input.        
// \item [[GUSISMOutputBuffer]] provides a [[GUSIBuffer]] for output.      
// \item [[GUSISMAsyncError]] provides storage for asynchronous errors.    
// \item [[GUSISMProcess]] maintains a link to the process instance.       
// \end{itemize}                                                           
//                                                                         
//                                                                         
// <GUSISocketMixins.h>=                                                   
#ifndef _GUSISocketMixins_
#define _GUSISocketMixins_

#ifdef GUSI_INTERNAL

#include "GUSISocket.h"
#include "GUSIBuffer.h"

#include <fcntl.h>
#include <sys/ioctl.h>

// \section{Definition of [[GUSISocketMixins]]}                            
//                                                                         
// [[GUSISMBlocking]] implements the [[fBlocking]] flags and the [[DoIoctl]] and
// [[DoFcntl]] variants to manipulate it. These two functions work like their
// [[GUSISocket]] member function counterparts, but handle the return value 
// differently: The POSIX function result is stored in [[*result]], while the
// return value indicates whether the request was handled.                 
//                                                                         
// <Definition of class [[GUSISMBlocking]]>=                               
class GUSISMBlocking {
public:
	GUSISMBlocking();
	bool	fBlocking;
	bool	DoFcntl(int * result, int cmd, va_list arg);
	bool 	DoIoctl(int * result, unsigned int request, va_list arg);
};
// [[GUSISMState]] captures the state of a socket over its lifetime. It starts out
// as [[Unbound]]. [[bind]] will put it into [[Unconnected]] state, though few
// socket classes care about this distinction. [[listen]] will put it into 
// [[Listening]] state. [[accept]] starts a [[Connected]] new socket.      
// [[connect]] puts an [[Unconnected]] socket into [[Connecting]] state from
// where it emerges [[Connected]]. [[fReadShutdown]] and [[fWriteShutdown]] record
// shutdown promises.                                                      
//                                                                         
// <Definition of class [[GUSISMState]]>=                                  
class GUSISMState {
public:
	enum State {
		Unbound,
		Unconnected, 	
		Listening,
		Connecting,
		Connected,
		Closing
	};
	GUSISMState();
	State	fState;
	bool	fReadShutdown;
	bool	fWriteShutdown;
	void	Shutdown(int how);
};
// [[GUSISMInputBuffer]] defines the input buffer and some socket options that go
// with it. [[DoGetSockOpt]] and [[DoSetSockOpt]] work the same way as     
// [[DoFcntl]] and [[DoIoctl]] above.                                      
//                                                                         
// <Definition of class [[GUSISMInputBuffer]]>=                            
class GUSISMInputBuffer {
public:
	GUSIRingBuffer	fInputBuffer;
	GUSISMInputBuffer();
	bool 			DoGetSockOpt(
						int * result, int level, int optname, 
						void *optval, socklen_t * optlen);
	bool 			DoSetSockOpt(
						int * result, int level, int optname, 
						void *optval, socklen_t optlen);	
	bool 			DoIoctl(int * result, unsigned int request, va_list arg);
};
// [[GUSISMOutputBuffer]] defines the output buffer and some socket options that go
// with it.                                                                
//                                                                         
// <Definition of class [[GUSISMOutputBuffer]]>=                           
class GUSISMOutputBuffer {
public:
	GUSIRingBuffer	fOutputBuffer;
	GUSISMOutputBuffer();
	bool 		DoGetSockOpt(
						int * result, int level, int optname, 
						void *optval, socklen_t * optlen);
	bool 		DoSetSockOpt(
						int * result, int level, int optname, 
						void *optval, socklen_t optlen);	
};
// [[GUSISMAsyncError]] stores asynchronous errors and makes them available via
// [[getsockopt]]. [[GetAsyncError]] returns the error and resets the stored value.
//                                                                         
// <Definition of class [[GUSISMAsyncError]]>=                             
class GUSISMAsyncError {
public:
	GUSISMAsyncError();
	int			fAsyncError;
	int			SetAsyncPosixError(int error);
	int			SetAsyncMacError(OSErr error);
	int			GetAsyncError();
	bool 		DoGetSockOpt(
						int * result, int level, int optname, 
						void *optval, socklen_t * optlen);
};
// [[GUSISMProcess]] stores a link to the global [[GUSIProcess]] instance, which is useful for completion routines.
//                                                                         
// <Definition of class [[GUSISMProcess]]>=                                
class GUSISMProcess {
public:
	GUSISMProcess();
	
	GUSIProcess * Process(); 
private:
	GUSIProcess *	fProcess;
};

// \section{Implementation of [[GUSISocketMixins]]}                        
//                                                                         
// Because all the member functions are simple and called in few places, it
// makes sense to inline them.                                             
//                                                                         
// All sockets start out blocking.                                         
//                                                                         
// <Inline member functions for class [[GUSISMBlocking]]>=                 
inline GUSISMBlocking::GUSISMBlocking() : fBlocking(true)	{}
// For historical reasons, there is both an [[ioctl]] and a [[fcntl]] interface
// to the blocking flag.                                                   
//                                                                         
// <Inline member functions for class [[GUSISMBlocking]]>=                 
inline bool GUSISMBlocking::DoFcntl(int * result, int cmd, va_list arg)
{
	switch(cmd) {
	case F_GETFL : 
		return (*result = fBlocking ? 0: FNDELAY), true;
	case F_SETFL : 
		fBlocking = !(va_arg(arg, int) & FNDELAY);
		
		return (*result = 0), true;
	}
	return false;
}
inline bool GUSISMBlocking::DoIoctl(int * result, unsigned int request, va_list arg)
{
	if (request == FIONBIO) {
		fBlocking = !*va_arg(arg, int *);
		return (*result = 0), true;
	}
	return false;
}
// Sockets start out as unconnected.                                       
//                                                                         
// <Inline member functions for class [[GUSISMState]]>=                    
inline GUSISMState::GUSISMState() : 
	fState(Unbound), fReadShutdown(false), fWriteShutdown(false)		{}
// <Inline member functions for class [[GUSISMState]]>=                    
inline void GUSISMState::Shutdown(int how)
{
	if (!(how & 1))
		fReadShutdown = true;
	if (how > 0)
		fWriteShutdown = true;
}
// Buffers initially are 8K.                                               
//                                                                         
// <Inline member functions for class [[GUSISMInputBuffer]]>=              
inline GUSISMInputBuffer::GUSISMInputBuffer() : fInputBuffer(8192)	{}
// [[getsockopt]] is used to obtain the buffer size.                       
//                                                                         
// <Inline member functions for class [[GUSISMInputBuffer]]>=              
inline bool GUSISMInputBuffer::DoGetSockOpt(
					int * result, int level, int optname, 
					void *optval, socklen_t *)
{
	if (level == SOL_SOCKET && optname == SO_RCVBUF) {
		*(int *)optval = (int)fInputBuffer.Size();
		
		return (*result = 0), true;
	}
	return false;
}
// [[setsockopt]] modifies the buffer size.                                
//                                                                         
// <Inline member functions for class [[GUSISMInputBuffer]]>=              
inline bool GUSISMInputBuffer::DoSetSockOpt(
					int * result, int level, int optname, 
					void *optval, socklen_t)
{
	if (level == SOL_SOCKET && optname == SO_RCVBUF) {
		fInputBuffer.SwitchBuffer(*(int *)optval);
		
		return (*result = 0), true;
	}
	return false;
}
// [[ioctl]] returns the number of available bytes.                        
//                                                                         
// <Inline member functions for class [[GUSISMInputBuffer]]>=              
inline bool GUSISMInputBuffer::DoIoctl(int * result, unsigned int request, va_list arg)
{
	if (request == FIONREAD) {
		*va_arg(arg, long *) = fInputBuffer.Valid();
		return (*result = 0), true;
	}
	return false;
}
// [[GUSISMOutputBuffer]] works identically to the input buffer.           
//                                                                         
// <Inline member functions for class [[GUSISMOutputBuffer]]>=             
inline GUSISMOutputBuffer::GUSISMOutputBuffer() : fOutputBuffer(8192)	{}
// [[getsockopt]] is used to obtain the buffer size.                       
//                                                                         
// <Inline member functions for class [[GUSISMOutputBuffer]]>=             
inline bool GUSISMOutputBuffer::DoGetSockOpt(
					int * result, int level, int optname, 
					void *optval, socklen_t *)
{
	if (level == SOL_SOCKET && optname == SO_SNDBUF) {
		*(int *)optval = (int)fOutputBuffer.Size();
		
		return (*result = 0), true;
	}
	return false;
}
// [[setsockopt]] is modifies the buffer size.                             
//                                                                         
// <Inline member functions for class [[GUSISMOutputBuffer]]>=             
inline bool GUSISMOutputBuffer::DoSetSockOpt(
					int * result, int level, int optname, 
					void *optval, socklen_t)
{
	if (level == SOL_SOCKET && optname == SO_SNDBUF) {
		fOutputBuffer.SwitchBuffer(*(int *)optval);
		
		return (*result = 0), true;
	}
	return false;
}
// <Inline member functions for class [[GUSISMAsyncError]]>=               
inline GUSISMAsyncError::GUSISMAsyncError()
 : fAsyncError(0)
{
}
// The central member functions of [[GUSISMAsyncError]] are [[SetAsyncXXXError]] and
// [[GetAsyncError]].                                                      
//                                                                         
// <Inline member functions for class [[GUSISMAsyncError]]>=               
inline int GUSISMAsyncError::SetAsyncPosixError(int error)
{
	if (error) {
		fAsyncError = error;
		GUSI_MESSAGE(("GUSISMAsyncError::SetAsyncPosixError %d\n", fAsyncError));
		
		return -1;
	}
	return 0;
}
inline int GUSISMAsyncError::GetAsyncError()
{
	int err = fAsyncError;
	fAsyncError = 0;
	return err;
}
// For some reason, the CW Pro 4 compilers generated bad code for this in some combination, so
// we make it out of line.                                                 
//                                                                         
// <Inline member functions for class [[GUSISMAsyncError]]>=               
inline int GUSISMAsyncError::SetAsyncMacError(OSErr error)
{
	if (error) {
		fAsyncError = GUSIMapMacError(error);
		GUSI_MESSAGE(("GUSISMAsyncError::SetAsyncMacError %d -> %d\n", error, fAsyncError));
		
		return -1;
	}
	return 0;
}
// [[DoGetSockOpt]] only handles [[SO_ERROR]] (hi Philippe!).              
//                                                                         
// <Inline member functions for class [[GUSISMAsyncError]]>=               
inline bool GUSISMAsyncError::DoGetSockOpt(
						int * result, int level, int optname, 
						void *optval, socklen_t * optlen)
{
	if (level == SOL_SOCKET && optname == SO_ERROR) {
		*(int *)optval 	= GetAsyncError();
		*optlen 		= sizeof(int);
		
		return (*result = 0), true;
	}
	return false;
}
// <Inline member functions for class [[GUSISMProcess]]>=                  
inline GUSISMProcess::GUSISMProcess()
 : fProcess(GUSIProcess::Instance())
{
}

inline GUSIProcess * GUSISMProcess::Process()
{
	return fProcess;
}

#endif /* GUSI_INTERNAL */

#endif /* _GUSISocketMixins_ */
