// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSIDiag.nw			-	Assertions and diagnostics                    
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSIDiag.h,v $
// % Revision 1.1  2001/03/11 22:35:00  sgehani%netscape.com
// % First Checked In.
// %                                                 
// % Revision 1.12  2000/06/12 04:20:58  neeri                             
// % Introduce GUSI_*printf                                                
// %                                                                       
// % Revision 1.11  2000/05/23 06:58:04  neeri                             
// % Improve formatting                                                    
// %                                                                       
// % Revision 1.10  1999/08/26 05:45:01  neeri                             
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.9  1999/08/02 07:02:43  neeri                              
// % Support for asynchronous errors and other socket options              
// %                                                                       
// % Revision 1.8  1999/05/29 06:26:42  neeri                              
// % Fixed header guards                                                   
// %                                                                       
// % Revision 1.7  1999/04/10 04:45:47  neeri                              
// % Add DCon stubs                                                        
// %                                                                       
// % Revision 1.6  1999/03/17 09:05:07  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// % Revision 1.5  1999/02/25 03:49:00  neeri                              
// % Switched to DCon for logging                                          
// %                                                                       
// % Revision 1.4  1998/01/25 20:53:53  neeri                              
// % Engine implemented, except for signals & scheduling                   
// %                                                                       
// % Revision 1.3  1996/12/16 02:17:20  neeri                              
// % Tune messages                                                         
// %                                                                       
// % Revision 1.2  1996/11/24  12:52:07  neeri                             
// % Added GUSIPipeSockets                                                 
// %                                                                       
// % Revision 1.1.1.1  1996/11/03  02:43:32  neeri                         
// % Imported into CVS                                                     
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{Assertions and diagnostic messages}                            
//                                                                         
// GUSI reports on three kinds of error conditions:                        
//                                                                         
// \begin{itemize}                                                         
// \item {\bf Internal} errors are usually caused by bugs in GUSI. They should be
// considered fatal.                                                       
// \item {\bf Client} errors are caused by calling GUSI routines with bad  
// parameters. They are typically handled by returning an appropriate error
// code.                                                                   
// \item {\bf External} errors are caused by various OS and Network related
// problems. Typically, they are also handled by returning an appropriate  
// error code.                                                             
// \end{itemize}                                                           
//                                                                         
// If you feel lucky, you can turn off the checking for internal and client errors,
// but external errors will always be checked.                             
//                                                                         
// <GUSIDiag.h>=                                                           
#ifndef _GUSIDiag_
#define _GUSIDiag_

#include <sys/cdefs.h>
#include <stdarg.h>
#include <string.h>

#include <MacTypes.h>

