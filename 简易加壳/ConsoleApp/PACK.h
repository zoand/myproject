#pragma once
#include "PE.h"

class CPACK
{
public:
	CPACK();
	~CPACK();
public:
	//加壳处理
	BOOL Pack(LPCWSTR pszFilePath,LPCWSTR pszOutputFilePath , BOOL bSelect[5], CHAR MachineCode[16],BYTE btKey);

	//保存最终加壳后的文件
	BOOL SaveFinalFile(LPBYTE pFinalBuf, DWORD pFinalBufSize, LPCWSTR pszOutputFilePath);		
};

