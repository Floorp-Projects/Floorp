// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSIBuffer.nw		-	Buffering for GUSI                           
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSIBuffer.h,v $
// % Revision 1.1  2001/03/11 22:33:31  sgehani%netscape.com
// % First Checked In.
// %                                               
// % Revision 1.20  2001/01/17 08:33:14  neeri                             
// % Need to set fOldBuffer to nil after deleting                          
// %                                                                       
// % Revision 1.19  2000/10/16 04:34:22  neeri                             
// % Releasing 2.1.2                                                       
// %                                                                       
// % Revision 1.18  2000/05/23 06:53:14  neeri                             
// % Improve formatting                                                    
// %                                                                       
// % Revision 1.17  2000/03/15 07:22:06  neeri                             
// % Enforce alignment choices                                             
// %                                                                       
// % Revision 1.16  1999/09/09 07:19:18  neeri                             
// % Fix read-ahead switch-off                                             
// %                                                                       
// % Revision 1.15  1999/08/26 05:44:59  neeri                             
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.14  1999/06/30 07:42:05  neeri                             
// % Getting ready to release 2.0b3                                        
// %                                                                       
// % Revision 1.13  1999/05/30 03:09:29  neeri                             
// % Added support for MPW compilers                                       
// %                                                                       
// % Revision 1.12  1999/03/17 09:05:05  neeri                             
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// % Revision 1.11  1998/11/22 23:06:50  neeri                             
// % Releasing 2.0a4 in a hurry                                            
// %                                                                       
// % Revision 1.10  1998/10/25 11:28:43  neeri                             
// % Added MSG_PEEK support, recursive locks.                              
// %                                                                       
// % Revision 1.9  1998/08/02 12:31:36  neeri                              
// % Another typo                                                          
// %                                                                       
// % Revision 1.8  1998/08/02 11:20:06  neeri                              
// % Fixed some typos                                                      
// %                                                                       
// % Revision 1.7  1998/01/25 20:53:51  neeri                              
// % Engine implemented, except for signals & scheduling                   
// %                                                                       
// % Revision 1.6  1997/11/13 21:12:08  neeri                              
// % Fall 1997                                                             
// %                                                                       
// % Revision 1.5  1996/12/22 19:57:55  neeri                              
// % TCP streams work                                                      
// %                                                                       
// % Revision 1.4  1996/12/16 02:16:02  neeri                              
// % Add Size(), make inlines inline, use BlockMoveData                    
// %                                                                       
// % Revision 1.3  1996/11/24  13:00:26  neeri                             
// % Fix comment leaders                                                   
// %                                                                       
// % Revision 1.2  1996/11/24  12:52:05  neeri                             
// % Added GUSIPipeSockets                                                 
// %                                                                       
// % Revision 1.1.1.1  1996/11/03  02:43:32  neeri                         
// % Imported into CVS                                                     
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{Buffering for GUSI}                                            
//                                                                         
// This section defines four classes that handle buffering for GUSI:       
// [[GUSIScatterer]], [[GUSIGatherer]], and their common ancestor          
// [[GUSIScattGath]] convert between [[iovecs]] and simple buffers in the  
// absence of specialized communications routines. A [[GUSIRingBuffer]]    
// mediates between a producer and a consumer, one of which is typically   
// normal code and the other interrupt level code.                         
//                                                                         
//                                                                         
// <GUSIBuffer.h>=                                                         
#ifndef _GUSIBuffer_
#define _GUSIBuffer_

#ifdef GUSI_SOURCE

#include <sys/types.h>
#include <sys/uio.h>

#include <MacTypes.h>

#include "GUSIDiag.h"
#include "GUSIBasics.h"

#include <ConditionalMacros.h>

#if PRAGMA_STRUCT_ALIGN
#pragma options align=native
#endif

