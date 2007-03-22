// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSIPThread.nw		-	Pthreads wrappers                           
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: pthread.h,v $
// % Revision 1.1  2001/03/11 22:39:05  sgehani%netscape.com
// % First Checked In.
// %                                              
// % Revision 1.14  2001/01/17 08:55:16  neeri                             
// % Detect and return ETIMEDOUT condition                                 
// %                                                                       
// % Revision 1.13  2000/10/29 20:31:53  neeri                             
// % Releasing 2.1.3                                                       
// %                                                                       
// % Revision 1.12  2000/05/23 07:16:35  neeri                             
// % Improve formatting, make data types opaque, tune mutexes              
// %                                                                       
// % Revision 1.11  2000/03/06 06:10:00  neeri                             
// % Reorganize Yield()                                                    
// %                                                                       
// % Revision 1.10  2000/01/17 01:40:31  neeri                             
// % Correct macro spelling, update references                             
// %                                                                       
// % Revision 1.9  1999/11/15 07:20:19  neeri                              
// % Safe context setup                                                    
// %                                                                       
// % Revision 1.8  1999/09/09 07:22:15  neeri                              
// % Add support for mutex and cond attribute creation/destruction         
// %                                                                       
// % Revision 1.7  1999/08/26 05:45:07  neeri                              
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.6  1999/07/07 04:17:42  neeri                              
// % Final tweaks for 2.0b3                                                
// %                                                                       
// % Revision 1.5  1999/06/30 07:42:07  neeri                              
// % Getting ready to release 2.0b3                                        
// %                                                                       
// % Revision 1.4  1999/05/30 03:06:55  neeri                              
// % MPW compiler compatibility, recursive mutex locks                     
// %                                                                       
// % Revision 1.3  1999/03/17 09:05:12  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// % Revision 1.2  1998/08/01 21:32:10  neeri                              
// % About ready for 2.0a1                                                 
// %                                                                       
// % Revision 1.1  1998/01/25 21:02:51  neeri                              
// % Engine implemented, except for signals & scheduling                   
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{Pthreads Wrappers}                                             
//                                                                         
// As opposed to POSIX.1, with which I think I'm reasonable competent by now,
// I have little practical experience, let alone in-depth familiarity with 
// Pthreads, so I'm going by what I learned from                           
//                                                                         
// \begin{itemize}                                                         
// \item Reading \emph{B.~Nicols, D.~Buttlar, and J.~Proulx Farrell,       
//       ``Pthreads Programming'', O'Reilly \& Associates} and             
// 	  \emph{D.~Butenhof, ``Programming with POSIX Threads'', Addison Wesley}.
// 	                                                                       
// \item Taking a few glimpses at Chris Provenzano's pthreads implementation.
// \item Reading the news:comp.programming.threads newsgroup.              
// \end{itemize}                                                           
//                                                                         
// If you believe that I've misunderstood Pthreads in my implementation, feel free
// to contact me.                                                          
//                                                                         
// As opposed to most other modules, the header files we're generating here don't
// have GUSI in its name.                                                  
//                                                                         
// <pthread.h>=                                                            
#ifndef _PTHREAD_H_
#define _PTHREAD_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/time.h>

__BEGIN_DECLS
// \section{Definition of Pthread data types}                              
//                                                                         
// I used to be fairly cavalier about exposing internal GUSI data structures, 
// on second thought this was not a good idea. To keep C happy, we define  
// [[struct]] wrappers for what ultimately will mostly be classes.         
//                                                                         
// <Pthread data types>=                                                   
typedef struct GUSIPThread *	pthread_t;
// A [[pthread_attr_t]] collects thread creation attributes. This is implemented
// as a pointer so it's easier to change the size of the underlying data structure.
//                                                                         
// <Pthread data types>=                                                   
typedef struct GUSIPThreadAttr	* pthread_attr_t;
// A [[pthread_key_t]] is a key to look up thread specific data.           
//                                                                         
// <Pthread data types>=                                                   
typedef struct GUSIPThreadKey *	pthread_key_t;
// A [[pthread_once_t]] registers whether some routine has run once. It must always
// be statically initialized to [[PTHREAD_ONCE_INIT]] (Although in our implementation,
// it doesn't matter).                                                     
//                                                                         
// <Pthread data types>=                                                   
typedef char	pthread_once_t;

