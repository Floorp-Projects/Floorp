// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSIMSL.nw			-	Interface to the MSL                           
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSIMSL.h,v $
// % Revision 1.1  2001/03/11 22:35:53  sgehani%netscape.com
// % First Checked In.
// %                                                  
// % Revision 1.8  2000/10/29 19:17:04  neeri                              
// % Accommodate MSL's non-compliant fopen signature                       
// %                                                                       
// % Revision 1.7  2000/10/16 04:34:22  neeri                              
// % Releasing 2.1.2                                                       
// %                                                                       
// % Revision 1.6  2000/05/23 07:03:25  neeri                              
// % Improve formatting                                                    
// %                                                                       
// % Revision 1.5  1999/08/26 05:45:03  neeri                              
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.4  1999/08/02 07:02:43  neeri                              
// % Support for asynchronous errors and other socket options              
// %                                                                       
// % Revision 1.3  1999/04/14 04:20:21  neeri                              
// % Override console hooks                                                
// %                                                                       
// % Revision 1.2  1999/04/10 04:53:58  neeri                              
// % Use simpler internal MSL routines                                     
// %                                                                       
// % Revision 1.1  1998/08/01 21:32:07  neeri                              
// % About ready for 2.0a1                                                 
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{The Interface to the MSL}                                      
//                                                                         
// This section interfaces GUSI to the Metrowerks Standard Library (MSL)   
// by reimplementing various internal MSL routines. Consequently, some of  
// the code used here is borrowed from the MSL code itself. The routines   
// here are in three different categories:                                 
//                                                                         
// \begin{itemize}                                                         
// \item Overrides of MSL functions (all internal, as it happens).         
// \item Implementations of ANSI library specific public GUSI functions like
// 	[[fdopen]].                                                            
// \item Implementations of ANSI library specific internal GUSI functions. 
// \end{itemize}                                                           
//                                                                         
//                                                                         
// <GUSIMSL.h>=                                                            
#ifndef _GUSIMSL_
#define _GUSIMSL_

#endif /* _GUSIMSL_ */
