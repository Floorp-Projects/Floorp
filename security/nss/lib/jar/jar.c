/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *  JAR.C
 *
 *  Jarnature.
 *  Routines common to signing and validating.
 *
 */

#include "jar.h"
#include "jarint.h"
#include "portreg.h"

static void
jar_destroy_list(ZZList *list);

static int
jar_find_first_cert(JAR_Signer *signer, jarType type, JAR_Item **it);

/*
 *  J A R _ n e w
 *
 *  Create a new instantiation of a manifest representation.
 *  Use this as a token to any calls to this API.
 *
 */
JAR *
JAR_new(void)
{
    JAR *jar;

    if ((jar = (JAR *)PORT_ZAlloc(sizeof(JAR))) == NULL)
        goto loser;
    if ((jar->manifest = ZZ_NewList()) == NULL)
        goto loser;
    if ((jar->hashes = ZZ_NewList()) == NULL)
        goto loser;
    if ((jar->phy = ZZ_NewList()) == NULL)
        goto loser;
    if ((jar->metainfo = ZZ_NewList()) == NULL)
        goto loser;
    if ((jar->signers = ZZ_NewList()) == NULL)
        goto loser;
    return jar;

loser:
    if (jar) {
        if (jar->manifest)
            ZZ_DestroyList(jar->manifest);
        if (jar->hashes)
            ZZ_DestroyList(jar->hashes);
        if (jar->phy)
            ZZ_DestroyList(jar->phy);
        if (jar->metainfo)
            ZZ_DestroyList(jar->metainfo);
        if (jar->signers)
            ZZ_DestroyList(jar->signers);
        PORT_Free(jar);
    }
    return NULL;
}

/*
 *  J A R _ d e s t r o y
 */
void PR_CALLBACK
JAR_destroy(JAR *jar)
{
    PORT_Assert(jar != NULL);

    if (jar == NULL)
        return;

    if (jar->fp)
        JAR_FCLOSE((PRFileDesc *)jar->fp);
    if (jar->url)
        PORT_Free(jar->url);
    if (jar->filename)
        PORT_Free(jar->filename);
    if (jar->globalmeta)
        PORT_Free(jar->globalmeta);

    /* Free the linked list elements */
    jar_destroy_list(jar->manifest);
    ZZ_DestroyList(jar->manifest);
    jar_destroy_list(jar->hashes);
    ZZ_DestroyList(jar->hashes);
    jar_destroy_list(jar->phy);
    ZZ_DestroyList(jar->phy);
    jar_destroy_list(jar->metainfo);
    ZZ_DestroyList(jar->metainfo);
    jar_destroy_list(jar->signers);
    ZZ_DestroyList(jar->signers);
    PORT_Free(jar);
}

static void
jar_destroy_list(ZZList *list)
{
    ZZLink *link, *oldlink;
    JAR_Item *it;
    JAR_Physical *phy;
    JAR_Digest *dig;
    JAR_Cert *fing;
    JAR_Metainfo *met;
    JAR_Signer *signer;

    if (list && !ZZ_ListEmpty(list)) {
        link = ZZ_ListHead(list);
        while (!ZZ_ListIterDone(list, link)) {
            it = link->thing;
            if (!it)
                goto next;
            if (it->pathname)
                PORT_Free(it->pathname);

            switch (it->type) {
                case jarTypeMeta:
                    met = (JAR_Metainfo *)it->data;
                    if (met) {
                        if (met->header)
                            PORT_Free(met->header);
                        if (met->info)
                            PORT_Free(met->info);
                        PORT_Free(met);
                    }
                    break;

                case jarTypePhy:
                    phy = (JAR_Physical *)it->data;
                    if (phy)
                        PORT_Free(phy);
                    break;

                case jarTypeSign:
                    fing = (JAR_Cert *)it->data;
                    if (fing) {
                        if (fing->cert)
                            CERT_DestroyCertificate(fing->cert);
                        if (fing->key)
                            PORT_Free(fing->key);
                        PORT_Free(fing);
                    }
                    break;

                case jarTypeSect:
                case jarTypeMF:
                case jarTypeSF:
                    dig = (JAR_Digest *)it->data;
                    if (dig) {
                        PORT_Free(dig);
                    }
                    break;

                case jarTypeOwner:
                    signer = (JAR_Signer *)it->data;
                    if (signer)
                        JAR_destroy_signer(signer);
                    break;

                default:
                    /* PORT_Assert( 1 != 2 ); */
                    break;
            }
            PORT_Free(it);

        next:
            oldlink = link;
            link = link->next;
            ZZ_DestroyLink(oldlink);
        }
    }
}

