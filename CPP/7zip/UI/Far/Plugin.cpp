// Plugin.cpp

#include "StdAfx.h"

#include "Plugin.h"

#include "Common/StringConvert.h"
#include "Common/Wildcard.h"

#include "Windows/PropVariantConversions.h"
#include "Windows/FileName.h"
#include "Windows/FileDir.h"

#include "../Common/PropIDUtils.h"

#include "Messages.h"
#include "FarUtils.h"

using namespace NWindows;
using namespace NFar;

CSysString ConvertPropertyToString2(const PROPVARIANT &propVariant, PROPID propID)
{
  if (propVariant.vt == VT_BSTR)
    return GetSystemString(propVariant.bstrVal, CP_OEMCP);
  if (propVariant.vt != VT_BOOL)
    return GetSystemString(ConvertPropertyToString(propVariant, propID), CP_OEMCP);
  int messageID = VARIANT_BOOLToBool(propVariant.boolVal) ? 
      NMessageID::kYes : NMessageID::kNo;
  return g_StartupInfo.GetMsgString(messageID);
}

CPlugin::CPlugin(const UString &fileName, 
    // const UString &defaultName, 
    IInFolderArchive *archiveHandler,
    UString archiveTypeName
    ):
  m_ArchiveHandler(archiveHandler),
  m_FileName(fileName),
  _archiveTypeName(archiveTypeName)
  // , m_DefaultName(defaultName)
  // , m_ArchiverInfo(archiverInfo)
{
  if (!NFile::NFind::FindFile(m_FileName, m_FileInfo))
    throw "error";
  archiveHandler->BindToRootFolder(&_folder);
}

CPlugin::~CPlugin()
{
}

static void MyGetFileTime(IFolderFolder *anArchiveFolder, UINT32 itemIndex,
    PROPID propID, FILETIME &fileTime)
{
  NCOM::CPropVariant propVariant;
  if (anArchiveFolder->GetProperty(itemIndex, propID, &propVariant) != S_OK)
    throw 271932;
  if (propVariant.vt == VT_EMPTY)
  {
    fileTime.dwHighDateTime = 0;
    fileTime.dwLowDateTime = 0;
  }
  else 
  {
    if (propVariant.vt != VT_FILETIME)
      throw 4191730;
    fileTime = propVariant.filetime;
  }
}
  
void CPlugin::ReadPluginPanelItem(PluginPanelItem &panelItem, UINT32 itemIndex)
{
  NCOM::CPropVariant propVariant;
  if (_folder->GetProperty(itemIndex, kpidName, &propVariant) != S_OK)
    throw 271932;

  if (propVariant.vt != VT_BSTR)
    throw 272340;

  CSysString oemString = UnicodeStringToMultiByte(propVariant.bstrVal, CP_OEMCP);
  MyStringCopy(panelItem.FindData.cFileName, (const char *)oemString);
  panelItem.FindData.cAlternateFileName[0] = 0;

  if (_folder->GetProperty(itemIndex, kpidAttributes, &propVariant) != S_OK)
    throw 271932;
  if (propVariant.vt == VT_UI4)
    panelItem.FindData.dwFileAttributes  = propVariant.ulVal;
  else if (propVariant.vt == VT_EMPTY)
    panelItem.FindData.dwFileAttributes = m_FileInfo.Attributes;
  else
    throw 21631;

  if (_folder->GetProperty(itemIndex, kpidIsFolder, &propVariant) != S_OK)
    throw 271932;
  if (propVariant.vt == VT_BOOL)
  {
    if (VARIANT_BOOLToBool(propVariant.boolVal))
      panelItem.FindData.dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
  }
  else if (propVariant.vt != VT_EMPTY)
    throw 21632;

  if (_folder->GetProperty(itemIndex, kpidSize, &propVariant) != S_OK)
    throw 271932;
  UINT64 length;
  if (propVariant.vt == VT_EMPTY)
    length = 0;
  else
    length = ::ConvertPropVariantToUInt64(propVariant);
  panelItem.FindData.nFileSizeLow = UINT32(length);
  panelItem.FindData.nFileSizeHigh = UINT32(length >> 32);

  MyGetFileTime(_folder, itemIndex, kpidCreationTime, panelItem.FindData.ftCreationTime);
  MyGetFileTime(_folder, itemIndex, kpidLastAccessTime, panelItem.FindData.ftLastAccessTime);
  MyGetFileTime(_folder, itemIndex, kpidLastWriteTime, panelItem.FindData.ftLastWriteTime);

  if (panelItem.FindData.ftLastWriteTime.dwHighDateTime == 0 && 
      panelItem.FindData.ftLastWriteTime.dwLowDateTime == 0)
    panelItem.FindData.ftLastWriteTime = m_FileInfo.LastWriteTime;

  if (_folder->GetProperty(itemIndex, kpidPackedSize, &propVariant) != S_OK)
    throw 271932;
  if (propVariant.vt == VT_EMPTY)
    length = 0;
  else
    length = ::ConvertPropVariantToUInt64(propVariant);
  panelItem.PackSize = UINT32(length);
  panelItem.PackSizeHigh = UINT32(length >> 32);

  panelItem.Flags = 0;
  panelItem.NumberOfLinks = 0;

  panelItem.Description = NULL;
  panelItem.Owner = NULL;
  panelItem.CustomColumnData = NULL;
  panelItem.CustomColumnNumber = 0;

  panelItem.Reserved[0] = 0;
  panelItem.Reserved[1] = 0;
  panelItem.Reserved[2] = 0;
  

}

