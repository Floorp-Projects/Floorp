// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSIMTInet.nw		-	Common routines for MacTCP                   
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSIInet.h,v $
// % Revision 1.1  2001/03/11 22:35:17  sgehani%netscape.com
// % First Checked In.
// %                                                 
// % Revision 1.5  1999/08/26 05:45:02  neeri                              
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.4  1999/05/29 06:26:43  neeri                              
// % Fixed header guards                                                   
// %                                                                       
// % Revision 1.3  1998/11/22 23:06:53  neeri                              
// % Releasing 2.0a4 in a hurry                                            
// %                                                                       
// % Revision 1.2  1998/10/25 11:57:34  neeri                              
// % Ready to release 2.0a3                                                
// %                                                                       
// % Revision 1.1  1998/08/01 21:32:05  neeri                              
// % About ready for 2.0a1                                                 
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
// \chapter{TCP/IP shared infrastructure}                                  
//                                                                         
// Both the MacTCP and the forthcoming open transport implementation of TCP/IP 
// sockets share a common registry.                                        
//                                                                         
// <GUSIInet.h>=                                                           
#ifndef _GUSIInet_
#define _GUSIInet_

#ifdef GUSI_SOURCE

#include <sys/cdefs.h>

__BEGIN_DECLS
// <Definition of [[GUSIwithInetSockets]]>=                                
void GUSIwithInetSockets();
__END_DECLS

#ifdef GUSI_INTERNAL

#include "GUSIFactory.h"

extern GUSISocketTypeRegistry gGUSIInetFactories;

#endif /* GUSI_INTERNAL */

#endif /* GUSI_SOURCE */

#endif /* _GUSIInet_ */