__BEGIN_DECLS
// \section{Definition of diagnostics}                                     
//                                                                         
// Depending on the diagnostic level, messages get printed to the DCon console.
//                                                                         
// <Definition of diagnostic logfile>=                                     
extern char * GUSI_diag_log;
// Printing is done by passing an argument list to [[GUSI_log]]. To simplify the job
// for the diagnostic macros, [[GUSI_log]] returns a [[bool]] value which is
// always [[false]]. [[GUSI_break]] stops with a [[DebugStr]] call. [[GUSI_pos]] 
// establishes the source code position for diagnostic messages.           
// instead of logging.                                                     
//                                                                         
// <Definition of diagnostic logfile>=                                     
bool GUSI_pos(const char * file, int line);
bool GUSI_log(const char * format, ...);
bool GUSI_break(const char * format, ...);
// There are four levels of diagnostic handling:                           
// \begin{itemize}                                                         
// \item [[GUSI_DIAG_RECKLESS]] totally ignores all internal and client errors. This
// setting is not recommended.                                             
// \item [[GUSI_DIAG_RELAXED]] ignores internal errors, but checks client errors. This
// setting is probably the best for production code.                       
// \item [[GUSI_DIAG_CAUTIOUS]] checks and logs internal and client errors. This 
// setting is best for GUSI client development.                            
// \item [[GUSI_DIAG_PARANOID]] immediately stops at internal errors, checks and
// logs client and external errors. This is the preferred setting for GUSI 
// internal development.                                                   
// \end{itemize}                                                           
//                                                                         
//                                                                         
// <Definition of diagnostic levels>=                                      
#define GUSI_DIAG_RECKLESS	0
#define GUSI_DIAG_RELAXED	1
#define GUSI_DIAG_CAUTIOUS	2
#define GUSI_DIAG_PARANOID	3
// [[GUSI_DIAG]] is defined to the diagnostic level. It can be overridden on a 
// module by module basis, or by changing the default setting here.        
// [[GUSI_MESSAGE_LEVEL]] determines how important a message has to be to print it.
// [[GUSI_LOG_POS]] determines whether we want to log the code position.   
//                                                                         
// <Definition of diagnostic levels>=                                      
#ifndef GUSI_DIAG
#define GUSI_DIAG GUSI_DIAG_PARANOID
#endif
#ifndef GUSI_LOG_POS
#define GUSI_LOG_POS	0
#endif
#ifndef GUSI_MESSAGE_LEVEL 
#define GUSI_MESSAGE_LEVEL	3
#endif
// <Definition of diagnostics>=                                            
#if GUSI_LOG_POS
#define GUSI_POS	GUSI_pos(__FILE__, __LINE__)
#define GUSI_LOG	GUSI_POS || GUSI_log
#else
#define GUSI_LOG	GUSI_log
#endif
#define GUSI_BREAK	GUSI_break
// <Definition of diagnostics>=                                            
#if GUSI_DIAG == GUSI_DIAG_RECKLESS
// [[GUSI_DIAG_RECKLESS]] only checks for external errors.                 
//                                                                         
// <Diagnostics for [[GUSI_DIAG_RECKLESS]]>=                               
#define GUSI_ASSERT_INTERNAL(COND, ARGLIST)		true
#define GUSI_ASSERT_CLIENT(COND, ARGLIST)		true
#define GUSI_ASSERT_EXTERNAL(COND, ARGLIST)		(COND)
#define GUSI_SASSERT_INTERNAL(COND, MSG)		true
#define GUSI_SASSERT_CLIENT(COND, MSG)			true
#define GUSI_SASSERT_EXTERNAL(COND, MSG)		(COND)
#elif GUSI_DIAG == GUSI_DIAG_RELAXED
// [[GUSI_DIAG_RELAXED]] ignores internal errors, but checks client errors.
//                                                                         
// <Diagnostics for [[GUSI_DIAG_RELAXED]]>=                                
#define GUSI_ASSERT_INTERNAL(COND, ARGLIST)		true
#define GUSI_ASSERT_CLIENT(COND, ARGLIST)		(COND)
#define GUSI_ASSERT_EXTERNAL(COND, ARGLIST)		(COND)
#define GUSI_SASSERT_INTERNAL(COND, MSG)		true
#define GUSI_SASSERT_CLIENT(COND, MSG)			(COND)
#define GUSI_SASSERT_EXTERNAL(COND, MSG)		(COND)
#elif GUSI_DIAG == GUSI_DIAG_CAUTIOUS
// [[GUSI_DIAG_CAUTIOUS]] checks and logs internal and client errors.      
//                                                                         
// <Diagnostics for [[GUSI_DIAG_CAUTIOUS]]>=                               
#define GUSI_ASSERT_INTERNAL(COND, ARGLIST)		((COND) || GUSI_LOG ARGLIST)
#define GUSI_ASSERT_CLIENT(COND, ARGLIST)		((COND) || GUSI_LOG ARGLIST)
#define GUSI_ASSERT_EXTERNAL(COND, ARGLIST)		(COND)
#define GUSI_SASSERT_INTERNAL(COND, MSG)		((COND) || GUSI_LOG ("%s", MSG))
#define GUSI_SASSERT_CLIENT(COND, MSG)			((COND) || GUSI_LOG ("%s", MSG))
#define GUSI_SASSERT_EXTERNAL(COND, MSG)		(COND)
#elif GUSI_DIAG == GUSI_DIAG_PARANOID
// [[GUSI_DIAG_PARANOID]] immediately stops at internal errors, checks and 
// logs client and external errors.                                        
//                                                                         
// <Diagnostics for [[GUSI_DIAG_PARANOID]]>=                               
#define GUSI_ASSERT_INTERNAL(COND, ARGLIST)		((COND) || GUSI_BREAK ARGLIST)
#define GUSI_ASSERT_CLIENT(COND, ARGLIST)		((COND) || GUSI_LOG ARGLIST)
#define GUSI_ASSERT_EXTERNAL(COND, ARGLIST)		((COND) || GUSI_LOG ARGLIST)
#define GUSI_SASSERT_INTERNAL(COND, MSG)		((COND) || GUSI_BREAK ("%s", (MSG)))
#define GUSI_SASSERT_CLIENT(COND, MSG)			((COND) || GUSI_LOG ("%s", MSG))
#define GUSI_SASSERT_EXTERNAL(COND, MSG)		((COND) || GUSI_LOG ("%s", MSG))
#else
#error "GUSI_DIAG defined to illegal value: " GUSI_DIAG
#endif
// There are 11 different diagnostic macros. [[GUSI_ASSERT_INTERNAL]],     
// [[GUSI_ASSERT_CLIENT]] and [[GUSI_ASSERT_EXTERNAL]] check for internal, 
// client, and external errors, respectively. Their first argument is a    
// conditional expression which when [[false]] indicates that an error     
// happened. [[GUSI_MESSAGE]] has the same logging status as               
// [[GUSI_ASSERT_EXTERNAL]], but does not check any conditions. The [[SASSERT]]
// and [[SMESSAGE]] variants do the same checking, but only take a simple  
// message instead of an argument list. The [[CASSERT]] variants simply output
// the condition as the message.                                           
//                                                                         
// <Definition of diagnostics>=                                            
#define GUSI_CASSERT_INTERNAL(COND)	\
	GUSI_SASSERT_INTERNAL(COND, "(" #COND ") -- assertion failed.\n" )
#define GUSI_CASSERT_CLIENT(COND)	\
	GUSI_SASSERT_CLIENT(COND,   "(" #COND ") -- assertion failed.\n" )
#define GUSI_CASSERT_EXTERNAL(COND)	\
	GUSI_SASSERT_EXTERNAL(COND, "(" #COND ") -- assertion failed.\n" )
#define GUSI_MESSAGE1(ARGLIST)		\
	GUSI_ASSERT_EXTERNAL(GUSI_MESSAGE_LEVEL>1, ARGLIST)
#define GUSI_SMESSAGE1(MSG)			\
	GUSI_SASSERT_EXTERNAL(GUSI_MESSAGE_LEVEL>1, MSG)
#define GUSI_MESSAGE2(ARGLIST)		\
	GUSI_ASSERT_EXTERNAL(GUSI_MESSAGE_LEVEL>2, ARGLIST)
#define GUSI_SMESSAGE2(MSG)			\
	GUSI_SASSERT_EXTERNAL(GUSI_MESSAGE_LEVEL>2, MSG)
#define GUSI_MESSAGE3(ARGLIST)		\
	GUSI_ASSERT_EXTERNAL(GUSI_MESSAGE_LEVEL>3, ARGLIST)
#define GUSI_SMESSAGE3(MSG)			\
	GUSI_SASSERT_EXTERNAL(GUSI_MESSAGE_LEVEL>3, MSG)
#define GUSI_MESSAGE4(ARGLIST)		\
	GUSI_ASSERT_EXTERNAL(GUSI_MESSAGE_LEVEL>4, ARGLIST)
#define GUSI_SMESSAGE4(MSG)			\
	GUSI_SASSERT_EXTERNAL(GUSI_MESSAGE_LEVEL>4, MSG)
#define GUSI_MESSAGE5(ARGLIST)		\
	GUSI_ASSERT_EXTERNAL(GUSI_MESSAGE_LEVEL>5, ARGLIST)
#define GUSI_SMESSAGE5(MSG)			\
	GUSI_SASSERT_EXTERNAL(GUSI_MESSAGE_LEVEL>5, MSG)
#define GUSI_MESSAGE(ARGLIST)	GUSI_MESSAGE3(ARGLIST)
#define GUSI_SMESSAGE(MSG)		GUSI_SMESSAGE3(MSG)
__END_DECLS

#endif /* _GUSIDiag_ */
