#include "nsIRelatedLinks.h"
#include "net.h"
#include "merrors.h"
#include "plstr.h"
#include "proto.h"

typedef uint8 RL_LOAD_STATUS;
char* relatedLinksProvider = NULL;

#define RL_STATUS_WAITING_TO_LOAD 1
#define RL_STATUS_LOADING 2
#define RL_STATUS_ABORTED 3
#define RL_STATUS_COMPLETED 4
#define RL_STATUS_NOT_AVAILABLE 5

#define RL_MAX_URL 300
#define RL_LINE_SIZE 1024

extern int XP_RL_FETCHING;
extern int XP_RL_UNAVAILABLE;

#define validRLItem(item) ((item->type == RL_CONTAINER) || (item->type == RL_SEPARATOR) || (item->type == RL_LINK) || (item->type == RL_LINK_NEW_WINDOW))

int32 gInitialDiff = 4;
int32 gMaxDiff = 4;
PRBool     enableRelatedLinksp = 1;

struct _RL_Window {
  char* url; 
  char* rlurl;
  RL_LOAD_STATUS status;
  uint8 parseStatus;
  uint8 type;
  char* holdOver;
  char* line;
  struct _RL_Item* items;  
  uint8  diff;
  uint8  count;
  void*  fe_data;
  struct _RL_Item* stack[3];
  uint8  depth;
  RLCallBackProc callBack;
  URL_Struct *currentlyLoading;
  struct _RL_Item* prevItems;
  PRBool demoUrlp;
}; 

struct _RL_Item {
  RL_ItemType type;
  char* name;
  char* convertedName;
  char* url;
  char* rurl;
  struct _RL_Item* child;
  struct _RL_Item* next;
};


typedef struct _RL_Get {
  RL_Window win;
  URL_Struct *url;
} *RL_Get;

typedef struct _RL_SpecialItems {
  RL_Item loading;
  RL_Item noData;
  RL_Item disabled;
} *RL_SpecialItems;


typedef struct _RL_EquivBucket {
  char* jumpPoint;
  char* seenAt;
  struct _RL_EquivBucket* next;
} *RL_EquivBucket;

RL_SpecialItems gRLSpecial = 0;


void RL_DestroyRLWindow(RL_Window win); 
void RL_SetRLWindowURL(RL_Window win, char* url); 
RL_Item RL_WindowItems (RL_Window win); 
RL_Item RL_NextItem(RL_Item item); 
RL_Item RL_ItemChild(RL_Item item); 
uint8 RL_ItemCount(RL_Window win); 
RL_ItemType RL_GetItemType(RL_Item item); 
char* RL_ItemName(RL_Item item); 
char* RL_ItemUrl(RL_Item item); 
typedef void (*RLCallBackProc)(void* pdata, RL_Window win);
RL_Window RL_MakeRLWindowWithCallback(RLCallBackProc callBack, void* fedata);

