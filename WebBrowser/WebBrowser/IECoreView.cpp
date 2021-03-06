// IECoreView.cpp : 实现文件
//

#include "stdafx.h"
#include "WebBrowser.h"
#include "IECoreView.h"
#include <ExDispid.h>
#include <strsafe.h>
#include "IIEOleClientSite.h"
#include <detours.h>
#pragma comment(lib,"wininet.lib")
#include "PrintWebView.h"

BEGIN_EVENTSINK_MAP(CIECoreView,  CHtmlView)
ON_EVENT(CIECoreView,  AFX_IDW_PANE_FIRST ,DISPID_NEWWINDOW3,NewWindow3,VTS_PDISPATCH  VTS_PBOOL  VTS_I4  VTS_BSTR  VTS_BSTR)
END_EVENTSINK_MAP()
// CIECoreView

IMPLEMENT_DYNCREATE(CIECoreView, CHtmlView)

BOOL CIECoreView::bInternalHook = FALSE;

typedef HRESULT (WINAPI *TypeGetPlatform)( IOmNavigator *pThis, BSTR *bstrPlatform );
TypeGetPlatform pGetNavigator = NULL;
HRESULT WINAPI MyGetPlatform( IOmNavigator *pThis, BSTR *bstrPlatform )
{
	CString strPlatform;
	strPlatform = L"Linux armv7l";
	*bstrPlatform = strPlatform.AllocSysString();
#ifdef DEBUG
	OutputDebugStringW(L"伪装Platform信息："+strPlatform+L"\n");
#endif
	return S_OK;
}

LPCWSTR pszUserAgent = L"Mozilla/5.0 (X11; Ubuntu; Linux i686; rv:41.0) Gecko/20100101 Firefox/41.0";

typedef HRESULT (WINAPI *TypeGetUserAgent)( IOmNavigator *pThis, BSTR *bstrUserAgent );
TypeGetUserAgent pGetUserAgent = NULL;
HRESULT WINAPI MyGetUserAgent( IOmNavigator *pThis, BSTR *bstrUserAgent )
{
	CString strUserAgent;
	strUserAgent = pszUserAgent;
	*bstrUserAgent = strUserAgent.AllocSysString();
#ifdef DEBUG
	OutputDebugStringW(L"伪装UserAgent信息："+strUserAgent+L"\n");
#endif
	return S_OK;
}


BOOL (WINAPI *pNetHttpSendRequestW)(
								 __in HINTERNET hRequest,
								 __in_ecount_opt(dwHeadersLength) LPCWSTR lpszHeaders,
								 __in DWORD dwHeadersLength,
								 __in_bcount_opt(dwOptionalLength) LPVOID lpOptional,
								 __in DWORD dwOptionalLength 
								 ) = HttpSendRequestW;
BOOL WINAPI MyNetHttpSendRequestW(
							   __in HINTERNET hRequest,
							   __in_ecount_opt(dwHeadersLength) LPCWSTR lpszHeaders,
							   __in DWORD dwHeadersLength,
							   __in_bcount_opt(dwOptionalLength) LPVOID lpOptional,
							   __in DWORD dwOptionalLength 
							   )
{
	CString strUserAgent;
	strUserAgent = L"User-Agent: ";
	strUserAgent += pszUserAgent;


	BOOL TReturn = pNetHttpSendRequestW(
		hRequest,
		strUserAgent,
		strUserAgent.GetLength(),
		lpOptional,
		dwOptionalLength
		);
	return TReturn;
};


CIECoreView::CIECoreView()
{
	m_pNotifyer = NULL;
	m_PageID = 0;
	m_bCanBack = FALSE;
	m_bCanForward = FALSE;
	bInit = FALSE;
	m_bFixed = FALSE;
	m_dwCookie = 0;
	m_hIEServer = NULL;
	m_nCurZoom = 100;
	m_pDevTools = NULL;
}

