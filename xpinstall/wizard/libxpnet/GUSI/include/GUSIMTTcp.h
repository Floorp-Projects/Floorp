// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSIMTTcp.nw		-	TCP code for MacTCP                           
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSIMTTcp.h,v $
// % Revision 1.1  2001/03/11 22:36:04  sgehani%netscape.com
// % First Checked In.
// %                                                
// % Revision 1.17  2000/10/16 04:01:59  neeri                             
// % Save A5 in completion routines                                        
// %                                                                       
// % Revision 1.16  2000/05/23 07:04:20  neeri                             
// % Improve formatting, fix hang on close                                 
// %                                                                       
// % Revision 1.15  2000/03/06 06:10:02  neeri                             
// % Reorganize Yield()                                                    
// %                                                                       
// % Revision 1.14  1999/08/26 05:42:13  neeri                             
// % Fix nonblocking connects                                              
// %                                                                       
// % Revision 1.13  1999/08/02 07:02:44  neeri                             
// % Support for asynchronous errors and other socket options              
// %                                                                       
// % Revision 1.12  1999/06/28 06:04:58  neeri                             
// % Support interrupted calls                                             
// %                                                                       
// % Revision 1.11  1999/06/08 04:31:29  neeri                             
// % Getting ready for 2.0b2                                               
// %                                                                       
// % Revision 1.10  1999/05/30 03:09:30  neeri                             
// % Added support for MPW compilers                                       
// %                                                                       
// % Revision 1.9  1999/03/17 09:05:08  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// % Revision 1.8  1998/11/22 23:06:55  neeri                              
// % Releasing 2.0a4 in a hurry                                            
// %                                                                       
// % Revision 1.7  1998/10/25 11:31:42  neeri                              
// % Add MSG_PEEK support, make releases more orderly.                     
// %                                                                       
// % Revision 1.6  1998/10/11 16:45:18  neeri                              
// % Ready to release 2.0a2                                                
// %                                                                       
// % Revision 1.5  1998/08/01 21:32:07  neeri                              
// % About ready for 2.0a1                                                 
// %                                                                       
// % Revision 1.4  1998/01/25 20:53:56  neeri                              
// % Engine implemented, except for signals & scheduling                   
// %                                                                       
// % Revision 1.3  1997/11/13 21:12:11  neeri                              
// % Fall 1997                                                             
// %                                                                       
// % Revision 1.2  1996/12/22 19:57:57  neeri                              
// % TCP streams work                                                      
// %                                                                       
// % Revision 1.1  1996/12/16 02:12:41  neeri                              
// % TCP Sockets sort of work                                              
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{MacTCP TCP sockets}                                            
//                                                                         
// A [[GUSIMTTcpSocket]] implements the TCP socket class for MacTCP. All instances
// of [[GUSIMTTcpSocket]] are created by the [[GUSIMTTcpFactory]] singleton, so
// there is no point in exporting the class itself.                        
//                                                                         
// <GUSIMTTcp.h>=                                                          
#ifndef _GUSIMTTcp_
#define _GUSIMTTcp_

#ifdef GUSI_SOURCE

#include <sys/cdefs.h>

__BEGIN_DECLS
// \section{Definition of [[GUSIMTTcpFactory]]}                            
//                                                                         
// [[GUSIMTTcpFactory]] is a singleton subclass of [[GUSISocketFactory]].  
//                                                                         
// <Definition of [[GUSIwithMTTcpSockets]]>=                               
void GUSIwithMTTcpSockets();
__END_DECLS

#ifdef GUSI_INTERNAL

#include "GUSIFactory.h"

// <Definition of class [[GUSIMTTcpFactory]]>=                             
class GUSIMTTcpFactory : public GUSISocketFactory {
public:
	static GUSISocketFactory *	Instance();
	virtual GUSISocket * 		socket(int domain, int type, int protocol);
private:
	GUSIMTTcpFactory()				{}
	static GUSISocketFactory *	instance;
};

// <Inline member functions for class [[GUSIMTTcpFactory]]>=               
inline GUSISocketFactory * GUSIMTTcpFactory::Instance()
{
	if (!instance)
		instance = new GUSIMTTcpFactory;
	return instance;
}

#endif /* GUSI_INTERNAL */

#endif /* GUSI_SOURCE */

#endif /* _GUSIMTTcp_ */
