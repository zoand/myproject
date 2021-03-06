// IECoreView.cpp : 实现文件
//

#include "stdafx.h"
#include "IECoreView.h"
#include <ExDispid.h>
#include <strsafe.h>
#include "IIEOleClientSite.h"
#include "MainFrm.h"

#include <WinInet.h>
#pragma comment(lib,"wininet.lib")

#include <detours.h>
#pragma comment(lib,"detours.lib")


extern BOOL g_bPhoneMode;

BOOL (WINAPI *pHttpSendRequestW)(
								 __in HINTERNET hRequest,
								 __in_ecount_opt(dwHeadersLength) LPCWSTR lpszHeaders,
								 __in DWORD dwHeadersLength,
								 __in_bcount_opt(dwOptionalLength) LPVOID lpOptional,
								 __in DWORD dwOptionalLength 
								 ) = HttpSendRequestW;
BOOL WINAPI MyHttpSendRequestW(
							   __in HINTERNET hRequest,
							   __in_ecount_opt(dwHeadersLength) LPCWSTR lpszHeaders,
							   __in DWORD dwHeadersLength,
							   __in_bcount_opt(dwOptionalLength) LPVOID lpOptional,
							   __in DWORD dwOptionalLength 
							   )
{
	CString strAddHeader;
	strAddHeader = L"User-Agent: "+CIECoreView::m_strUserAgent;
	BOOL TReturn = pHttpSendRequestW(
		hRequest,
		strAddHeader.GetBuffer(),
		strAddHeader.GetLength(),
		lpOptional,
		dwOptionalLength
		);
	DWORD dwErrorCode = GetLastError();
	return TReturn;
};


typedef HRESULT (WINAPI *TypeGetPlatform)( IOmNavigator *pThis, BSTR *bstrPlatform );
TypeGetPlatform pGetNavigator = NULL;
HRESULT WINAPI MyGetPlatform( IOmNavigator *pThis, BSTR *bstrPlatform )
{
	CString strPlatform;
	strPlatform = CIECoreView::m_strPlatform;
	*bstrPlatform = strPlatform.AllocSysString();
#ifdef DEBUG
	OutputDebugStringW(L"伪装Platform信息："+strPlatform+L"\n");
#endif
	return S_OK;
}


typedef HRESULT (WINAPI *TypeGetUserAgent)( IOmNavigator *pThis, BSTR *bstrUserAgent );
TypeGetUserAgent pGetUserAgent = NULL;
HRESULT WINAPI MyGetUserAgent( IOmNavigator *pThis, BSTR *bstrUserAgent )
{
	CString strUserAgent;
	strUserAgent = CIECoreView::m_strUserAgent;
	*bstrUserAgent = strUserAgent.AllocSysString();
#ifdef DEBUG
	OutputDebugStringW(L"伪装UserAgent信息："+strUserAgent+L"\n");
#endif
	return S_OK;
}

IWebBrowser2 * GetIWebBrowser2Interface(HWND hBrowserWnd) 
{
	CoInitialize(NULL);

	HRESULT hr;
	LRESULT lRes; 
	const UINT nMsg = ::RegisterWindowMessage( L"WM_HTML_GETOBJECT" );
	::SendMessageTimeout( hBrowserWnd, nMsg, 0L, 0L, SMTO_ABORTIFHUNG, 1000, (DWORD_PTR*)&lRes );
	static LPFNOBJECTFROMLRESULT pfObjectFromLresult = NULL;
	if ( NULL == pfObjectFromLresult )
	{
		HINSTANCE hInst = ::LoadLibrary( L"OLEACC.DLL" );
		if ( hInst )
		{
			pfObjectFromLresult = (LPFNOBJECTFROMLRESULT)::GetProcAddress( hInst, "ObjectFromLresult" );
		}
	}
	if ( pfObjectFromLresult  )
	{
		CComPtr<IServiceProvider> spServiceProv;
		hr = (*pfObjectFromLresult)( lRes, IID_IServiceProvider, 0, (void**)&spServiceProv );
		if ( SUCCEEDED(hr) )
		{
			IWebBrowser2* pWebBrowser2=NULL;
			hr = spServiceProv->QueryService(SID_SWebBrowserApp,
				IID_IWebBrowser2,(void**)&pWebBrowser2);
			return pWebBrowser2;
		} 
	}

	CoUninitialize();

	return NULL;
}

BOOL                   CIECoreView::bInternalHook = FALSE;
CString                CIECoreView::m_strUserAgent = L"Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1)";
CString                CIECoreView::m_strPlatform = L"win32";

BEGIN_EVENTSINK_MAP(CIECoreView,  CHtmlView)
ON_EVENT(CIECoreView,  AFX_IDW_PANE_FIRST ,DISPID_NEWWINDOW3,NewWindow3,VTS_PDISPATCH  VTS_PBOOL  VTS_I4  VTS_BSTR  VTS_BSTR)
END_EVENTSINK_MAP()

IMPLEMENT_DYNCREATE(CIECoreView, CHtmlView)

CIECoreView::CIECoreView()
{
	bCanBack = FALSE;
	bCanForward = FALSE;
	bInit = FALSE;
	m_bFixed = FALSE;
	m_hIEServerWnd = NULL;
}