void freeMem(void* x);
RL_Window RL_MakeRLWindow();
void RL_DestroyRLWindow(RL_Window win);
void RL_EnableRelatedLinks();
void RL_DisableRelatedLinks();
void RL_SetRLWindowURL(RL_Window win, char* url);
void RL_AddItem (RL_Window win, char* url, char* name, uint8 type);
RL_Item RL_WindowItems (RL_Window win);
RL_Item RL_NextItem(RL_Item item);
RL_ItemType RL_GetItemType(RL_Item item);
char* RL_ItemName(RL_Item item);
char* RL_ItemUrl(RL_Item item);
void GetRL (RL_Window win);
void cleanRLTree(RL_Item item);
MWContext* GetRLContext(RL_Window win);
void rl_GetUrlExitFunc (URL_Struct *urls, int status, MWContext *cx) ;
void TryLoadRLFromCache(RL_Window win) ;
void parseNextRDFXMLBlob (RL_Window f, char* blob, int32 size) ;
void parseNextRDFToken (RL_Window f, char* token) ;
void extractAttributes (char* attr, char** attlist) ;
PRBool tokenEquals(char* token, char* string, PRBool endp);
char* getAttributeValue(char** attlist, char* elName);
PUBLIC NET_StreamClass* RL_Converter(FO_Present_Types  format_out, void
*data_object,  URL_Struct *URL_s, MWContext  *window_id) ;
int rl_write(void *obj, const char *str, int32 len) ;
unsigned int rl_write_ready(void *obj) ;
void rl_abort(void *obj, int status) ;
void rl_complete (void *obj) ;
PRBool startsWith (char* pattern, char* uuid) ;
PRBool rlStartsWith (char* pattern, char* uuid) ;
int16 charSearch (char c, char* data) ;
/*char* copyString (char* source) ; */
void parseNextRLBlob (RL_Window f, char* blob, int32 len) ;
void parseNextAlexaLine (RL_Window f, char* line) ;
static PRBool findNextDomain(const char *inDomains, size_t *ioBegin,
size_t *outEnd);
static PRBool nextDomain (char* dom, size_t *n, size_t maxLength);
static PRBool validateDomain(const char *inDomains, size_t inBegin,
size_t inEnd);

class nsRelatedLinks: public nsIRelatedLinks {

public:
   nsRelatedLinks() {
      NS_INIT_REFCNT();
   }

   NS_DECL_ISUPPORTS

   void DestroyRLWindow (RL_Window win) {
      RL_DestroyRLWindow (win);
   }

   void SetRLWindowURL (RL_Window win, char* url) {
      RL_SetRLWindowURL (win, url);
   }

   RL_Item WindowItems (RL_Window win) {
      return RL_WindowItems (win);
   }

   RL_Item NextItem (RL_Item item) {
      return RL_NextItem (item);
   }

   RL_Item ItemChild (RL_Item item) {
      return RL_ItemChild (item);
   }

   uint8 ItemCount (RL_Window win) {
      return RL_ItemCount (win);
   }

   RL_ItemType GetItemType (RL_Item item) {
      return RL_GetItemType (item);
   }

   char* ItemName (RL_Item item) {
      return RL_ItemName (item);
   }

   char* ItemUrl (RL_Item item) {
      return RL_ItemUrl (item);
   }

   RL_Window MakeRLWindowWithCallback(RLCallBackProc callBack, void*fedata) {
      return RL_MakeRLWindowWithCallback(callBack,fedata);
   }

};

NS_IMPL_ADDREF(nsRelatedLinks)
NS_IMPL_RELEASE(nsRelatedLinks)

NS_DEFINE_IID(kIRelatedLinksIID, NS_IRELATEDLINKS_IID);

