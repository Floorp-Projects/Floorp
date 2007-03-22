// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSINull.nw			-	Null device                                   
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSINull.h,v $
// % Revision 1.1  2001/03/11 22:37:20  sgehani%netscape.com
// % First Checked In.
// %                                                 
// % Revision 1.7  2000/03/06 06:03:29  neeri                              
// % Check device families for file paths                                  
// %                                                                       
// % Revision 1.6  1999/08/26 05:45:05  neeri                              
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.5  1999/05/29 06:26:44  neeri                              
// % Fixed header guards                                                   
// %                                                                       
// % Revision 1.4  1999/04/29 05:34:22  neeri                              
// % Support stat/fstat                                                    
// %                                                                       
// % Revision 1.3  1999/03/17 09:05:10  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// % Revision 1.2  1998/11/22 23:06:59  neeri                              
// % Releasing 2.0a4 in a hurry                                            
// %                                                                       
// % Revision 1.1  1998/08/01 21:32:09  neeri                              
// % About ready for 2.0a1                                                 
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{Null device}                                                   
//                                                                         
// A [[GUSINullSocket]] implements the null socket class for MacTCP. All instances
// of [[GUSINullSocket]] are created by the [[GUSINullDevice]] singleton, so
// there is no point in exporting the class itself.                        
//                                                                         
// <GUSINull.h>=                                                           
#ifndef _GUSINull_
#define _GUSINull_

#ifdef GUSI_INTERNAL

#include "GUSIDevice.h"

// \section{Definition of [[GUSINullDevice]]}                              
//                                                                         
// [[GUSINullDevice]] is a singleton subclass of [[GUSIDevice]].           
//                                                                         
// <Definition of class [[GUSINullDevice]]>=                               
class GUSINullDevice : public GUSIDevice {
public:
	static GUSINullDevice *	Instance();
	virtual bool	Want(GUSIFileToken & file);
	virtual GUSISocket * open(GUSIFileToken & file, int flags);
	virtual int stat(GUSIFileToken & file, struct stat * buf);
	GUSISocket * open();
protected:
	GUSINullDevice()				{}
	static GUSINullDevice *	sInstance;
};

// <Inline member functions for class [[GUSINullDevice]]>=                 
inline GUSINullDevice * GUSINullDevice::Instance()
{
	if (!sInstance)
		sInstance = new GUSINullDevice;
	return sInstance;
}

#endif /* GUSI_INTERNAL */

#endif /* _GUSINull_ */