/*
 *  J A R _ g e t _ m e t a i n f o
 *
 *  Retrieve meta information from the manifest file.
 *  It doesn't matter whether it's from .MF or .SF, does it?
 *
 */

int
JAR_get_metainfo(JAR *jar, char *name, char *header, void **info,
                 unsigned long *length)
{
    JAR_Item *it;
    ZZLink *link;
    ZZList *list;

    PORT_Assert(jar != NULL && header != NULL);

    if (jar == NULL || header == NULL)
        return JAR_ERR_PNF;

    list = jar->metainfo;

    if (ZZ_ListEmpty(list))
        return JAR_ERR_PNF;

    for (link = ZZ_ListHead(list);
         !ZZ_ListIterDone(list, link);
         link = link->next) {
        it = link->thing;
        if (it->type == jarTypeMeta) {
            JAR_Metainfo *met;

            if ((name && !it->pathname) || (!name && it->pathname))
                continue;
            if (name && it->pathname && strcmp(it->pathname, name))
                continue;
            met = (JAR_Metainfo *)it->data;
            if (!PORT_Strcasecmp(met->header, header)) {
                *info = PORT_Strdup(met->info);
                *length = PORT_Strlen(met->info);
                return 0;
            }
        }
    }
    return JAR_ERR_PNF;
}

/*
 *  J A R _ f i n d
 *
 *  Establish the search pattern for use
 *  by JAR_find_next, to traverse the filenames
 *  or certificates in the JAR structure.
 *
 *  See jar.h for a description on how to use.
 *
 */
JAR_Context *
JAR_find(JAR *jar, char *pattern, jarType type)
{
    JAR_Context *ctx;

    PORT_Assert(jar != NULL);

    if (!jar)
        return NULL;

    ctx = (JAR_Context *)PORT_ZAlloc(sizeof(JAR_Context));
    if (ctx == NULL)
        return NULL;

    ctx->jar = jar;
    if (pattern) {
        if ((ctx->pattern = PORT_Strdup(pattern)) == NULL) {
            PORT_Free(ctx);
            return NULL;
        }
    }
    ctx->finding = type;

    switch (type) {
        case jarTypeMF:
            ctx->next = ZZ_ListHead(jar->hashes);
            break;

        case jarTypeSF:
        case jarTypeSign:
            ctx->next = NULL;
            ctx->nextsign = ZZ_ListHead(jar->signers);
            break;

        case jarTypeSect:
            ctx->next = ZZ_ListHead(jar->manifest);
            break;

        case jarTypePhy:
            ctx->next = ZZ_ListHead(jar->phy);
            break;

        case jarTypeOwner:
            if (jar->signers)
                ctx->next = ZZ_ListHead(jar->signers);
            else
                ctx->next = NULL;
            break;

        case jarTypeMeta:
            ctx->next = ZZ_ListHead(jar->metainfo);
            break;

        default:
            PORT_Assert(1 != 2);
            break;
    }
    return ctx;
}

/*
 *  J A R _ f i n d _ e n d
 *
 *  Destroy the find iterator context.
 *
 */
void
JAR_find_end(JAR_Context *ctx)
{
    PORT_Assert(ctx != NULL);
    if (ctx) {
        if (ctx->pattern)
            PORT_Free(ctx->pattern);
        PORT_Free(ctx);
    }
}

/*
 *  J A R _ f i n d _ n e x t
 *
 *  Return the next item of the given type
 *  from one of the JAR linked lists.
 *
 */

