// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSISpecific.nw		-	Thread specific variables                  
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSISpecific.h,v $
// % Revision 1.1  2001/03/11 22:38:39  sgehani%netscape.com
// % First Checked In.
// %                                             
// % Revision 1.9  2000/10/16 04:11:21  neeri                              
// % Plug memory leak                                                      
// %                                                                       
// % Revision 1.8  2000/03/15 07:22:07  neeri                              
// % Enforce alignment choices                                             
// %                                                                       
// % Revision 1.7  1999/08/26 05:45:10  neeri                              
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.6  1999/05/30 03:09:31  neeri                              
// % Added support for MPW compilers                                       
// %                                                                       
// % Revision 1.5  1999/04/29 04:58:20  neeri                              
// % Fix key destruction bug                                               
// %                                                                       
// % Revision 1.4  1999/03/17 09:05:13  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// % Revision 1.3  1998/10/11 16:45:25  neeri                              
// % Ready to release 2.0a2                                                
// %                                                                       
// % Revision 1.2  1998/08/01 21:32:11  neeri                              
// % About ready for 2.0a1                                                 
// %                                                                       
// % Revision 1.1  1998/01/25 21:02:52  neeri                              
// % Engine implemented, except for signals & scheduling                   
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{Thread Specific Variables}                                     
//                                                                         
// It is often useful to have variables which maintain a different value   
// for each process. The [[GUSISpecific]] class implements such a mechanism
// in a way that is easily mappable to pthreads.                           
//                                                                         
//                                                                         
// <GUSISpecific.h>=                                                       
#ifndef _GUSISpecific_
#define _GUSISpecific_

#ifndef GUSI_SOURCE

typedef struct GUSISpecific	GUSISpecific;

#else

#include <Types.h>
#include <ConditionalMacros.h>

#if PRAGMA_STRUCT_ALIGN
#pragma options align=native
#endif

// \section{Definition of Thread Specific Variables}                       
//                                                                         
// A [[GUSISpecific]] instance contains a variable ID and a per-process    
// destructor.                                                             
//                                                                         
// <Definition of class [[GUSISpecific]]>=                                 
extern "C"  {
	typedef void (*GUSISpecificDestructor)(void *);
}

class GUSISpecific {
	friend class GUSISpecificTable;
public:
	GUSISpecific(GUSISpecificDestructor destructor = nil);
	~GUSISpecific();
	
	void 					Destruct(void * data);
private:
	GUSISpecificDestructor	fDestructor;
	unsigned				fID;
	static unsigned			sNextID;
};
// A [[GUSIContext]] contains a [[GUSISpecificTable]] storing the values of all
// thread specific variables defined for this thread.                      
//                                                                         
// <Definition of class [[GUSISpecificTable]]>=                            
class GUSISpecificTable {
	friend class GUSISpecific;
public:
	GUSISpecificTable();
	~GUSISpecificTable();
	void * 	GetSpecific(const GUSISpecific * key) const;
	void	SetSpecific(const GUSISpecific * key, void * value);
	void 	DeleteSpecific(const GUSISpecific * key);
private:
	static void Register(GUSISpecific * key);
	static void Destruct(GUSISpecific * key);
	
	// We store a [[GUSISpecificTable]] as a contiguous range of IDs.          
 //                                                                         
 // <Privatissima of [[GUSISpecificTable]]>=                                
 void ***				fValues;
 unsigned				fAlloc;

 bool					Valid(const GUSISpecific * key) const;
 // All keys are registered in a global table.                              
 //                                                                         
 // <Privatissima of [[GUSISpecificTable]]>=                                
 static GUSISpecific ***	sKeys;
 static unsigned			sKeyAlloc;
};
// To simplify having a particular variable assume a different instance in every
// thread, we define the [[GUSISpecificData]] template.                    
//                                                                         
// <Definition of template [[GUSISpecificData]]>=                          
template <class T, GUSISpecificDestructor D> 
class GUSISpecificData {
public:
	GUSISpecificData() : fKey(D)    		{										}
	T & operator*() 						{ return *get(); 						}
	T * operator->()						{ return get();  						}
	
	const GUSISpecific *	Key() const 	{ return &fKey; 						}
	T * get(GUSISpecificTable * table);
	T * get()								{ return get(GUSIContext::Current()); 	}
protected:
	GUSISpecific	fKey;
};

template <class T, GUSISpecificDestructor D> 
	T * GUSISpecificData<T,D>::get(GUSISpecificTable * table)
{
	void *				data  = table->GetSpecific(&fKey);
	
	if (!data)
		table->SetSpecific(&fKey, data = new T);
	
	return static_cast<T *>(data);
}

#if PRAGMA_STRUCT_ALIGN
#pragma options align=reset
#endif

// <Inline member functions for class [[GUSISpecific]]>=                   
inline GUSISpecific::GUSISpecific(GUSISpecificDestructor destructor)
	: fDestructor(destructor), fID(sNextID++)
{
	GUSISpecificTable::Register(this);
}

inline GUSISpecific::~GUSISpecific()
{
	GUSISpecificTable::Destruct(this);
}

inline void GUSISpecific::Destruct(void * data)
{
	if (fDestructor)
		fDestructor(data);
}
// <Inline member functions for class [[GUSISpecificTable]]>=              
inline bool GUSISpecificTable::Valid(const GUSISpecific * key) const
{
	return key && key->fID < fAlloc;
}
// <Inline member functions for class [[GUSISpecificTable]]>=              
inline GUSISpecificTable::GUSISpecificTable()
	: fValues(nil), fAlloc(0)
{	
}
// <Inline member functions for class [[GUSISpecificTable]]>=              
inline void * GUSISpecificTable::GetSpecific(const GUSISpecific * key) const
{
	if (Valid(key))
		return fValues[0][key->fID];
	else
		return nil;
}

#endif /* GUSI_SOURCE */

#endif /* _GUSISpecific_ */
