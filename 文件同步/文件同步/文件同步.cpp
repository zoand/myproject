// 文件同步.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <Windows.h>
#include <UrlMon.h>
#include <atlstr.h>
#include <WinInet.h>

#include <list>
using namespace std;

#include "HelpFun.h"

#pragma comment(lib,"Wininet.lib")
#pragma comment(lib,"UrlMon.lib")



typedef struct tagUPLOAD_TASK
{
	CString strUploadFile;
	DWORD   dwAddTime;
}UPLOAD_TASK,*PUPLOAD_TASK;
typedef list<UPLOAD_TASK> LIST_UPLOAD_TASK,*PLIST_UPLOAD_TASK;
typedef LIST_UPLOAD_TASK::iterator LIST_UPLOAD_TASK_PTR;

class CCSLock
{
private:
	CRITICAL_SECTION m_cs;
public:
	CCSLock()
	{
		InitializeCriticalSection(&m_cs);
	}
	~CCSLock()
	{
		DeleteCriticalSection(&m_cs);
	}
	VOID Lock()
	{
		EnterCriticalSection(&m_cs);
	}

	VOID UnLock()
	{
		LeaveCriticalSection(&m_cs);
	}
};

CCSLock           UploadTaskLock;
LIST_UPLOAD_TASK UploadTask;


VOID AddTask(LPCWSTR pszFilePath)
{
	UploadTaskLock.Lock();
	
	UPLOAD_TASK Task;
	Task.strUploadFile = pszFilePath;
	Task.dwAddTime = GetTickCount();

	UploadTask.push_back(Task);

	UploadTaskLock.UnLock();
}

DWORD WINAPI UpdateThread(PVOID pParam)
{

	while (TRUE)
	{
		LIST_UPLOAD_TASK tmpUploadTask;

		UploadTaskLock.Lock();

		for (LIST_UPLOAD_TASK_PTR it = UploadTask.begin();it!=UploadTask.end();it++)
		{
			tmpUploadTask.push_back(*it);
		}

		UploadTask.clear();

		UploadTaskLock.UnLock();

		for (LIST_UPLOAD_TASK_PTR it = tmpUploadTask.begin();it!=tmpUploadTask.end();it++)
		{
			OutputDebugStringW(it->strUploadFile+L"\r\n");
		}


		Sleep(1000);
	}

	return 0;
}

