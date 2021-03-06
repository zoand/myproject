#pragma once

class CPE
{
public:
	CPE();
	~CPE();
public:
	HANDLE					m_hFile;			//PE文件句柄
	LPBYTE					m_pFileBuf;			//PE文件缓冲区
	DWORD					m_dwFileSize;		//文件大小
	DWORD					m_dwImageSize;		//镜像大小
	PIMAGE_DOS_HEADER		m_pDosHeader;		//Dos头
	PIMAGE_NT_HEADERS		m_pNtHeader;		//NT头
	PIMAGE_SECTION_HEADER	m_pSecHeader;		//第一个SECTION结构体指针
	DWORD					m_dwImageBase;		//镜像基址
	DWORD					m_dwCodeBase;		//代码基址
	DWORD					m_dwCodeSize;		//代码大小
	DWORD					m_dwPEOEP;			//OEP地址
	DWORD					m_dwShellOEP;		//新OEP地址
	DWORD					m_dwSizeOfHeader;	//文件头大小
	DWORD					m_dwSectionNum;		//区段数量

	DWORD					m_dwFileAlign;		//文件对齐
	DWORD					m_dwMemAlign;		//内存对齐

	DWORD					m_IATSectionBase;	//IAT所在段基址
	DWORD					m_IATSectionSize;	//IAT所在段大小

	IMAGE_DATA_DIRECTORY	m_PERelocDir;		//重定位表信息
	IMAGE_DATA_DIRECTORY	m_PEImportDir;		//导入表信息
	
	//自定义IAT结构体
	//以下资源待释放
	typedef struct _MYIMPORT
	{
		DWORD	m_dwIATAddr;
		DWORD	m_dwModNameRVA;
		DWORD	m_dwFunNameRVA;
		BOOL	m_bIsOrdinal;
		DWORD	m_Ordinal;
	}MYIMPORT, *PMYIMPORT;
	PMYIMPORT				m_pMyImport;
	PVOID					m_pModNameBuf;
	PVOID					m_pFunNameBuf;
	DWORD					m_dwNumOfIATFuns;
	DWORD					m_dwSizeOfModBuf;
	DWORD					m_dwSizeOfFunBuf;

	DWORD					m_dwIATBaseRVA;

public:
	BOOL InitPE(CString strFilePath);			//初始化PE，读取PE文件，保存PE信息
	void InitValue();							//初始化变
	BOOL OpenPEFile(CString strFilePath);		//打开文件
	BOOL IsPE();								//判断是否为PE文件
	void GetPEInfo();							//获取PE信息
	DWORD XorCode(BYTE byXOR);					//代码段加密
	void XorMachineCode(CHAR MachineCode[16]);						//机器码绑定

	BOOL SetShellReloc(LPBYTE pShellBuf, DWORD hShell);		
												//设置Shell的重定位信息

	void MergeBuf(LPBYTE pFileBuf, DWORD pFileBufSize, 
		LPBYTE pShellBuf, DWORD pShellBufSize, 
		LPBYTE& pFinalBuf, DWORD& pFinalBufSize,BYTE btKey);
												//合并PE文件和Shell
	void SetNewOEP(DWORD dwShellOEP);			//修改新的OEP为Shell的Start函数

	void SaveImportTab();						//以自己定义的数据结构形式保存IAT(导入表)
	void ClsImportTab();						//抹去IAT(导入表)数据
};

