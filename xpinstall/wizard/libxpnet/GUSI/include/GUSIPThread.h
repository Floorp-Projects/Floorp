// <GUSIPThread.h>=                                                        
#ifndef _GUSIPThread_
#define _GUSIPThread_

#include "GUSISpecific.h"
#include "GUSIContext.h"
#include "GUSIContextQueue.h"

#include <pthread.h>

// <Implementation of Pthread data types>=                                 
struct GUSIPThread : public GUSIContext {
private:
	GUSIPThread() : GUSIContext(0) {} // Never called
};
// <Implementation of Pthread data types>=                                 
struct GUSIPThreadKey : public GUSISpecific {
	GUSIPThreadKey(GUSIPThreadKeyDestructor destructor) : GUSISpecific(destructor) {}
};
// <Implementation of Pthread data types>=                                 
struct GUSIPThreadMutex : public GUSIContextQueue {
	bool	fPolling;

	GUSIPThreadMutex() : fPolling(false) {}	
};
// <Implementation of Pthread data types>=                                 
struct GUSIPThreadCond : public GUSIContextQueue {
};

#endif /* _GUSIPThread_ */