int CPlugin::GetFindData(PluginPanelItem **panelItems, 
    int *itemsNumber, int opMode)
{
  // CScreenRestorer screenRestorer;
  if ((opMode & OPM_SILENT) == 0 && (opMode & OPM_FIND ) == 0)
  {
    /*
    screenRestorer.Save();
    const char *aMsgItems[]=
    {
      g_StartupInfo.GetMsgString(NMessageID::kWaitTitle),
        g_StartupInfo.GetMsgString(NMessageID::kReadingList)
    };
    g_StartupInfo.ShowMessage(0, NULL, aMsgItems,
      sizeof(aMsgItems) / sizeof(aMsgItems[0]), 0);
    */
  }

  UINT32 numItems;
  _folder->GetNumberOfItems(&numItems);
  *panelItems = new PluginPanelItem[numItems];
  try
  {
    for(UINT32 i = 0; i < numItems; i++)
    {
      PluginPanelItem &panelItem = (*panelItems)[i];
      ReadPluginPanelItem(panelItem, i);
      panelItem.UserData = i;
    }
  }
  catch(...)
  {
    delete [](*panelItems);
    throw;
  }
  *itemsNumber = numItems;
  return(TRUE);
}

void CPlugin::FreeFindData(struct PluginPanelItem *panelItems,
    int itemsNumber)
{
  for(int i = 0; i < itemsNumber; i++)
    if(panelItems[i].Description != NULL)
      delete []panelItems[i].Description;
  delete []panelItems;
}


void CPlugin::EnterToDirectory(const UString &aDirName)
{
  CMyComPtr<IFolderFolder> newFolder;
  _folder->BindToFolder(aDirName, &newFolder);
  if(newFolder == NULL)
    if (aDirName.IsEmpty())
      return;
    else
      throw 40325;
  _folder = newFolder;
}

int CPlugin::SetDirectory(const char *aszDir, int /* opMode */)
{
  UString path = MultiByteToUnicodeString(aszDir, CP_OEMCP);
  if (path == L"\\")
  {
    _folder.Release();
    m_ArchiveHandler->BindToRootFolder(&_folder);  
  }
  else if (path == L"..")
  {
    CMyComPtr<IFolderFolder> newFolder;
    _folder->BindToParentFolder(&newFolder);  
    if (newFolder == NULL)
      throw 40312;
    _folder = newFolder;
  }
  else if (path.IsEmpty())
    EnterToDirectory(path);
  else
  {
    if (path[0] == L'\\')
    {
      _folder.Release();
      m_ArchiveHandler->BindToRootFolder(&_folder);  
      path = path.Mid(1);
    }
    UStringVector pathParts;
    SplitPathToParts(path, pathParts);
    for(int i = 0; i < pathParts.Size(); i++)
      EnterToDirectory(pathParts[i]);
  }
  GetCurrentDir();
  return TRUE;
}