CIECoreView::CIECoreView(IWBCoreNotifyer *pNotifyer)
{
	m_PageID = 0;
	m_pNotifyer = pNotifyer;
	m_bCanBack = FALSE;
	m_bCanForward = FALSE;
	bInit = FALSE;
	m_bFixed = FALSE;
	m_nCurZoom = 100;
	m_pDevTools = NULL;
}

CIECoreView::~CIECoreView()
{
	int a=0;
}

void CIECoreView::DoDataExchange(CDataExchange* pDX)
{
	CHtmlView::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CIECoreView, CHtmlView)
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
	ON_WM_PAINT()
	ON_WM_CREATE()
	ON_MESSAGE(WM_ASYNC_GOTO_URL,OnAsyncGotoUrl)
	ON_MESSAGE(WM_ASYNC_SIMPLE_CONTROL,OnAsyncSimpleControl)
	ON_MESSAGE(WM_ASYNC_SHOW_WINDOW,OnAsyncShowWindow)
	ON_MESSAGE(WM_ASYNC_MOVE_WINDOW,OnAsyncMoveWindow)
	ON_MESSAGE(WM_ASYNC_SET_FOCUS,OnAsyncSetFocus)
	ON_MESSAGE(WM_ASYNC_ZOOM,OnAsyncZoom)
	ON_WM_CLOSE()
	ON_WM_SETFOCUS()
END_MESSAGE_MAP()


// CIECoreView 诊断

#ifdef _DEBUG
void CIECoreView::AssertValid() const
{
	CHtmlView::AssertValid();
}

void CIECoreView::Dump(CDumpContext& dc) const
{
	CHtmlView::Dump(dc);
}
#endif //_DEBUG

BOOL CIECoreView::OnEraseBkgnd(CDC* pDC)
{
	return true;
}

 BOOL CIECoreView::CreateControlSite(COleControlContainer* pContainer, 
 					   COleControlSite** ppSite, UINT nID, REFCLSID clsid)
 {
  	if(ppSite == NULL)
  	{
  		ASSERT(FALSE);
  		return FALSE;
  	}
  	IIEOleControlSite* m_pBrowserSite =  new IIEOleControlSite (pContainer,this);
  	if (!m_pBrowserSite)
  		return FALSE;
  	*ppSite = m_pBrowserSite;
 
 	return TRUE;
 }

// CIECoreView 消息处理程序

void CIECoreView::NewWindow3( IDispatch **ppDisp,VARIANT_BOOL *Cancel,DWORD dwFlags,BSTR bstrUrlContext,BSTR bstrUrl)
{
	if (!ppDisp)
		return;

	if (m_pNotifyer)
	{
		BOOL   bCancel = FALSE;
		*ppDisp = NULL;
		m_pNotifyer->NotifyNewWindow( (PVOID *)ppDisp , NULL , &bCancel, (NWMF_USERREQUESTED&dwFlags || NWMF_FORCEWINDOW&dwFlags), FALSE );
		*Cancel = bCancel?VARIANT_TRUE:VARIANT_FALSE;
	}
} 


void CIECoreView::OnTitleChange(LPCTSTR lpszText)
{
    if(CString(lpszText)==_T("about:blank"))
    {
        return ;
    }
    
	m_strLastTitle = lpszText;
	if(m_pNotifyer)
	{
		m_pNotifyer->NotifyTitleChange(m_PageID,lpszText);
	}
	CHtmlView::OnTitleChange(lpszText);
}

void CIECoreView::OnCommandStateChange(long nCommand, BOOL bEnable)
{
	BOOL bChanged = TRUE;
	switch (nCommand)
	{
	case CSC_NAVIGATEBACK:
		{
			if( m_bCanBack != bEnable )
			{
				int a=0;
			}
			m_bCanBack = bEnable;
		}
		break;
	case CSC_NAVIGATEFORWARD:
		{
			if( m_bCanForward != bEnable )
			{
				int a=0;
			}
			m_bCanForward = bEnable;
		}
		break;
	default:
		{
			bChanged = FALSE;
		}
	}
	if (bChanged && m_pNotifyer)
	{
		m_pNotifyer->NotifyStatusCommand(m_PageID,m_bCanBack,m_bCanForward);
	}
	CHtmlView::OnCommandStateChange(nCommand, bEnable);
}

