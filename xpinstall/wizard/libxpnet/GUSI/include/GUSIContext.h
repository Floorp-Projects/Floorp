// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSIContext.nw		-	Thread and Process structures               
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSIContext.h,v $
// % Revision 1.1  2001/03/11 22:33:38  sgehani%netscape.com
// % First Checked In.
// %                                              
// % Revision 1.22  2001/01/22 04:31:11  neeri                             
// % Last minute changes for 2.1.5                                         
// %                                                                       
// % Revision 1.21  2001/01/17 08:43:42  neeri                             
// % Tweak scheduling                                                      
// %                                                                       
// % Revision 1.20  2000/12/23 06:09:21  neeri                             
// % May need to create context for IO completions                         
// %                                                                       
// % Revision 1.19  2000/10/16 04:34:22  neeri                             
// % Releasing 2.1.2                                                       
// %                                                                       
// % Revision 1.18  2000/06/01 06:31:09  neeri                             
// % Delete SigContext                                                     
// %                                                                       
// % Revision 1.17  2000/05/23 06:56:19  neeri                             
// % Improve formatting, add socket closing queue, tune scheduling         
// %                                                                       
// % Revision 1.16  2000/03/15 07:11:50  neeri                             
// % Fix detached delete (again), switcher restore                         
// %                                                                       
// % Revision 1.15  2000/03/06 08:10:09  neeri                             
// % Fix sleep in main thread                                              
// %                                                                       
// % Revision 1.14  2000/03/06 06:13:46  neeri                             
// % Speed up thread/process switching through minimal quotas              
// %                                                                       
// % Revision 1.13  1999/12/13 02:40:50  neeri                             
// % GUSISetThreadSwitcher had Boolean <-> bool inconsistency              
// %                                                                       
// % Revision 1.12  1999/11/15 07:25:32  neeri                             
// % Safe context setup. Check interrupts only in foreground.              
// %                                                                       
// % Revision 1.11  1999/09/09 07:18:06  neeri                             
// % Added support for foreign threads                                     
// %                                                                       
// % Revision 1.10  1999/08/26 05:44:59  neeri                             
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.9  1999/06/28 05:59:02  neeri                              
// % Add signal handling support                                           
// %                                                                       
// % Revision 1.8  1999/05/30 03:09:29  neeri                              
// % Added support for MPW compilers                                       
// %                                                                       
// % Revision 1.7  1999/03/17 09:05:05  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// % Revision 1.6  1999/02/25 03:34:24  neeri                              
// % Introduced GUSIContextFactory, simplified wakeup                      
// %                                                                       
// % Revision 1.5  1998/11/22 23:06:51  neeri                              
// % Releasing 2.0a4 in a hurry                                            
// %                                                                       
// % Revision 1.4  1998/10/11 16:45:11  neeri                              
// % Ready to release 2.0a2                                                
// %                                                                       
// % Revision 1.3  1998/08/01 21:26:18  neeri                              
// % Switch dynamically to threading model                                 
// %                                                                       
// % Revision 1.2  1998/02/11 12:57:11  neeri                              
// % PowerPC Build                                                         
// %                                                                       
// % Revision 1.1  1998/01/25 21:02:41  neeri                              
// % Engine implemented, except for signals & scheduling                   
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{Thread and Process structures}                                 
//                                                                         
// This section defines the process and thread switching engine of GUSI.   
//                                                                         
// In some execution environments, completion routines execute at interrupt level.
// GUSI therefore is designed so all information needed to operate from interrupt
// level is accessible from a [[GUSISocket]]. This information is separated into
// per-process data, collected in [[GUSIProcess]], and per-thread data, collected 
// in [[GUSIContext]]. [[GUSIProcess]] is always a singleton, while [[GUSIContext]]
// is a singleton if threading is disabled, and has multiple instances if threading
// is enabled. By delegating the [[GUSIContext]] creation process to an instance
// of a [[GUSIContextFactory]], we gain some extra flexibility.            
//                                                                         
// As soon as GUSI has started an asynchronous call, it calls the [[Wait]] member
// function of its context. [[msec]] will set a time limit after which the call will 
// return in any case. Exceptional events may also cause [[GUSIWait]] to return, so 
// it is not safe to assume that the call will have completed upon return. 
//                                                                         
//                                                                         
// <GUSIContext.h>=                                                        
#ifndef _GUSIContext_
#define _GUSIContext_