CIECoreView::~CIECoreView()
{
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
	ON_WM_TIMER()
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

HWND CIECoreView::GetIEServerWnd()
{
	return m_hIEServerWnd;
}
int CIECoreView::OnCreate(LPCREATESTRUCT lpcs)
{
	int nRes = CHtmlView::OnCreate(lpcs);

	return nRes;
}
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

	CString strPopupUrl;
	strPopupUrl = bstrUrl;

	{
#ifdef DEBUG
		CString strMsgOut;
		strMsgOut.Format(L"阻止新窗口弹出：%s\r\n",strPopupUrl);
		OutputDebugStringW(strMsgOut);
#endif
		*Cancel = VARIANT_TRUE;
	}

	
} 


void CIECoreView::OnTitleChange(LPCTSTR lpszText)
{
    if(CString(lpszText)==_T("about:blank"))
    {
        return ;
    }
    
	m_strLastTitle = lpszText;

	CHtmlView::OnTitleChange(lpszText);
}

void CIECoreView::OnCommandStateChange(long nCommand, BOOL bEnable)
{
	BOOL bChanged = TRUE;
	switch (nCommand)
	{
	case CSC_NAVIGATEBACK:
		{
			if( bCanBack != bEnable )
			{
				int a=0;
			}
			bCanBack = bEnable;
		}
		break;
	case CSC_NAVIGATEFORWARD:
		{
			if( bCanForward != bEnable )
			{
				int a=0;
			}
			bCanForward = bEnable;
		}
		break;
	default:
		{
			bChanged = FALSE;
		}
	}
	CHtmlView::OnCommandStateChange(nCommand, bEnable);
}

void CIECoreView::NavigateComplete2(LPDISPATCH pDisp, VARIANT* URL)
{
	if(pDisp == GetApplication() /* 仅处理最顶层Frame的事件 */)
	{
		if ( g_bPhoneMode && FALSE == bInternalHook )
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
				//DetourAttach((LPVOID*)&pInternetOpenW,(PVOID)&MyInternetOpenW);
				DetourAttach((LPVOID*)&pHttpSendRequestW,(PVOID)&MyHttpSendRequestW);
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
						m_hIEServerWnd=::FindWindowExW(hDoc,NULL,TEXT("Internet Explorer_Server"),NULL);
					}
				}
			}
			if(m_hIEServerWnd!=NULL )
			{
				m_wndFixer.SubclassWindow(m_hIEServerWnd);
			}
		}

		CString strLocationUrl;
		strLocationUrl = GetLocationURL();
		if (strLocationUrl != m_strLastLocantionUrl)
		{
			if(strLocationUrl!=_T("about:blank"))
			{
				m_strLastLocantionUrl = strLocationUrl;
			}
		}
		
		PostThreadMessageW(GetCurrentThreadId(),WM_USER+1112,(WPARAM)this,(LPARAM)NULL);
		

		OnMainDocumentComplete(pDisp, URL);
	}
}

void CIECoreView::BeforeNavigate2(LPDISPATCH pDisp, VARIANT* URL,
							 VARIANT* Flags, VARIANT* TargetFrameName, VARIANT* PostData,
							 VARIANT* Headers, VARIANT_BOOL* Cancel)
{

	if (GetApplication() == pDisp)
	{
	}
}

void CIECoreView::OnMainDocumentComplete(LPDISPATCH pDisp, VARIANT* URL)
{

}

void CIECoreView::DocumentComplete(LPDISPATCH pDisp, VARIANT* URL)
{
	if (GetApplication() == pDisp)
	{
		OnMainDocumentComplete(pDisp,URL);
	}
}
void CIECoreView::NavigateError(LPDISPATCH pDisp, VARIANT* pvURL,
						   VARIANT* pvFrame, VARIANT* pvStatusCode, VARIANT_BOOL* pvbCancel)
{
	if(pDisp == GetApplication()/* 仅处理最顶层Frame的事件 */)
	{
		
	}
}

void CIECoreView::OnSize(UINT nType, int cx, int cy)
{
	CHtmlView::OnSize(nType, cx, cy);

	// TODO: 在此处添加消息处理程序代码
}

void CIECoreView::OnStatusTextChange(LPCTSTR lpszText)
{
}

IWebBrowser * CIECoreView::GetGlobalWebBrowser(void)
{
 	IWebBrowser *pWb = NULL;
	IWebBrowser2 *pWb2 = NULL;

	pWb2 = GetIWebBrowser2Interface(m_hIEServerWnd);
	if (pWb2)
	{
		pWb2->QueryInterface(IID_IWebBrowser,(PVOID *)&pWb);
		pWb2->Release();
	}
 	return pWb;
}
IWebBrowser2 * CIECoreView::GetGlobalWebBrowser2(void)
{
	return GetIWebBrowser2Interface(m_hIEServerWnd);
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

HRESULT CIECoreView::OnDownloadFile( LPCWSTR pszFileUrl )
{
	return S_OK;
}

BOOL CIECoreView::PreTranslateMessage(MSG* pMsg)
{
    return CHtmlView::PreTranslateMessage(pMsg);
}

