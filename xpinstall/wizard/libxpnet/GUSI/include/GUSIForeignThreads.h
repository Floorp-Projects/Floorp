// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI					-	Grand Unified Socket Interface                   
// % File		:	GUSIForeignThreads.nw	-	Foreign thread support                
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSIForeignThreads.h,v $
// % Revision 1.1  2001/03/11 22:35:11  sgehani%netscape.com
// % First Checked In.
// %                                       
// % Revision 1.4  2000/12/23 06:10:48  neeri                              
// % Use kPowerPCCFragArch, NOT GetCurrentArchitecture()                   
// %                                                                       
// % Revision 1.3  2000/05/23 07:00:00  neeri                              
// % Improve formatting                                                    
// %                                                                       
// % Revision 1.2  2000/03/06 08:28:32  neeri                              
// % Releasing 2.0.5                                                       
// %                                                                       
// % Revision 1.1  1999/09/09 07:18:06  neeri                              
// % Added support for foreign threads                                     
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{Supporting threads made outside of GUSI}                       
//                                                                         
// As convenient as the pthreads interface is, some applications may link to other 
// libraries which create thread manager threads directly, such as the PowerPlant
// thread classes.                                                         
//                                                                         
// Unfortunately, there is no really elegant way to welcome these lost sheep into the
// pthread flock, since the thread manager offers no way to retrieve thread switching
// and termination procedures. We therefore have to resort to a violent technique used
// already successfully for MPW support: On CFM, we override the default entry point and
// use the CFM manager to find the real implementation.                    
//                                                                         
// For non-CFM, we unfortunately don't have such an effective technique, since the 
// thread manager is called through traps (and no, I'm not going to patch any traps
// for this). You will therefore have to recompile your foreign libraries with
// a precompiled header that includes \texttt{GUSIForeignThreads.h}.       
//                                                                         
// <GUSIForeignThreads.h>=                                                 
#ifndef _GUSIForeignThreads_
#define _GUSIForeignThreads_

#define NewThread(style, entry, param, stack, options, result, made)	\
	GUSINewThread((style), (entry), (param), (stack), (options), (result), (made))
#define SetThreadSwitcher(thread, switcher, param, inOrOut) \
	GUSISetThreadSwitcher((thread), (switcher), (param), (inOrOut))
#define SetThreadTerminator(thread, threadTerminator, terminationProcParam) \
	GUSISetThreadTerminator((thread), (threadTerminator), (terminationProcParam))
#endif /* _GUSIForeignThreads_ */