#include <errno.h>
#include <sys/cdefs.h>
#include <sys/signal.h>

#include <MacTypes.h>
#include <Threads.h>

__BEGIN_DECLS
// To maintain correct state, we have to remain informed which thread is active, so
// we install all sorts of hooks. Clients have to use the C++ interface or call
// [[GUSINewThread]], [[GUSISetThreadSwitcher]], and [[GUSISetThreadTerminator]].
// instead of the thread manager routines.                                 
//                                                                         
// <Definition of thread manager hooks>=                                   
OSErr GUSINewThread(
		ThreadStyle threadStyle, ThreadEntryProcPtr threadEntry, void *threadParam, 
		Size stackSize, ThreadOptions options, 
		void **threadResult, ThreadID *threadMade);
OSErr GUSISetThreadSwitcher(ThreadID thread, 
		ThreadSwitchProcPtr threadSwitcher, void *switchProcParam, Boolean inOrOut);
OSErr GUSISetThreadTerminator(ThreadID thread, 
		ThreadTerminationProcPtr threadTerminator, void *terminationProcParam); 
__END_DECLS

#ifndef GUSI_SOURCE

typedef struct GUSIContext GUSIContext;

#else

#include "GUSISpecific.h"
#include "GUSIBasics.h"
#include "GUSIContextQueue.h"

#include <Files.h>
#include <Processes.h>
#include <OSUtils.h>

// \section{Definition of completion handling}                             
//                                                                         
// {\tt GUSIContext} is heavily circular both with classes declared herein and
// in other files. Therefore, we start by declaring a few class names.     
//                                                                         
// <Name dropping for file GUSIContext>=                                   
class GUSISocket;
class GUSIContext;
class GUSIProcess;
class GUSISigProcess;
class GUSISigContext;
class GUSITimer;

#include <ConditionalMacros.h>

#if PRAGMA_STRUCT_ALIGN
#pragma options align=native
#endif

// Ultimately, we will call through to the thread manager, but if an application uses foreign 
// sources of threads, we might have to go through indirections.           
//                                                                         
// <Definition of class [[GUSIThreadManagerProxy]]>=                       
class GUSIThreadManagerProxy {
public:
	virtual OSErr NewThread(
				ThreadStyle threadStyle, ThreadEntryProcPtr threadEntry, void *threadParam, 
				Size stackSize, ThreadOptions options, void **threadResult, ThreadID *threadMade);
	virtual OSErr SetThreadSwitcher(
				ThreadID thread, ThreadSwitchProcPtr threadSwitcher, void *switchProcParam, 
				Boolean inOrOut);
	virtual OSErr SetThreadTerminator(
				ThreadID thread, ThreadTerminationProcPtr threadTerminator, void *terminatorParam);
	
	virtual ~GUSIThreadManagerProxy() {}
	
	static GUSIThreadManagerProxy * Instance();
protected:
	GUSIThreadManagerProxy()	{}
	
	static GUSIThreadManagerProxy * MakeInstance();
};
// A [[GUSIProcess]] contains all the data needed to wake up a process:    
//                                                                         
// \begin{itemize}                                                         
// \item The [[ProcessSerialNumber]] of the process.                       
// \item The [[ThreadTaskRef]] if threads are enabled.                     
// \item The contents of the A5 register.                                  
// \end{itemize}                                                           
//                                                                         
// The sole instance of [[GUSIProcess]] is obtained by calling the [[GUSIProcess::Instance]] static member
// function, which will create the instance if necessary. Interrupt level prcedures may access the application's 
// A5 register either manually by calling [[GetA5]] or simply by declaring a [[GUSIProcess::A5Saver]] in a scope.
//                                                                         
// <Definition of class [[GUSIProcess]]>=                                  
enum GUSIYieldMode {
	kGUSIPoll,		// Busy wait for some unblockable condition
	kGUSIBlock,		// Wait for some blockable condition
	kGUSIYield		// Yield to some other eligible thread
};

