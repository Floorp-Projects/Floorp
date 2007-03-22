// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSINetDB.nw		-	Convert between names and adresses            
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSINetDB.h,v $
// % Revision 1.1  2001/03/11 22:37:15  sgehani%netscape.com
// % First Checked In.
// %                                                
// % Revision 1.10  2000/12/23 06:11:55  neeri                             
// % Add SSH service                                                       
// %                                                                       
// % Revision 1.9  2000/05/23 07:10:35  neeri                              
// % Improve formatting                                                    
// %                                                                       
// % Revision 1.8  2000/03/15 07:18:43  neeri                              
// % Fix GUSIBuiltinServiceDB::sServices                                   
// %                                                                       
// % Revision 1.7  1999/11/15 07:23:23  neeri                              
// % Fix gethostname for non-TCP/IP case                                   
// %                                                                       
// % Revision 1.6  1999/08/26 05:45:05  neeri                              
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.5  1999/05/30 03:09:30  neeri                              
// % Added support for MPW compilers                                       
// %                                                                       
// % Revision 1.4  1999/03/17 09:05:10  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// % Revision 1.3  1998/11/22 23:06:58  neeri                              
// % Releasing 2.0a4 in a hurry                                            
// %                                                                       
// % Revision 1.2  1998/10/25 11:33:38  neeri                              
// % Fixed disastrous bug in inet_addr, support alternative NL conventions 
// %                                                                       
// % Revision 1.1  1998/10/11 16:45:20  neeri                              
// % Ready to release 2.0a2                                                
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{Converting Between Names and IP Addresses}                     
//                                                                         
// The [[GUSINetDB]] class coordinates access to the domain name server database.
//                                                                         
// The [[GUSIServiceDB]] class is responsible for a database of TCP/IP service
// name to port number mappings.                                           
//                                                                         
// The [[hostent]] and [[servent]] classes are somewhat inconvenient to set up as
// they reference extra chunks of memory, so we define the wrapper classes 
// [[GUSIhostent]] and [[GUSIservent]].                                    
//                                                                         
// <GUSINetDB.h>=                                                          
#ifndef _GUSINetDB_
#define _GUSINetDB_

#ifdef GUSI_SOURCE
#include "GUSISpecific.h"

#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <ConditionalMacros.h>

#if PRAGMA_STRUCT_ALIGN
#pragma options align=native
#endif

// \section{Definition of [[GUSIhostent]] and [[GUSIservent]]}             
//                                                                         
// A [[GUSIhostent]] may need a lot of data, so we allocate the name data  
// dynamically.                                                            
//                                                                         
// <Definition of class [[GUSIhostent]]>=                                  
class GUSIhostent : public hostent {
public:
	GUSIhostent();
	
	void	Alloc(size_t size);

	char *	fAlias[16];
	char *	fAddressList[16];
	char * 	fName;
	size_t	fAlloc;
	char 	fAddrString[16];
};

extern "C" void GUSIKillHostEnt(void * hostent);

// A [[GUSIservent]] typically will remain more modest in its needs, so the
// data is allocated statically.                                           
//                                                                         
// <Definition of class [[GUSIservent]]>=                                  
class GUSIservent : public servent {
public:
	GUSIservent();
	
	char *	fAlias[8];
	char 	fName[256];	
};
// \section{Definition of [[GUSIServiceDB]]}                               
//                                                                         
// [[GUSIServiceDB]] is a singleton, used as a primitive iterator. The semantics of
// these iterators conform only very superficially to real iterators:      
//                                                                         
// \begin{itemize}                                                         
// \item Only a single instance of the iterator is supported.              
// \item Comparison operators all compare against [[end]], no matter what  
// arguments are passed.                                                   
// \end{itemize}                                                           
//                                                                         
// <Definition of class [[GUSIServiceDB]]>=                                
extern "C" void GUSIKillServiceDBData(void * entry);

class GUSIServiceDB {
public:
	static GUSIServiceDB *	Instance();
	// Iterating is accomplished by a public interface conforming to STL iterator
 // protocols.                                                              
 //                                                                         
 // <Iterating over the [[GUSIServiceDB]]>=                                 
 class iterator {
 public:
 	inline bool 			operator==(const iterator & other);
 	inline bool 			operator!=(const iterator & other);
 	inline iterator &		operator++();
 	inline servent *		operator*();
 };
 inline static iterator	begin();
 inline static iterator	end();
protected:
	static GUSIServiceDB *	sInstance;
							GUSIServiceDB()		{}
	virtual 				~GUSIServiceDB()	{}
	
	friend void GUSIKillServiceDBData(void * entry);
	