int
JAR_find_next(JAR_Context *ctx, JAR_Item **it)
{
    JAR *jar;
    ZZList *list = NULL;
    jarType finding;
    JAR_Signer *signer = NULL;

    PORT_Assert(ctx != NULL);
    PORT_Assert(ctx->jar != NULL);

    jar = ctx->jar;

    /* Internally, convert jarTypeSign to jarTypeSF, and return
       the actual attached certificate later */
    finding = (ctx->finding == jarTypeSign) ? jarTypeSF : ctx->finding;
    if (ctx->nextsign) {
        if (ZZ_ListIterDone(jar->signers, ctx->nextsign)) {
            *it = NULL;
            return -1;
        }
        PORT_Assert(ctx->nextsign->thing != NULL);
        signer = (JAR_Signer *)ctx->nextsign->thing->data;
    }

    /* Find out which linked list to traverse. Then if
       necessary, advance to the next linked list. */
    while (1) {
        switch (finding) {
            case jarTypeSign: /* not any more */
                PORT_Assert(finding != jarTypeSign);
                list = signer->certs;
                break;

            case jarTypeSect:
                list = jar->manifest;
                break;

            case jarTypePhy:
                list = jar->phy;
                break;

            case jarTypeSF: /* signer, not jar */
                PORT_Assert(signer != NULL);
                list = signer ? signer->sf : NULL;
                break;

            case jarTypeMF:
                list = jar->hashes;
                break;

            case jarTypeOwner:
                list = jar->signers;
                break;

            case jarTypeMeta:
                list = jar->metainfo;
                break;

            default:
                PORT_Assert(1 != 2);
                list = NULL;
                break;
        }
        if (list == NULL) {
            *it = NULL;
            return -1;
        }
        /* When looping over lists of lists, advance to the next signer.
	   This is done when multiple signers are possible. */
        if (ZZ_ListIterDone(list, ctx->next)) {
            if (ctx->nextsign && jar->signers) {
                ctx->nextsign = ctx->nextsign->next;
                if (!ZZ_ListIterDone(jar->signers, ctx->nextsign)) {
                    PORT_Assert(ctx->nextsign->thing != NULL);
                    signer = (JAR_Signer *)ctx->nextsign->thing->data;
                    PORT_Assert(signer != NULL);
                    ctx->next = NULL;
                    continue;
                }
            }
            *it = NULL;
            return -1;
        }

        /* if the signer changed, still need to fill in the "next" link */
        if (ctx->nextsign && ctx->next == NULL) {
            switch (finding) {
                case jarTypeSF:
                    ctx->next = ZZ_ListHead(signer->sf);
                    break;

                case jarTypeSign:
                    ctx->next = ZZ_ListHead(signer->certs);
                    break;

                case jarTypeMF:
                case jarTypeMeta:
                case jarTypePhy:
                case jarTypeSect:
                case jarTypeOwner:
                    break;
            }
        }
        PORT_Assert(ctx->next != NULL);
        if (ctx->next == NULL) {
            *it = NULL;
            return -1;
        }
        while (!ZZ_ListIterDone(list, ctx->next)) {
            *it = ctx->next->thing;
            ctx->next = ctx->next->next;
            if (!*it || (*it)->type != finding)
                continue;
            if (ctx->pattern && *ctx->pattern) {
                if (PORT_RegExpSearch((*it)->pathname, ctx->pattern))
                    continue;
            }
            /* We have a valid match. If this is a jarTypeSign
	       return the certificate instead.. */
            if (ctx->finding == jarTypeSign) {
                JAR_Item *itt;

                /* just the first one for now */
                if (jar_find_first_cert(signer, jarTypeSign, &itt) >= 0) {
                    *it = itt;
                    return 0;
                }
                continue;
            }
            return 0;
        }
    } /* end while */
}

static int
jar_find_first_cert(JAR_Signer *signer, jarType type, JAR_Item **it)
{
    ZZLink *link;
    ZZList *list = signer->certs;
    int status = JAR_ERR_PNF;

    *it = NULL;
    if (ZZ_ListEmpty(list)) {
        /* empty list */
        return JAR_ERR_PNF;
    }

    for (link = ZZ_ListHead(list);
         !ZZ_ListIterDone(list, link);
         link = link->next) {
        if (link->thing->type == type) {
            *it = link->thing;
            status = 0;
            break;
        }
    }
    return status;
}

JAR_Signer *
JAR_new_signer(void)
{
    JAR_Signer *signer = (JAR_Signer *)PORT_ZAlloc(sizeof(JAR_Signer));
    if (signer == NULL)
        goto loser;

    /* certs */
    signer->certs = ZZ_NewList();
    if (signer->certs == NULL)
        goto loser;

    /* sf */
    signer->sf = ZZ_NewList();
    if (signer->sf == NULL)
        goto loser;
    return signer;

loser:
    if (signer) {
        if (signer->certs)
            ZZ_DestroyList(signer->certs);
        if (signer->sf)
            ZZ_DestroyList(signer->sf);
        PORT_Free(signer);
    }
    return NULL;
}

