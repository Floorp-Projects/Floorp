// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSIMPW.nw			-	MPW Interface                                  
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSIMPW.h,v $
// % Revision 1.1  2001/03/11 22:35:49  sgehani%netscape.com
// % First Checked In.
// %                                                  
// % Revision 1.13  2001/01/17 08:48:04  neeri                             
// % Introduce Expired(), Reset()                                          
// %                                                                       
// % Revision 1.12  2000/05/23 07:01:53  neeri                             
// % Improve formatting                                                    
// %                                                                       
// % Revision 1.11  1999/08/26 05:45:03  neeri                             
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.10  1999/07/19 06:17:08  neeri                             
// % Add SIOW support                                                      
// %                                                                       
// % Revision 1.9  1999/07/07 04:17:41  neeri                              
// % Final tweaks for 2.0b3                                                
// %                                                                       
// % Revision 1.8  1999/06/08 04:31:29  neeri                              
// % Getting ready for 2.0b2                                               
// %                                                                       
// % Revision 1.7  1999/05/29 06:26:43  neeri                              
// % Fixed header guards                                                   
// %                                                                       
// % Revision 1.6  1999/04/29 05:33:20  neeri                              
// % Fix fcntl prototype                                                   
// %                                                                       
// % Revision 1.5  1999/03/29 09:51:28  neeri                              
// % New configuration system with support for hardcoded configurations.   
// %                                                                       
// % Revision 1.4  1999/03/17 09:05:08  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// % Revision 1.3  1998/11/22 23:06:54  neeri                              
// % Releasing 2.0a4 in a hurry                                            
// %                                                                       
// % Revision 1.2  1998/10/25 11:57:35  neeri                              
// % Ready to release 2.0a3                                                
// %                                                                       
// % Revision 1.1  1998/08/01 21:32:06  neeri                              
// % About ready for 2.0a1                                                 
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{MPW Support}                                                   
//                                                                         
// In MPW tools, we have to direct some of the I/O operations to the standard
// library functions, which we otherwise try to avoid as much as possible. 
// Getting at those library calls is a bit tricky: For 68K, we rename entries
// in the MPW glue library, while for PowerPC, we look up the symbols dynamically.
//                                                                         
// MPW support is installed implicitly through [[GUSISetupConsoleDescriptors]]
//                                                                         
// <GUSIMPW.h>=                                                            
#ifndef _GUSIMPW_
#define _GUSIMPW_

#endif /* _GUSIMPW_ */