void CPlugin::GetPathParts(UStringVector &pathParts)
{
  pathParts.Clear();
  CMyComPtr<IFolderFolder> folderItem = _folder;
  for (;;)
  {
    CMyComPtr<IFolderFolder> newFolder;
    folderItem->BindToParentFolder(&newFolder);  
    if (newFolder == NULL)
      break;
    CMyComBSTR name;
    folderItem->GetName(&name);
    pathParts.Insert(0, (const wchar_t *)name);
    folderItem = newFolder;
  }
}

void CPlugin::GetCurrentDir()
{
  m_CurrentDir.Empty();
  UStringVector pathParts;
  GetPathParts(pathParts);
  for (int i = 0; i < pathParts.Size(); i++)
  {
    m_CurrentDir += L'\\';
    m_CurrentDir += pathParts[i];
  }
}

static char *kPluginFormatName = "7-ZIP";


struct CPROPIDToName
{
  PROPID PropID;
  int PluginID;
};

static CPROPIDToName kPROPIDToName[] =  
{
  { kpidName, NMessageID::kName },
  { kpidExtension, NMessageID::kExtension },
  { kpidIsFolder, NMessageID::kIsFolder }, 
  { kpidSize, NMessageID::kSize },
  { kpidPackedSize, NMessageID::kPackedSize },
  { kpidAttributes, NMessageID::kAttributes },
  { kpidCreationTime, NMessageID::kCreationTime },
  { kpidLastAccessTime, NMessageID::kLastAccessTime },
  { kpidLastWriteTime, NMessageID::kLastWriteTime },
  { kpidSolid, NMessageID::kSolid },
  { kpidCommented, NMessageID::kCommented },
  { kpidEncrypted, NMessageID::kEncrypted },
  { kpidSplitBefore, NMessageID::kSplitBefore },
  { kpidSplitAfter, NMessageID::kSplitAfter },
  { kpidDictionarySize, NMessageID::kDictionarySize },
  { kpidCRC, NMessageID::kCRC },
  { kpidType, NMessageID::kType },
  { kpidIsAnti, NMessageID::kAnti },
  { kpidMethod, NMessageID::kMethod },
  { kpidHostOS, NMessageID::kHostOS },
  { kpidFileSystem, NMessageID::kFileSystem },
  { kpidUser, NMessageID::kUser },
  { kpidGroup, NMessageID::kGroup },
  { kpidBlock, NMessageID::kBlock },
  { kpidComment, NMessageID::kComment },
  { kpidPosition, NMessageID::kPosition }
  
};

static const int kNumPROPIDToName = sizeof(kPROPIDToName) /  sizeof(kPROPIDToName[0]);

static int FindPropertyToName(PROPID propID)
{
  for(int i = 0; i < kNumPROPIDToName; i++)
    if(kPROPIDToName[i].PropID == propID)
      return i;
  return -1;
}

/*
struct CPropertyIDInfo
{
  PROPID PropID;
  const char *FarID;
  int Width;
  // char CharID;
};

static CPropertyIDInfo kPropertyIDInfos[] =  
{
  { kpidName, "N", 0},
  { kpidSize, "S", 8},
  { kpidPackedSize, "P", 8},
  { kpidAttributes, "A", 0},
  { kpidCreationTime, "DC", 14},
  { kpidLastAccessTime, "DA", 14},
  { kpidLastWriteTime, "DM", 14},
  
  { kpidSolid, NULL, 0, 'S'},
  { kpidEncrypted, NULL, 0, 'P'}

  { kpidDictionarySize, IDS_PROPERTY_DICTIONARY_SIZE },
  { kpidSplitBefore, NULL, 'B'},
  { kpidSplitAfter, NULL, 'A'},
  { kpidComment, , NULL, 'C'},
  { kpidCRC, IDS_PROPERTY_CRC }
  // { kpidType, L"Type" }
};

static const int kNumPropertyIDInfos = sizeof(kPropertyIDInfos) /  
    sizeof(kPropertyIDInfos[0]);

static int FindPropertyInfo(PROPID propID)
{
  for(int i = 0; i < kNumPropertyIDInfos; i++)
    if(kPropertyIDInfos[i].PropID == propID)
      return i;
  return -1;
}
*/

