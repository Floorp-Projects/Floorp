
#ifndef CACHEUTILS_H
#define CACHEUTILS_H

/* Find an actively-loading cache file for URL_s in context, and copy the first
 * nbytes of it to a new cache file.  Return a cache converter stream by which
 * the caller can append to the cloned cache file.
 */
extern NET_VoidStreamClass *
NET_CloneWysiwygCacheFile(MWContext *context, URL_Struct *URL_s,
                          uint32 nbytes, const char * wysiwyg_url,
                          const char * base_href);

/* Create a wysiwyg cache converter to a copy of the current entry for URL_s.
 */
extern NET_VoidStreamClass *
net_CloneWysiwygMemCacheEntry(MWContext *window_id, URL_Struct *URL_s,
                              uint32 nbytes, const char * wysiwyg_url,
                              const char * base_href);

#endif /* CACHEUTILS_H */