// \section{Definition of scattering/gathering}                            
//                                                                         
// A [[GUSIScattGath]] translates between an array of [[iovecs]] and a simple buffer,
// allocating scratch space if necessary.                                  
//                                                                         
// <Definition of class [[GUSIScattGath]]>=                                
class GUSIScattGath	{
protected:
	// On constructing a [[GUSIScattGath]], we pass an array of [[iovecs]]. For the
 // simpler functions, a variant with a single [[buffer]] and [[length]] is also
 // available.                                                              
 //                                                                         
 // <Constructor and destructor for [[GUSIScattGath]]>=                     
 GUSIScattGath(const iovec *iov, int count, bool gather);
 GUSIScattGath(void * buffer, size_t length, bool gather);
 virtual ~GUSIScattGath();
public:
	// The [[iovec]], the buffer and its length are then available for public scrutinity.
 // Copy constructor and assignment both are a bit nontrivial.              
 //                                                                         
 // <Public interface to [[GUSIScattGath]]>=                                
 const iovec *	IOVec()  const;
 int				Count()  const;
 void *			Buffer() const;
 operator 		void *() const;
 int				Length() const;
 int				SetLength(int len) const;
 void			operator=(const GUSIScattGath & other);
 GUSIScattGath(const GUSIScattGath & other);
private:
	// \section{Implementation of scattering/gathering}                        
 //                                                                         
 // A [[GUSIScattGath]] always consists of [[fIo]], an array of [[iovecs]], [[fCount]],
 // the number of sections in the array, and [[fLen]], the total size of the data area.
 // If [[fCount]] is 1, [[fBuf]] will be a copy of the pointer to the single section. If 
 // [[fCount]] is greater than 1, [[fScratch]] will contain a [[Handle]] to a scratch 
 // area of size [[len]] and [[fBuf]] will contain [[*scratch]]. If the object was
 // constructed without providing an [[iovec]] array, [[fTrivialIo]] will be set up
 // to hold one.                                                            
 //                                                                         
 // <Privatissima of [[GUSIScattGath]]>=                                    
 const iovec *	fIo;
 iovec			fTrivialIo;		
 mutable int		fCount;
 mutable Handle	fScratch;
 mutable void *	fBuf;
 mutable int		fLen;
 bool			fGather;
};
// A [[GUSIScatterer]] distributes the contents of a buffer over an array of
// [[iovecs]].                                                             
//                                                                         
// <Definition of class [[GUSIScatterer]]>=                                
class GUSIScatterer : public GUSIScattGath {
public:
	GUSIScatterer(const iovec *iov, int count) 
		: GUSIScattGath(iov, count, false) {}
	GUSIScatterer(void * buffer, size_t length) 
		: GUSIScattGath(buffer, length, false) {}
		
	GUSIScatterer & operator=(const GUSIScatterer & other)
		{ *static_cast<GUSIScattGath *>(this) = other; return *this; }
};
// A [[GUSIGatherer]] collects the contents of an array of [[iovecs]] into a single
// buffer.                                                                 
//                                                                         
// <Definition of class [[GUSIGatherer]]>=                                 
class GUSIGatherer : public GUSIScattGath {
public:
	GUSIGatherer(const struct iovec *iov, int count)
		: GUSIScattGath(iov, count, true) {}
	GUSIGatherer(const void * buffer, size_t length)
		: GUSIScattGath(const_cast<void *>(buffer), length, true) {}
		
	GUSIGatherer & operator=(const GUSIGatherer & other)
		{ *static_cast<GUSIScattGath *>(this) = other; return *this; }
};