// char *g_Titles[] = { "a", "f", "v" };
/*
static void SmartAddToString(AString &destString, const char *srcString)
{
  if (!destString.IsEmpty())
    destString += ',';
  destString += srcString;
}
*/

/*
void CPlugin::AddColumn(PROPID propID)
{
  int index = FindPropertyInfo(propID);
  if (index >= 0)
  {
    for(int i = 0; i < m_ProxyHandler->m_InternalProperties.Size(); i++)
    {
      const CArchiveItemProperty &aHandlerProperty = m_ProxyHandler->m_InternalProperties[i];
      if (aHandlerProperty.ID == propID)
        break;
    }
    if (i == m_ProxyHandler->m_InternalProperties.Size())
      return;

    const CPropertyIDInfo &aPropertyIDInfo = kPropertyIDInfos[index];
    SmartAddToString(PanelModeColumnTypes, aPropertyIDInfo.FarID);
    char aTmp[32];
    itoa(aPropertyIDInfo.Width, aTmp, 10);
    SmartAddToString(PanelModeColumnWidths, aTmp);
    return;
  }
}
*/

void CPlugin::GetOpenPluginInfo(struct OpenPluginInfo *info)
{
  info->StructSize = sizeof(*info);
  info->Flags = OPIF_USEFILTER | OPIF_USESORTGROUPS| OPIF_USEHIGHLIGHTING|
              OPIF_ADDDOTS | OPIF_COMPAREFATTIME;

  UINT codePage = ::AreFileApisANSI() ? CP_ACP : CP_OEMCP;

  MyStringCopy(m_FileNameBuffer, (const char *)UnicodeStringToMultiByte(m_FileName, codePage));
  info->HostFile = m_FileNameBuffer; // test it it is not static
  
  MyStringCopy(m_CurrentDirBuffer, (const char *)UnicodeStringToMultiByte(m_CurrentDir, CP_OEMCP));
  info->CurDir = m_CurrentDirBuffer;

  info->Format = kPluginFormatName;

  UString name;
  {
    UString fullName;
    int index;
    NFile::NDirectory::MyGetFullPathName(m_FileName, fullName, index);
    name = fullName.Mid(index);
  }

  m_PannelTitle = 
      UString(L' ') + 
      _archiveTypeName + 
      UString(L':') + 
      name +
      UString(L' ');
  if(!m_CurrentDir.IsEmpty())
  {
    // m_PannelTitle += '\\';
    m_PannelTitle += m_CurrentDir;
  }
 
  MyStringCopy(m_PannelTitleBuffer, (const char *)UnicodeStringToMultiByte(m_PannelTitle, CP_OEMCP));
  info->PanelTitle = m_PannelTitleBuffer;

  memset(m_InfoLines,0,sizeof(m_InfoLines));
  MyStringCopy(m_InfoLines[0].Text,"");
  m_InfoLines[0].Separator = TRUE;

  MyStringCopy(m_InfoLines[1].Text, g_StartupInfo.GetMsgString(NMessageID::kArchiveType));
  MyStringCopy(m_InfoLines[1].Data, (const char *)UnicodeStringToMultiByte(_archiveTypeName, CP_OEMCP));

  int numItems = 2;
  UINT32 numProps;
  if (m_ArchiveHandler->GetNumberOfArchiveProperties(&numProps) == S_OK)
  {
    for (UINT32 i = 0; i < numProps && numItems < 30; i++)
    {
      CMyComBSTR name;
      PROPID propID;
      VARTYPE vt;
      if (m_ArchiveHandler->GetArchivePropertyInfo(i, &name, &propID, &vt) != S_OK)
        continue;

      InfoPanelLine &item = m_InfoLines[numItems];
      int index = FindPropertyToName(propID);
      if (index < 0)
      {
        if (name != 0)
          MyStringCopy(item.Text, (const char *)UnicodeStringToMultiByte(
              (const wchar_t *)name, CP_OEMCP));
        else
          MyStringCopy(item.Text, "");
      }
      else
        MyStringCopy(item.Text, g_StartupInfo.GetMsgString(kPROPIDToName[index].PluginID));

      NCOM::CPropVariant propVariant;
      if (m_ArchiveHandler->GetArchiveProperty(propID, &propVariant) != S_OK)
        continue;
      CSysString s = ConvertPropertyToString2(propVariant, propID);
      MyStringCopy(item.Data, (const char *)s);
      numItems++;
    }
  }

  //m_InfoLines[1].Separator = 0;

  info->InfoLines = m_InfoLines;
  info->InfoLinesNumber = numItems;

  
  info->DescrFiles = NULL;
  info->DescrFilesNumber = 0;

  PanelModeColumnTypes.Empty();
  PanelModeColumnWidths.Empty();

  /*
  AddColumn(kpidName);
  AddColumn(kpidSize);
  AddColumn(kpidPackedSize);
  AddColumn(kpidLastWriteTime);
  AddColumn(kpidCreationTime);
  AddColumn(kpidLastAccessTime);
  AddColumn(kpidAttributes);
  
  PanelMode.ColumnTypes = (char *)(const char *)PanelModeColumnTypes;
  PanelMode.ColumnWidths = (char *)(const char *)PanelModeColumnWidths;
  PanelMode.ColumnTitles = NULL;
  PanelMode.FullScreen = TRUE;
  PanelMode.DetailedStatus = FALSE;
  PanelMode.AlignExtensions = FALSE;
  PanelMode.CaseConversion = FALSE;
  PanelMode.StatusColumnTypes = "N";
  PanelMode.StatusColumnWidths = "0";
  PanelMode.Reserved[0] = 0;
  PanelMode.Reserved[1] = 0;

  info->PanelModesArray = &PanelMode;
  info->PanelModesNumber = 1;
  */

  info->PanelModesArray = NULL;
  info->PanelModesNumber = 0;

  info->StartPanelMode = 0;
  info->StartSortMode = 0;
  info->KeyBar = NULL;
  info->ShortcutData = NULL;
}

