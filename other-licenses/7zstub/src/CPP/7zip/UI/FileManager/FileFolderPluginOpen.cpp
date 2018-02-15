// FileFolderPluginOpen.cpp

#include "StdAfx.h"

#include "resource.h"

#include "../../../Windows/FileName.h"
#include "../../../Windows/Thread.h"

#include "../Agent/Agent.h"

#include "LangUtils.h"
#include "OpenCallback.h"
#include "PluginLoader.h"
#include "RegistryPlugins.h"

using namespace NWindows;

struct CThreadArchiveOpen
{
  UString Path;
  UString ArcFormat;
  CMyComPtr<IInStream> InStream;
  CMyComPtr<IFolderManager> FolderManager;
  CMyComPtr<IProgress> OpenCallback;
  COpenArchiveCallback *OpenCallbackSpec;

  CMyComPtr<IFolderFolder> Folder;
  HRESULT Result;

  void Process()
  {
    try
    {
      CProgressCloser closer(OpenCallbackSpec->ProgressDialog);
      Result = FolderManager->OpenFolderFile(InStream, Path, ArcFormat, &Folder, OpenCallback);
    }
    catch(...) { Result = E_FAIL; }
  }
  
  static THREAD_FUNC_DECL MyThreadFunction(void *param)
  {
    ((CThreadArchiveOpen *)param)->Process();
    return 0;
  }
};

/*
static int FindPlugin(const CObjectVector<CPluginInfo> &plugins, const UString &pluginName)
{
  for (int i = 0; i < plugins.Size(); i++)
    if (plugins[i].Name.CompareNoCase(pluginName) == 0)
      return i;
  return -1;
}
*/

static void SplitNameToPureNameAndExtension(const FString &fullName,
    FString &pureName, FString &extensionDelimiter, FString &extension)
{
  int index = fullName.ReverseFind_Dot();
  if (index < 0)
  {
    pureName = fullName;
    extensionDelimiter.Empty();
    extension.Empty();
  }
  else
  {
    pureName.SetFrom(fullName, index);
    extensionDelimiter = '.';
    extension = fullName.Ptr(index + 1);
  }
}

HRESULT OpenFileFolderPlugin(
    IInStream *inStream,
    const FString &path,
    const UString &arcFormat,
    HMODULE *module,
    IFolderFolder **resultFolder,
    HWND parentWindow,
    bool &encrypted, UString &password)
{
  CObjectVector<CPluginInfo> plugins;
  ReadFileFolderPluginInfoList(plugins);

  FString extension, name, pureName, dot;

  int slashPos = path.ReverseFind_PathSepar();
  FString dirPrefix;
  FString fileName;
  if (slashPos >= 0)
  {
    dirPrefix.SetFrom(path, slashPos + 1);
    fileName = path.Ptr(slashPos + 1);
  }
  else
    fileName = path;

  SplitNameToPureNameAndExtension(fileName, pureName, dot, extension);

  /*
  if (!extension.IsEmpty())
  {
    CExtInfo extInfo;
    if (ReadInternalAssociation(extension, extInfo))
    {
      for (int i = extInfo.Plugins.Size() - 1; i >= 0; i--)
      {
        int pluginIndex = FindPlugin(plugins, extInfo.Plugins[i]);
        if (pluginIndex >= 0)
        {
          const CPluginInfo plugin = plugins[pluginIndex];
          plugins.Delete(pluginIndex);
          plugins.Insert(0, plugin);
        }
      }
    }
  }
  */

  FOR_VECTOR (i, plugins)
  {
    const CPluginInfo &plugin = plugins[i];
    if (!plugin.ClassIDDefined)
      continue;
    CPluginLibrary library;

    CThreadArchiveOpen t;

    if (plugin.FilePath.IsEmpty())
      t.FolderManager = new CArchiveFolderManager;
    else if (library.LoadAndCreateManager(plugin.FilePath, plugin.ClassID, &t.FolderManager) != S_OK)
      continue;

    t.OpenCallbackSpec = new COpenArchiveCallback;
    t.OpenCallback = t.OpenCallbackSpec;
    t.OpenCallbackSpec->PasswordIsDefined = encrypted;
    t.OpenCallbackSpec->Password = password;
    t.OpenCallbackSpec->ParentWindow = parentWindow;

    if (inStream)
      t.OpenCallbackSpec->SetSubArchiveName(fs2us(fileName));
    else
      t.OpenCallbackSpec->LoadFileInfo(dirPrefix, fileName);

    t.InStream = inStream;
    t.Path = fs2us(path);
    t.ArcFormat = arcFormat;

    UString progressTitle = LangString(IDS_OPENNING);
    t.OpenCallbackSpec->ProgressDialog.MainWindow = parentWindow;
    t.OpenCallbackSpec->ProgressDialog.MainTitle = "7-Zip"; // LangString(IDS_APP_TITLE);
    t.OpenCallbackSpec->ProgressDialog.MainAddTitle = progressTitle + L' ';
    t.OpenCallbackSpec->ProgressDialog.WaitMode = true;

    {
      NWindows::CThread thread;
      RINOK(thread.Create(CThreadArchiveOpen::MyThreadFunction, &t));
      t.OpenCallbackSpec->StartProgressDialog(progressTitle, thread);
    }

    if (t.Result == E_ABORT)
      return t.Result;

    encrypted = t.OpenCallbackSpec->PasswordIsDefined;
    if (t.Result == S_OK)
    {
      // if (openCallbackSpec->PasswordWasAsked)
      {
        password = t.OpenCallbackSpec->Password;
      }
      *module = library.Detach();
      *resultFolder = t.Folder.Detach();
      return S_OK;
    }
    
    if (t.Result != S_FALSE)
      return t.Result;
  }
  return S_FALSE;
}