enum {
	PTHREAD_ONCE_INIT = 0
};
// A [[pthread_mutex_t]] is a mutual exclusion variable, implemented as a pointer
// to a [[GUSIContextQueue]]. For initialization convenience, a 0 value means 
// an unlocked mutex. No attributes are supported so far.                  
//                                                                         
// <Pthread data types>=                                                   
typedef struct GUSIPThreadMutex * 	pthread_mutex_t;
typedef void *						pthread_mutexattr_t;

#define	PTHREAD_MUTEX_INITIALIZER	0
// A [[pthread_cond_t]] is a condition variable, which looks rather similar
// to a mutex, but has different semantics. No attributes are supported so far.
//                                                                         
// <Pthread data types>=                                                   
typedef struct GUSIPThreadCond * 	pthread_cond_t;
typedef void *			   			pthread_condattr_t;

#define PTHREAD_COND_INITIALIZER	0
// [[pthread_attr_init]] initializes an attribute object with the          
// default values.                                                         
//                                                                         
// <Pthread function declarations>=                                        
int pthread_attr_init(pthread_attr_t * attr);
// [[pthread_attr_destroy]] destroys an attribute object.                  
//                                                                         
// <Pthread function declarations>=                                        
int pthread_attr_destroy(pthread_attr_t * attr);
// The detach state defines whether a thread will be defined joinable or   
// detached.                                                               
//                                                                         
// <Pthread function declarations>=                                        
enum {
	PTHREAD_CREATE_JOINABLE,
	PTHREAD_CREATE_DETACHED
};

int pthread_attr_getdetachstate(const pthread_attr_t * attr, int * state);
int pthread_attr_setdetachstate(pthread_attr_t * attr, int state);
// The stack size defines how much stack space will be allocated. Stack overflows
// typically lead to utterly nasty crashes.                                
//                                                                         
// <Pthread function declarations>=                                        
int pthread_attr_getstacksize(const pthread_attr_t * attr, size_t * size);
int pthread_attr_setstacksize(pthread_attr_t * attr, size_t size);
// \section{Creation and Destruction of PThreads}                          
//                                                                         
// First, we define wrapper to map the different calling conventions of Pthreads 
// and Macintosh threads.                                                  
//                                                                         
// <Pthread function declarations>=                                        
__BEGIN_DECLS
typedef void * (*GUSIPThreadProc)(void *);
__END_DECLS
// [[pthread_create]] stuffs the arguments in a [[CreateArg]] and creates the
// context.                                                                
//                                                                         
// <Pthread function declarations>=                                        
int pthread_create(
		pthread_t * thread, 
		const pthread_attr_t * attr, GUSIPThreadProc proc, void * arg);
// A thread can either be detached, in which case it will just go away after it's 
// done, or it can be joinable, in which case it will wait for [[pthread_join]]
// to be called.                                                           
//                                                                         
// <Pthread function declarations>=                                        
int pthread_detach(pthread_t thread);
// [[pthread_join]] waits for the thread to die and optionally returns its last
// words.                                                                  
//                                                                         
// <Pthread function declarations>=                                        
int pthread_join(pthread_t thread, void **value);
// [[pthread_exit]] ends the existence of a thread.                        
//                                                                         
// <Pthread function declarations>=                                        
int pthread_exit(void *value);
// \section{Pthread thread specific data}                                  
//                                                                         
// Thread specific data offers a possibility to quickly look up a value that may be 
// different for every thread.                                             
//                                                                         
// [[pthread_key_create]] creates an unique key visible to all threads in a 
// process.                                                                
//                                                                         
// <Pthread function declarations>=                                        
__BEGIN_DECLS
typedef void (*GUSIPThreadKeyDestructor)(void *);
__END_DECLS