struct CArchiveItemProperty
{
  AString Name;
  PROPID ID;
  VARTYPE Type;
};

HRESULT CPlugin::ShowAttributesWindow()
{
  PluginPanelItem pluginPanelItem;
  if (!g_StartupInfo.ControlGetActivePanelCurrentItemInfo(pluginPanelItem))
    return S_FALSE;
  if (strcmp(pluginPanelItem.FindData.cFileName, "..") == 0 && 
        NFile::NFind::NAttributes::IsDirectory(pluginPanelItem.FindData.dwFileAttributes))
    return S_FALSE;
  int itemIndex = pluginPanelItem.UserData;

  CObjectVector<CArchiveItemProperty> properties;
  UINT32 numProps;
  RINOK(m_ArchiveHandler->GetNumberOfProperties(&numProps));
  int i;
  for (i = 0; i < (int)numProps; i++)
  {
    CMyComBSTR name;
    PROPID propID;
    VARTYPE vt;
    RINOK(m_ArchiveHandler->GetPropertyInfo(i, &name, &propID, &vt));
    CArchiveItemProperty destProperty;
    destProperty.Type = vt;
    destProperty.ID = propID;
    if (destProperty.ID  == kpidPath)
      destProperty.ID  = kpidName;
    AString propName;
    {
      if (name != NULL)
        destProperty.Name = UnicodeStringToMultiByte(
            (const wchar_t *)name, CP_OEMCP);
      else
        destProperty.Name = "Error";
    }
    properties.Add(destProperty);
  }

  /*
  LPCITEMIDLIST aProperties;
  if (index < m_FolderItem->m_DirSubItems.Size())
  {
    const CArchiveFolderItem &anItem = m_FolderItem->m_DirSubItems[index];
    aProperties = anItem.m_Properties;
  }
  else
  {
    const CArchiveFolderFileItem &anItem = 
        m_FolderItem->m_FileSubItems[index - m_FolderItem->m_DirSubItems.Size()];
    aProperties = anItem.m_Properties;
  }
  */

  int size = 2;
  CRecordVector<CInitDialogItem> initDialogItems;
  
  int xSize = 70;
  CInitDialogItem initDialogItem = 
  { DI_DOUBLEBOX, 3, 1, xSize - 4, size - 2, false, false, 0, false, NMessageID::kProperties, NULL, NULL };
  initDialogItems.Add(initDialogItem);
  AStringVector aValues;

  for (i = 0; i < properties.Size(); i++)
  {
    const CArchiveItemProperty &property = properties[i];

    CInitDialogItem initDialogItem = 
      { DI_TEXT, 5, 3 + i, 0, 0, false, false, 0, false, 0, NULL, NULL };
    int index = FindPropertyToName(property.ID);
    if (index < 0)
    {
      initDialogItem.DataMessageId = -1;
      initDialogItem.DataString = property.Name;
    }
    else
      initDialogItem.DataMessageId = kPROPIDToName[index].PluginID;
    initDialogItems.Add(initDialogItem);
    
    NCOM::CPropVariant propVariant;
    RINOK(_folder->GetProperty(itemIndex, 
        property.ID, &propVariant));
    CSysString aString = ConvertPropertyToString2(propVariant, property.ID);
    aValues.Add(aString);
    
    {
      CInitDialogItem initDialogItem = 
      { DI_TEXT, 30, 3 + i, 0, 0, false, false, 0, false, -1, NULL, NULL };
      initDialogItems.Add(initDialogItem);
    }
  }

  int numLines = aValues.Size();
  for(i = 0; i < numLines; i++)
  {
    CInitDialogItem &initDialogItem = initDialogItems[1 + i * 2 + 1];
    initDialogItem.DataString = aValues[i];
  }
  
  int numDialogItems = initDialogItems.Size();
  
  CRecordVector<FarDialogItem> dialogItems;
  dialogItems.Reserve(numDialogItems);
  for(i = 0; i < numDialogItems; i++)
    dialogItems.Add(FarDialogItem());
  g_StartupInfo.InitDialogItems(&initDialogItems.Front(), 
      &dialogItems.Front(), numDialogItems);
  
  int maxLen = 0;
  for (i = 0; i < numLines; i++)
  {
    FarDialogItem &dialogItem = dialogItems[1 + i * 2];
    int len = (int)strlen(dialogItem.Data);
    if (len > maxLen)
      maxLen = len;
  }
  int maxLen2 = 0;
  const int kSpace = 10;
  for (i = 0; i < numLines; i++)
  {
    FarDialogItem &dialogItem = dialogItems[1 + i * 2 + 1];
    int len = (int)strlen(dialogItem.Data);
    if (len > maxLen2)
      maxLen2 = len;
    dialogItem.X1 = maxLen + kSpace;
  }
  size = numLines + 6;
  xSize = maxLen + kSpace + maxLen2 + 5;
  FarDialogItem &aFirstDialogItem = dialogItems.Front();
  aFirstDialogItem.Y2 = size - 2;
  aFirstDialogItem.X2 = xSize - 4;
  
  /* int askCode = */ g_StartupInfo.ShowDialog(xSize, size, NULL, &dialogItems.Front(), numDialogItems);
  return S_OK;
}

