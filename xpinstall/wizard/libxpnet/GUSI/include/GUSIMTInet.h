// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSIMTInet.nw		-	Common routines for MacTCP                   
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSIMTInet.h,v $
// % Revision 1.1  2001/03/11 22:35:57  sgehani%netscape.com
// % First Checked In.
// %                                               
// % Revision 1.14  2000/10/16 04:34:23  neeri                             
// % Releasing 2.1.2                                                       
// %                                                                       
// % Revision 1.13  2000/05/23 07:03:25  neeri                             
// % Improve formatting                                                    
// %                                                                       
// % Revision 1.12  1999/08/26 05:45:04  neeri                             
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.11  1999/08/02 07:02:43  neeri                             
// % Support for asynchronous errors and other socket options              
// %                                                                       
// % Revision 1.10  1999/06/30 07:42:06  neeri                             
// % Getting ready to release 2.0b3                                        
// %                                                                       
// % Revision 1.9  1999/05/29 06:26:43  neeri                              
// % Fixed header guards                                                   
// %                                                                       
// % Revision 1.8  1999/04/29 05:33:19  neeri                              
// % Fix fcntl prototype                                                   
// %                                                                       
// % Revision 1.7  1999/03/17 09:05:08  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// % Revision 1.6  1998/10/25 11:57:35  neeri                              
// % Ready to release 2.0a3                                                
// %                                                                       
// % Revision 1.5  1998/10/11 16:45:16  neeri                              
// % Ready to release 2.0a2                                                
// %                                                                       
// % Revision 1.4  1998/02/11 12:57:12  neeri                              
// % PowerPC Build                                                         
// %                                                                       
// % Revision 1.3  1998/01/25 20:53:55  neeri                              
// % Engine implemented, except for signals & scheduling                   
// %                                                                       
// % Revision 1.2  1996/12/22 19:57:56  neeri                              
// % TCP streams work                                                      
// %                                                                       
// % Revision 1.1  1996/12/16 02:12:40  neeri                              
// % TCP Sockets sort of work                                              
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{Basic MacTCP code}                                             
//                                                                         
// A [[GUSIMTInetSocket]] defines the infrastructure shared between        
// MacTCP TCP and UDP sockets.                                             
//                                                                         
// <GUSIMTInet.h>=                                                         
#ifndef _GUSIMTInet_
#define _GUSIMTInet_

#ifdef GUSI_SOURCE

#include <sys/cdefs.h>

__BEGIN_DECLS
// <Definition of [[GUSIwithMTInetSockets]]>=                              
void GUSIwithMTInetSockets();
__END_DECLS

#ifdef GUSI_INTERNAL

#include "GUSISocket.h"
#include "GUSISocketMixins.h"

#include <netinet/in.h>
#include <MacTCP.h>

