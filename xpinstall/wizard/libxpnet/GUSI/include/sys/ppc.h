// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSIPPC.nw			-	Program-Program Communications                 
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: ppc.h,v $
// % Revision 1.1  2001/03/11 22:41:01  sgehani%netscape.com
// % First Checked In.
// %                                                  
// % Revision 1.9  2000/10/29 19:13:57  neeri                              
// % Numerous fixes to make it actually work                               
// %                                                                       
// % Revision 1.8  2000/10/16 04:34:23  neeri                              
// % Releasing 2.1.2                                                       
// %                                                                       
// % Revision 1.7  2000/05/23 07:15:31  neeri                              
// % Improve formatting                                                    
// %                                                                       
// % Revision 1.6  2000/03/06 06:10:00  neeri                              
// % Reorganize Yield()                                                    
// %                                                                       
// % Revision 1.5  1999/11/15 07:21:36  neeri                              
// % Add GUSIwithPPCSockets                                                
// %                                                                       
// % Revision 1.4  1999/08/26 05:45:07  neeri                              
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.3  1999/06/28 06:05:00  neeri                              
// % Support interrupted calls                                             
// %                                                                       
// % Revision 1.2  1999/06/08 04:31:30  neeri                              
// % Getting ready for 2.0b2                                               
// %                                                                       
// % Revision 1.1  1999/03/17 09:05:12  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{The GUSI PPC Socket Class}                                     
//                                                                         
// The [[GUSIPPCSocket]] class implements communication via the Program-Program
// Communications Toolbox (For a change, the PPC does not stand for ``PowerPC'' 
// here).                                                                  
//                                                                         
// We give PPC sockets their own official looking header.                  
//                                                                         
// <sys/ppc.h>=                                                            
#include <PPCToolbox.h>

struct sockaddr_ppc {
	sa_family_t		sppc_family;	/* AF_PPC */
	LocationNameRec	sppc_location;
	PPCPortRec		sppc_port;
};