int CPlugin::ProcessKey(int aKey, unsigned int controlState)
{
  if (controlState == PKF_CONTROL && aKey == 'A')
  {
    HRESULT result = ShowAttributesWindow();
    if (result == S_OK)
      return TRUE;
    if (result == S_FALSE)
      return FALSE;
    throw "Error";
  }
  if ((controlState & PKF_ALT) != 0 && aKey == VK_F6)
  {
    UString folderPath;
    if (!NFile::NDirectory::GetOnlyDirPrefix(m_FileName, folderPath))
      return FALSE;
    PanelInfo panelInfo;
    g_StartupInfo.ControlGetActivePanelInfo(panelInfo);
    GetFilesReal(panelInfo.SelectedItems,
        panelInfo.SelectedItemsNumber, FALSE, 
        UnicodeStringToMultiByte(folderPath, CP_OEMCP), OPM_SILENT, true);  
    g_StartupInfo.Control(this, FCTL_UPDATEPANEL, NULL);
    g_StartupInfo.Control(this, FCTL_REDRAWPANEL, NULL);
    g_StartupInfo.Control(this, FCTL_UPDATEANOTHERPANEL, NULL);
    g_StartupInfo.Control(this, FCTL_REDRAWANOTHERPANEL, NULL);
    return TRUE;
  }

  return FALSE;
}