// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSIDCon.nw			-	DCon interface                                
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSIDCon.h,v $
// % Revision 1.1  2001/03/11 22:33:45  sgehani%netscape.com
// % First Checked In.
// %                                                 
// % Revision 1.4  2000/03/06 06:03:30  neeri                              
// % Check device families for file paths                                  
// %                                                                       
// % Revision 1.3  1999/08/26 05:45:00  neeri                              
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.2  1999/05/29 06:26:41  neeri                              
// % Fixed header guards                                                   
// %                                                                       
// % Revision 1.1  1999/03/17 09:05:06  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{DCon interface}                                                
//                                                                         
// A [[GUSIDConSocket]] implements an interface to DCon, Cache Computing's 
// debugging console. For more information about DCon, see                 
// \href{http://www.cache-computing.com/products/dcon/}{Cache Computing's site} 
// at \verb|http://www.cache-computing.com/products/dcon/|.                
//                                                                         
// All instances of [[GUSIDConSocket]] are created by the [[GUSIDConDevice]] 
// singleton, so                                                           
// there is no point in exporting the class itself.                        
//                                                                         
// <GUSIDCon.h>=                                                           
#ifndef _GUSIDCon_
#define _GUSIDCon_

#ifdef GUSI_INTERNAL

#include "GUSIDevice.h"

// \section{Definition of [[GUSIDConDevice]]}                              
//                                                                         
// [[GUSIDConDevice]] is a singleton subclass of [[GUSIDevice]].           
//                                                                         
// <Definition of class [[GUSIDConDevice]]>=                               
class GUSIDConDevice : public GUSIDevice {
public:
	static GUSIDConDevice *	Instance();
	virtual bool	Want(GUSIFileToken & file);
	virtual GUSISocket * open(GUSIFileToken & file, int flags);
protected:
	GUSIDConDevice()				{}
	static GUSIDConDevice *	sInstance;
};

// <Inline member functions for class [[GUSIDConDevice]]>=                 
inline GUSIDConDevice * GUSIDConDevice::Instance()
{
	if (!sInstance)
		sInstance = new GUSIDConDevice;
	return sInstance;
}

#endif /* GUSI_INTERNAL */

#endif /* _GUSIDCon_ */
