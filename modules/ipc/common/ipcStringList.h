#ifndef ipcStringList_h__
#define ipcStringList_h__

#include <string.h>
#include "plstr.h"
#include "ipcList.h"

//-----------------------------------------------------------------------------
// string node
//-----------------------------------------------------------------------------

class ipcStringNode
{
public:
    const char *Value() const { return mData; }

    PRBool Equals(const char *val) const { return strcmp(mData, val) == 0; }
    PRBool EqualsIgnoreCase(const char *val) const { return PL_strcasecmp(mData, val) == 0; }

    class ipcStringNode *mNext;
private:
    void *operator new(size_t size, const char *str) CPP_THROW_NEW;

    // this is actually bigger
    char mData[1];

    friend class ipcStringList;
};

//-----------------------------------------------------------------------------
// singly-linked list of strings
//-----------------------------------------------------------------------------

class ipcStringList : public ipcList<ipcStringNode>
{
public:
    typedef ipcList<ipcStringNode> Super;

    void Prepend(const char *str)
    {
        Super::Prepend(new (str) ipcStringNode());
    }

    void Append(const char *str)
    {
        Super::Append(new (str) ipcStringNode());
    }

    const ipcStringNode *Find(const char *str) const
    {
        return FindNode(mHead, str);
    }

    void FindAndDelete(const char *str)
    {
        ipcStringNode *node = FindNodeBefore(mHead, str);
        if (node)
            DeleteAfter(node);
        else
            DeleteFirst();
    }

private:
    static ipcStringNode *FindNode      (ipcStringNode *head, const char *str);
    static ipcStringNode *FindNodeBefore(ipcStringNode *head, const char *str);
};

#endif // !ipcStringList_h__