class GUSIProcess {
public:
	static GUSIProcess *	Instance();
	static void				DeleteInstance();
	void					GetPSN(ProcessSerialNumber * psn);
	void					AcquireTaskRef();
	ThreadTaskRef			GetTaskRef();
	long					GetA5();
	bool					Threading();
	void					Yield(GUSIYieldMode wait);
	void					Wakeup();
	GUSISigProcess *		SigProcess() { return fSigProcess; }
	void					QueueForClose(GUSISocket * sock);
	// A [[GUSIProcess::A5Saver]] is a class designed to restore the process A5 
 // register for the scope of its declaration.                              
 //                                                                         
 // <Definition of class [[GUSIProcess::A5Saver]]>=                         
 class A5Saver {
 public:
 	A5Saver(long processA5);
 	A5Saver(GUSIContext * context);
 	A5Saver(GUSIProcess * process);
 	~A5Saver();
 private:
 	long	fSavedA5;
 };
protected:
	friend class GUSIContext;
	
	GUSIProcess(bool threading);
	~GUSIProcess();
	
	int						fReadyThreads;
	int						fExistingThreads;
	GUSISigProcess *		fSigProcess;
private:
	// \section{Implementation of completion handling}                         
 //                                                                         
 // [[Instance]] returns the sole instance of [[GUSIProcess]], creating it if
 // necessary.                                                              
 //                                                                         
 // <Privatissima of [[GUSIProcess]]>=                                      
 static GUSIProcess *	sInstance;
 // Much of the information stored in a [[GUSIProcess]] is static and read-only.
 //                                                                         
 // <Privatissima of [[GUSIProcess]]>=                                      
 ProcessSerialNumber	fProcess;
 ThreadTaskRef		fTaskRef;
 long				fA5;
 // The exception is the [[fClosing]] socket queue and some yielding related flags.
 //                                                                         
 // <Privatissima of [[GUSIProcess]]>=                                      
 GUSISocket *		fClosing;
 UInt32				fResumeTicks;
 bool				fWillSleep;
 bool				fDontSleep;
};
// A [[GUSIContext]] gathers thread related data. The central operation on a 
// [[GUSIContext]] is [[Wakeup]]. If the process is not asleep when [[Wakeup]] 
// is called, it is marked for deferred wakeup.                            
//                                                                         
// A [[GUSIContext]] can either be created from an existing thread manager 
// [[ThreadID]] or by specifying the parameters for a [[NewThread]] call.  
//                                                                         
// [[Current]] returns the current [[GUSIContext]]. [[Setup]] initializes the
// default context for either the threading or the non-threading model.    
//                                                                         
// [[Yield]] suspends the current process or thread until something interesting
// happens if [[wait]] is [[kGUSIBlock]. Otherwise, [[Yield]] switches,    
// but does not suspend. For an ordinary thread context, [[Yield]] simply yields 
// the thread. For the context in a non-threading application, [[Yield]] does a 
// [[WaitNextEvent]]. For the main thread context, [[Yield]] does both.    
//                                                                         
// [[Done]] tests whether the thread has terminated yet. If [[join]] is set,
// the caller is willing to wait. [[Result]] returns the default location to store 
// the thread result if no other is specified.                             
//                                                                         
// By default, a context is joinable. Calling [[Detach]] will cause the context to
// be destroyed automatically upon thread termination, and joins are no longer allowed.
// A joinable context will not be destroyed automatically before the end of the 
// program, so you will have to call [[Liquidate]] to do that.             
//                                                                         
// [[SetSwitchIn]], [[SetSwitchOut]], and [[SetTerminator]] set per-thread user
// switch and termination procedures. [[SwitchIn]], [[SwitchOut]], and [[Terminate]]
// call the user defined procedures then perform their own actions.        
//                                                                         
// <Definition of class [[GUSIContext]]>=                                  
class GUSIContext : public GUSISpecificTable {
public:
	friend class GUSIProcess;
	friend class GUSIContextFactory;
	
	ThreadID				ID()			{	return fThreadID;			}
	virtual void 			Wakeup();
	void					ClearWakeups()	{ fWakeup = false; 			}
	GUSIProcess *			Process()		{	return fProcess;			}
	void 					Detach()		{	fFlags |= detached;			}
	void 					Liquidate();
	OSErr					Error()			{ 	return sError;				}
	bool					Done(bool join);
	void *					Result()		{   return fResult;				}
	GUSISigContext *		SigContext()	{ 	return fSigContext;			}

