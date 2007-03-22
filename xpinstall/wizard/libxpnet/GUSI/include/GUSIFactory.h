// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSIFactory.nw		-	Socket factories                            
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSIFactory.h,v $
// % Revision 1.1  2001/03/11 22:35:04  sgehani%netscape.com
// % First Checked In.
// %                                              
// % Revision 1.10  2000/05/23 07:00:00  neeri                             
// % Improve formatting                                                    
// %                                                                       
// % Revision 1.9  2000/03/15 07:22:07  neeri                              
// % Enforce alignment choices                                             
// %                                                                       
// % Revision 1.8  1999/11/15 07:27:18  neeri                              
// % Getting ready for GUSI 2.0.1                                          
// %                                                                       
// % Revision 1.7  1999/08/26 05:45:02  neeri                              
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.6  1999/05/29 06:26:43  neeri                              
// % Fixed header guards                                                   
// %                                                                       
// % Revision 1.5  1998/11/22 23:06:53  neeri                              
// % Releasing 2.0a4 in a hurry                                            
// %                                                                       
// % Revision 1.4  1998/10/25 11:37:37  neeri                              
// % More configuration hooks                                              
// %                                                                       
// % Revision 1.3  1998/08/02 11:20:08  neeri                              
// % Fixed some typos                                                      
// %                                                                       
// % Revision 1.2  1998/01/25 20:53:54  neeri                              
// % Engine implemented, except for signals & scheduling                   
// %                                                                       
// % Revision 1.1  1996/12/16 02:12:40  neeri                              
// % TCP Sockets sort of work                                              
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{Socket Factories}                                              
//                                                                         
// Instead of creating sockets of some specific subtype of [[GUSISocket]], 
// directly, we choose the more flexible approach of creating them in      
// some instance of a subtype of the abstract factory class [[GUSISocketFactory]].
// For even more flexibility and a direct mapping to BSD socket domains,   
// [[GUSISocketFactory]] instances are collected in a [[GUSISocketDomainRegistry]].
// If several types and or protocols in a domain are implemented, they are collected 
// in a [[GUSISocketTypeRegistry]].                                        
//                                                                         
// <GUSIFactory.h>=                                                        
#ifndef _GUSIFactory_
#define _GUSIFactory_

#ifdef GUSI_SOURCE

#include "GUSISocket.h"

#include <ConditionalMacros.h>

#if PRAGMA_STRUCT_ALIGN
#pragma options align=native
#endif

