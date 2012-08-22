#include "TestCommon.h"
#include <stdio.h>
#include "nsIURLParser.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"

static void
print_field(const char *label, char *str, int32_t len)
{
    char c = str[len];
    str[len] = '\0';
    printf("[%s=%s]\n", label, str);
    str[len] = c;
}

#define PRINT_FIELD(x) \
        print_field(# x, x, x ## Len)

#define PRINT_SUBFIELD(base, x) \
    PR_BEGIN_MACRO \
        if (x ## Len != -1) \
            print_field(# x, base + x ## Pos, x ## Len); \
    PR_END_MACRO

static void
parse_authority(nsIURLParser *urlParser, char *auth, int32_t authLen)
{
    PRINT_FIELD(auth);

    uint32_t usernamePos, passwordPos;
    int32_t usernameLen, passwordLen;
    uint32_t hostnamePos;
    int32_t hostnameLen, port;

    urlParser->ParseAuthority(auth, authLen,
                              &usernamePos, &usernameLen,
                              &passwordPos, &passwordLen,
                              &hostnamePos, &hostnameLen,
                              &port);

    PRINT_SUBFIELD(auth, username);
    PRINT_SUBFIELD(auth, password);
    PRINT_SUBFIELD(auth, hostname);
    if (port != -1)
        printf("[port=%d]\n", port);
}

static void
parse_file_path(nsIURLParser *urlParser, char *filepath, int32_t filepathLen)
{
    PRINT_FIELD(filepath);

    uint32_t dirPos, basePos, extPos;
    int32_t dirLen, baseLen, extLen;

    urlParser->ParseFilePath(filepath, filepathLen,
                             &dirPos, &dirLen,
                             &basePos, &baseLen,
                             &extPos, &extLen);

    PRINT_SUBFIELD(filepath, dir);
    PRINT_SUBFIELD(filepath, base);
    PRINT_SUBFIELD(filepath, ext);
}

static void
parse_path(nsIURLParser *urlParser, char *path, int32_t pathLen)
{
    PRINT_FIELD(path);

    uint32_t filePos, queryPos, refPos;
    int32_t fileLen, queryLen, refLen;

    urlParser->ParsePath(path, pathLen,
                         &filePos, &fileLen,
                         &queryPos, &queryLen,
                         &refPos, &refLen);

    if (fileLen != -1)
        parse_file_path(urlParser, path + filePos, fileLen);
    PRINT_SUBFIELD(path, query);
    PRINT_SUBFIELD(path, ref);
}

int
main(int argc, char **argv)
{
    if (test_common_init(&argc, &argv) != 0)
        return -1;

    if (argc < 2) {
        printf("usage: TestURLParser [-std|-noauth|-auth] <url>\n");
        return -1;
    }
    nsCOMPtr<nsIURLParser> urlParser;
    if (strcmp(argv[1], "-noauth") == 0) {
        urlParser = do_GetService(NS_NOAUTHURLPARSER_CONTRACTID);
        argv[1] = argv[2];
    }
    else if (strcmp(argv[1], "-auth") == 0) {
        urlParser = do_GetService(NS_AUTHURLPARSER_CONTRACTID);
        argv[1] = argv[2];
    }
    else {
        urlParser = do_GetService(NS_STDURLPARSER_CONTRACTID);
        if (strcmp(argv[1], "-std") == 0)
            argv[1] = argv[2];
        else
            printf("assuming -std\n");
    }
    if (urlParser) {
        printf("have urlParser @%p\n", static_cast<void*>(urlParser.get()));

        char *spec = argv[1];
        uint32_t schemePos, authPos, pathPos;
        int32_t schemeLen, authLen, pathLen;

        urlParser->ParseURL(spec, -1,
                            &schemePos, &schemeLen,
                            &authPos, &authLen,
                            &pathPos, &pathLen);

        if (schemeLen != -1)
            PRINT_SUBFIELD(spec, scheme);
        if (authLen != -1)
            parse_authority(urlParser, spec + authPos, authLen);
        if (pathLen != -1)
            parse_path(urlParser, spec + pathPos, pathLen);
    }
    else
        printf("no urlParser\n");
    return 0;
}