nsresult nsRelatedLinks::QueryInterface(const nsIID &aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }
    static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

    if (aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) ((nsISupports *)this);
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(kIRelatedLinksIID)) {
        *aInstancePtr = (void*) ((nsIRelatedLinks *)this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

extern "C" nsIRelatedLinks * NS_NewRelatedLinks(void) {
   nsRelatedLinks * pIObject= new nsRelatedLinks;
   if (pIObject) {
      pIObject->AddRef();
      return (nsIRelatedLinks*)pIObject;
   }
   return nsnull;
}


#define copyString(x) PL_strdup(x)

PRBool startsWith (char* pattern, char* uuid) {
  short l1 = strlen(pattern);
  short l2 = strlen(uuid);
  
  if (l2 < l1) return PR_FALSE;
  return (strncmp(pattern, uuid, l1) == 0) ? PR_TRUE : PR_FALSE;
}

PRBool rlStartsWith (char* pattern, char* uuid) {
  short l1 = strlen(pattern);
  short l2 = strlen(uuid);
  short index;
  
  if (l2 < l1) return PR_FALSE;
  
  for (index = 0; index < l1; index++) {
    if ((index == l1-1) && (pattern[index] == '*')) return PR_TRUE;
    if (pattern[index] != uuid[index]) return PR_FALSE;
  }
        
  return PR_TRUE;
}


int16 charSearch (char c, char* data) {
  int32 l1 = strlen(data);
  int16 index;
  
  for (index = 0; index < l1; index++) {
    if (data[index] == c) return index;
  }
  
  return -1;
}

PRIVATE
char* getMem(size_t x) {
  char* ans = (char *)calloc(1,(x));
  if (ans != NULL) {
    memset(ans, '\0', x);
  } else {
    ans = 0;
  }
  return ans;
}

PRIVATE
RL_Item makeRLItem (char* name, char* url) {
  RL_Item item = (RL_Item) getMem(sizeof(struct _RL_Item));
  if (item) {
    item->name = name;
    item->url  = url; 
    item->type = RL_LINK;
    return item;
  } else return NULL;
}

PRIVATE
PRBool specialItemp (RL_Item item) {
  return ((item == gRLSpecial->loading) ||
          (item == gRLSpecial->noData) ||
          (item == gRLSpecial->disabled)) ? PR_TRUE : PR_FALSE;
}


PRIVATE
void readRLPrefs () {
  int32 autoload = -1;
}


extern "C" void RL_Init () {
  gRLSpecial = (RL_SpecialItems)getMem(sizeof(struct _RL_SpecialItems));
  if (gRLSpecial == NULL) {
    enableRelatedLinksp = 0;
  }
  gRLSpecial->loading  = makeRLItem("Fetching ...", NULL);
  gRLSpecial->noData =  makeRLItem("No data available", NULL);
  gRLSpecial->disabled = makeRLItem("Related Links Disabled", NULL);
  if (gRLSpecial->loading == NULL || gRLSpecial->noData == NULL ||  
      gRLSpecial->disabled == NULL) {
    enableRelatedLinksp = 0;
    freeMem(gRLSpecial->loading);
    freeMem(gRLSpecial->noData);  
    freeMem(gRLSpecial->disabled);
  }
  NET_RegisterContentTypeConverter( "*", FO_CACHE_AND_RDF, NULL,
NET_CacheConverter);
  NET_RegisterContentTypeConverter( "*", FO_RDF, NULL, RL_Converter);
  enableRelatedLinksp = 1;
  gInitialDiff = gMaxDiff = 4;
  relatedLinksProvider = copyString("http://www-rl.netscape.com/wtgn?");

}

void freeMem(void* x) {
  if (x != NULL) free(x);
}

static RL_Window gRLWindow = 0;

RL_Window RL_MakeRLWindowWithCallback (RLCallBackProc callBack, void*
fe_data) {
  RL_Window win = RL_MakeRLWindow();
  if (win == NULL) return NULL;
  win->callBack = callBack;
  win->fe_data = fe_data;
  gRLWindow = win;
  return win;
}


RL_Window RL_MakeRLWindow() {
  RL_Window ans = (RL_Window) getMem(sizeof(struct _RL_Window));
  if (ans == NULL) return NULL;
  ans->line = getMem(RL_LINE_SIZE);
  if (ans->line == NULL) {
    freeMem(ans);
    return NULL;
  }
  ans->holdOver = getMem(RL_LINE_SIZE);
  if (ans->holdOver == NULL) {
    freeMem(ans->line);
    freeMem(ans);
    return NULL;
  }
  ans->rlurl = getMem(RL_MAX_URL);
  if (ans->rlurl == NULL) {    
    freeMem(ans->line);
    freeMem(ans->rlurl);
    freeMem(ans);
    return NULL;
  }
  ans->diff = gInitialDiff;
  return ans;
}

void RL_DestroyRLWindow(RL_Window win) {
  if (!win) return;
  cleanRLTree(win->prevItems);
  cleanRLTree(win->items);
  if (win->currentlyLoading) {
    win->currentlyLoading->fe_data = NULL;      /* |fe_data| is really |win|
and will be */
                                                                                /* disposed of below */
    NET_InterruptStream (win->currentlyLoading);
  }
  freeMem(win->line);
  freeMem(win->holdOver);
  freeMem(win);
}

PRIVATE
PRBool implicitDomainURL (char* url) {
  uint16 n = 7;
  uint16 size = strlen(url);
  while (n < size) {
    if (url[n] == '/') return PR_TRUE;
    if (url[n] == '.') return PR_FALSE;
    n++;
  }
  return PR_TRUE;
}


PRIVATE
PRBool relatedLinksEnabledURL (char* url) {
  if (url == NULL) return PR_FALSE;
  if (strlen(url) > 300) return PR_FALSE; 
  if (!startsWith("http:", url)) {
    return PR_FALSE;
  } else if (implicitDomainURL(url)) {
    return PR_FALSE;
  } else return PR_TRUE;
}


PRIVATE
PRBool getRLURL (RL_Window win, char* url) {
  size_t n, m, l;
  memset(win->rlurl, '\0', RL_MAX_URL);
  if (!win->demoUrlp) {
          if (!strstr(url, "http://")) return PR_FALSE;
  }
  if (strlen(url) > 370) return PR_FALSE;    
  sprintf(win->rlurl, "%s", relatedLinksProvider);
  n = strlen(relatedLinksProvider) ;
  m = (win->demoUrlp ? 25 : 7);
  l = strlen(url);
  while (m < l) {
    if ((url[m] == '?') || (url[m] == '#') || (url[m] == '@')) {
      win->rlurl[n++] = url[m++];
      break;
    }
    win->rlurl[n++] = url[m++];
  }
  return PR_TRUE;
}


void RL_SetRLWindowURL(RL_Window win, char* url) {
  if (!win || !relatedLinksProvider || !enableRelatedLinksp) return;
  
  cleanRLTree(win->prevItems);
  win->prevItems = win->items;
  win->items = NULL;
  freeMem(win->url);
  win->url = NULL;
  if (win->currentlyLoading != NULL) {
    NET_InterruptStream (win->currentlyLoading);
    win->currentlyLoading = NULL;
  }
  win->count = 0;
  if (win->url && url && !strcmp(url,win->url)) return;
  win->demoUrlp = startsWith("file:///c%7C/NS1998Demos", url);
  if (!win->demoUrlp && !relatedLinksEnabledURL(url)) {
    win->status = RL_STATUS_NOT_AVAILABLE;
    return;
  }
  if (!getRLURL(win, url)) return;
  win->url = copyString(url);
  win->diff++;
  if (win->diff < gMaxDiff) {
    win->status = RL_STATUS_LOADING;
    GetRL(win);
  } else {
    win->status = RL_STATUS_WAITING_TO_LOAD;
  }
}

void cleanRLTree (RL_Item item) {
  while (item) {
    RL_Item old = item;
    freeMem(item->name);
    PL_strfree(item->convertedName);
    freeMem(item->rurl);
    item->name = item->url = NULL;    
    if (item->child) cleanRLTree(item->child);
    item->child = NULL;
    item = item->next;
    freeMem(old);
  }
}

void RL_AddItem (RL_Window win, char* nurl, char* name, uint8 type) {
  if (win != NULL && (win->status == RL_STATUS_LOADING)) {
    RL_Item item = (RL_Item)getMem(sizeof(struct _RL_Item));
    PRBool zerop = (win->count == 0) ? PR_TRUE : PR_FALSE;
    RL_Item existing; 
	char* url = NULL;
	if (nurl) url = strstr(&nurl[7], "http://");
	if (!url) url = nurl;
    if (item == NULL) return;
	item->rurl = nurl;
    item->name = name;
    item->url = url;
    item->type = type;
    win->count++;
    existing =  (win->depth == 0 ? win->items : win->stack[win->depth-1]->child);
    if (!existing) {
      if (win->depth == 0) {
        win->items = item;
      } else {
        win->stack[win->depth-1]->child = item;
      }
    } else {
      while (existing->next) {
	if (existing->url && url && (strcmp(existing->url, url) == 0)) {
	  freeMem(item);
	  return;
	}
	existing = existing->next; 
      }
      existing->next = item;
    }
    if (type == RL_CONTAINER)  win->stack[win->depth++] = item;
  }
}

  
RL_Item RL_WindowItems (RL_Window win) {
  if ((win != NULL) && (win->rlurl[0] != '\0') && enableRelatedLinksp) {
    win->diff = 0;
    gInitialDiff = 0;
    if (win->items) {
      return win->items;
    } else if (win->status == RL_STATUS_WAITING_TO_LOAD) {
      win->status = RL_STATUS_LOADING; 
      GetRL(win);
      if (win->items == NULL) return gRLSpecial->loading;
      return win->items;
    } else if (win->status == RL_STATUS_LOADING) {
      return gRLSpecial->loading;
    } else return gRLSpecial->noData;
  } else if (!enableRelatedLinksp) {
    return gRLSpecial->disabled;
  } else return gRLSpecial->noData;
}

PRIVATE
void RL_PossiblyLoad (RL_Window win) {
  if ((win != NULL) && (win->diff < gMaxDiff) && enableRelatedLinksp &&
      (win->status == RL_STATUS_WAITING_TO_LOAD)) {
      win->status = RL_STATUS_LOADING;
      GetRL(win);
  }
}

RL_Item RL_NextItem(RL_Item item) {
  if (!validRLItem(item)) return NULL;
  return item->next;
}

RL_Item RL_ItemChild(RL_Item item) {
  if (!validRLItem(item)) return NULL;
  return item->child;
}

uint8 RL_ItemCount(RL_Window win) {
  return (win->count ? win->count : 1);
}


RL_ItemType RL_GetItemType(RL_Item item) {
  if (!validRLItem(item)) return NULL;
  return item->type;
}

char* RL_ItemName(RL_Item item) {
  if (!validRLItem(item)) return NULL;
  return item->name;
}


char* RL_ItemUrl(RL_Item item) {
  if (!validRLItem(item)) return NULL;
  return item->url;
}


PRIVATE
char* RL_WinReferer(RL_Window win) {
  return win->rlurl;
}

void GetRL (RL_Window win) {
  URL_Struct                      *urls;
  
  if (!enableRelatedLinksp) return;
/*  XP_Trace("\nFetching %s", win->rlurl);  */
  urls = NET_CreateURLStruct(win->rlurl, NET_DONT_RELOAD);
  if (urls == NULL)     return;
  
  urls->load_background = TRUE;
  urls->fe_data = win;                  /* hookup url_struct to RL_Window and vice
versa */
  win->currentlyLoading = urls; /*  so you can get one from the other */
  
  win->status = RL_STATUS_LOADING;
  MWContext * pContext = XP_FindContextOfType(0,MWContextBrowser);
  NET_GetURL(urls, FO_CACHE_AND_RDF,  pContext,
rl_GetUrlExitFunc);
  
}



void rl_GetUrlExitFunc (URL_Struct *urls, int status, MWContext *cx) {
  if (urls != NULL) {
    RL_Window win = (RL_Window)urls->fe_data;
    if ( win != NULL ) {
            win->status = RL_STATUS_ABORTED;
         /*   if (win->callBack) (*(win->callBack))(win->fe_data, win);  */
            NET_FreeURLStruct (urls);
        }
  }
}

PUBLIC NET_StreamClass *
RL_Converter(FO_Present_Types  format_out, void *data_object, 
                 URL_Struct *URL_s, MWContext  *window_id) {
  NET_StreamClass* stream;
  RL_Window rl     = (RL_Window) URL_s->fe_data;
  RL_Get   rg;

/*  XP_Trace("\nSetting up display stream. Have URL: %s\n",
URL_s->address); */
  if ((rl == NULL) || (rl->status != RL_STATUS_LOADING))
        return NULL;
  
  rg       = (RL_Get) getMem(sizeof(struct _RL_Get));
  memset(rl->line, '\0', RL_LINE_SIZE);
  memset(rl->holdOver, '\0', RL_LINE_SIZE);
  rg->win      = rl;
  rl->depth    = 0;
  rg->url      = URL_s;
  rl->currentlyLoading = URL_s;

  stream = NET_NewStream("RL", 
      (MKStreamWriteFunc)rl_write, 
      (MKStreamCompleteFunc)rl_complete, 
      (MKStreamAbortFunc)rl_abort,
      (MKStreamWriteReadyFunc)rl_write_ready, 
      rg, window_id);

  return(stream);
}


int rl_write(void *obj, const char *str, int32 len) {
  RL_Get rg = (RL_Get)obj;
  RL_Window win;
  if (obj == NULL ||  len < 0) {
      return MK_INTERRUPTED;
  }
  win = gRLWindow;
 /* if (rg->url != win->currentlyLoading) return len; */
  parseNextRDFXMLBlob(win, (char *)str, len);
  return(len);
}

unsigned int rl_write_ready(void *obj) {
   return RL_LINE_SIZE;
}

void rl_abort(void *obj, int status) {
  RL_Get rg = (RL_Get)obj;
  RL_Window win ;
  if (rg == NULL) return;
  win = gRLWindow;
  if (win->currentlyLoading == rg->url)
    win->status = RL_STATUS_ABORTED;
  if (win->callBack) (*(win->callBack))(win->fe_data, win);  
  freeMem(obj);
}

void rl_complete (void *obj) {
  RL_Get rg = (RL_Get)obj;
  RL_Window win;
  if (rg == NULL) return;
  win  = gRLWindow;
  if (win->currentlyLoading == rg->url) win->status =
RL_STATUS_COMPLETED;
  win->currentlyLoading = NULL;
  
}




#define MAX_ATTRIBUTES 9

void parseNextRDFXMLBlob (RL_Window f, char* blob, int32 size) {
  int32 n, last, m;
  PRBool somethingseenp = PR_FALSE;
  n = last = 0;
  
  while (n < size) {
    char c = blob[n];
    m = 0;
    somethingseenp = PR_FALSE;
    memset(f->line, '\0', RL_LINE_SIZE);
    if (f->holdOver[0] != '\0') {
      memcpy(f->line, f->holdOver, strlen(f->holdOver));
      m = strlen(f->holdOver);
      somethingseenp = PR_TRUE;
      memset(f->holdOver, '\0', RL_LINE_SIZE);
    }
    while ((m < 300) && (c != '<') && (c != '>')) {
      f->line[m] = c;
      m++;
      somethingseenp = (somethingseenp || ((c != ' ') && (c != '\r') &&
(c != '\n'))) ? PR_TRUE : PR_FALSE;
      n++;      
      if (n < size) c = blob[n]; 
      else break;
    }
    if (c == '>') f->line[m] = c;
    n++;
    if (m > 0) {
      if ((c == '<') || (c == '>')) {
        last = n;
        if (c == '<') f->holdOver[0] = '<'; 
        if (somethingseenp == 1) parseNextRDFToken(f, f->line);
      } else if (size > last) {
        memcpy(f->holdOver, f->line, m);
      }
    } else if (c == '<') f->holdOver[0] = '<';
  }
}

PRIVATE
void parseRDFProcessingInstruction (RL_Window win, char* token) {
  char* attlist[MAX_ATTRIBUTES+1];
  char* newServer;
  extractAttributes(token, attlist);
  newServer = getAttributeValue(attlist, "relatedLinksServer");
  if (newServer) {
    freeMem(relatedLinksProvider);
    relatedLinksProvider = getMem(strlen(newServer));
    if (relatedLinksProvider) 
      memcpy(relatedLinksProvider, newServer, strlen(newServer));
  }
} 

char* getAttributeValue (char** attlist, char* elName) {
  size_t n = 0;
  while ((n < MAX_ATTRIBUTES) && (*(attlist + n) != NULL)) {
    if (strcmp(*(attlist + n), elName) == 0) return *(attlist + n + 1);
    n = n + 2;
  }
  return NULL;
}

PRBool tokenEquals (char* token, char* elName, PRBool endp) {
  if (endp) {
    return ((token[1] == '/') && (strncmp(elName, &token[2],
strlen(elName)) ==0)) ? PR_TRUE : PR_FALSE;
  } else {
    return  (strncmp(elName, &token[1], strlen(elName)) ==0) ? PR_TRUE :
PR_FALSE;
  }
}

void parseNextRDFToken (RL_Window f, char* token) {
  if (token[0] != '<')   {
    return;
  } else if (tokenEquals(token, "RelatedLinks", PR_TRUE)) {
  (*f->callBack)(f->fe_data, f);  

  } else if (tokenEquals(token, "Topic", PR_TRUE)) {
    if (f->depth > 0) f->depth--;
  } else if (token[1] == '?')  {
    parseRDFProcessingInstruction(f, token);
  } else { 
    char* attlist[MAX_ATTRIBUTES+1];
    extractAttributes(token, attlist);
    if (tokenEquals(token, "aboutPage", PR_FALSE)) {
    } else if (tokenEquals(token, "child", PR_FALSE)) {
      char* url = getAttributeValue(attlist, "href");
      char* name = getAttributeValue(attlist, "name");
      char* type = getAttributeValue(attlist, "instanceOf");
      if (url && name) RL_AddItem(f, copyString(url),  copyString(name),
RL_LINK); 
      if (type && (strcmp(type, "Separator"))) RL_AddItem(f, NULL, NULL,
RL_SEPARATOR);
    } else if (tokenEquals(token, "Topic", PR_FALSE)) {
      char* name = getAttributeValue(attlist, "name");
      if (name) RL_AddItem(f, NULL, copyString(name), RL_CONTAINER); 
    }
  }
}
 

void extractAttributes (char* attr, char** attlist) {
  size_t n = 1;
  size_t s = strlen(attr); 
  char c ;
  size_t m = 0;
  size_t atc = 0;
  PRBool emptyTagp =  (attr[s-2] == '/') ? PR_TRUE : PR_FALSE;
  PRBool inAttrNamep = PR_TRUE;
  c = attr[n++]; 
  while (n < s) {
    if ((c == ' ') || (c == '\n') || (c == '\r')) break;
    c = attr[n++];
  }
  while (atc < MAX_ATTRIBUTES+1) {*(attlist + atc++) = NULL;}
  atc = 0;
  s = (emptyTagp ? s-2 : s-1);
  while (n < s) {
    PRBool attributeOpenStringSeenp = PR_FALSE;
    m = 0;
    c = attr[n++];
    while ((n <= s) && (atc < MAX_ATTRIBUTES)) {
      if (inAttrNamep && (m > 0) && ((c == ' ') || (c == '\n') || (c ==
'\r') || (c == '='))) {
        attr[n-1] = '\0';
        *(attlist + atc++) = &attr[n-m-1];
        break;
      }
      if  (!inAttrNamep && attributeOpenStringSeenp && (c == '"')) {
        attr[n-1] = '\0';
        *(attlist + atc++) = &attr[n-m-1];
        break;
      }
      if (inAttrNamep) {
        if ((m > 0) || (c != ' ')) m++;
      } else {
        if (c == '"') {
          attributeOpenStringSeenp = PR_TRUE;
        } else {
          if ((m > 0) || (c != ' ')) m++;
        }
      }
      c = attr[n++];
    }
    inAttrNamep = (inAttrNamep ? PR_FALSE : PR_TRUE);
  }
}
