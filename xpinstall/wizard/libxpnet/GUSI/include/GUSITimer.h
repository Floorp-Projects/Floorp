// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSITimer.nw		-	Timing functions                              
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSITimer.h,v $
// % Revision 1.1  2001/03/11 22:38:42  sgehani%netscape.com
// % First Checked In.
// %                                                
// % Revision 1.12  2001/01/17 08:48:04  neeri                             
// % Introduce Expired(), Reset()                                          
// %                                                                       
// % Revision 1.11  2000/10/29 18:36:32  neeri                             
// % Fix time_t signedness issues                                          
// %                                                                       
// % Revision 1.10  2000/06/12 04:24:50  neeri                             
// % Fix time, localtime, gmtime                                           
// %                                                                       
// % Revision 1.9  2000/05/23 07:24:58  neeri                              
// % Improve formatting                                                    
// %                                                                       
// % Revision 1.8  2000/03/15 07:22:07  neeri                              
// % Enforce alignment choices                                             
// %                                                                       
// % Revision 1.7  1999/11/15 07:20:18  neeri                              
// % Safe context setup                                                    
// %                                                                       
// % Revision 1.6  1999/08/26 05:45:10  neeri                              
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.5  1999/08/02 07:02:46  neeri                              
// % Support for asynchronous errors and other socket options              
// %                                                                       
// % Revision 1.4  1999/07/07 04:17:43  neeri                              
// % Final tweaks for 2.0b3                                                
// %                                                                       
// % Revision 1.3  1999/06/28 06:08:46  neeri                              
// % Support flexible timer classes                                        
// %                                                                       
// % Revision 1.2  1999/05/30 03:06:21  neeri                              
// % Fixed various bugs in cleanup and wakeup                              
// %                                                                       
// % Revision 1.1  1999/03/17 09:05:14  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{Timing functions}                                              
//                                                                         
// This section defines mechanisms to measure time. The basic mechanism is 
// [[GUSITimer]] which can wake up a [[GUSIContext]] at some later time.   
//                                                                         
// <GUSITimer.h>=                                                          
#ifndef _GUSITimer_
#define _GUSITimer_

#ifndef GUSI_SOURCE

typedef struct GUSITimer GUSITimer;

#else
#include "GUSISpecific.h"

#include <errno.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/time.h>
#include <inttypes.h>

#include <MacTypes.h>
#include <Timer.h>
#include <Math64.h>

#include <ConditionalMacros.h>

#if PRAGMA_STRUCT_ALIGN
#pragma options align=native
#endif

// \section{Definition of timing}                                          
//                                                                         
// [[GUSITime]] is an universal (if somewhat costly) format for            
// the large variety of timing formats used in MacOS and POSIX.            
//                                                                         
// <Definition of class [[GUSITime]]>=                                     
class GUSITime {
public:
	enum Format {seconds, ticks, msecs, usecs, nsecs};
	
#if !TYPE_LONGLONG
	GUSITime(int32_t  val, Format format);
	GUSITime(uint32_t val, Format format);
#endif
	GUSITime(int64_t val, Format format=nsecs) 	{ Construct(val, format); }
	GUSITime(const timeval & tv);
	GUSITime(const timespec & ts);
	GUSITime(const tm & t);
	GUSITime() {}
	
	int32_t		Get(Format format)		{ return S32Set(Get64(format)); 		}
	uint32_t	UGet(Format format)		
							{ return U32SetU(SInt64ToUInt64(Get64(format)));	}
	int64_t		Get64(Format format);
	
	operator int64_t()		{	return fTime;	}
	operator timeval();
	operator timespec();
	operator tm();
	
	GUSITime GM2LocalTime();
	GUSITime Local2GMTime();
	
	GUSITime & operator +=(const GUSITime & other) 
		{ fTime = S64Add(fTime, other.fTime); return *this; }
	GUSITime & operator -=(const GUSITime & other) 
		{ fTime = S64Subtract(fTime, other.fTime); return *this; }
	
	
	static GUSITime 	Now();
	static timezone	&	Zone();
private:
	void	Construct(int64_t val, Format format);
	time_t	Deconstruct(int64_t & remainder);
	
	int64_t	fTime;
	
	static	int64_t		sTimeOffset;
	static	timezone	sTimeZone;
};

inline GUSITime operator+(const GUSITime & a, const GUSITime & b)
					{ GUSITime t(a); return t+=b; }
inline GUSITime operator-(const GUSITime & a, const GUSITime & b)
					{ GUSITime t(a); return t-=b; }
// A [[GUSITimer]] is a time manager task that wakes up a [[GUSIContext]]. 
//                                                                         
// <Definition of class [[GUSITimer]]>=                                    
#if PRAGMA_STRUCT_ALIGN
#pragma options align=mac68k
#endif
class GUSIContext;

extern "C" void GUSIKillTimers(void * timers);

class GUSITimer : public TMTask {
public:
	GUSITimer(bool wakeup = true, GUSIContext * context = 0);
	virtual ~GUSITimer();
	
	void			Sleep(long ms, bool driftFree = false);
	void			MicroSleep(long us, bool driftFree = false)
										{ Sleep(-us, driftFree);				}
	GUSIContext *	Context()			{ return fQueue->fContext;				}
	GUSITimer *		Next()				{ return fNext;							}
	bool			Primed()			{ return (qType&kTMTaskActive) != 0; 	}
	bool			Expired()			{ return !(qType&kTMTaskActive); 		}
	virtual void	Wakeup();
	void			Kill();
	void			Reset();
	
	struct Queue {
		GUSITimer 	*	fTimer;
		GUSIContext *	fContext;
		
		Queue() : fTimer(0) {}
	};
	
	QElem *	Elem()		{	return reinterpret_cast<QElem *>(&this->qLink); }
protected:
	Queue *			fQueue;
	GUSITimer * 	fNext;
	
	class TimerQueue : public GUSISpecificData<Queue,GUSIKillTimers> {
	public:
		~TimerQueue();
	};
	
	static TimerQueue	sTimerQueue;
	static TimerUPP		sTimerProc;
};
#if PRAGMA_STRUCT_ALIGN
#pragma options align=reset
#endif

#if PRAGMA_STRUCT_ALIGN
#pragma options align=reset
#endif

#endif /* GUSI_SOURCE */

#endif /* _GUSITimer_ */