BYTE *BuildPostData(LPCWSTR pszFilePath,LPCWSTR pszBoudary,DWORD *pdwDataSize)
{
	CStringA straBoundary;
	straBoundary = pszBoudary;

	CStringA strFormHeader;
	strFormHeader.Format(
		"%s\r\n"
		"Content-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"
		"Content-Type: application/octet-stream\r\n\r\n"
		,straBoundary,pszFilePath
		);
	CStringA strFormTail;
	strFormTail.Format(
		"\r\n%s\r\n"
		"Content-Disposition: form-data; name=\"submit\"\r\n"
		"\r\n"
		"Submit\r\n"
		"%s--\r\n"
		,straBoundary,straBoundary
		);
	
	DWORD dwTotalDataLen = 0;

	dwTotalDataLen += strFormHeader.GetLength();
	dwTotalDataLen += strFormTail.GetLength();

	
	HANDLE hFile = CreateFile(pszFilePath,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
	LARGE_INTEGER liFileSize;
	if ( INVALID_HANDLE_VALUE != hFile )
	{
		
		GetFileSizeEx(hFile,&liFileSize);

		dwTotalDataLen +=liFileSize.LowPart;
	}

	DWORD dwErrorCode = GetLastError();

	BYTE *pPostData = new BYTE[dwTotalDataLen];
	DWORD dwCurWriteLen = 0;
	memcpy_s(pPostData+dwCurWriteLen,dwTotalDataLen-dwCurWriteLen,strFormHeader.GetBuffer(),strFormHeader.GetLength());
	dwCurWriteLen+=strFormHeader.GetLength();

	if ( INVALID_HANDLE_VALUE != hFile )
	{
		DWORD dwReadLen = 0;
		ReadFile(hFile,pPostData+dwCurWriteLen,liFileSize.LowPart,&dwReadLen,NULL);

		dwCurWriteLen+=dwReadLen;
		
		CloseHandle(hFile);
	}

	memcpy_s(pPostData+dwCurWriteLen,dwTotalDataLen-dwCurWriteLen,strFormTail.GetBuffer(),strFormTail.GetLength());

	if (pdwDataSize)
	{
		*pdwDataSize = dwTotalDataLen;
	}

	return pPostData;
}

BOOL UploadFile( LPCWSTR pszFilePath,LPCWSTR pszUploadPath )
{
	BOOL bUploadRes = FALSE;
	
	LPCWSTR pszBoundary = L"---------------------------7e029231808ce";

	CString strContentType;
	strContentType.Format(L"Content-Type: multipart/form-data; boundary=%s",pszBoundary);

	// for clarity, error-checking has been removed
	HINTERNET hSession = NULL;
	HINTERNET hConnect = NULL;
	HINTERNET hRequest = NULL;
	PBYTE     pFileData = NULL;

	do 
	{
		hSession = InternetOpen(NULL,INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
		if ( NULL == hSession )
		{
			break;
		}

		hConnect = InternetConnect(hSession, _T("freedev.top"),
			INTERNET_DEFAULT_HTTP_PORT,
			NULL,
			NULL,
			INTERNET_SERVICE_HTTP,
			0,
			1);

		if ( NULL == hConnect )
		{
			break;
		}
		
		CStringA strUploadPath;
		strUploadPath = pszUploadPath;
		if (strUploadPath.GetAt(0) == '\\')
		{
			strUploadPath.Delete(0);
		}

		CStringW strUrlPath;
		strUrlPath.Format(L"/cookiesync/up.php?path=%s",CString(Base64_Encode((const unsigned char *)(strUploadPath.GetBuffer()),strUploadPath.GetLength())));
		hRequest = HttpOpenRequestW(hConnect, 
			L"POST",
			strUrlPath, 
			NULL, 
			NULL, 
			NULL, 
			0, 
			1);

		if( NULL == hRequest )
		{
			break;
		}
		


		DWORD dwDataSize = 0;
		pFileData = BuildPostData(pszFilePath,L"--"+CString(pszBoundary),&dwDataSize);

		bUploadRes = HttpSendRequest(hRequest, 
			strContentType, 
			strContentType.GetLength(), 
			pFileData, 
			dwDataSize);
		
	} while (FALSE);

	if (pFileData)
	{
		delete pFileData;
	}

	if (hRequest)
	{
		InternetCloseHandle(hRequest);
	}

	if (hConnect)
	{
		InternetCloseHandle(hConnect);
	}

	if (hSession)
	{
		InternetCloseHandle(hSession);
	}


	return bUploadRes;

}

VOID CreateFileDirectory( LPCWSTR pszFilePath )
{
	CString strFilePath;
	strFilePath = pszFilePath;

	int nIndex = 0;
	int nStartPos = -1;
	while ( (nStartPos=strFilePath.Find(L"\\",nIndex)) >= 0 )
	{
		nIndex = nStartPos+1;
		CreateDirectoryW(strFilePath.Left(nStartPos),NULL);
	}
}



/*
返回值：是否继续扫描
*/
typedef BOOL (WINAPI *TypeFileFindCallBack)( LPCWSTR pszFileFullPath , PVOID pParam );

/*
返回值：是否扫描这个文件夹
*/
typedef BOOL (WINAPI *TypeDirectFindCallBack)( LPCWSTR pszDirFullPath , PVOID pParam );

BOOL FindPath(LPCWSTR pszFindPath,LPCWSTR pszFindFile,TypeFileFindCallBack pFileFindCallBack,PVOID pFileParam,TypeDirectFindCallBack pDirFindCallBack,PVOID pDirParam)
{

	if ( NULL == pszFindPath || NULL == pszFindFile )
	{
		return FALSE;
	}

	WCHAR szRightFindPath[MAX_PATH];

	wcscpy_s(szRightFindPath,MAX_PATH,pszFindPath);

	const int nPathLen = wcslen(pszFindPath);
	if ( pszFindPath[nPathLen-1] != L'\\' && pszFindPath[nPathLen-1] != L'/' )
	{
		wcscat_s(szRightFindPath,MAX_PATH,L"\\");    
	}

	WCHAR szFindParam[MAX_PATH];
	wcscpy_s(szFindParam,MAX_PATH,szRightFindPath);
	wcscat_s(szFindParam,MAX_PATH,pszFindFile);


	WIN32_FIND_DATA FindFileData;
	HANDLE hFind=::FindFirstFile(szFindParam,&FindFileData);  
	if(INVALID_HANDLE_VALUE == hFind)
	{
		return FALSE;
	}

	while(TRUE)
	{
		BOOL bStopScan = FALSE;
		if( !( StrCmpIW(FindFileData.cFileName,L".") == 0 || StrCmpIW(FindFileData.cFileName,L"..") == 0 ))
		{
			if( FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
			{
				WCHAR szSubPath[MAX_PATH];
				wcscpy_s(szSubPath,MAX_PATH,szRightFindPath);
				wcscat_s(szSubPath,MAX_PATH,FindFileData.cFileName);

				BOOL bScanThisDir = TRUE;
				if(pDirFindCallBack)
				{
					bScanThisDir = pDirFindCallBack( szSubPath,pDirParam );
				}
				if (bScanThisDir)
				{
					FindPath(szSubPath,pszFindFile,pFileFindCallBack,pFileParam,pDirFindCallBack,pDirParam);
				}
			}  
			else  
			{        
				if(pFileFindCallBack)
				{
					WCHAR szFileFullPath[MAX_PATH];
					wcscpy_s(szFileFullPath,MAX_PATH,szRightFindPath);
					wcscat_s(szFileFullPath,MAX_PATH,FindFileData.cFileName);
					if( FALSE == pFileFindCallBack(szFileFullPath,pFileParam) )
					{
						bStopScan = TRUE;
					}
				}
			}
		}

		if (bStopScan)
		{
			break;
		}

		if(!FindNextFile(hFind,&FindFileData))
		{
			DWORD dwErrorCode  = GetLastError();
			break;
		}
	}  
	FindClose(hFind);

	return FALSE;
}

BOOL WINAPI FileFindCallBack( LPCWSTR pszFileFullPath , PVOID pParam)
{
	CString strUploadPath;
	strUploadPath = pszFileFullPath;
	strUploadPath.Replace((LPCWSTR)pParam,L"");

	UploadFile(pszFileFullPath,strUploadPath);
	OutputDebugStringW(pszFileFullPath);
	OutputDebugStringW(L"\r\n");
	return TRUE;
}

BOOL UploadFiles( LPCWSTR pszUploadPath )
{
	CString strUploadPath;
	strUploadPath = pszUploadPath;
	if (strUploadPath.Right(1) != L"\\")
	{
		strUploadPath+=L"\\";
	}

	return FindPath(strUploadPath,L"*.*",FileFindCallBack,strUploadPath.GetBuffer(),NULL,NULL);
}

BOOL DownloadFiles( LPCWSTR pszDownloadPath )
{

	CString strDownloadPath;
	strDownloadPath = pszDownloadPath;
	if (strDownloadPath.Right(1) != L"\\")
	{
		strDownloadPath+=L"\\";
	}

	//创建唯一临时文件
	WCHAR  szTempFile[MAX_PATH];
	WCHAR  szTempPath[MAX_PATH];
	GetTempPathW(MAX_PATH,szTempPath);
	GetTempFileNameW(szTempPath,L"tmp", 0,szTempFile);

	DeleteUrlCacheEntry(L"http://freedev.top/cookiesync/filessnapshot.php");
	HRESULT hr = URLDownloadToFileW(NULL,L"http://freedev.top/cookiesync/filessnapshot.php",szTempFile,0,NULL);
	if ( S_OK == hr )
	{
		int nBufLen = 1024;
		WCHAR *pszSections = NULL;
		while ( TRUE )
		{
			if(pszSections)
			{
				delete pszSections;
			}

			pszSections = new WCHAR[nBufLen];
			DWORD dwRes = GetPrivateProfileStringW(L"AllFiles",NULL,NULL,pszSections,nBufLen,szTempFile);
			if ( dwRes > 0 && dwRes == nBufLen - 2 )
			{
				nBufLen*=2;
			}
			else
			{
				break;
			}
		}

	
		LPCWSTR pszSection = pszSections;
		while (TRUE)
		{
			int nSectionLen = wcslen(pszSection);
			if (nSectionLen == 0)
			{
				break;
			}

			CString strRemoteFilePath;
			strRemoteFilePath = pszSection;

			CString strLocalFilePath;
			strLocalFilePath = strDownloadPath;
			strLocalFilePath += strRemoteFilePath;

			CString strLocalModifyTime;
			SYSTEMTIME time;
			WIN32_FILE_ATTRIBUTE_DATA lpinf;
			GetFileAttributesEx(strLocalFilePath,GetFileExInfoStandard,&lpinf);//获取文件信息，path为文件路径

			FILETIME localFileTime;
			FileTimeToLocalFileTime(&lpinf.ftLastWriteTime,&localFileTime);
			FileTimeToSystemTime(&localFileTime,&time);//转换时间格式：FILETIME到SYSTEMTIME

			strLocalModifyTime.Format(_T("%4d-%02d-%02d %02d:%02d:%02d"),time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond);

			//if ( FALSE == PathFileExistsW(strLocalFilePath) )
			{
				strRemoteFilePath = Base64_Encode((const unsigned char *)(CStringA(strRemoteFilePath).GetBuffer()),CStringA(strRemoteFilePath).GetLength());

				CString strRemoteFileUrl;
				strRemoteFileUrl = L"http://freedev.top/cookiesync/down.php?path="+strRemoteFilePath;
				CreateFileDirectory(strLocalFilePath);

				DeleteFile(strLocalFilePath);
				DeleteUrlCacheEntry(strRemoteFileUrl);
				HRESULT hr = URLDownloadToFileW(NULL,strRemoteFileUrl,strLocalFilePath,0,NULL);
				OutputDebugStringW(strRemoteFileUrl+L"\r\n");
			}

			pszSection+=nSectionLen+1;
		}
	}

	return TRUE;
}


BOOL SyncFile( LPCWSTR pszSyncPath )
{
	UploadFiles( pszSyncPath );
	DownloadFiles( pszSyncPath );
	return FALSE;
}


DWORD WINAPI ThreadProc( LPVOID lParam )  //线程函数
{
	CoInitialize(NULL);

	HANDLE hFolder = CreateFileW(            //打开目录，得到目录的句柄
		L"F:\\个人项目\\trunk\\文件同步\\Debug\\Cookies",
		GENERIC_READ|GENERIC_WRITE,
		FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS,
		NULL
		); 
	if(hFolder ==INVALID_HANDLE_VALUE  )
	{
		return -1;
	}

	char buf[(sizeof(FILE_NOTIFY_INFORMATION)+MAX_PATH)*2];
	FILE_NOTIFY_INFORMATION* pNotify=(FILE_NOTIFY_INFORMATION*)buf;
	DWORD dwBytesReturned;        
	while(TRUE)
	{
		if( ::ReadDirectoryChangesW( hFolder,
			pNotify,
			sizeof(buf),
			TRUE,
			FILE_NOTIFY_CHANGE_FILE_NAME|
			FILE_NOTIFY_CHANGE_DIR_NAME|
			FILE_NOTIFY_CHANGE_ATTRIBUTES|
			FILE_NOTIFY_CHANGE_SIZE|
			FILE_NOTIFY_CHANGE_LAST_WRITE|
			//FILE_NOTIFY_CHANGE_LAST_ACCESS|
			FILE_NOTIFY_CHANGE_CREATION|
			FILE_NOTIFY_CHANGE_SECURITY,
			&dwBytesReturned,
			NULL,
			NULL ) )
		{
			pNotify->FileName[pNotify->FileNameLength/2] = 0;
			int a=0;
			switch(pNotify->Action)
			{
			case FILE_ACTION_ADDED:
				{
					int a=0;
				}
				break;
			case FILE_ACTION_MODIFIED:
				{
					int a=0;
				}
				break;
			case FILE_ACTION_REMOVED:
				{
					int a=0;
				}
				break;
			}
		}
		else           
			break;           
	}
	return 0;
}


int WINAPI WinMain (
		 __in HINSTANCE hInstance,
		 __in_opt HINSTANCE hPrevInstance,
		 __in_opt LPSTR lpCmdLine,
		 __in int nShowCmd
		 )
{
	CreateThread(NULL,0,UpdateThread,0,0,NULL);
	while (1)
	{
		AddTask(L"C:\\sdfsdfsd.txt");
		Sleep(1000);
	}
	
	return 0;

	CString strLastMarkFile;

	//获取当前路径
	WCHAR szLocalPath[MAX_PATH]={0};
	GetModuleFileNameW(NULL,szLocalPath,MAX_PATH);
	WCHAR *pPathEnd = (WCHAR *)szLocalPath+wcslen(szLocalPath);
	while (pPathEnd != szLocalPath && *pPathEnd != L'\\') pPathEnd--;
	*(pPathEnd+1) = 0;


	wcscat_s(szLocalPath,L"Cookies\\");

	
	strLastMarkFile = szLocalPath;
	strLastMarkFile += L"Last";

	CreateThread(NULL,0,ThreadProc,NULL,0,NULL);

	while (TRUE)
	{
		//SyncFile(szLocalPath);

		//每分钟同步一次
		Sleep(30*1000);
	}

	

	return 0;
}

