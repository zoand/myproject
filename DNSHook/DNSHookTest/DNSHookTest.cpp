// DNSHookTest.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <Windows.h>
#include <atlstr.h>
#include <TlHelp32.h>
#include "QueryCmdLine.h"
#include "LibCreateRemoteThread.h"

BOOL KillProcess( DWORD dwPid )
{
	BOOL bKillRes = FALSE;
	HANDLE hProcGame = OpenProcess(PROCESS_TERMINATE,FALSE,dwPid);
	if (hProcGame)
	{
		bKillRes = TerminateProcess(hProcGame,0);
		CloseHandle(hProcGame);
	}

	return bKillRes;
}

BOOL EnableDebugPrivilege()
{  
	HANDLE hToken;  
	LUID sedebugnameValue;  
	TOKEN_PRIVILEGES tkp;  
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
	{  
		return   FALSE;  
	}  
	if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &sedebugnameValue))  
	{  
		CloseHandle(hToken);  
		return FALSE;  
	}  
	tkp.PrivilegeCount = 1;  
	tkp.Privileges[0].Luid = sedebugnameValue;  
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;  
	if (!AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(tkp), NULL, NULL))
	{  
		CloseHandle(hToken);  
		return FALSE;  
	}  
	return TRUE;  
}

BOOL InjectDll_RemoteThread(DWORD ProcessID,LPCWSTR szDllPath,DWORD dwTimeOut)
{
	HANDLE ProcessHandle = OpenProcess(PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ ,FALSE,ProcessID);

	if (ProcessHandle)
	{
		UINT nAllocLen = wcslen(szDllPath)*2+10;
		LPVOID pRemoteBase=VirtualAllocEx(ProcessHandle,NULL,nAllocLen,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);

 		if(!pRemoteBase)
 		{
 			DWORD dwErrorCode = GetLastError();
 			OutputDebugStringW(L"VirtualAllocEx Failed\n");
 			return FALSE;
 		}


		if (!WriteProcessMemory(ProcessHandle,pRemoteBase,(LPTSTR)szDllPath,wcslen(szDllPath)*2+2,NULL))
		{
			VirtualFreeEx(ProcessHandle,pRemoteBase,nAllocLen,MEM_DECOMMIT);

			OutputDebugStringW(L"WriteProcessMemory Failed\n");
			return FALSE;
		}
		
		HMODULE hKrl32 = NULL;
		LPTHREAD_START_ROUTINE pfn=(LPTHREAD_START_ROUTINE)GetProcAddress(hKrl32=GetModuleHandleW(TEXT("Kernel32.dll")),"LoadLibraryW");


		HANDLE hRemoteThread = LibCreateRemoteThread(ProcessHandle,pfn,pRemoteBase,0,NULL);
		if (hRemoteThread==NULL)
		{
			VirtualFreeEx(ProcessHandle,pRemoteBase,nAllocLen,MEM_DECOMMIT);

			OutputDebugStringW(L"CreateRemoteThread Failed\n");
			DWORD dwErrorCode = GetLastError();
			return FALSE;
		}
		WaitForSingleObject(hRemoteThread,dwTimeOut);

		DWORD dwExitCode = 0;

		GetExitCodeThread(hRemoteThread,&dwExitCode);

		WCHAR szMsgOut[500];
		wsprintfW(szMsgOut,L"RemoteThread ExitCode %d\n",dwExitCode);
		OutputDebugStringW(szMsgOut);

		VirtualFreeEx(ProcessHandle,pRemoteBase,nAllocLen,MEM_DECOMMIT);

		CloseHandle(hRemoteThread);
		CloseHandle(ProcessHandle);

		return TRUE;
	}
	else
	{
		OutputDebugStringW(L"OpenProcess Failed\n");
	}
	return FALSE;
}

