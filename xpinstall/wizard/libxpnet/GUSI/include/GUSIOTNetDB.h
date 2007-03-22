// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSIOTNetDB.nw		-	Open Transport DNS lookups                  
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSIOTNetDB.h,v $
// % Revision 1.1  2001/03/11 22:37:31  sgehani%netscape.com
// % First Checked In.
// %                                              
// % Revision 1.8  2000/06/12 04:20:59  neeri                              
// % Introduce GUSI_*printf                                                
// %                                                                       
// % Revision 1.7  2000/05/23 07:11:45  neeri                              
// % Improve formatting, handle failed lookups correctly                   
// %                                                                       
// % Revision 1.6  2000/03/06 06:10:01  neeri                              
// % Reorganize Yield()                                                    
// %                                                                       
// % Revision 1.5  1999/12/14 06:27:47  neeri                              
// % initialize OT before opening resolver                                 
// %                                                                       
// % Revision 1.4  1999/08/26 05:45:06  neeri                              
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.3  1999/06/30 07:42:06  neeri                              
// % Getting ready to release 2.0b3                                        
// %                                                                       
// % Revision 1.2  1999/05/30 03:09:31  neeri                              
// % Added support for MPW compilers                                       
// %                                                                       
// % Revision 1.1  1999/03/17 09:05:10  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{IP Name Lookup in Open Transport}                              
//                                                                         
//                                                                         
// <GUSIOTNetDB.h>=                                                        
#ifndef _GUSIOTNetDB_
#define _GUSIOTNetDB_

#ifdef GUSI_INTERNAL
#include "GUSINetDB.h"
#include "GUSIContext.h"
#include "GUSIOpenTransport.h"

// \section{Definition of [[GUSIOTNetDB]]}                                 
//                                                                         
// We don't want to open the Open Transport headers files in our public header, but we
// need [[InetSvcRef]].                                                    
//                                                                         
// <Name dropping for file GUSIOTNetDB>=                                   
class	TInternetServices;
typedef TInternetServices*	InetSvcRef;

// <Definition of class [[GUSIOTNetDB]]>=                                  
class GUSIOTNetDB : public GUSINetDB {
public:
	static void	Instantiate();
	bool Resolver();
	
	// <Overridden member functions for [[GUSIOTNetDB]]>=                      
 virtual hostent * gethostbyname(const char * name);
 // <Overridden member functions for [[GUSIOTNetDB]]>=                      
 virtual hostent * gethostbyaddr(const void * addr, size_t len, int type);
 // <Overridden member functions for [[GUSIOTNetDB]]>=                      
 virtual char * inet_ntoa(in_addr inaddr);
 // <Overridden member functions for [[GUSIOTNetDB]]>=                      
 virtual long gethostid();
private:
	GUSISpecificData<GUSIhostent, GUSIKillHostEnt>	fHost;
	// \section{Implementation of [[GUSIOTNetDB]]}                             
 //                                                                         
 //                                                                         
 // <Privatissima of [[GUSIOTNetDB]]>=                                      
 GUSIOTNetDB();
 // The [[GUSIOTNetDB]] notifier operates similarly to the [[GUSIOTSocket]] notifier,
 // but it has to get the context to wake up somehow from its parameters.   
 //                                                                         
 // <Privatissima of [[GUSIOTNetDB]]>=                                      
 uint16_t		fEvent;
 uint32_t		fCompletion;
 OSStatus		fAsyncError;
 InetSvcRef		fSvc;
 GUSIContext *	fCreationContext;
 friend pascal void GUSIOTNetDBNotify(GUSIOTNetDB *, OTEventCode, OTResult, void *);
};

#endif /* GUSI_INTERNAL */

#endif /* _GUSIOTNetDB_ */