void CIECoreView::NavigateComplete2(LPDISPATCH pDisp, VARIANT* URL)
{

	if(pDisp == GetApplication() /* 仅处理最顶层Frame的事件 */)
	{

		if ( FALSE == bInternalHook )
		{
			bInternalHook = TRUE;
			IHTMLDocument2 *pDoc2 = (IHTMLDocument2 *)GetHtmlDocument();
			if (pDoc2)
			{
				IHTMLWindow2 *pHW2 = NULL;
				pDoc2->get_parentWindow(&pHW2);

				IOmNavigator *pON = NULL;
				pHW2->get_navigator(&pON);

				DWORD dwTemp = *(DWORD *)((BYTE *)pON+0x10);
				pGetNavigator = (TypeGetPlatform)*((DWORD *)(dwTemp+0x58));

				pGetUserAgent =  (TypeGetUserAgent) *(PVOID *)((DWORD)(*(DWORD*)((BYTE *)pON+0xC+4))+0x28);

				DetourTransactionBegin();
				DetourUpdateThread(GetCurrentThread());

				//DetourAttach((LPVOID*)&pNetHttpSendRequestW,(PVOID)&MyNetHttpSendRequestW);
				DetourAttach((LPVOID*)&pGetNavigator,(PVOID)&MyGetPlatform);
				DetourAttach((LPVOID*)&pGetUserAgent,(PVOID)&MyGetUserAgent);

				DetourTransactionCommit();

			}
		}

		if(m_bFixed == FALSE)
		{
			m_bFixed = TRUE;

			HWND hEmbed=NULL;
			HWND hDoc=NULL;
			
			if(m_hWnd)
			{
				hEmbed=::FindWindowExW(m_hWnd,NULL,TEXT("Shell Embedding"),NULL);
				if(hEmbed!=NULL)
				{
					hDoc=::FindWindowExW(hEmbed,NULL,TEXT("Shell DocObject View"),NULL);
					if(hDoc!=NULL)
					{
						m_hIEServer=::FindWindowExW(hDoc,NULL,TEXT("Internet Explorer_Server"),NULL);
					}
				}
			}
			if(m_hIEServer!=NULL )
			{
				m_wndFixer.SubclassWindow(m_hIEServer);
				m_wndFixer.SetZoomNotifyWindow(m_hWnd,WM_ASYNC_ZOOM);
			}
		}

		if( m_pNotifyer )
		{
			CString strLocationUrl;
			strLocationUrl = GetLocationURL();
			if (strLocationUrl != m_strLastLocantionUrl)
			{
				if(strLocationUrl!=_T("about:blank"))
				{
                    m_strLastLocantionUrl = strLocationUrl;
				    m_pNotifyer->NotifyUrlChange(m_PageID,strLocationUrl);
				}
				this->SetFocus();
			}
		}
	}
}

void CIECoreView::BeforeNavigate2(LPDISPATCH pDisp, VARIANT* URL,
							 VARIANT* Flags, VARIANT* TargetFrameName, VARIANT* PostData,
							 VARIANT* Headers, VARIANT_BOOL* Cancel)
{
	if( m_pNotifyer )
	{
		BOOL bCancel = FALSE;
		if( pDisp == GetApplication() /* 仅处理最顶层Frame的事件 */)
		{
			m_pNotifyer->NotifyBeforeMainNavigate(m_PageID,V_BSTR(URL),&bCancel);
		}
		else
		{
			m_pNotifyer->NotifyBeforeSubNavigate(m_PageID,V_BSTR(URL),&bCancel);
		}
		*Cancel = bCancel?VARIANT_TRUE:VARIANT_FALSE;
	}
}


