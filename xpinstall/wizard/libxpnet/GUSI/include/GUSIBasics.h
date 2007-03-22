// <GUSIBasics.h>=                                                         
#ifndef _GUSIBasics_
#define _GUSIBasics_

#ifdef GUSI_SOURCE

#include <errno.h>
#include <sys/cdefs.h>
#include <stdarg.h>

#include <ConditionalMacros.h>

// \section{Definition of compiler features}                               
//                                                                         
// If possible, we use unnamed namespaces to wrap internal code.           
//                                                                         
// <Definition of compiler features>=                                      
#ifdef __MWERKS__
#define GUSI_COMPILER_HAS_NAMESPACE
#endif

#ifdef GUSI_COMPILER_HAS_NAMESPACE
#define GUSI_USING_STD_NAMESPACE using namespace std; using namespace std::rel_ops;
#else
#define GUSI_USING_STD_NAMESPACE
#endif

// Asynchronous MacOS calls need completion procedures which in classic 68K code 
// often take parameters in address registers. The way to handle this differs
// a bit between compilers. Note that the [[pascal]] keyword is ignored when
// generating CFM code.                                                    
//                                                                         
// <Definition of compiler features>=                                      
#if GENERATINGCFM
#define GUSI_COMPLETION_PROC_A0(proc, type) \
	void (*const proc##Entry)(type * param) 	= 	proc;
#define GUSI_COMPLETION_PROC_A1(proc, type) \
	void (*const proc##Entry)(type * param) 	= 	proc;
#elif defined(__MWERKS__)
#define GUSI_COMPLETION_PROC_A0(proc, type) \
	static pascal void proc##Entry(type * param : __A0) { proc(param); }
#define GUSI_COMPLETION_PROC_A1(proc, type) \
	static pascal void proc##Entry(type * param : __A1) { proc(param); }
#else
void * GUSIGetA0()  ONEWORDINLINE(0x2008);
void * GUSIGetA1()  ONEWORDINLINE(0x2009);
#define GUSI_COMPLETION_PROC_A0(proc, type) \
	static pascal void proc##Entry() 		\
		{ proc(reinterpret_cast<type *>(GUSIGetA0())); }
#define GUSI_COMPLETION_PROC_A1(proc, type) \
	static pascal void proc##Entry() 		\
		{ proc(reinterpret_cast<type *>(GUSIGetA1())); }
#endif
// %define GUSI_COMPLETION_PROC_A0 GUSI_COMPLETION_PROC_A1                 
//                                                                         
// SC seems to have an issue with mutable fields.                          
//                                                                         
// <Definition of compiler features>=                                      
#if defined(__SC__)
#define mutable
#define GUSI_MUTABLE(class, field) const_cast<class *>(this)->field
#else
#define GUSI_MUTABLE(class, field) field
#endif
// SC pretends to support standard scoping rules, but is in fact broken in 
// some cases.                                                             
//                                                                         
// <Definition of compiler features>=                                      
#if defined(__SC__)
#define for	if (0) ; else for
#endif
// The MPW compilers don't predeclare [[qd]].                              
//                                                                         
// <Definition of compiler features>=                                      
#if defined(__SC__) || defined(__MRC__)
#define GUSI_NEEDS_QD	QDGlobals	qd;
#else
#define GUSI_NEEDS_QD	
#endif
// \section{Definition of hook handling}                                   
//                                                                         
// GUSI supports a number of hooks. Every one of them has a different prototype, 
// but is passed as a [[GUSIHook]]. Hooks are encoded with an [[OSType]].  
//                                                                         
// <Definition of hook handling>=                                          
typedef unsigned long OSType;
typedef void (*GUSIHook)(void);
void GUSISetHook(OSType code, GUSIHook hook);
GUSIHook GUSIGetHook(OSType code);
// Currently, three hooks are supported: [[GUSI_SpinHook]] defines a function to 
// be called when GUSI waits on an event.                                  
// [[GUSI_ExecHook]] defines a function that determines whether a file is to be 
// considered ``executable''. [[GUSI_EventHook]] defines a routine that is called
// when a certain event happens. To install an event hook, pass [[GUSI_EventHook]]
// plus the event code. A few events, that is mouse-down and high level events,
// are handled automatically by GUSI. Passing [[-1]] for the hook disables default
// handling of an event.                                                   
//                                                                         
// <Definition of hook handling>=                                          
typedef bool	(*GUSISpinFn)(bool wait);
#define GUSI_SpinHook	'spin'

struct FSSpec;
typedef bool	(*GUSIExecFn)(const FSSpec * file);
#define GUSI_ExecHook	'exec'

struct EventRecord;
typedef void (*GUSIEventFn)(EventRecord * ev);
#define GUSI_EventHook	'evnt'
// For the purposes of the functions who actually call the hooks, here's the direct
// interface.                                                              
//                                                                         
// <Definition of hook handling>=                                          
#ifdef GUSI_INTERNAL
extern GUSISpinFn	gGUSISpinHook;
extern GUSIExecFn	gGUSIExecHook;
#endif /* GUSI_INTERNAL */
// \section{Definition of error handling}                                  
//                                                                         
// Like a good POSIX citizen, GUSI reports all errors in the [[errno]] global
// variable. This happens either through the [[GUSISetPosixError]] routine, which
// stores its argument untranslated, or through the [[GUSISetMacError]] routine,
// which translates MacOS error codes into the correct POSIX codes. The mapping
// of [[GUSISetMacError]] is not always appropriate, so some routines will have to
// preprocess some error codes. [[GUSIMapMacError]] returns the POSIX error corresponding
// to a MacOS error.                                                       
//                                                                         
// The domain name routines use an analogous variable [[h_errno]], which is 
// manipulated with [[GUSISetHostError]] and [[GUSISetMacHostError]].      
//                                                                         
// All routines return 0 if 0 was passed and -1 otherwise.                 
//                                                                         
// <Definition of error handling>=                                         
typedef short OSErr;

int GUSISetPosixError(int error);
int GUSISetMacError(OSErr error);
int GUSIMapMacError(OSErr error);
int GUSISetHostError(int error);
int GUSISetMacHostError(OSErr error);
// POSIX routines should never set [[errno]] from nonzero to zero. On the other
// hand, it's sometimes useful to see whether some particular region of the
// program set the error code or not. Therefore, we have such regions allocate
// a [[GUSIErrorSaver]] statically, which guarantees that previous error codes 
// get restored if necessary.                                              
//                                                                         
// <Definition of error handling>=                                         
class GUSIErrorSaver {
public:
	GUSIErrorSaver()  		{ fSavedErrno = ::errno; ::errno = 0; 	}
	~GUSIErrorSaver() 		{ if (!::errno) ::errno = fSavedErrno;  }
private:
	int fSavedErrno;
};
// \section{Definition of event handling}                                  
//                                                                         
// [[GUSIHandleNextEvent]] events by calling handlers installed            
// using the [[GUSI_EventHook]] mechanism.                                 
//                                                                         
// <Definition of event handling>=                                         
void GUSIHandleNextEvent(long sleepTime);
// \section{Definition of string formatting}                               
//                                                                         
// We occasionally need sprintf. To keep compatibility with MSL, Stdio, and Sfio, 
// we use an internal version which can be overridden.                     
//                                                                         
// <Definition of string formatting>=                                      
int GUSI_vsprintf(char * s, const char * format, va_list args);
int GUSI_sprintf(char * s, const char * format, ...);

#endif /* GUSI_SOURCE */

#endif /* _GUSIBasics_ */
