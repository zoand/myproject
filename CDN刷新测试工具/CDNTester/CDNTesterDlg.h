
// CDNTesterDlg.h : 头文件
//

#pragma once
#include "afxwin.h"
#include "EditEx.h"

// CCDNTesterDlg 对话框
class CCDNTesterDlg : public CDialog
{
// 构造
public:
	CCDNTesterDlg(CWnd* pParent = NULL);	// 标准构造函数

// 对话框数据
	enum { IDD = IDD_CDNTESTER_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedOk();
	CString m_strTestUrl;
	CEdit m_wndReqAppendHeaders;
	CEdit m_wndTestIps;
	afx_msg void OnBnClickedButton1();
	CEditEx m_wndMsgOut;

	LRESULT OnMsgOut(WPARAM wParam,LPARAM lParam);
	LRESULT OnWorkDone(WPARAM wParam,LPARAM lParam);
	CEdit m_wndHeadMsg;

	HANDLE *m_phWorkThreads;
	HANDLE m_hWatchThread;
	CEdit m_editSavePath;
	CEdit m_editFileExt;
	CButton m_chkSaveToFile;
	afx_msg void OnBnClickedCheck1();
};
