#include "stdafx.h"
#include "QueryCmdLine.h"


BOOL GetProcessCmdLine(DWORD dwId,LPWSTR wBuf,DWORD dwBufLen)
{

	// open the process
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwId);
	DWORD err = 0;
	if (hProcess == NULL)
	{
		err = GetLastError();
		return FALSE;
	}

	// determine if 64 or 32-bit processor
	SYSTEM_INFO si;
	GetNativeSystemInfo(&si);

	// determine if this process is running on WOW64
	BOOL wow;
	IsWow64Process(GetCurrentProcess(), &wow);

	// use WinDbg "dt ntdll!_PEB" command and search for ProcessParameters offset to find the truth out
	DWORD ProcessParametersOffset = si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? 0x20 : 0x10;
	DWORD CommandLineOffset = si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? 0x70 : 0x40;

	// read basic info to get ProcessParameters address, we only need the beginning of PEB
	DWORD pebSize = ProcessParametersOffset + 8;
	PBYTE peb = (PBYTE)malloc(pebSize);
	ZeroMemory(peb, pebSize);

	// read basic info to get CommandLine address, we only need the beginning of ProcessParameters
	DWORD ppSize = CommandLineOffset + 16;
	PBYTE pp = (PBYTE)malloc(ppSize);
	ZeroMemory(pp, ppSize);


	BOOL bGetRes = FALSE;

	if (wow)
	{
		do 
		{
			// we're running as a 32-bit process in a 64-bit OS
			PROCESS_BASIC_INFORMATION_WOW64 pbi;
			ZeroMemory(&pbi, sizeof(pbi));

			// get process information from 64-bit world
			static _NtQueryInformationProcess query= (_NtQueryInformationProcess)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtWow64QueryInformationProcess64");
			err = query(hProcess, 0, &pbi, sizeof(pbi), NULL);
			if (err != 0)
			{
				break;
			}

			// read PEB from 64-bit address space
			static _NtWow64ReadVirtualMemory64 read = (_NtWow64ReadVirtualMemory64)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtWow64ReadVirtualMemory64");
			err = read(hProcess, pbi.PebBaseAddress, peb, pebSize, NULL);
			if (err != 0)
			{
				break;
			}

			// read ProcessParameters from 64-bit address space
			PBYTE* parameters = (PBYTE*)*(LPVOID*)(peb + ProcessParametersOffset); // address in remote process adress space
			err = read(hProcess, parameters, pp, ppSize, NULL);
			if (err != 0)
			{
				break;
			}

			// read CommandLine
			UNICODE_STRING_WOW64* pCommandLine = (UNICODE_STRING_WOW64*)(pp + CommandLineOffset);

			err = read(hProcess, pCommandLine->Buffer, wBuf, min(pCommandLine->MaximumLength,dwBufLen), NULL);
			if (err != 0)
			{
				break;
			}

			bGetRes = TRUE;
		} while (FALSE);

	}
	else
	{
		do 
		{
			// we're running as a 32-bit process in a 32-bit OS, or as a 64-bit process in a 64-bit OS
			PROCESS_BASIC_INFORMATION pbi;
			ZeroMemory(&pbi, sizeof(pbi));

			// get process information
			static _NtQueryInformationProcess query = (_NtQueryInformationProcess)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationProcess");
			err = query(hProcess, 0, &pbi, sizeof(pbi), NULL);
			if (err != 0)
			{
				break;
			}

			// read PEB
			if (!ReadProcessMemory(hProcess, pbi.PebBaseAddress, peb, pebSize, NULL))
			{
				break;
			}

			// read ProcessParameters
			PBYTE* parameters = (PBYTE*)*(LPVOID*)(peb + ProcessParametersOffset); // address in remote process adress space
			if (!ReadProcessMemory(hProcess, parameters, pp, ppSize, NULL))
			{
				break;
			}

			// read CommandLine
			UNICODE_STRING* pCommandLine = (UNICODE_STRING*)(pp + CommandLineOffset);
			if (!ReadProcessMemory(hProcess, pCommandLine->Buffer, wBuf, min(pCommandLine->MaximumLength,dwBufLen), NULL))
			{
				break;
			}
			bGetRes = TRUE;
		} while (FALSE);
	}

	CloseHandle(hProcess);
	if(pp)
	{
		free(pp);
	}
	
	if (peb)
	{
		free(peb);
	}
	

	return bGetRes;
}