	// This interface does not access any data elements in the iterator, but directly 
 // calls through to a private interface in the [[GUSIServiceDB]], which explains
 // the limitations in the iterator implementation.                         
 //                                                                         
 // <Internal iterator protocol of [[GUSIServiceDB]]>=                      
 friend class iterator;

 class Data {
 public:
 	Data() : fCurrent(0) {}
 	
 	servent *	fCurrent;
 	GUSIservent	fServent;
 };
 typedef GUSISpecificData<Data, GUSIKillServiceDBData> SpecificData;
 static 	SpecificData sData;

 virtual void	Reset() = 0;
 virtual void	Next()	= 0;
}; 
// \section{Definition of [[GUSINetDB]]}                                   
//                                                                         
//                                                                         
// <Definition of class [[GUSINetDB]]>=                                    
class GUSINetDB {
public:
	// [[GUSINetDB]] is a singleton, but usually instantiated by an instance of a 
 // derived class.                                                          
 //                                                                         
 // <Constructing instances of [[GUSINetDB]]>=                              
 	static GUSINetDB * 	Instance();
	// The public interface of [[GUSINetDB]] consists of three areas. The first set of
 // calls is concerned with host names and IP numbers.                      
 //                                                                         
 // <[[GUSINetDB]] host database>=                                          
 virtual hostent *	gethostbyname(const char * name);
 virtual hostent *	gethostbyaddr(const void * addr, size_t len, int type);
 virtual char *		inet_ntoa(in_addr inaddr);
 virtual in_addr_t	inet_addr(const char *address);
 virtual long		gethostid();
 virtual int			gethostname(char *machname, int buflen);
	// The next set of calls is concerned with TCP and UDP services.           
 //                                                                         
 // <[[GUSINetDB]] service database>=                                       
 virtual servent *	getservbyname(const char * name, const char * proto);
 virtual servent *	getservbyport(int port, const char * proto);
 virtual servent *	getservent();
 virtual void		setservent(int stayopen);
 virtual void		endservent();
	// Finally, there is a set of calls concerned with protocols.              
 //                                                                         
 // <[[GUSINetDB]] protocol database>=                                      
 virtual protoent *	getprotobyname(const char * name);
 virtual protoent *	getprotobynumber(int proto);
 virtual protoent * 	getprotoent();
 virtual void		setprotoent(int stayopen);
 virtual void		endprotoent();
protected:
	GUSINetDB();
	virtual ~GUSINetDB()		{}
	// \section{Implementation of [[GUSINetDB]]}                               
 //                                                                         
 // [[GUSINetDB]] is a singleton, but typically implemented by an instance  
 // of a subclass (stored into [[fInstance]] by that subclass) rather than the
 // base class.                                                             
 //                                                                         
 // <Privatissima of [[GUSINetDB]]>=                                        
 static GUSINetDB *		sInstance;
 // The service database is implemented in terms of [[GUSIServiceDB]]. Only 
 // [[getservent]] and [[setservent]] accesse [[GUSIServiceDB]] directly, however.
 //                                                                         
 // <Privatissima of [[GUSINetDB]]>=                                        
 bool					fServiceOpen;
 GUSIServiceDB::iterator	fServiceIter;
 // The protocol database is similar, in principle, to the service database, but it
 // lends itself naturally to a much simpler implementation.                
 //                                                                         
 // <Privatissima of [[GUSINetDB]]>=                                        
 int				fNextProtocol;
 static protoent	sProtocols[2];
}; 

#if PRAGMA_STRUCT_ALIGN
#pragma options align=reset
#endif

#ifdef GUSI_INTERNAL

// Iterators can be defined without regard to the implementation of the    
// [[GUSIServiceDB]] currently used.                                       
//                                                                         
// <Inline member functions for class [[GUSIServiceDB]]>=                  
GUSIServiceDB::iterator	GUSIServiceDB::begin()
{
	Instance()->Reset();
	Instance()->Next();
	
	return iterator();
}
GUSIServiceDB::iterator	GUSIServiceDB::end()
{
	return iterator();
}
bool GUSIServiceDB::iterator::operator==(const GUSIServiceDB::iterator &)
{
	return !GUSIServiceDB::sData->fCurrent;
}
bool GUSIServiceDB::iterator::operator!=(const GUSIServiceDB::iterator &)
{
	return GUSIServiceDB::sData->fCurrent 
		== static_cast<servent *>(nil);
}
GUSIServiceDB::iterator & GUSIServiceDB::iterator::operator++()
{
	GUSIServiceDB::Instance()->Next();
	return *this;
}
servent * GUSIServiceDB::iterator::operator*()
{
	return GUSIServiceDB::sData->fCurrent;
}

#endif /* GUSI_INTERNAL */

#endif /* GUSI_SOURCE */

#endif /* _GUSINetDB_ */
