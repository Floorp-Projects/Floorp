// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSISignal.nw		-	Signal engine                                
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSISignal.h,v $
// % Revision 1.1  2001/03/11 22:38:04  sgehani%netscape.com
// % First Checked In.
// %                                               
// % Revision 1.7  2000/10/16 04:08:51  neeri                              
// % Add binary compatibility for CW SIGINT                                
// %                                                                       
// % Revision 1.6  2000/05/23 07:18:03  neeri                              
// % Improve formatting                                                    
// %                                                                       
// % Revision 1.5  2000/03/15 07:22:07  neeri                              
// % Enforce alignment choices                                             
// %                                                                       
// % Revision 1.4  1999/12/13 03:07:25  neeri                              
// % Releasing 2.0.2                                                       
// %                                                                       
// % Revision 1.3  1999/11/15 07:20:18  neeri                              
// % Safe context setup                                                    
// %                                                                       
// % Revision 1.2  1999/08/26 05:45:09  neeri                              
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.1  1999/06/30 07:42:07  neeri                              
// % Getting ready to release 2.0b3                                        
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{Signal support}                                                
//                                                                         
// We support signals in the half assed way characteristic for GUSI's approach to 
// asynchronous issues: Delivery is very much synchronous, basically within [[Yield]]
// calls. Signal handling behavior is encapsulated in the classes [[GUSISigContext]] and
// [[GUSISigProcess]] whose instances are manufactured by a [[GUSISigFactory]].
//                                                                         
// <GUSISignal.h>=                                                         
#ifndef _GUSISIGNAL_
#define _GUSISIGNAL_

#include <signal.h>

#ifdef GUSI_SOURCE

#include <ConditionalMacros.h>

#if PRAGMA_STRUCT_ALIGN
#pragma options align=native
#endif

// \section{Definition of the signal handling engine}                      
//                                                                         
// A [[GUSISigProcess]] contains the per-process signal state. [[GetAction]] and [[SetAction]] manipulate the
// action associated with a signal, [[Pending]] returns the set of pending signals, [[Post]] marks a signal 
// as pending (but possibly blocked), and [[Raise]] executes a signal (which we have determined is not 
// blocked).                                                               
//                                                                         
// <Definition of class [[GUSISigProcess]]>=                               
class GUSISigContext;

class GUSISigProcess {
public:
	virtual struct sigaction &	GetAction(int sig);
	virtual int					SetAction(int sig, const struct sigaction & act);
	virtual sigset_t			Pending() const;
	virtual void				ClearPending(sigset_t clear);
	virtual void				Post(int sig);
	virtual bool				Raise(int sig, GUSISigContext * context);
	
	virtual ~GUSISigProcess();
protected:
	// [[GUSISigProcess]] stores the signal handlers and the set of signals pending against the process.
 //                                                                         
 // <Privatissima of [[GUSISigProcess]]>=                                   
 sigset_t			fPending;
 struct sigaction	fAction[NSIG-1];
 // Some actions can't be caught and/or ignored. [[CantCatch]] and [[CantIgnore]] report those.
 //                                                                         
 // <Privatissima of [[GUSISigProcess]]>=                                   
 virtual bool CantCatch(int sig);
 virtual bool CantIgnore(int sig);
 // The default behavior for many signals is to abort the process.          
 //                                                                         
 // <Privatissima of [[GUSISigProcess]]>=                                   
 virtual bool DefaultAction(int sig, const struct sigaction & act);
	
	friend class GUSISigFactory;
	GUSISigProcess();
};
// A [[GUSISigContext]] contains the per-thread signal state, primarily blocking info. To support 
// [[pthread_kill]], we have out own set of pending signals. [[GetBlocked]] and [[SetBlocked]] manipulate
// the set of blocking signals, [[Pending]] returns the set of pending signals, [[Post]] marks a 
// signal as pending (but possibly blocked), and [[Raise]] executes all eligible signals.
//                                                                         
// <Definition of class [[GUSISigContext]]>=                               
class GUSISigContext {
public:
	virtual	sigset_t	GetBlocked() const;
	virtual void		SetBlocked(sigset_t sigs);
	virtual sigset_t	Pending() const;
	virtual sigset_t	Pending(GUSISigProcess * proc) const;
	virtual void		ClearPending(sigset_t clear);
	virtual void		Post(int sig);
	virtual	sigset_t	Ready(GUSISigProcess * proc);	
	virtual	bool		Raise(GUSISigProcess * proc, bool allSigs = false);	

	virtual ~GUSISigContext();
protected:
	// [[GUSISigContext]] mainly deals with a set of blocked signals, which it inherits from its parent.
 //                                                                         
 // <Privatissima of [[GUSISigContext]]>=                                   
 sigset_t	fPending;
 sigset_t	fBlocked;
 // Many signals cannot be blocked. [[CantBlock]] defines those.            
 //                                                                         
 // <Privatissima of [[GUSISigContext]]>=                                   
 virtual sigset_t	CantBlock();
	
	friend class GUSISigFactory;
	GUSISigContext(const GUSISigContext * parent);
};
// The [[GUSISigFactory]] singleton creates the above two classes, allowing a future extension to
// handle more signals.                                                    
//                                                                         
// <Definition of class [[GUSISigFactory]]>=                               
class GUSISigFactory {
public:
	virtual GUSISigProcess *	CreateSigProcess();
	virtual GUSISigContext *	CreateSigContext(const GUSISigContext * parent);

	virtual ~GUSISigFactory();
	
	static GUSISigFactory *		Instance();
	static void					SetInstance(GUSISigFactory * instance);
protected:
	GUSISigFactory()			{}
};

#if PRAGMA_STRUCT_ALIGN
#pragma options align=reset
#endif

#endif

#endif /* _GUSISIGNAL_ */
