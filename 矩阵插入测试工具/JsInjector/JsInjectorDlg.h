
// JsInjectorDlg.h : 头文件
//

#pragma once
#include "afxwin.h"

#include "MyEdit.h"

// CJsInjectorDlg 对话框
class CJsInjectorDlg : public CDialog
{
// 构造
public:
	CJsInjectorDlg(CWnd* pParent = NULL);	// 标准构造函数

// 对话框数据
	enum { IDD = IDD_JSINJECTOR_DIALOG };

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
	LRESULT OnDebugMsg(WPARAM ,LPARAM);
	afx_msg void OnBnClickedOk();
	CString m_strJsData;
	afx_msg void OnEnChangeEdit1();
	CListBox m_wndListBox;
	afx_msg void OnBnClickedButton1();
	CMyEdit m_wndJsEdit;
};
