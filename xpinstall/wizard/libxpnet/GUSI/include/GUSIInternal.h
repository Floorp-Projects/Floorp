// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSIBasics.nw		-	Common routines for GUSI                     
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSIInternal.h,v $
// % Revision 1.1  2001/03/11 22:35:21  sgehani%netscape.com
// % First Checked In.
// %                                               
// % Revision 1.20  2001/01/17 08:32:30  neeri                             
// % Atomic locks turned out not to be necessary after all                 
// %                                                                       
// % Revision 1.19  2001/01/17 08:31:10  neeri                             
// % Added PPC error codes, tweaked nullEvent handling, added atomic locks 
// %                                                                       
// % Revision 1.18  2000/10/16 04:34:22  neeri                             
// % Releasing 2.1.2                                                       
// %                                                                       
// % Revision 1.17  2000/06/12 04:20:58  neeri                             
// % Introduce GUSI_*printf                                                
// %                                                                       
// % Revision 1.16  2000/05/23 06:51:55  neeri                             
// % Reorganize errors to improve formatting                               
// %                                                                       
// % Revision 1.15  2000/03/15 07:22:05  neeri                             
// % Enforce alignment choices                                             
// %                                                                       
// % Revision 1.14  1999/08/26 05:44:58  neeri                             
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.13  1999/08/02 07:02:42  neeri                             
// % Support for asynchronous errors and other socket options              
// %                                                                       
// % Revision 1.12  1999/06/28 05:56:01  neeri                             
// % Get rid of STL includes in header                                     
// %                                                                       
// % Revision 1.11  1999/06/08 04:31:29  neeri                             
// % Getting ready for 2.0b2                                               
// %                                                                       
// % Revision 1.10  1999/05/30 03:09:28  neeri                             
// % Added support for MPW compilers                                       
// %                                                                       
// % Revision 1.9  1999/04/10 04:45:05  neeri                              
// % Handle MacTCP errors correctly                                        
// %                                                                       
// % Revision 1.8  1999/03/17 09:05:04  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// % Revision 1.7  1998/10/25 11:57:33  neeri                              
// % Ready to release 2.0a3                                                
// %                                                                       
// % Revision 1.6  1998/10/11 16:45:09  neeri                              
// % Ready to release 2.0a2                                                
// %                                                                       
// % Revision 1.5  1998/01/25 20:53:50  neeri                              
// % Engine implemented, except for signals & scheduling                   
// %                                                                       
// % Revision 1.4  1996/12/22 19:57:54  neeri                              
// % TCP streams work                                                      
// %                                                                       
// % Revision 1.3  1996/11/24  12:52:04  neeri                             
// % Added GUSIPipeSockets                                                 
// %                                                                       
// % Revision 1.2  1996/11/18  00:53:46  neeri                             
// % TestTimers (basic threading/timer test) works                         
// %                                                                       
// % Revision 1.1.1.1  1996/11/03  02:43:32  neeri                         
// % Imported into CVS                                                     
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{Common routines for GUSI}                                      
//                                                                         
// This section defines various services used by all parts of GUSI:        
//                                                                         
// \begin{itemize}                                                         
// \item Various hooks to customize GUSI.                                  
// \item The propagation of {\bf errors} to the [[errno]] and [[h_errno]]  
// global variables.                                                       
// \item Waiting for completion of asynchronous calls.                     
// \item Event handling.                                                   
// \item Compiler features.                                                
// \end{itemize}                                                           
//                                                                         
// To protect our name space further, we maintain a strict C interface unless 
// [[GUSI_SOURCE]] is defined, and may avoid defining some stuff unless    
// [[GUSI_INTERNAL]] is defined. The following header is therefore included 
// first by all GUSI source files.                                         
//                                                                         
// <GUSIInternal.h>=                                                       
#ifndef _GUSIInternal_
#define _GUSIInternal_

#include <ConditionalMacros.h>

#define GUSI_SOURCE
#define GUSI_INTERNAL

#if !TARGET_RT_MAC_CFM
#pragma segment GUSI
#endif

#endif /* _GUSIInternal_ */
