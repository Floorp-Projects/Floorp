// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSIPipe.nw			-	Pipes                                         
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSIPipe.h,v $
// % Revision 1.1  2001/03/11 22:37:35  sgehani%netscape.com
// % First Checked In.
// %                                                 
// % Revision 1.12  2000/05/23 07:18:03  neeri                             
// % Improve formatting                                                    
// %                                                                       
// % Revision 1.11  2000/03/06 06:09:59  neeri                             
// % Reorganize Yield()                                                    
// %                                                                       
// % Revision 1.10  1999/11/15 07:20:59  neeri                             
// % Add GUSIwithLocalSockets                                              
// %                                                                       
// % Revision 1.9  1999/08/26 05:45:07  neeri                              
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.8  1999/06/28 06:05:00  neeri                              
// % Support interrupted calls                                             
// %                                                                       
// % Revision 1.7  1999/05/29 06:26:45  neeri                              
// % Fixed header guards                                                   
// %                                                                       
// % Revision 1.6  1999/03/17 09:05:12  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// % Revision 1.5  1998/11/22 23:07:00  neeri                              
// % Releasing 2.0a4 in a hurry                                            
// %                                                                       
// % Revision 1.4  1998/10/25 11:57:38  neeri                              
// % Ready to release 2.0a3                                                
// %                                                                       
// % Revision 1.3  1998/01/25 20:53:57  neeri                              
// % Engine implemented, except for signals & scheduling                   
// %                                                                       
// % Revision 1.2  1996/12/22 19:57:58  neeri                              
// % TCP streams work                                                      
// %                                                                       
// % Revision 1.1  1996/11/24  12:52:08  neeri                             
// % Added GUSIPipeSockets                                                 
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{The GUSI Pipe Socket Class}                                    
//                                                                         
// Pipes and socket pairs are implemented with the [[GUSIPipeSocket]] class.
// The [[GUSIPipeFactory]] singleton creates pairs of [[GUSIPipeSockets]]. 
//                                                                         
// <GUSIPipe.h>=                                                           
#ifndef _GUSIPipe_
#define _GUSIPipe_

#ifdef GUSI_INTERNAL

#include "GUSISocket.h"
#include "GUSIFactory.h"

// \section{Definition of [[GUSIPipeFactory]]}                             
//                                                                         
// [[GUSIPipeFactory]] is a singleton subclass of [[GUSISocketFactory]].   
//                                                                         
// <Definition of class [[GUSIPipeFactory]]>=                              
class GUSIPipeFactory : public GUSISocketFactory {
public:
	static GUSISocketFactory *	Instance();
	virtual GUSISocket * 		socket(int domain, int type, int protocol);
	virtual int socketpair(int domain, int type, int protocol, GUSISocket * s[2]);
private:
	GUSIPipeFactory()				{}
	static GUSISocketFactory *	sInstance;
};

// <Inline member functions for class [[GUSIPipeFactory]]>=                
inline GUSISocketFactory * GUSIPipeFactory::Instance()
{
	if (!sInstance)
		sInstance = new GUSIPipeFactory;
	return sInstance;
}

#endif /* GUSI_INTERNAL */

#endif /* _GUSIPipe_ */