// \section{Definition of ring buffering}                                  
//                                                                         
// A [[GUSIRingBuffer]] typically has on one side a non-preeemptive piece of code
// and on the other side a piece of interrupt code. To transfer data from and to
// the buffer, two interfaces are available: A direct interface that transfers 
// memory, and an indirect interface that allocates memory regions and then
// has OS routines transfer data from or to them                           
//                                                                         
// <Definition of class [[GUSIRingBuffer]]>=                               
class GUSIRingBuffer {
public:
	// On construction of a [[GUSIRingBuffer]], a buffer of the specified size is 
 // allocated and not released until destruction. [[operator void*]] may be used
 // to determine whether construction was successful.                       
 //                                                                         
 // <Constructor and destructor for [[GUSIRingBuffer]]>=                    
 GUSIRingBuffer(size_t bufsiz);
 ~GUSIRingBuffer();
 operator void*();
	// The direct interface to [[GUSIRingBuffer]] is straightforward: [[Produce]] copies
 // memory into the buffer, [[Consume]] copies memory from the buffer, [[Free]]
 // determines how much space there is for [[Produce]] and [[Valid]] determines
 // how much space there is for [[Consume]].                                
 //                                                                         
 // <Direct interface for [[GUSIRingBuffer]]>=                              
 void	Produce(void * from, size_t & len);
 void 	Produce(const GUSIGatherer & gather, size_t & len, size_t & offset);
 void 	Produce(const GUSIGatherer & gather, size_t & len);
 void	Consume(void * to, size_t & len);
 void 	Consume(const GUSIScatterer & scatter, size_t & len, size_t & offset);
 void 	Consume(const GUSIScatterer & scatter, size_t & len);
 size_t	Free();	
 size_t	Valid();
	// [[ProduceBuffer]] tries to find in the ring buffer a contiguous free block of
 // memory of the specified size [[len]] or otherwise the biggest available free 
 // block, returns a pointer to it and sets [[len]] to its length. [[ValidBuffer]]
 // specifies that the next [[len]] bytes of the ring buffer now contain valid data.
 //                                                                         
 // [[ConsumeBuffer]] returns a pointer to the next valid byte and sets [[len]] to 
 // the minimum of the number of contiguous valid bytes and the value of len on 
 // entry. [[FreeBuffer]] specifies that the next [[len]] bytes of the ring 
 // buffer were consumed and are no longer needed.                          
 //                                                                         
 // <Indirect interface for [[GUSIRingBuffer]]>=                            
 void *	ProduceBuffer(size_t & len);
 void *	ConsumeBuffer(size_t & len);
 void	ValidBuffer(void * buffer, size_t len);
 void 	FreeBuffer(void * buffer, size_t len);
	// Before the nonpreemptive partner changes any of the buffer's data structures,
 // the [[GUSIRingBuffer]] member functions call [[Lock]], and after the change is
 // complete, they call [[Release]]. An interrupt level piece of code before 
 // changing any data structures has to determine whether the buffer is locked by
 // calling [[Locked]]. If the buffer is locked or otherwise in an unsuitable state,
 // the code can specify a procedure to be called during the next [[Release]] by 
 // calling [[Defer]]. A deferred procedure should call [[ClearDefer]] to avoid
 // getting activated again at the next opportunity.                        
 //                                                                         
 // <Synchronization support for [[GUSIRingBuffer]]>=                       
 void 		Lock();
 void 		Release();
 bool		Locked();
 typedef void (*Deferred)(void *);
 void		Defer(Deferred def, void * ar);
 void		ClearDefer();
	// It is possible to switch buffer sizes during the existence of a buffer, but we
 // have to be somewhat careful, since some asynchronous call may still be writing
 // into the old buffer. [[PurgeBuffers]], called at safe times, cleans up old
 // buffers.                                                                
 //                                                                         
 // <Buffer switching for [[GUSIRingBuffer]]>=                              
 void		SwitchBuffer(size_t bufsiz);
 size_t		Size();
 void		PurgeBuffers();
	// Sometimes, it's necessary to do nondestructive reads, a task complex enough to
 // warrant its own class.                                                  
 //                                                                         
 // <Definition of class [[GUSIRingBuffer::Peeker]]>=                       
 class Peeker {
 public:
 	Peeker(GUSIRingBuffer & buffer);
 	~Peeker();
 	
 	void	Peek(void * to, size_t & len);
 	void 	Peek(const GUSIScatterer & scatter, size_t & len);
 private:
 	// A [[GUSIRingBuffer::Peeker]] has to keep its associated [[GUSIRingBuffer]] locked during
  // its entire existence.                                                   
  //                                                                         
  // <Privatissima of [[GUSIRingBuffer::Peeker]]>=                           
  GUSIRingBuffer &	fTopBuffer;
  GUSIRingBuffer *	fCurBuffer;
  Ptr					fPeek;
  // The core routine for reading is [[PeekBuffer]] which automatically advances the
  // peeker as well.                                                         
  //                                                                         
  // <Privatissima of [[GUSIRingBuffer::Peeker]]>=                           
  void *	PeekBuffer(size_t & len);
 };
 friend class Peeker;

