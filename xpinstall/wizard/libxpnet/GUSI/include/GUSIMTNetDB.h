// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSINetDB.nw		-	Convert between names and adresses            
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSIMTNetDB.h,v $
// % Revision 1.1  2001/03/11 22:36:01  sgehani%netscape.com
// % First Checked In.
// %                                              
// % Revision 1.7  2000/06/12 04:20:59  neeri                              
// % Introduce GUSI_*printf                                                
// %                                                                       
// % Revision 1.6  2000/03/06 06:10:02  neeri                              
// % Reorganize Yield()                                                    
// %                                                                       
// % Revision 1.5  1999/08/26 05:45:04  neeri                              
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.4  1999/05/30 03:09:30  neeri                              
// % Added support for MPW compilers                                       
// %                                                                       
// % Revision 1.3  1999/03/17 09:05:08  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// % Revision 1.2  1998/10/25 11:57:36  neeri                              
// % Ready to release 2.0a3                                                
// %                                                                       
// % Revision 1.1  1998/10/11 16:45:17  neeri                              
// % Ready to release 2.0a2                                                
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{IP Name Lookup in MacTCP}                                      
//                                                                         
//                                                                         
// <GUSIMTNetDB.h>=                                                        
#ifndef _GUSIMTNetDB_
#define _GUSIMTNetDB_

#ifdef GUSI_INTERNAL
#include "GUSINetDB.h"

// \section{Definition of [[GUSIMTNetDB]]}                                 
//                                                                         
//                                                                         
// <Definition of class [[GUSIMTNetDB]]>=                                  
class GUSIMTNetDB : public GUSINetDB {
public:
	static void	Instantiate();
	static bool Resolver();
	
	// <Overridden member functions for [[GUSIMTNetDB]]>=                      
 virtual hostent * gethostbyname(const char * name);
 // <Overridden member functions for [[GUSIMTNetDB]]>=                      
 virtual hostent * gethostbyaddr(const void * addr, size_t len, int type);
 // <Overridden member functions for [[GUSIMTNetDB]]>=                      
 virtual char * inet_ntoa(in_addr inaddr);
 // <Overridden member functions for [[GUSIMTNetDB]]>=                      
 virtual long gethostid();
private:
	GUSIMTNetDB()									{}
	GUSISpecificData<GUSIhostent,GUSIKillHostEnt>	fHost;
	static OSErr	sResolverState;
};

#endif /* GUSI_INTERNAL */

#endif /* _GUSIMTNetDB_ */