BOOL WalkDocument(IHTMLDocument2 *pqHtmlDoc2 , int nFrameLevel = 0 )
{
	IHTMLElementCollection *pHtmlElemCol=NULL;
	pqHtmlDoc2->get_all(&pHtmlElemCol);
	if (pHtmlElemCol == NULL)
	{
		return FALSE;
	}

	LONG lElemCount;
	pHtmlElemCol->get_length(&lElemCount);
	VARIANT varIndex,var2;
	for (long i=0;i<lElemCount;i++)
	{
		varIndex.vt=VT_UINT;
		varIndex.lVal=i;
		VariantInit(&var2);
		IDispatch* pDisp;
		HRESULT hr = pHtmlElemCol->item(varIndex,var2,&pDisp);
		if (SUCCEEDED(hr))
		{
			CComQIPtr<IHTMLElement> pqElem(pDisp);

			//如果是iframe元素 则进行递归遍历
			CComQIPtr<IHTMLIFrameElement2> pqFrameElem2(pDisp);
			if (pqFrameElem2)
			{
				CComQIPtr<IWebBrowser> pSubFrameWb;
				pqFrameElem2->QueryInterface(IID_IWebBrowser,(VOID **)&pSubFrameWb);
				if (pSubFrameWb)
				{
					CComQIPtr<IDispatch> pDisp;
					pSubFrameWb->get_Document(&pDisp);
					CComQIPtr<IHTMLDocument2> pSubFrameDoc2(pDisp);
					if( pSubFrameDoc2 )
					{
						WalkDocument(pSubFrameDoc2,nFrameLevel+1);
					}
				}
			}

			CComBSTR bstrTagName;
			pqElem->get_tagName(&bstrTagName);
			CString strTagName;
			strTagName = bstrTagName;
			if ( strTagName.CompareNoCase(L"script") == 0 )
			{
				CComVariant vtAttrValue;
				hr = pqElem->getAttribute(L"src",2,&vtAttrValue);
				
				if ( S_OK == hr && vtAttrValue.vt == VT_BSTR )
				{
					CString strSource;
					strSource = vtAttrValue.bstrVal;
					
					if(!strSource.IsEmpty())
					{
						CString strMsgOut;
						strMsgOut.Format(L"Level %d %s\r\n",nFrameLevel,strSource);
						OutputDebugStringW(strMsgOut);
					}
			
				}

			}

		}
	}

	return TRUE;
}



void CIECoreView::DocumentComplete(LPDISPATCH pDisp, VARIANT* URL)
{	
	if(m_pNotifyer && pDisp == GetApplication()/* 仅处理最顶层Frame的事件 */)
	{
		m_pNotifyer->NotifyMainDocumentComplete(m_PageID,V_BSTR(URL));

		IHTMLDocument2 *pDoc2 = (IHTMLDocument2 *)GetHtmlDocument();

		WalkDocument( pDoc2 );
	}
}
void CIECoreView::NavigateError(LPDISPATCH pDisp, VARIANT* pvURL,
						   VARIANT* pvFrame, VARIANT* pvStatusCode, VARIANT_BOOL* pvbCancel)
{
	if(m_pNotifyer && pDisp == GetApplication()/* 仅处理最顶层Frame的事件 */)
	{

	}
}