 void	Peek(void * to, size_t & len);
 void 	Peek(const GUSIScatterer & scatter, size_t & len);
private:
	// \section{Implementation of ring buffering}                              
 //                                                                         
 // The buffer area of a ring buffer extends between [[fBuffer]] and [[fEnd]]. [[fValid]] 
 // contains the number of valid bytes, while [[fFree]] and [[fSpare]] (Whose purpose
 // will be explained later) sum up to the number of free bytes. [[fProduce]] points at the 
 // next free byte, while [[fConsume]] points at the next valid byte. [[fInUse]] 
 // indicates that an asynchronous call might be writing into the buffer.   
 //                                                                         
 // <Privatissima of [[GUSIRingBuffer]]>=                                   
 Ptr		fBuffer;
 Ptr		fEnd;
 Ptr 	fConsume;
 Ptr		fProduce;
 size_t	fFree;
 size_t	fValid;
 size_t	fSpare;
 bool	fInUse;
 // The relationships between the various pointers are captured by [[Invariant]] which
 // uses the auxiliary function [[Distance]] to determine the distance between two
 // pointers in the presence of wrap around areas.                          
 //                                                                         
 // <Privatissima of [[GUSIRingBuffer]]>=                                   
 bool	Invariant();
 size_t	Distance(Ptr from, Ptr to);
 // The lock mechanism relies on [[fLocked]], and the deferred procedure and its argument
 // are stored in [[fDeferred]] and [[fDeferredArg]].                       
 //                                                                         
 // <Privatissima of [[GUSIRingBuffer]]>=                                   
 int			fLocked;
 Deferred	fDeferred;
 void *		fDeferredArg;
 // We only switch the next time the buffer is empty, so we are prepared to create
 // the new buffer dynamically and forward requests to it for a while.      
 //                                                                         
 // <Privatissima of [[GUSIRingBuffer]]>=                                   
 GUSIRingBuffer * 	fNewBuffer;
 GUSIRingBuffer * 	fOldBuffer;
 void 				ObsoleteBuffer();
 // The scatter/gather variants of [[Produce]] and [[Consume]] rely on a common
 // strategy.                                                               
 //                                                                         
 // <Privatissima of [[GUSIRingBuffer]]>=                                   
 void IterateIOVec(const GUSIScattGath & sg, size_t & len, size_t & offset, bool produce);
};

#if PRAGMA_STRUCT_ALIGN
#pragma options align=reset
#endif

// Clients need readonly access to the buffer address and read/write access to the length.
// [[operator void*]] server to check whether the [[GUSIScattGath]] was constructed 
// successfully.                                                           
//                                                                         
// <Inline member functions for class [[GUSIScattGath]]>=                  
inline const iovec * GUSIScattGath::IOVec()	 const		
	{	return	fIo;		}
inline int			 GUSIScattGath::Count()	 const		
	{	return	fCount;		}
inline               GUSIScattGath::operator  void *() const
	{	return	Buffer();	}
inline int			 GUSIScattGath::Length() const		
	{	return	fLen;		}
inline int			 GUSIScattGath::SetLength(int len) const
	{	return	GUSI_MUTABLE(GUSIScattGath, fLen) = len;	}
// <Inline member functions for class [[GUSIRingBuffer]]>=                 
inline void GUSIRingBuffer::Produce(const GUSIGatherer & gather, size_t & len, size_t & offset)
{
	IterateIOVec(gather, len, offset, true);
}

inline void GUSIRingBuffer::Consume(const GUSIScatterer & scatter, size_t & len, size_t & offset)
{
	IterateIOVec(scatter, len, offset, false);
}

inline void GUSIRingBuffer::Produce(const GUSIGatherer & gather, size_t & len)
{
	size_t offset = 0;
	
	IterateIOVec(gather, len, offset, true);
}

inline void GUSIRingBuffer::Consume(const GUSIScatterer & scatter, size_t & len)
{
	size_t offset = 0;
	
	IterateIOVec(scatter, len, offset, false);
}
// The lock support is rather straightforward.                             
//                                                                         
// <Inline member functions for class [[GUSIRingBuffer]]>=                 
inline void 	GUSIRingBuffer::Lock()			{ 	++fLocked;				}
inline bool		GUSIRingBuffer::Locked()		{	return (fLocked!=0);	}
inline void		GUSIRingBuffer::ClearDefer()	{	fDeferred	=	nil;	}
inline void 	GUSIRingBuffer::Release()		
{ 
	GUSI_CASSERT_INTERNAL(fLocked > 0);
	if (--fLocked <= 0 && fDeferred)
		fDeferred(fDeferredArg);
}
inline void		GUSIRingBuffer::Defer(Deferred def, void * ar)
{
	fDeferred 		= 	def;
	fDeferredArg	=	ar;
}
// The size is stored only implicitely.                                    
//                                                                         
// <Inline member functions for class [[GUSIRingBuffer]]>=                 
inline size_t GUSIRingBuffer::Size()				{	return fEnd - fBuffer;	}
// <Inline member functions for class [[GUSIRingBuffer]]>=                 
inline void GUSIRingBuffer::Peek(void * to, size_t & len)
{
	Peeker	peeker(*this);
	
	peeker.Peek(to, len);
}

inline void GUSIRingBuffer::Peek(const GUSIScatterer & scatter, size_t & len)
{
	Peeker	peeker(*this);
	
	peeker.Peek(scatter, len);
}
#endif /* GUSI_SOURCE */

#endif /* _GUSIBuffer_ */
