// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSIMTUdp.nw		-	UDP code for MacTCP                           
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSIMTUdp.h,v $
// % Revision 1.1  2001/03/11 22:36:08  sgehani%netscape.com
// % First Checked In.
// %                                                
// % Revision 1.11  2000/10/16 04:02:00  neeri                             
// % Save A5 in completion routines                                        
// %                                                                       
// % Revision 1.10  2000/05/23 07:05:16  neeri                             
// % Improve formatting                                                    
// %                                                                       
// % Revision 1.9  2000/03/06 06:10:01  neeri                              
// % Reorganize Yield()                                                    
// %                                                                       
// % Revision 1.8  1999/08/26 05:45:04  neeri                              
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.7  1999/08/02 07:02:44  neeri                              
// % Support for asynchronous errors and other socket options              
// %                                                                       
// % Revision 1.6  1999/07/20 04:25:53  neeri                              
// % Fixed race condition in sendto()                                      
// %                                                                       
// % Revision 1.5  1999/06/28 06:04:59  neeri                              
// % Support interrupted calls                                             
// %                                                                       
// % Revision 1.4  1999/05/29 06:26:44  neeri                              
// % Fixed header guards                                                   
// %                                                                       
// % Revision 1.3  1999/03/17 09:05:09  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// % Revision 1.2  1998/11/22 23:06:57  neeri                              
// % Releasing 2.0a4 in a hurry                                            
// %                                                                       
// % Revision 1.1  1998/10/25 11:57:37  neeri                              
// % Ready to release 2.0a3                                                
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{MacTCP UDP sockets}                                            
//                                                                         
// A [[GUSIMTUdpSocket]] implements the UDP socket class for MacTCP. All instances
// of [[GUSIMTUdpSocket]] are created by the [[GUSIMTUdpFactory]] singleton, so
// there is no point in exporting the class itself.                        
//                                                                         
// <GUSIMTUdp.h>=                                                          
#ifndef _GUSIMTUdp_
#define _GUSIMTUdp_

#ifdef GUSI_SOURCE

#include <sys/cdefs.h>

__BEGIN_DECLS
// \section{Definition of [[GUSIMTUdpFactory]]}                            
//                                                                         
// [[GUSIMTUdpFactory]] is a singleton subclass of [[GUSISocketFactory]].  
//                                                                         
// <Definition of [[GUSIwithMTUdpSockets]]>=                               
void GUSIwithMTUdpSockets();
__END_DECLS

#ifdef GUSI_INTERNAL

#include "GUSIFactory.h"

// <Definition of class [[GUSIMTUdpFactory]]>=                             
class GUSIMTUdpFactory : public GUSISocketFactory {
public:
	static GUSISocketFactory *	Instance();
	virtual GUSISocket * 		socket(int domain, int type, int protocol);
private:
	GUSIMTUdpFactory()				{}
	static GUSISocketFactory *	instance;
};

// <Inline member functions for class [[GUSIMTUdpFactory]]>=               
inline GUSISocketFactory * GUSIMTUdpFactory::Instance()
{
	if (!instance)
		instance = new GUSIMTUdpFactory;
	return instance;
}

#endif /* GUSI_INTERNAL */

#endif /* GUSI_SOURCE */

#endif /* _GUSIMTUdp_ */
