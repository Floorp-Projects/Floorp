
#include "nsSetupTypeDlg.h"

nsSetupTypeDlg::nsSetupTypeDlg() :
    mMsg0(NULL),
    mSetupTypeList(NULL)
{
}

nsSetupTypeDlg::~nsSetupTypeDlg()
{
    FreeSetupTypeList();

    if (mMsg0)
        free (mMsg0);
}

int
nsSetupTypeDlg::Back()
{
    return OK;
}

int 
nsSetupTypeDlg::Next()
{
    return OK;
}

int
nsSetupTypeDlg::SetMsg0(char *aMsg)
{
    if (!aMsg)
        return E_PARAM;

    mMsg0 = aMsg;

    return OK;
}

char *
nsSetupTypeDlg::GetMsg0()
{
    if (mMsg0)
        return mMsg0;

    return NULL;
}

int
nsSetupTypeDlg::AddSetupType(nsSetupType *aSetupType)
{
    if (!aSetupType)
        return E_PARAM;

    if (!mSetupTypeList)
    {
        mSetupTypeList = aSetupType;
        return OK;
    }

    nsSetupType *curr = mSetupTypeList;
    nsSetupType *next;
    while (curr)
    {
        next = NULL;
        next = curr->GetNext();
    
        if (!next)
        {
            return curr->SetNext(aSetupType);
        }

        curr = next;
    }

    return OK;
}

nsSetupType *
nsSetupTypeDlg::GetSetupTypeList()
{
    if (mSetupTypeList)
        return mSetupTypeList;

    return NULL;
}

void
nsSetupTypeDlg::FreeSetupTypeList()
{
    nsSetupType *curr = mSetupTypeList;
    nsSetupType *prev;
    
    while (curr)
    {
        prev = curr;
        curr = curr->GetNext();

        if (prev)
            delete prev;
    }
}
