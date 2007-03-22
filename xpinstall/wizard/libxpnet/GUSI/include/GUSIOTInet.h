// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSIOpenTransport.nw-	OpenTransport sockets                   
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSIOTInet.h,v $
// % Revision 1.1  2001/03/11 22:37:27  sgehani%netscape.com
// % First Checked In.
// %                                               
// % Revision 1.11  2001/01/17 08:54:12  neeri                             
// % Add Clone() implementations                                           
// %                                                                       
// % Revision 1.10  2000/06/12 04:20:59  neeri                             
// % Introduce GUSI_*printf                                                
// %                                                                       
// % Revision 1.9  2000/05/23 09:05:27  neeri                              
// % Improve formatting                                                    
// %                                                                       
// % Revision 1.8  1999/09/26 03:57:12  neeri                              
// % Renamed broadcast option                                              
// %                                                                       
// % Revision 1.7  1999/09/09 07:20:29  neeri                              
// % Fix numerous bugs, add support for interface ioctls                   
// %                                                                       
// % Revision 1.6  1999/08/26 05:45:06  neeri                              
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.5  1999/08/02 07:02:45  neeri                              
// % Support for asynchronous errors and other socket options              
// %                                                                       
// % Revision 1.4  1999/05/29 06:26:44  neeri                              
// % Fixed header guards                                                   
// %                                                                       
// % Revision 1.3  1999/04/14 04:21:19  neeri                              
// % Correct option sizes                                                  
// %                                                                       
// % Revision 1.2  1999/04/10 05:17:51  neeri                              
// % Implement broadcast/multicast options                                 
// %                                                                       
// % Revision 1.1  1999/03/17 09:05:10  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{Open Transport TCP/IP sockets}                                 
//                                                                         
// For TCP and UDP, the strategy classes do most of the work, the derived socket 
// classes only have to do option management.                              
//                                                                         
// <GUSIOTInet.h>=                                                         
#ifndef _GUSIOTInet_
#define _GUSIOTInet_

#ifdef GUSI_SOURCE

#include <sys/cdefs.h>

__BEGIN_DECLS
// \section{Definition of Open Transport Internet hooks}                   
//                                                                         
//                                                                         
// <Definition of [[GUSIwithOTInetSockets]]>=                              
void GUSIwithOTInetSockets();
void GUSIwithOTTcpSockets();
void GUSIwithOTUdpSockets();
__END_DECLS

#endif /* GUSI_SOURCE */

#endif /* _GUSIOTInet_ */