// \section{Definition of [[GUSISocketFactory]]}                           
//                                                                         
// [[GUSISocketFactory]] consists of a few maintenance functions and the socket 
// operations.                                                             
//                                                                         
// <Definition of class [[GUSISocketFactory]]>=                            
class GUSISocketFactory {
public:
	virtual int socketpair(int domain, int type, int protocol, GUSISocket * s[2]);
	virtual GUSISocket * socket(int domain, int type, int protocol) = 0;
protected:
	GUSISocketFactory()				{}
	virtual ~GUSISocketFactory()	{}
};
// \section{Definition of [[GUSISocketDomainRegistry]]}                    
//                                                                         
// The [[GUSISocketDomainRegistry]] is a singleton class registering all socket 
// domains.                                                                
//                                                                         
// <Definition of class [[GUSISocketDomainRegistry]]>=                     
class GUSISocketDomainRegistry : public GUSISocketFactory {
public:
	// The only instance of [[GUSISocketDomainRegistry]] is, as usual, obtained by calling
 // [[Instance]]. Calling [[socket]] on this instance will then create a socket.
 //                                                                         
 // <Socket creation interface of [[GUSISocketDomainRegistry]]>=            
 virtual GUSISocket * socket(int domain, int type, int protocol);
 virtual int  socketpair(int domain, int type, int protocol, GUSISocket * s[2]);
 static  GUSISocketDomainRegistry *	Instance();
	// [[AddFactory]] and [[RemoveFactory]] add and remove a [[GUSISocketFactory]] 
 // for a given domain number. Both return the previous registrant.         
 //                                                                         
 // <Registration interface of [[GUSISocketDomainRegistry]]>=               
 GUSISocketFactory * AddFactory(int domain, GUSISocketFactory * factory);
 GUSISocketFactory * RemoveFactory(int domain);
private:
	// <Privatissima of [[GUSISocketDomainRegistry]]>=                         
 static GUSISocketDomainRegistry *	sInstance;
 // We store domain factories in a table that is quite comfortably sized.   
 //                                                                         
 // <Privatissima of [[GUSISocketDomainRegistry]]>=                         
 GUSISocketFactory * factory[AF_MAX];
 GUSISocketDomainRegistry();
};
// \section{Definition of [[GUSISocketTypeRegistry]]}                      
//                                                                         
// A [[GUSISocketTypeRegistry]] registers factories for some domain by type and
// protocol.                                                               
//                                                                         
// <Definition of class [[GUSISocketTypeRegistry]]>=                       
class GUSISocketTypeRegistry : public GUSISocketFactory {
public:
	// [[GUSISocketTypeRegistry]] is not a singleton, but each instance is somewhat
 // singletonish in that it does some delayed initialization only when it's used
 // and at that point registers itself with the [[GUSISocketDomainRegistry]].
 // Calling [[socket]] on these instances will then create a socket.        
 //                                                                         
 // <Socket creation interface of [[GUSISocketTypeRegistry]]>=              
 GUSISocketTypeRegistry(int domain, int maxfactory);
 virtual GUSISocket * socket(int domain, int type, int protocol);
 virtual int socketpair(int domain, int type, int protocol, GUSISocket * s[2]);
	// [[AddFactory]] and [[RemoveFactory]] add and remove a [[GUSISocketFactory]] 
 // for a given type and protocol (both of which can be specified as 0 to match any
 // value). Both return the previous registrant.                            
 //                                                                         
 // <Registration interface of [[GUSISocketTypeRegistry]]>=                 
 GUSISocketFactory * AddFactory(int type, int protocol, GUSISocketFactory * factory);
 GUSISocketFactory * RemoveFactory(int type, int protocol);
private:
	// \section{Implementation of [[GUSISocketTypeRegistry]]}                  
 //                                                                         
 // We store type factories in a fixed size table. This table is only       
 // initialized when any non-constructor public member is called.           
 //                                                                         
 // <Privatissima of [[GUSISocketTypeRegistry]]>=                           
 struct Entry {
 	int					type;
 	int					protocol;
 	GUSISocketFactory * factory;
 	Entry()	: type(0), protocol(0), factory(nil) {}
 };
 Entry 	* 	factory;
 int			domain;
 int			maxfactory;
 // [[Initialize]] initializes the table and registers the object with the  
 // [[GUSISocketDomainRegistry]] the first time it's called.                
 //                                                                         
 // <Privatissima of [[GUSISocketTypeRegistry]]>=                           
 void			Initialize();
 // Unlike for a [[GUSISocketDomainRegistry]], match identification for a   
 // [[GUSISocketTypeRegistry]] takes a linear search. [[Find]] stops        
 // when it has found either a match or an empty slot.                      
 //                                                                         
 // <Privatissima of [[GUSISocketTypeRegistry]]>=                           
 bool Find(int type, int protocol, bool exact, Entry *&found);
};

#if PRAGMA_STRUCT_ALIGN
#pragma options align=reset
#endif

// \section{Implementation of [[GUSISocketDomainRegistry]]}                
//                                                                         
// By now, you should know how singletons are created. Could it be that the
// combination of Design Patterns and Literate Programming leads to a      
// proliferation of cliches?                                               
//                                                                         
// <Definition of [[GUSISetupFactories]] hook>=                            
extern "C" void GUSISetupFactories();

// <Inline member functions for class [[GUSISocketDomainRegistry]]>=       
inline GUSISocketDomainRegistry * GUSISocketDomainRegistry::Instance()
{
	if (!sInstance) {
		sInstance = new GUSISocketDomainRegistry();
		GUSISetupFactories();
	}
	
	return sInstance;
}
// [[RemoveFactory]] can actually be implemented in terms of [[AddFactory]] but
// that might confuse readers.                                             
//                                                                         
// <Inline member functions for class [[GUSISocketDomainRegistry]]>=       
inline GUSISocketFactory * GUSISocketDomainRegistry::RemoveFactory(int domain)
{
	return AddFactory(domain, nil);
}
// <Inline member functions for class [[GUSISocketTypeRegistry]]>=         
inline GUSISocketTypeRegistry::GUSISocketTypeRegistry(int domain, int maxfactory)
 : domain(domain), maxfactory(maxfactory), factory(nil)
{
}

#endif /* GUSI_SOURCE */

#endif /* _GUSIFactory_ */