DWORD GetNetworkServicePid()
{
	DWORD dwNetworkServicePid = 0;

	CString strCmdLine;

	HANDLE   hProcessSnapshot=CreateToolhelp32Snapshot(TH32CS_SNAPALL,0);  
	PROCESSENTRY32 Info;
	Info.dwSize = sizeof(PROCESSENTRY32); 
	if(::Process32First(hProcessSnapshot,&Info))  
	{
		while(::Process32Next(hProcessSnapshot,&Info)!=FALSE)  
		{
			GetProcessCmdLine(Info.th32ProcessID,strCmdLine.GetBuffer(2000),2000);
			strCmdLine.ReleaseBuffer();

			if ( strCmdLine.CompareNoCase(L"c:\\windows\\system32\\svchost.exe -k networkservice") == 0 )
			{
				dwNetworkServicePid = Info.th32ProcessID;

				break;
			}
		}
		::CloseHandle(hProcessSnapshot);
		memset(&Info,0,sizeof(PROCESSENTRY32));
	}

	return dwNetworkServicePid;
}



VOID InjectNetworkService( LPCWSTR pszDllPath )
{
	DWORD dwNetworkServicePid =  GetNetworkServicePid();
	CString strMsgOut;
	strMsgOut.Format(L"NetworkService Pid %d\r\n",dwNetworkServicePid);
	OutputDebugStringW(strMsgOut);
	InjectDll_RemoteThread( dwNetworkServicePid,pszDllPath,3000);
}

#include <WinDNS.h>
int _tmain(int argc, _TCHAR* argv[])
{
	EnableDebugPrivilege();

	BOOL bFound = FALSE;
	DWORD dwNetworkServicePid = GetNetworkServicePid();
	
	if ( dwNetworkServicePid == 0 )
	{
		system("sc start WinRM");
		system("sc start Wecsvc");
		system("sc start TermService");
		system("sc start TapiSrv");
		system("sc start NlaSvc");
		system("sc start napagent");
		system("sc start LanmanWorkstation");
		system("sc start Dnscache");
		system("sc start CryptSvc");
		Sleep(500);
		dwNetworkServicePid = GetNetworkServicePid();
	}

	HANDLE  hModuleSnap  =  NULL;
	MODULEENTRY32   me32   =   {0}; 
	hModuleSnap  =  ::CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,dwNetworkServicePid);     	
	if( hModuleSnap != INVALID_HANDLE_VALUE && hModuleSnap )    	
	{
		me32.dwSize = sizeof(MODULEENTRY32);     	
		if(::Module32First(hModuleSnap,   &me32))     	
		{     		
			do     		
			{
				CString strFileName;
				CString strModulePath;
				strModulePath = me32.szExePath;
				strFileName = strModulePath.Right(strModulePath.GetLength() - strModulePath.ReverseFind(L'\\')-1);

				if (strFileName.CompareNoCase(L"DnsHook.dll") == 0 )
				{
					bFound = TRUE;
					break;
				}
			}
			while(::Module32Next(hModuleSnap,&me32));     	
		}     	
		::CloseHandle(hModuleSnap);	    	
	}
	

	
	if (bFound)
	{
		KillProcess(dwNetworkServicePid);
		system("sc start WinRM");
		system("sc start Wecsvc");
		system("sc start TermService");
		system("sc start TapiSrv");
		system("sc start NlaSvc");
		system("sc start napagent");
		system("sc start LanmanWorkstation");
		system("sc start Dnscache");
		system("sc start CryptSvc");


		Sleep(500);
	}



	WCHAR szLocalPath[MAX_PATH]={0};
	GetModuleFileNameW(NULL,szLocalPath,MAX_PATH);
	WCHAR *pPathEnd = (WCHAR *)szLocalPath+wcslen(szLocalPath);
	while (pPathEnd != szLocalPath && *pPathEnd != L'\\') pPathEnd--;
	*(pPathEnd+1) = 0;

	wcscat_s(szLocalPath,MAX_PATH,L"DnsHook.dll");

	InjectNetworkService(szLocalPath);

	while (TRUE)
	{
		Sleep(2000);
	}

	return 0;
}

