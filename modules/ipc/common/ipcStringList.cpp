#include "ipcStringList.h"

void *
ipcStringNode::operator new(size_t size, const char *str) CPP_THROW_NEW
{
    int len = strlen(str);

    size += len;

    ipcStringNode *node = (ipcStringNode *) ::operator new(size);
    if (!node)
        return NULL;

    node->mNext = NULL;
    memcpy(node->mData, str, len);
    node->mData[len] = '\0';

    return node;
}

ipcStringNode *
ipcStringList::FindNode(ipcStringNode *node, const char *str)
{
    while (node) {
        if (node->Equals(str))
            return node;
        node = node->mNext;
    }
    return NULL;
}

ipcStringNode *
ipcStringList::FindNodeBefore(ipcStringNode *node, const char *str)
{
    ipcStringNode *prev = NULL;
    while (node) {
        if (node->Equals(str))
            return prev;
        prev = node;
        node = node->mNext;
    }
    return NULL;
}
