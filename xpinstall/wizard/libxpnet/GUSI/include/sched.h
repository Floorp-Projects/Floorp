// <sched.h>=                                                              
#ifndef _SCHED_H_
#define _SCHED_H_

#include <sys/cdefs.h>
/* Required by UNIX 98 */
#include <sys/time.h>

__BEGIN_DECLS
// [[sched_yield]] yields the processor for other runnable threads.        
//                                                                         
// <Sched function declarations>=                                          
int sched_yield();
__END_DECLS

#endif /* _SCHED_H_ */