LRESULT CIECoreView::OnAsyncGotoUrl(WPARAM wParam,LPARAM lParam)
{
	LPCWSTR pszTargetUrl = (LPCWSTR)wParam;
	CString strUrl;
	strUrl = pszTargetUrl;
	
	Navigate(strUrl.AllocSysString());
	
	delete pszTargetUrl;

	return 0L;
}
LRESULT CIECoreView::OnAsyncSimpleControl(WPARAM wParam,LPARAM lParam)
{
	WB_CONTROL_CODE nControlCode = (WB_CONTROL_CODE)wParam;

	switch(nControlCode)
	{
	case CC_GO_BACK:
		{
			GoBack();
		}
		break;
	case CC_GO_FORWARD:
		{
			GoForward();
		}
		break;
	case CC_REFRESH:
		{
			Refresh();
		}
		break;
	case CC_STOP:
		{
			Stop();
		}
		break;
	case CC_EDIT_CUT:
		{
			ExecWB(OLECMDID_OPEN,OLECMDEXECOPT_DODEFAULT, NULL, NULL);
		}
		break;
	case CC_EDIT_COPY:
		{
			ExecWB(OLECMDID_COPY,OLECMDEXECOPT_DODEFAULT, NULL, NULL);
		}
		break;
	case CC_EDIT_PASTE:
		{
			ExecWB(OLECMDID_PASTE,OLECMDEXECOPT_DODEFAULT, NULL, NULL);
		}
		break;
	case CC_EDIT_SELALL:
		{
			ExecWB(OLECMDID_SELECTALL,OLECMDEXECOPT_DODEFAULT, NULL, NULL);
		}
		break;
	}

	return 0;
}

LRESULT CIECoreView::OnAsyncShowWindow(WPARAM wParam,LPARAM lParam)
{
	BOOL bShow = (BOOL)wParam;
	if (bShow)
	{
		ShowWindow(SW_SHOW);
	}
	else
	{
		ShowWindow(SW_HIDE);
	}
	return 0L;
}
LRESULT CIECoreView::OnAsyncMoveWindow(WPARAM wParam,LPARAM lParam)
{
	int nLeft = LOWORD(wParam);
	int nTop = HIWORD(wParam);
	int nWidth = LOWORD(lParam);
	int nHeight = HIWORD(lParam);

	MoveWindow(nLeft,nTop,nWidth,nHeight,FALSE);
	return 0L;
}
LRESULT CIECoreView::OnAsyncSetFocus(WPARAM wParam,LPARAM lParam)
{	
	::SetFocus(m_hIEServer);
	return 0;
}

LRESULT CIECoreView::OnAsyncZoom(WPARAM wParam,LPARAM lParam)
{
	if ( lParam == 0 )
	{
		if ((int)wParam > 0)
		{
			m_nCurZoom+=(int)10;
		}
		else
		{
			m_nCurZoom-=(int)10;
		}

		if (m_nCurZoom > 400)
		{
			m_nCurZoom = 400;
		}

		if (m_nCurZoom < 50)
		{
			m_nCurZoom = 50;
		}

#ifdef DEBUG
		CString strMstOut;
		strMstOut.Format(L"ZoomLevel %d\n",m_nCurZoom);
		OutputDebugStringW(strMstOut);
#endif

		CComVariant in;
		CComVariant out;
		in=(int) m_nCurZoom;
		HRESULT hr=((IWebBrowser2 *)GetApplication())->ExecWB(OLECMDID_OPTICAL_ZOOM,OLECMDEXECOPT_DODEFAULT,&in,&out);

	}
	else
	{
		m_nCurZoom = lParam;

		if (m_nCurZoom > 400)
		{
			m_nCurZoom = 400;
		}

		if (m_nCurZoom < 50)
		{
			m_nCurZoom = 50;
		}

		CComVariant in;
		CComVariant out;
		in=(int) m_nCurZoom;
		HRESULT hr=((IWebBrowser2 *)GetApplication())->ExecWB(OLECMDID_OPTICAL_ZOOM,OLECMDEXECOPT_DODEFAULT,&in,&out);

	}

	return 0;
}
unsigned long CIECoreView::SetNotifyPtr( IWBCoreNotifyer *pNotifyer )
{
	m_pNotifyer = pNotifyer;
	return 0;
}
unsigned long CIECoreView::GetNotifyPtr( IWBCoreNotifyer **ppNotifyer )
{
	if( ppNotifyer )
	{
		*ppNotifyer = m_pNotifyer;
	}
	return 0;
}