void
JAR_destroy_signer(JAR_Signer *signer)
{
    if (signer) {
        if (signer->owner)
            PORT_Free(signer->owner);
        if (signer->digest)
            PORT_Free(signer->digest);
        jar_destroy_list(signer->sf);
        ZZ_DestroyList(signer->sf);
        jar_destroy_list(signer->certs);
        ZZ_DestroyList(signer->certs);
        PORT_Free(signer);
    }
}

JAR_Signer *
jar_get_signer(JAR *jar, char *basename)
{
    JAR_Item *it;
    JAR_Context *ctx = JAR_find(jar, NULL, jarTypeOwner);
    JAR_Signer *candidate;
    JAR_Signer *signer = NULL;

    if (ctx == NULL)
        return NULL;

    while (JAR_find_next(ctx, &it) >= 0) {
        candidate = (JAR_Signer *)it->data;
        if (*basename == '*' || !PORT_Strcmp(candidate->owner, basename)) {
            signer = candidate;
            break;
        }
    }
    JAR_find_end(ctx);
    return signer;
}

/*
 *  J A R _ g e t _ f i l e n a m e
 *
 *  Returns the filename associated with
 *  a JAR structure.
 *
 */
char *
JAR_get_filename(JAR *jar)
{
    return jar->filename;
}

/*
 *  J A R _ g e t _ u r l
 *
 *  Returns the URL associated with
 *  a JAR structure. Nobody really uses this now.
 *
 */
char *
JAR_get_url(JAR *jar)
{
    return jar->url;
}

/*
 *  J A R _ s e t _ c a l l b a c k
 *
 *  Register some manner of callback function for this jar.
 *
 */
int
JAR_set_callback(int type, JAR *jar, jar_settable_callback_fn *fn)
{
    if (type == JAR_CB_SIGNAL) {
        jar->signal = fn;
        return 0;
    }
    return -1;
}

/*
 *  Callbacks
 *
 */

/* To return an error string */
char *(*jar_fn_GetString)(int) = NULL;

/* To return an MWContext for Java */
void *(*jar_fn_FindSomeContext)(void) = NULL;

/* To fabricate an MWContext for FE_GetPassword */
void *(*jar_fn_GetInitContext)(void) = NULL;

void
JAR_init_callbacks(char *(*string_cb)(int),
                   void *(*find_cx)(void),
                   void *(*init_cx)(void))
{
    jar_fn_GetString = string_cb;
    jar_fn_FindSomeContext = find_cx;
    jar_fn_GetInitContext = init_cx;
}

/*
 *  J A R _ g e t _ e r r o r
 *
 *  This is provided to map internal JAR errors to strings for
 *  the Java console. Also, a DLL may call this function if it does
 *  not have access to the XP_GetString function.
 *
 *  These strings aren't UI, since they are Java console only.
 *
 */
char *
JAR_get_error(int status)
{
    char *errstring = NULL;

    switch (status) {
        case JAR_ERR_GENERAL:
            errstring = "General JAR file error";
            break;

        case JAR_ERR_FNF:
            errstring = "JAR file not found";
            break;

        case JAR_ERR_CORRUPT:
            errstring = "Corrupt JAR file";
            break;

        case JAR_ERR_MEMORY:
            errstring = "Out of memory";
            break;

        case JAR_ERR_DISK:
            errstring = "Disk error (perhaps out of space)";
            break;

        case JAR_ERR_ORDER:
            errstring = "Inconsistent files in META-INF directory";
            break;

        case JAR_ERR_SIG:
            errstring = "Invalid digital signature file";
            break;

        case JAR_ERR_METADATA:
            errstring = "JAR metadata failed verification";
            break;

        case JAR_ERR_ENTRY:
            errstring = "No Manifest entry for this JAR entry";
            break;

        case JAR_ERR_HASH:
            errstring = "Invalid Hash of this JAR entry";
            break;

        case JAR_ERR_PK7:
            errstring = "Strange PKCS7 or RSA failure";
            break;

        case JAR_ERR_PNF:
            errstring = "Path not found inside JAR file";
            break;

        default:
            if (jar_fn_GetString) {
                errstring = jar_fn_GetString(status);
            } else {
                /* this is not a normal situation, and would only be
	       called in cases of improper initialization */
                char *err = (char *)PORT_Alloc(40);
                if (err)
                    PR_snprintf(err, 39, "Error %d\n", status); /* leak me! */
                else
                    err = "Error! Bad! Out of memory!";
                return err;
            }
            break;
    }
    return errstring;
}
