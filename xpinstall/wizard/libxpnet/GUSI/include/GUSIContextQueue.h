// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSIContext.nw		-	Thread and Process structures               
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSIContextQueue.h,v $
// % Revision 1.1  2001/03/11 22:33:41  sgehani%netscape.com
// % First Checked In.
// %                                         
// % Revision 1.9  2001/01/17 08:45:13  neeri                              
// % Improve memory allocation safety somewhat                             
// %                                                                       
// % Revision 1.8  2000/05/23 06:58:03  neeri                              
// % Improve formatting                                                    
// %                                                                       
// % Revision 1.7  2000/03/15 07:22:06  neeri                              
// % Enforce alignment choices                                             
// %                                                                       
// % Revision 1.6  1999/08/26 05:45:00  neeri                              
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.5  1999/05/30 03:09:29  neeri                              
// % Added support for MPW compilers                                       
// %                                                                       
// % Revision 1.4  1999/03/17 09:05:06  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// % Revision 1.3  1998/08/02 11:20:07  neeri                              
// % Fixed some typos                                                      
// %                                                                       
// % Revision 1.2  1998/08/01 21:32:02  neeri                              
// % About ready for 2.0a1                                                 
// %                                                                       
// % Revision 1.1  1998/01/25 21:02:43  neeri                              
// % Engine implemented, except for signals & scheduling                   
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{Context Queues}                                                
//                                                                         
// At all times through its existence, a [[GUSIContext]] will exist in various
// queues: A queue of all contexts, queues of contexts waiting on a socket 
// event, a mutex, or a condition variable, and so on. Since a context is often
// in several queues simultaneously, it's better to define queues non-intrusively.
//                                                                         
// <GUSIContextQueue.h>=                                                   
#ifndef _GUSIContextQueue_
#define _GUSIContextQueue_

#ifndef GUSI_SOURCE

typedef struct GUSIContextQueue GUSIContextQueue;
#else

#include <stdlib.h>

// \section{Definition of context queues}                                  
//                                                                         
// We'd like to avoid having to include \texttt{GUSIContext} here, for reasons that
// should be rather obvious.                                               
//                                                                         
// <Name dropping for file GUSIContextQueue>=                              
class GUSIContext;

#include <ConditionalMacros.h>

#if PRAGMA_STRUCT_ALIGN
#pragma options align=native
#endif

// The class [[GUSIContextQueue]] tries to present an interface that is a subset of
// what C++ standard library list template classes offer.                  
//                                                                         
// <Definition of class [[GUSIContextQueue]]>=                             
class GUSIContextQueue {
public:
	GUSIContextQueue();
	~GUSIContextQueue();
	
	bool			empty();
	GUSIContext *	front() const;
	GUSIContext *	back() const;
	void			push_front(GUSIContext * context);
	void			push_back(GUSIContext * context);
	void			push(GUSIContext * context) 		{ push_back(context); }
	void			pop_front();
	void			pop() 								{ pop_front(); }
	void			remove(GUSIContext * context);
	
	void			Wakeup();

	// We define a forward iterator, but no reverse iterator.                  
 //                                                                         
 // <Define [[iterator]] for [[GUSIContextQueue]]>=                         
 struct element;
 class iterator {
 	friend class GUSIContextQueue;
 public:
 	iterator & operator++();
 	iterator operator++(int);
 	bool operator==(const iterator other) const;
 	GUSIContext * operator*();
 	GUSIContext * operator->();
 private:
 	// A [[GUSIContextQueue::iterator]] is just a wrapper for a                
  // [[GUSIContextQueue::element]].                                          
  //                                                                         
  // <Privatissima of [[GUSIContextQueue::iterator]]>=                       
  element *	fCurrent;

  iterator(element * elt) : fCurrent(elt) {}
  iterator()				: fCurrent(0)   {}
 };

 iterator 		begin();
 iterator 		end();
private:
	// \section{Implementation of context queues}                              
 //                                                                         
 // Efficiency of context queues is quite important, so we provide a custom 
 // allocator for queue elements.                                           
 //                                                                         
 // <Privatissima of [[GUSIContextQueue]]>=                                 
 struct element {
 	GUSIContext *	fContext;
 	element *		fNext;
 	
 	element(GUSIContext * context, element * next = 0) 
 		: fContext(context), fNext(next) {}
 	void * operator new(size_t);
 	void   operator delete(void *, size_t);
 private:
 	// Elements are allocated in blocks of increasing size.                    
  //                                                                         
  // <Privatissima of [[GUSIContextQueue::element]]>=                        
  struct header {
  	short	fFree;
  	short 	fMax;
  	header *fNext;
  };
  static header *	sBlocks;
 };
 // A [[GUSIContextQueue]] is a single linked list with a separate back pointer.
 //                                                                         
 // <Privatissima of [[GUSIContextQueue]]>=                                 
 element *	fFirst;
 element *	fLast;
};

#if PRAGMA_STRUCT_ALIGN
#pragma options align=reset
#endif

// <Inline member functions for class [[GUSIContextQueue]]>=               
inline GUSIContextQueue::GUSIContextQueue() 
	: fFirst(0), fLast(0)
{
}
// None of the member functions are very large, so we'll inline them.      
//                                                                         
// <Inline member functions for class [[GUSIContextQueue]]>=               
inline bool GUSIContextQueue::empty()
{
	return !fFirst;
}

inline GUSIContext * GUSIContextQueue::front() const
{
	return fFirst ? fFirst->fContext : reinterpret_cast<GUSIContext *>(0);
}

inline GUSIContext * GUSIContextQueue::back() const
{
	return fLast ? fLast->fContext : reinterpret_cast<GUSIContext *>(0);
}

inline void GUSIContextQueue::push_front(GUSIContext * context)
{
	fFirst = new element(context, fFirst);
	if (!fLast)
		fLast = fFirst;
}

inline void GUSIContextQueue::pop_front()
{
	if (element * e = fFirst) {
		if (!(fFirst = fFirst->fNext))
			fLast = 0;
		delete e;
	}
}
// The constructors are not public, so only [[begin]] and [[end]] call them.
//                                                                         
// <Inline member functions for class [[GUSIContextQueue]]>=               
inline GUSIContextQueue::iterator GUSIContextQueue::begin()
{
	return iterator(fFirst);
}

inline GUSIContextQueue::iterator GUSIContextQueue::end()
{
	return iterator();
}
// <Inline member functions for class [[GUSIContextQueue]]>=               
inline GUSIContextQueue::iterator & GUSIContextQueue::iterator::operator++()
{
	fCurrent = fCurrent->fNext;
	
	return *this;
}

inline GUSIContextQueue::iterator GUSIContextQueue::iterator::operator++(int)
{
	GUSIContextQueue::iterator it(*this);
	fCurrent = fCurrent->fNext;
	
	return it;
}

inline bool GUSIContextQueue::iterator::operator==(const iterator other) const
{
	return fCurrent == other.fCurrent;
}

inline GUSIContext * GUSIContextQueue::iterator::operator*()
{
	return fCurrent->fContext;
}

inline GUSIContext * GUSIContextQueue::iterator::operator->()
{
	return fCurrent->fContext;
}
#endif /* GUSI_SOURCE */

#endif /* _GUSIContextQueue_ */