unsigned long CIECoreView::SetPageID( PAGEID nPageID )
{
	m_PageID = nPageID;
	return  0;
}
unsigned long CIECoreView::GetPageID( PAGEID *pPageID)
{
	if(pPageID)
	{
		*pPageID = m_PageID;
	}
	return 0;
}
unsigned long CIECoreView::ControlClose()
{
	ControlStopLoading();
	::PostMessage(m_hWnd,WM_CLOSE,0,0);
	return 0;
}
unsigned long CIECoreView::ControlQueryWnd(HWND *phWnd)
{
	if(phWnd)
	{
		*phWnd = m_hWnd;
	}
	return 0;
}

unsigned long CIECoreView::ControlShowWindow(BOOL bShow) 
{
	PostMessage(WM_ASYNC_SHOW_WINDOW,(WPARAM)bShow,(LPARAM)0);
	return 0;
}
unsigned long CIECoreView::ControlMoveWindow(int nLeft,int nTop,int nWidth,int nHeight)
{
	PostMessage(WM_ASYNC_MOVE_WINDOW,MAKELONG(nLeft,nTop),MAKELONG(nWidth,nHeight));
	return 0;
}

unsigned long CIECoreView::ControlSetFocus()
{
	PostMessage(WM_ASYNC_SET_FOCUS,0,0);
	return 0;
}

unsigned long CIECoreView::ControlQueryUrl(LPTSTR pszUrl,UINT nLen)
{
	wcscpy_s(pszUrl,nLen,m_strLastLocantionUrl.GetBuffer());
	return 0;
}
unsigned long CIECoreView::ControlQueryTitle(LPTSTR pszTitle,UINT nLen)
{
	wcscpy_s(pszTitle,nLen,m_strLastTitle.GetBuffer());
	return 0;
}
unsigned long CIECoreView::ControlGotoUrl( const wchar_t *pszTargetUrl )
{
 	WCHAR *pszUrl = new WCHAR[wcslen(pszTargetUrl)+1];
 	wcscpy_s(pszUrl,wcslen(pszTargetUrl)+1,pszTargetUrl);
 	PostMessage(WM_ASYNC_GOTO_URL,(WPARAM)pszUrl,(LPARAM)0);

	return 0;
}
unsigned long CIECoreView::ControlGoBack( )
{
	PostMessage(WM_ASYNC_SIMPLE_CONTROL,(WPARAM)CC_GO_BACK,(LPARAM)0);
	return 0;
}
unsigned long CIECoreView::ControlGoForward( )
{
	PostMessage(WM_ASYNC_SIMPLE_CONTROL,(WPARAM)CC_GO_FORWARD,(LPARAM)0);
	return 0;
}
unsigned long CIECoreView::ControlQueryBackForwardStatus(BOOL *pbCanBack,BOOL *pbCanForward)
{
	if (pbCanBack)
	{
		*pbCanBack = m_bCanBack;
	}
	if (pbCanForward)
	{
		*pbCanForward = m_bCanForward;
	}
	return 0;
}