int pthread_key_create(pthread_key_t * key, GUSIPThreadKeyDestructor destructor);
// [[pthread_key_delete]] deletes a key, but does not call any destructors.
//                                                                         
// <Pthread function declarations>=                                        
int pthread_key_delete(pthread_key_t key);
// [[pthread_getspecific]] returns the thread specific value for a key.    
//                                                                         
// <Pthread function declarations>=                                        
void * pthread_getspecific(pthread_key_t key);
// [[pthread_setspecific]] sets a new thread specific value for a key.     
//                                                                         
// <Pthread function declarations>=                                        
int pthread_setspecific(pthread_key_t key, void * value);
// \section{Synchronization mechanisms of PThreads}                        
//                                                                         
// Since we're only dealing with cooperative threads, all synchronization  
// mechanisms can be implemented using means that might look naive to a student
// of computer science, but that actually work perfectly well in our case. 
//                                                                         
// We currently don't support mutex and condition variable attributes. To minimize
// the amount of code changes necessary inclients, we support creating and destroying
// them, at least.                                                         
//                                                                         
// <Pthread function declarations>=                                        
int pthread_mutexattr_init(pthread_mutexattr_t * attr);
int pthread_mutexattr_destroy(pthread_mutexattr_t * attr);
// <Pthread function declarations>=                                        
int pthread_mutex_init(pthread_mutex_t * mutex, const pthread_mutexattr_t *);
int pthread_mutex_destroy(pthread_mutex_t * mutex);
// Lock may create the queue if it was allocated statically. Mutexes are implemented
// as a queue of context, with the frontmost context holding the lock. Simple enough,
// isn't it?                                                               
//                                                                         
// <Pthread function declarations>=                                        
int pthread_mutex_lock(pthread_mutex_t * mutex);
// Strangely enough, [[pthread_mutex_trylock]] is much more of a problem if we want 
// to maintain a semblance of scheduling fairness. In particular, we need the [[Yield]]
// in case somebody checks a mutex in a loop with no other yield point.    
//                                                                         
// <Pthread function declarations>=                                        
int pthread_mutex_trylock(pthread_mutex_t * mutex);
// Unlocking pops us off the queue and wakes up the new lock owner.        
//                                                                         
// <Pthread function declarations>=                                        
int pthread_mutex_unlock(pthread_mutex_t * mutex);
// On to condition variable attributes, which we don't really support either.
//                                                                         
// <Pthread function declarations>=                                        
int pthread_condattr_init(pthread_condattr_t * attr);
int pthread_condattr_destroy(pthread_condattr_t * attr);
// Condition variables in some respects work very similar to mutexes.      
//                                                                         
// <Pthread function declarations>=                                        
int pthread_cond_init(pthread_cond_t * cond, const pthread_condattr_t *);
int pthread_cond_destroy(pthread_cond_t * cond);
// [[pthread_cond_wait]] releases the mutex, sleeps on the condition variable,
// and reacquires the mutex.                                               
//                                                                         
// <Pthread function declarations>=                                        
int pthread_cond_wait(pthread_cond_t * cond, pthread_mutex_t * mutex);
// [[pthread_cond_timedwait]] adds a timeout value (But it still could block
// indefinitely trying to reacquire the mutex). Note that the timeout specifies
// an absolute wakeup time, not an interval.                               
//                                                                         
// <Pthread function declarations>=                                        
int pthread_cond_timedwait(
		pthread_cond_t * cond, pthread_mutex_t * mutex, 
		const struct timespec * patience);
// [[pthread_cond_signal]] wakes up a context from the queue. Since we typically
// still hold the associated mutex, it would be a bad idea (though not a disastrous
// one) to put a yield in here.                                            
//                                                                         
// <Pthread function declarations>=                                        
int pthread_cond_signal(pthread_cond_t * cond);
// [[pthread_cond_broadcast]] wakes up a the entire queue (but only one context
// will get the mutex).                                                    
//                                                                         
// <Pthread function declarations>=                                        
int pthread_cond_broadcast(pthread_cond_t * cond);
// \section{Pthread varia}                                                 
//                                                                         
// [[pthread_self]] returns the current thread.                            
//                                                                         
// <Pthread function declarations>=                                        
pthread_t pthread_self(void);
// [[pthread_equal]] compares two thread handles.                          
//                                                                         
// <Pthread function declarations>=                                        
int pthread_equal(pthread_t t1, pthread_t t2);
// [[pthread_once]] calls a routines, guaranteeing that it will be called exactly
// once per process.                                                       
//                                                                         
// <Pthread function declarations>=                                        
__BEGIN_DECLS
typedef void (*GUSIPThreadOnceProc)(void);
__END_DECLS

int pthread_once(pthread_once_t * once_block, GUSIPThreadOnceProc proc);
__END_DECLS

#endif /* _PTHREAD_H_ */