// \section{Definition of [[GUSIMTInetSocket]]}                            
//                                                                         
// MacTCP related sockets are buffered, have a standard state model, and can be
// nonblocking.                                                            
//                                                                         
// <Definition of class [[GUSIMTInetSocket]]>=                             
class GUSIMTInetSocket : 
	public 		GUSISocket, 
	protected 	GUSISMBlocking,
	protected	GUSISMState,
	protected	GUSISMInputBuffer,
	protected 	GUSISMOutputBuffer,
	protected	GUSISMAsyncError
{
public:
	GUSIMTInetSocket();
	// [[bind]] for MacTCP sockets has the fatal flaw that it is totally unable to
 // reserve a socket.                                                       
 //                                                                         
 // <Overridden member functions for [[GUSIMTInetSocket]]>=                 
 virtual int bind(void * addr, socklen_t namelen);
 // [[getsockname]] and [[getpeername]] return the stored values.           
 //                                                                         
 // <Overridden member functions for [[GUSIMTInetSocket]]>=                 
 virtual int getsockname(void * addr, socklen_t * namelen);
 virtual int getpeername(void * addr, socklen_t * namelen);
 // [[shutdown]] just delegates to [[GUSISMState]].                         
 //                                                                         
 // <Overridden member functions for [[GUSIMTInetSocket]]>=                 
 virtual int shutdown(int how);
 // [[fcntl]] handles the blocking support.                                 
 //                                                                         
 // <Overridden member functions for [[GUSIMTInetSocket]]>=                 
 virtual int fcntl(int cmd, va_list arg);
 // [[ioctl]] deals with blocking support and with [[FIONREAD]].            
 //                                                                         
 // <Overridden member functions for [[GUSIMTInetSocket]]>=                 
 virtual int ioctl(unsigned int request, va_list arg);
 // [[getsockopt]] and [[setsockopt]] are available for setting buffer sizes and 
 // getting asynchronous errors.                                            
 //                                                                         
 // <Overridden member functions for [[GUSIMTInetSocket]]>=                 
 virtual int getsockopt(int level, int optname, void *optval, socklen_t * optlen);
 // <Overridden member functions for [[GUSIMTInetSocket]]>=                 
 virtual int setsockopt(int level, int optname, void *optval, socklen_t optlen);
 // MacTCP sockets implement socket style calls.                            
 //                                                                         
 // <Overridden member functions for [[GUSIMTInetSocket]]>=                 
 virtual bool Supports(ConfigOption config);
	// MacTCP I/O calls communicate by means of read and write data structures,
 // of which we need only the most primitive variants.                      
 //                                                                         
 // <Definition of classes [[MiniWDS]] and [[MidiWDS]]>=                    
 #if PRAGMA_STRUCT_ALIGN
 	#pragma options align=mac68k
 #endif
 class MiniWDS {
 public:
 	u_short	fLength;
 	char * 	fDataPtr;
 	u_short fZero;

 	MiniWDS() : fZero(0)		{}
 	Ptr operator &()			{	return (Ptr)this;	}
 };
 class MidiWDS {
 public:
 	u_short	fLength;
 	char * 	fDataPtr;
 	u_short	fLength2;
 	char * 	fDataPtr2;
 	u_short fZero;

 	MidiWDS() : fZero(0)		{}
 	Ptr operator &()			{	return (Ptr)this;	}
 };
 #if PRAGMA_STRUCT_ALIGN
 	#pragma options align=reset
 #endif
	// The only other interesting bit in the interface is the driver management, which
 // arranges to open the MacTCP driver and domain name resolver at most once,
 // as late as possible in the program (If you open some SLIP or PPP drivers 
 // before the Toolbox is initialized, you'll wish you'd curled up by the fireside
 // with a nice Lovecraft novel instead). [[Driver]] returns the driver reference
 // number of the MacTCP driver. [[HostAddr]] returns our host's IP address.
 //                                                                         
 // <MacTCP driver management>=                                             
 static short	Driver();
 static u_long	HostAddr();
protected:
	// All MacTCP related sockets need a [[StreamPtr]]; they store their own and 
 // their peer's address away, and the save errors reported at interrupt time
 // in an [[fAsyncError]] field.                                            
 //                                                                         
 // <Data members for [[GUSIMTInetSocket]]>=                                
 StreamPtr	fStream;
 sockaddr_in	fSockAddr;
 sockaddr_in	fPeerAddr;
 // \section{Implementation of [[GUSIMTInetSocket]]}                        
 //                                                                         
 // [[Driver]] preserves error status in an [[OSErr]]                       
 // variable, initially [[1]] to convey unresolvedness.                     
 //                                                                         
 // <Data members for [[GUSIMTInetSocket]]>=                                
 static short	sDrvrRefNum;
 static OSErr	sDrvrState;
 static u_long	sHostAddress;
};

#endif /* GUSI_INTERNAL */

#endif /* GUSI_SOURCE */

#endif /* _GUSIMTInet_ */