unsigned long CIECoreView::ControlRefresh( )
{
	PostMessage(WM_ASYNC_SIMPLE_CONTROL,(WPARAM)CC_REFRESH,(LPARAM)0);
	return 0;
}
unsigned long CIECoreView::ControlStopLoading()
{
	PostMessage(WM_ASYNC_SIMPLE_CONTROL,(WPARAM)CC_STOP,(LPARAM)0);
	return 0;
}
unsigned long CIECoreView::ControlEditCut()
{
	PostMessage(WM_ASYNC_SIMPLE_CONTROL,(WPARAM)CC_EDIT_CUT,(LPARAM)0);
	return 0;
}
unsigned long CIECoreView::ControlEditCopy()
{
	PostMessage(WM_ASYNC_SIMPLE_CONTROL,(WPARAM)CC_EDIT_COPY,(LPARAM)0);
	return 0;
}
unsigned long CIECoreView::ControlEditPaste()
{
	PostMessage(WM_ASYNC_SIMPLE_CONTROL,(WPARAM)CC_EDIT_PASTE,(LPARAM)0);
	return 0;
}
unsigned long CIECoreView::ControlEditSelectAll()
{
	PostMessage(WM_ASYNC_SIMPLE_CONTROL,(WPARAM)CC_EDIT_SELALL,(LPARAM)0);
	return 0;
}
unsigned long CIECoreView::ControlEditFind()
{
	//IWebBrowser2 *pWb2 = GetGlobalWebBrowser2();
	//pWb2->ExecFormsCommand(IDM_FIND, NULL, NULL);
	return 0;
}

unsigned long CIECoreView::ControlPrintWebView()
{
	CComQIPtr<IHTMLDocument2> pDoc2;
	pDoc2 = (IHTMLDocument2 *)GetHtmlDocument();

	PrintWebViewToFile(L"C:\\test\\printweb.jpg",pDoc2,m_hIEServer);

	return 0;
}

void CIECoreView::OnSize(UINT nType, int cx, int cy)
{
	CHtmlView::OnSize(nType, cx, cy);

	// TODO: 在此处添加消息处理程序代码
}

void CIECoreView::OnStatusTextChange(LPCTSTR lpszText)
{
	if (m_pNotifyer)
	{
		m_pNotifyer->NotifyStatusTextChange(m_PageID,lpszText);
	}
}

IWebBrowser * CIECoreView::GetGlobalWebBrowser(void)
{
// 	IWebBrowser *pTempWb = NULL;
// 	m_pBrowserApp->QueryInterface(IID_IWebBrowser,(void **)&pTempWb);
// 	return pTempWb;

 	IDispatch *pDisp;
 	theApp.GetGITPtr()->GetInterfaceFromGlobal(m_dwCookie,IID_IDispatch,(void **)&pDisp);
 	IWebBrowser *pWb = NULL;
 	pDisp->QueryInterface(IID_IWebBrowser,(void **)&pWb);
 	return pWb;
}
IWebBrowser2 * CIECoreView::GetGlobalWebBrowser2(void)
{
	//return m_pBrowserApp;

 	IWebBrowser2 *pWb2 = NULL;
 	IWebBrowser *pWb = GetGlobalWebBrowser();
 	if(pWb)
 	{
 		pWb->QueryInterface(IID_IWebBrowser2,(void **)&pWb2);
 	}
 
 	return pWb2;
}

static const CLSID CLSID_IEDTExplorerBar = {
	0x1a6fe369, 0xf28c, 0x4ad9, { 0xa3, 0xe6, 0x2b, 0xcb, 0x50, 0x80, 0x7c, 0xf1 }
};

static const IID IID_IEDevTools = {
	0x181e3828, 0xfe6e, 0x4602, { 0xa3, 0x27, 0x78, 0x6a, 0x76, 0xfd, 0xfb, 0x3a }
};

BOOL CIECoreView::LoadDeveloporTools()
{
	if ( NULL == m_pDevTools )
	{
		m_pDevTools = new CDevToolsHost(m_hIEServer);
		m_pDevTools->Show();
	}

	return FALSE;
}

void CIECoreView::OnPaint()
{
	CPaintDC dc(this); // device context for painting

}

