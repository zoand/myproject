// 文件同步.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <Windows.h>
#include <UrlMon.h>
#include <atlstr.h>
#include <WinInet.h>
#include <string>
using namespace std;


#pragma comment(lib,"Wininet.lib")
#pragma comment(lib,"UrlMon.lib")

CStringA Base64_Encode(const unsigned char* Data,int DataByte)
{
	//编码表
	const char EncodeTable[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	//返回值
	CStringA strEncode;
	unsigned char Tmp[4]={0};
	int LineLength=0;
	for(int i=0;i<(int)(DataByte / 3);i++)
	{
		Tmp[1] = *Data++;
		Tmp[2] = *Data++;
		Tmp[3] = *Data++;
		strEncode+= EncodeTable[Tmp[1] >> 2];
		strEncode+= EncodeTable[((Tmp[1] << 4) | (Tmp[2] >> 4)) & 0x3F];
		strEncode+= EncodeTable[((Tmp[2] << 2) | (Tmp[3] >> 6)) & 0x3F];
		strEncode+= EncodeTable[Tmp[3] & 0x3F];
		if(LineLength+=4,LineLength==76) {strEncode+="\r\n";LineLength=0;}
	}
	//对剩余数据进行编码
	int Mod=DataByte % 3;
	if(Mod==1)
	{
		Tmp[1] = *Data++;
		strEncode+= EncodeTable[(Tmp[1] & 0xFC) >> 2];
		strEncode+= EncodeTable[((Tmp[1] & 0x03) << 4)];
		strEncode+= "==";
	}
	else if(Mod==2)
	{
		Tmp[1] = *Data++;
		Tmp[2] = *Data++;
		strEncode+= EncodeTable[(Tmp[1] & 0xFC) >> 2];
		strEncode+= EncodeTable[((Tmp[1] & 0x03) << 4) | ((Tmp[2] & 0xF0) >> 4)];
		strEncode+= EncodeTable[((Tmp[2] & 0x0F) << 2)];
		strEncode+= "=";
	}

	return strEncode;
}


string Base64_Decode(const char* Data,int DataByte,int& OutByte)
{
	//解码表
	const char DecodeTable[] =
	{
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		62, // '+'
		0, 0, 0,
		63, // '/'
		52, 53, 54, 55, 56, 57, 58, 59, 60, 61, // '0'-'9'
		0, 0, 0, 0, 0, 0, 0,
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
		13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, // 'A'-'Z'
		0, 0, 0, 0, 0, 0,
		26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
		39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, // 'a'-'z'
	};
	//返回值
	string strDecode;
	int nValue;
	int i= 0;
	while (i < DataByte)
	{
		if (*Data != '\r' && *Data!='\n')
		{
			nValue = DecodeTable[*Data++] << 18;
			nValue += DecodeTable[*Data++] << 12;
			strDecode+=(nValue & 0x00FF0000) >> 16;
			OutByte++;
			if (*Data != '=')
			{
				nValue += DecodeTable[*Data++] << 6;
				strDecode+=(nValue & 0x0000FF00) >> 8;
				OutByte++;
				if (*Data != '=')
				{
					nValue += DecodeTable[*Data++];
					strDecode+=nValue & 0x000000FF;
					OutByte++;
				}
			}
			i += 4;
		}
		else// 回车换行,跳过
		{
			Data++;
			i++;
		}
	}
	return strDecode;
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

BOOL UploadFile( LPCWSTR pszFilePath )
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

		hRequest = HttpOpenRequestW(hConnect, 
			L"POST",
			_T("/cookiesync/up.php?path="), 
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

int _tmain(int argc, _TCHAR* argv[])
{

	UploadFile(L"C:\\新建文本文档.txt");

	return 0;
	LPCWSTR pszSavePath=L"C:\\tempdata.dat";
	HRESULT hr = URLDownloadToFileW(NULL,L"http://freedev.top/cookiesync/filessnapshot.php",pszSavePath,0,NULL);
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
			DWORD dwRes = GetPrivateProfileStringW(L"AllFiles",NULL,NULL,pszSections,nBufLen,pszSavePath);
			if ( dwRes > 0 && dwRes == nBufLen - 2 )
			{
				nBufLen*=2;
			}
			else
			{
				break;
			}
		}

		LPCWSTR pszSyncPath=L"C:\\cookies\\";

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
			strLocalFilePath = pszSyncPath;
			strLocalFilePath += strRemoteFilePath;

			CString strLocalModifyTime;
			SYSTEMTIME time;
			WIN32_FILE_ATTRIBUTE_DATA lpinf;
			GetFileAttributesEx(strLocalFilePath,GetFileExInfoStandard,&lpinf);//获取文件信息，path为文件路径
			FileTimeToSystemTime(&lpinf.ftLastWriteTime,&time);//转换时间格式：FILETIME到SYSTEMTIME
			strLocalModifyTime.Format(_T("%4d-%02d-%02d %02d:%02d:%02d"),time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond);

			if ( FALSE == PathFileExistsW(strLocalFilePath) )
			{
				strRemoteFilePath = Base64_Encode((const unsigned char *)(CStringA(strRemoteFilePath).GetBuffer()),CStringA(strRemoteFilePath).GetLength());

				CString strRemoteFileUrl;
				strRemoteFileUrl = L"http://freedev.top/cookiesync/down.php?path="+strRemoteFilePath;
				CreateFileDirectory(strLocalFilePath);
				HRESULT hr = URLDownloadToFileW(NULL,strRemoteFileUrl,strLocalFilePath,0,NULL);
				OutputDebugStringW(strRemoteFileUrl+L"\r\n");
			}

			pszSection+=nSectionLen+1;
		}
	}
	return 0;
}

