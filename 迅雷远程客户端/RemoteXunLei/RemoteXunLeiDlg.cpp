
// RemoteXunLeiDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "RemoteXunLei.h"
#include "RemoteXunLeiDlg.h"

#include "XunleiClient.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// 对话框数据
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


// CRemoteXunLeiDlg 对话框




CRemoteXunLeiDlg::CRemoteXunLeiDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CRemoteXunLeiDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CRemoteXunLeiDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST1, m_wndDownloaders);
	DDX_Control(pDX, IDC_LIST2, m_wndDownloadItems);
}

BEGIN_MESSAGE_MAP(CRemoteXunLeiDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDOK, &CRemoteXunLeiDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDC_BUTTON1, &CRemoteXunLeiDlg::OnBnClickedButton1)
	ON_BN_CLICKED(IDC_BUTTON2, &CRemoteXunLeiDlg::OnBnClickedButton2)
END_MESSAGE_MAP()


// CRemoteXunLeiDlg 消息处理程序

BOOL CRemoteXunLeiDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	m_wndDownloaders.InsertColumn(0,L"名称",LVCFMT_LEFT,90);
	m_wndDownloaders.InsertColumn(1,L"状态",LVCFMT_LEFT,50);

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CRemoteXunLeiDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CRemoteXunLeiDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CRemoteXunLeiDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CRemoteXunLeiDlg::OnBnClickedOk()
{
	CString strErrorMsg;
	BOOL bRes = XunLeiCheckLogin(strErrorMsg);
	//if ( FALSE == bRes )
	{
		bRes = XunLeiLongin(L"gaozan198912",L"zan123456",strErrorMsg);
	}
	
	int a=0;

}

void CRemoteXunLeiDlg::OnBnClickedButton1()
{
	m_wndDownloaders.DeleteAllItems();
	int nCount = XunLeiQueryDownloaders(m_pDownloaders,10);
	
	for (int i=0;i<nCount;i++)
	{
		m_wndDownloaders.InsertItem(i,m_pDownloaders[i]->GetName());
		m_wndDownloaders.SetItemText(i,1,L"在线");
	}
}

void CRemoteXunLeiDlg::OnBnClickedButton2()
{
	XunLeiQueryItems(m_pDownloaders[0]->GetPid(),DIT_LOADING);
}