HRESULT CIECoreView::OnShowContextMenu(DWORD dwID, LPPOINT ppt,
								  LPUNKNOWN pcmdtReserved, LPDISPATCH pdispReserved)
{
	return E_NOTIMPL;
}
HRESULT CIECoreView::OnGetExternal(LPDISPATCH *lppDispatch)
{
	//不应该返回E_NOIMPLE 否则腾讯视频看不了
	return S_OK;
}
HRESULT CIECoreView::OnGetHostInfo(DOCHOSTUIINFO *pInfo)
{
	pInfo->cbSize = sizeof(DOCHOSTUIINFO);
	pInfo-> dwFlags= DOCHOSTUIFLAG_NO3DBORDER|DOCHOSTUIFLAG_THEME;
	pInfo-> dwDoubleClick= DOCHOSTUIDBLCLK_DEFAULT;
	return S_OK;
}
HRESULT CIECoreView::OnShowUI(DWORD dwID,
						 LPOLEINPLACEACTIVEOBJECT pActiveObject,
						 LPOLECOMMANDTARGET pCommandTarget, LPOLEINPLACEFRAME pFrame,
						 LPOLEINPLACEUIWINDOW pDoc)
{
	return E_NOTIMPL;
}
HRESULT CIECoreView::OnHideUI()
{
	return E_NOTIMPL;
}
HRESULT CIECoreView::OnUpdateUI()
{
	return E_NOTIMPL;
}
HRESULT CIECoreView::OnEnableModeless(BOOL fEnable)
{
	return E_NOTIMPL;
}
HRESULT CIECoreView::OnDocWindowActivate(BOOL fActivate)
{
	return E_NOTIMPL;
}
HRESULT CIECoreView::OnFrameWindowActivate(BOOL fActivate)
{
	return E_NOTIMPL;
}
HRESULT CIECoreView::OnResizeBorder(LPCRECT prcBorder,
							   LPOLEINPLACEUIWINDOW pUIWindow, BOOL fFrameWindow)
{
	return E_NOTIMPL;
}
HRESULT CIECoreView::OnTranslateAccelerator(LPMSG lpMsg,
									   const GUID* pguidCmdGroup, DWORD nCmdID)
{
	return E_NOTIMPL;
}
HRESULT CIECoreView::OnGetOptionKeyPath(LPOLESTR* pchKey, DWORD dwReserved)
{
	return E_NOTIMPL;
}

HRESULT CIECoreView::OnGetOverrideKeyPath(LPOLESTR* pchKey, DWORD dwReserved)
{
	return E_NOTIMPL;

	//HRESULT hr;
	//WCHAR* szKey = L"Software\\58WangWei\\NetBarWebBrowser";

	//size_t cbLength;
	//hr = StringCbLengthW(szKey, 1280, &cbLength);

	//if (pchKey)
	//{
	//	*pchKey = (LPOLESTR)CoTaskMemAlloc(cbLength + sizeof(WCHAR));
	//	if (*pchKey)
	//		hr = StringCbCopyW(*pchKey, cbLength + sizeof(WCHAR), szKey);
	//}
	//else
	//	hr = E_INVALIDARG;

	//return hr;


}
HRESULT CIECoreView::OnFilterDataObject(LPDATAOBJECT pDataObject,
								   LPDATAOBJECT* ppDataObject)
{
	return E_NOTIMPL;
}
HRESULT CIECoreView::OnTranslateUrl(DWORD dwTranslate,
							   OLECHAR* pchURLIn, OLECHAR** ppchURLOut)
{
	return E_NOTIMPL;
}
HRESULT CIECoreView::OnGetDropTarget(LPDROPTARGET pDropTarget,
								LPDROPTARGET* ppDropTarget)
{
	return E_NOTIMPL;
}


BOOL CIECoreView::PreTranslateMessage(MSG* pMsg)
{

	if (pMsg->message == WM_KEYDOWN)
	{
		if (pMsg->wParam == VK_F12)
		{
			LoadDeveloporTools();
		}
	}

    return CHtmlView::PreTranslateMessage(pMsg);
}

void CIECoreView::OnClose()
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值

	__super::OnClose();
}

void CIECoreView::OnSetFocus(CWnd* pOldWnd)
{
	__super::OnSetFocus(pOldWnd);

	ControlSetFocus();
}