	static GUSIContext *	Current()		{	return sCurrentContext;		}
	static GUSIContext *	CreateCurrent(bool threading = false)	
		{	if (!sCurrentContext) Setup(threading);	return sCurrentContext;	}
	static GUSIContext *	Lookup(ThreadID id);
	static void				Setup(bool threading);
	static bool				Yield(GUSIYieldMode wait);
	static void				SigWait(sigset_t sigs);
	static void				SigSuspend();
	static bool				Raise(bool allSigs = false);
	static sigset_t			Pending();
	static sigset_t			Blocked();
	
	void SetSwitchIn(ThreadSwitchProcPtr switcher, void *switchParam);
	void SetSwitchOut(ThreadSwitchProcPtr switcher, void *switchParam);
	void SetTerminator(ThreadTerminationProcPtr terminator, void *terminationParam);	
	
	static GUSIContextQueue::iterator	begin()	{	return sContexts.begin();	}
	static GUSIContextQueue::iterator	end()	{	return sContexts.end();		}
	static void	LiquidateAll()					{ 	sContexts.LiquidateAll(); 	}
protected:
	// <Friends of [[GUSIContext]]>=                                           
 friend class GUSIContextFactory;
 // The thread switcher updates the pointer to the current context and switches
 // the global error variables.                                             
 //                                                                         
 // <Friends of [[GUSIContext]]>=                                           
 friend pascal void GUSIThreadSwitchIn(ThreadID thread, GUSIContext * context);
 friend pascal void GUSIThreadSwitchOut(ThreadID thread, GUSIContext * context);
 // The terminator wakes up the joining thread if a join is pending.        
 //                                                                         
 // <Friends of [[GUSIContext]]>=                                           
 friend pascal void GUSIThreadTerminator(ThreadID thread, GUSIContext * context);

	GUSIContext(ThreadID id);	
	GUSIContext(
		ThreadEntryProcPtr threadEntry, void *threadParam, 
		Size stackSize, ThreadOptions options = kCreateIfNeeded, 
		void **threadResult = nil, ThreadID *threadMade = nil);

	virtual void SwitchIn();
	virtual void SwitchOut();
	virtual void Terminate();
	
	// At this point, we need to introduce all the private data of a [[GUSIContext]].
 //                                                                         
 // \begin{itemize}                                                         
 // \item [[fThreadID]] stores the thread manager thread ID.                
 // \item [[fProcess]] keeps a pointer to the process structure, so completion 
 // routines can get at it.                                                 
 // \item [[sCurrentContext]] always points at the current context.         
 // \item [[sContexts]] contains a queue of all contexts.                   
 // \item [[sHasThreads]] reminds us whether we are threading or not.       
 // \item We define our own switch-in and termination procedures. If the user specifies procedures 
 // we store them in [[fSwitchInProc]], [[fSwitchOutProc]], and [[fTerminateProc]] and their parameters 
 // in [[fSwitchInParam]], [[fSwitchOutParam]], and [[fTerminateParam]] so we can call through to them 
 // from our procedures.                                                    
 // \item [[fJoin]] contains the context waiting for us to die;             
 // \item [[done]] reminds us if the thread is still alive. [[detached]] guarantees
 // that we will never wait for that thread anymore.                        
 // \item Last of all, we keep the global error variables [[errno]] and [[h_errno]] 
 // for each context in the [[fErrno]] and [[fHostErrno]] fields.           
 // \end{itemize}                                                           
 //                                                                         
 //                                                                         
 // <Privatissima of [[GUSIContext]]>=                                      
 ThreadID 					fThreadID;	
 GUSIProcess *				fProcess;
 GUSIContext *				fNext;
 GUSISigContext *			fSigContext;
 ThreadSwitchProcPtr			fSwitchInProc;
 ThreadSwitchProcPtr			fSwitchOutProc;
 ThreadTerminationProcPtr	fTerminateProc;
 void *						fSwitchInParam;
 void *						fSwitchOutParam;
 void *						fTerminateParam;
 void *						fResult;
 GUSIContext *				fJoin;
 enum {
 	done 	= 1 << 0, 
 	detached= 1 << 1,
 	asleep	= 1 << 2
 };
 char						fFlags;
 bool						fWakeup;
 UInt32						fEntryTicks;
 int							fErrno;
 int							fHostErrno;

 class Queue : public GUSIContextQueue {
 public:
 	void LiquidateAll();
 	
 	~Queue()			{ LiquidateAll(); }
 };

 static Queue			sContexts;
 static GUSIContext *	sCurrentContext;
 static bool				sHasThreading;
 static OSErr			sError;
 // The [[GUSIContext]] constructor links the context into the queue of existing
 // contexts and installs the appropriate thread hooks. We split this into two 
 // routines: [[StartSetup]] does static setup before the thread id is determined,
 // [[FinishSetup]] does the queueing.                                      
 //                                                                         
 // <Privatissima of [[GUSIContext]]>=                                      
 void StartSetup();
 void FinishSetup();
 // Destruction of a [[GUSIContext]] requires some cleanup.                 
 //                                                                         
 // <Privatissima of [[GUSIContext]]>=                                      
 ~GUSIContext();
};
// [[GUSIContext]] instances are created by instances of [[GUSIContextFactory]].
//                                                                         
// <Definition of class [[GUSIContextFactory]]>=                           
class GUSIContextFactory {
public:
	static GUSIContextFactory *	Instance();
	static void 				SetInstance(GUSIContextFactory * instance);
	
	virtual GUSIContext	* CreateContext(ThreadID id);
	virtual GUSIContext * CreateContext(
		ThreadEntryProcPtr threadEntry, void *threadParam, 
		Size stackSize, ThreadOptions options = kCreateIfNeeded, 
		void **threadResult = nil, ThreadID *threadMade = nil);

	virtual ~GUSIContextFactory();
protected:
	GUSIContextFactory();
};
// Many asynchronous calls take the same style of I/O parameter block and thus
// can be handled by the same completion procedure. [[StartIO]] prepares   
// a parameter block for asynchronous I/O; [[FinishIO]] waits for the I/O  
// to complete. The parameter block has to be wrapped in a [[GUSIIOPBWrapper]].
//                                                                         
// <Definition of IO wrappers>=                                            
void GUSIStartIO(IOParam * pb);
OSErr GUSIFinishIO(IOParam * pb);
OSErr GUSIControl(IOParam * pb);
template <class PB> struct GUSIIOPBWrapper {
	GUSIContext *	fContext;
	PB				fPB;
	
	GUSIIOPBWrapper() 				{}
	GUSIIOPBWrapper(const PB & pb) 	{ memcpy(&fPB, &pb, sizeof(PB)); 			}
	
	PB * operator->(){ return &fPB;												}
	void StartIO() 	 { GUSIStartIO(reinterpret_cast<IOParam *>(&fPB));			}
	OSErr FinishIO() { return GUSIFinishIO(reinterpret_cast<IOParam *>(&fPB)); 	}
	OSErr Control()	 { return GUSIControl(reinterpret_cast<IOParam *>(&fPB));  	}
};

#if PRAGMA_STRUCT_ALIGN
#pragma options align=reset
#endif

// <Inline member functions for file GUSIContext>=                         
inline GUSIProcess * GUSIProcess::Instance()
{
	if (!sInstance) 
		sInstance = new GUSIProcess(GUSIContext::sHasThreading);
	return sInstance;
}
inline void GUSIProcess::DeleteInstance()
{
	delete sInstance;
	sInstance = 0;
}
// <Inline member functions for file GUSIContext>=                         
inline void 			GUSIProcess::GetPSN(ProcessSerialNumber * psn)	
							{ *psn = fProcess;	}
inline void				GUSIProcess::AcquireTaskRef()
							{ GetThreadCurrentTaskRef(&fTaskRef); }
inline ThreadTaskRef	GUSIProcess::GetTaskRef()				
							{ return fTaskRef;	}
inline long				GUSIProcess::GetA5()								
							{ return fA5;		}
inline bool				GUSIProcess::Threading()							
							{ return fTaskRef!=0;}
// An [[A5Saver]] is trivially implemented but it simplifies bookkeeping.  
//                                                                         
// <Inline member functions for file GUSIContext>=                         
inline GUSIProcess::A5Saver::A5Saver(long processA5)		
	{	fSavedA5 = SetA5(processA5);					}
inline GUSIProcess::A5Saver::A5Saver(GUSIProcess * process)
	{	fSavedA5 = SetA5(process->GetA5());			}
inline GUSIProcess::A5Saver::A5Saver(GUSIContext * context)
	{	fSavedA5 = SetA5(context->Process()->GetA5());	}
inline GUSIProcess::A5Saver::~A5Saver()
	{	SetA5(fSavedA5);								}

#endif /* GUSI_SOURCE */

#endif /* _GUSIContext_ */
