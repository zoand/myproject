// MusicDisplayWnd.cpp : 实现文件
//

#include "stdafx.h"
#include "DJMaster.h"
#include "MusicDisplayWnd.h"


// CMusicDisplayWnd

IMPLEMENT_DYNAMIC(CMusicDisplayWnd, CWnd)

CMusicDisplayWnd::CMusicDisplayWnd()
{
	m_bDragingButton = FALSE;
	m_bLockEdit = FALSE;

	m_nBtnSelIndex = -1;
	m_nDragTaggetIndexPre = -1;
	m_nDragTaggetIndexNext = -1;
	m_nMouseClkY = -1;

	m_nSelIndex = -1;

	m_hMemDC = NULL;
	m_hMemBmp = NULL;

	m_hWndNotify = NULL;
	m_uMsgNotify = 0;
}

CMusicDisplayWnd::~CMusicDisplayWnd()
{
}


BEGIN_MESSAGE_MAP(CMusicDisplayWnd, CWnd)
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_RBUTTONUP()
	ON_WM_SIZE()
	ON_WM_CREATE()
	ON_WM_VSCROLL()

	ON_WM_SETFOCUS()
	ON_WM_KEYDOWN()
END_MESSAGE_MAP()

VOID CMusicDisplayWnd::SetNotifyParam(HWND hWndNotify,UINT uMsgNotify)
{
	m_hWndNotify = hWndNotify;
	m_uMsgNotify = uMsgNotify;
}

BOOL CMusicDisplayWnd::AddMusic( LPVOID pParam ,LPCWSTR pszMusicDesc )
{
	if ( m_bLockEdit )
	{
		return FALSE;
	}

	const int nBtnHeight = 30;
	CRect rcClient;
	GetClientRect(&rcClient);

	BUTTON_INFO Info;
	Info.nIndex = m_lstButtons.size();
	Info.strMusicDesc = pszMusicDesc;
	Info.pParam = pParam;

	m_lstButtons.push_back(Info);

	InvalidateRect(&rcClient);

	return FALSE;
}

VOID CMusicDisplayWnd::DelMusic( int nIndex)
{
	if ( m_bLockEdit )
	{
		return;
	}

	if (nIndex < 0 || nIndex > m_lstButtons.size())
	{
		return;
	}

	LIST_MUSIC_BUTTON tmpListButtons;
	for (LIST_MUSIC_BUTTON_PTR it = m_lstButtons.begin();it!=m_lstButtons.end();it++)
	{
		if (it->nIndex != nIndex )
		{
			if( it->nIndex > nIndex )
			{
				it->nIndex--;
			}
			tmpListButtons.push_back(*it);
		}
	}

	m_lstButtons.clear();

	for (LIST_MUSIC_BUTTON_PTR it = tmpListButtons.begin();it!=tmpListButtons.end();it++)
	{
		m_lstButtons.push_back(*it);
	}

	this->RedrawWindow();
}

VOID CMusicDisplayWnd::CalcBtnRect(int nIndex,CRect &rcBtn)
{
	CRect rcClient;
	GetClientRect(&rcClient);

	rcBtn.left = 0;
	rcBtn.top = nIndex*30;
	rcBtn.right = rcClient.Width();
	rcBtn.bottom = (nIndex+1)*30;
	rcBtn.DeflateRect(2,2,2,2);
}

int CMusicDisplayWnd::MoveButton(int nIndex,int nTargetIndexPre,int nTargetIndexNext)
{
	int nNewIndex = -1;

	if ( m_bLockEdit )
	{
		return -1;
	}

	if ( nIndex < 0 || nIndex > m_lstButtons.size()|| nIndex == nTargetIndexPre || nIndex == nTargetIndexNext  || nTargetIndexNext - nTargetIndexPre != 1 )
	{
		return  -1;
	}
	
	//向前移动
	if ( nIndex > nTargetIndexNext )
	{
		for (LIST_MUSIC_BUTTON_PTR it = m_lstButtons.begin();it!=m_lstButtons.end();it++)
		{
			if (it->nIndex == nIndex )
			{
				nNewIndex = it->nIndex = nTargetIndexNext;
			}
			else if (it->nIndex > nTargetIndexPre && it->nIndex < nIndex )
			{
				it->nIndex++;
			}
		}
	}
	//向后移动
	else if( nIndex < nTargetIndexPre )
	{
		for (LIST_MUSIC_BUTTON_PTR it = m_lstButtons.begin();it!=m_lstButtons.end();it++)
		{
			if (it->nIndex == nIndex )
			{
				nNewIndex = it->nIndex = nTargetIndexPre;
			}
			else if (it->nIndex > nIndex && it->nIndex < nTargetIndexNext )
			{
				it->nIndex--;
			}
		}
	}

	return nNewIndex;

}

int  CMusicDisplayWnd::PointToIndex( int nX,int nY )
{
	int nIndex = -1;
	for (LIST_MUSIC_BUTTON_PTR it = m_lstButtons.begin();it!=m_lstButtons.end();it++)
	{
		CRect rcBtn;
		CPoint ptCheck;
		CalcBtnRect(it->nIndex,rcBtn);
		ptCheck.SetPoint(nX,nY);
		if (PtInRect(&rcBtn,ptCheck))
		{
			nIndex = it->nIndex;
			break;
		}
	}

	return nIndex;
}
int CMusicDisplayWnd::PointToIndex( LPPOINT pptCheck )
{
	return PointToIndex(pptCheck->x,pptCheck->y);
}

CMusicDisplayWnd::LIST_MUSIC_BUTTON_PTR CMusicDisplayWnd::IndexToIterator(int nIndex)
{
	LIST_MUSIC_BUTTON_PTR it = m_lstButtons.begin();
	for (;it!=m_lstButtons.end() ;it++)
	{
		if ( nIndex == it->nIndex)
		{
			break;
		}
	}

	return it;
}

BOOL CMusicDisplayWnd::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
	CRect rcClient;
	GetClientRect(&rcClient);

	pDC->Rectangle(rcClient);

	return TRUE;
}

void CMusicDisplayWnd::OnPaint()
{
	CPaintDC paintdc(this);

	CRect rcClient;

	GetClientRect(&rcClient);

	CDC dc;
	dc.Attach(m_hMemDC);

	dc.Rectangle(rcClient);

	dc.SetBkMode(TRANSPARENT);
	dc.SelectObject(GetStockObject(DEFAULT_GUI_FONT));

	if ( m_lstButtons.size() == 0 )
	{
		CRect rcTip;
		rcTip = rcClient;
		rcTip.bottom = rcTip.bottom/2;
		dc.DrawTextEx(L"拖拽音乐文件或文件夹到此处...",rcTip,DT_CENTER|DT_VCENTER|DT_SINGLELINE,0);
	}

	for (LIST_MUSIC_BUTTON_PTR it = m_lstButtons.begin();it!=m_lstButtons.end();it++)
	{
		CRect rcBtn;
		CalcBtnRect(it->nIndex,rcBtn);
		dc.Rectangle(rcBtn);
		CRect rcTemp;
		rcTemp = rcBtn;
		rcTemp.DeflateRect(1,1,1,1);

		if (m_nSelIndex == it->nIndex)
		{
			dc.FillSolidRect(rcTemp,RGB(210,210,210));
		}
		else
		{
			dc.FillSolidRect(rcTemp,RGB(230,230,230));
		}

		
		dc.DrawTextEx(it->strMusicDesc,rcBtn,DT_CENTER|DT_VCENTER|DT_SINGLELINE,0);

		CString strIndex;
		strIndex.Format(L"%d",it->nIndex+1);
		rcBtn.right = rcBtn.Height();
		dc.DrawTextEx(strIndex,rcBtn,DT_CENTER|DT_VCENTER|DT_SINGLELINE,0);
	}
	
	if ( m_bDragingButton )
	{
		CRect rcBtn;
		CalcBtnRect(m_nDragTaggetIndexNext,rcBtn);

		rcBtn.top-=5;
		rcBtn.bottom = rcBtn.top+5;

		dc.FillSolidRect(rcBtn,RGB(0,0,0));

	}

	paintdc.BitBlt(0,0,rcClient.Width(),rcClient.Height(),&dc,0,0,SRCCOPY);

	dc.Detach();
}

void CMusicDisplayWnd::OnLButtonDown(UINT nFlags, CPoint point)
{
	SetFocus();

	m_nSelIndex = m_nBtnSelIndex = PointToIndex(&point);
	m_nDragTaggetIndexPre = -1;
	m_nDragTaggetIndexNext = -1;
	m_nMouseClkY = point.y;

	CRect rcClient;
	GetClientRect(&rcClient);
	InvalidateRect(&rcClient);

	CWnd::OnLButtonDown(nFlags, point);
}

void CMusicDisplayWnd::OnLButtonUp(UINT nFlags, CPoint point)
{
	if ( m_nBtnSelIndex >=0 && m_nBtnSelIndex< m_lstButtons.size() )
	{
		//拖拽后释放鼠标
		if( m_bDragingButton )
		{
			m_nSelIndex = MoveButton(m_nBtnSelIndex,m_nDragTaggetIndexPre,m_nDragTaggetIndexNext);
		}
		else //点击了按钮
		{
			int nIndexClick = PointToIndex(&point);
			if ( nIndexClick != -1 && nIndexClick == m_nBtnSelIndex )
			{
				LIST_MUSIC_BUTTON_PTR it = IndexToIterator(nIndexClick);

				if(m_hWndNotify)
				{
					::PostMessage(m_hWndNotify,m_uMsgNotify,(WPARAM)(it->pParam),0);
				}

				//AfxMessageBox(it->strMusicDesc);
			}

		}

		CRect rcClient;
		GetClientRect(&rcClient);
		InvalidateRect(&rcClient);
	}

	m_bDragingButton = FALSE;
	m_nBtnSelIndex = -1;
	m_nDragTaggetIndexPre = -1;
	m_nDragTaggetIndexNext = -1;
	m_nMouseClkY = -1;

	CWnd::OnLButtonUp(nFlags, point);
}

void CMusicDisplayWnd::OnMouseMove(UINT nFlags, CPoint point)
{
	if ( m_bLockEdit )
	{
		return;
	}
	if ( m_nBtnSelIndex >= 0 && m_nBtnSelIndex < m_lstButtons.size() && (abs(m_nMouseClkY-point.y) > 10 || m_bDragingButton) )
	{
		m_bDragingButton = TRUE;
	}

	if ( m_bDragingButton )
	{
		CRect rcClient;
		GetClientRect(&rcClient);

		int nBtnCount = m_lstButtons.size();
		for (int i=-1;i<nBtnCount;i++)
		{
			CRect rcPreBtn;
			CRect rcNextBtn;
			CalcBtnRect(i,rcPreBtn);
			CalcBtnRect(i+1,rcNextBtn);

			if (point.y <= rcNextBtn.CenterPoint().y && point.y >= rcPreBtn.CenterPoint().y)
			{
				if (m_nDragTaggetIndexPre != i || m_nDragTaggetIndexNext != i+1 )
				{
					m_nDragTaggetIndexPre = i;
					m_nDragTaggetIndexNext = i+1;

					InvalidateRect(&rcClient);
				}

				break;
			}

		}
	}

	CWnd::OnMouseMove(nFlags, point);
}

void CMusicDisplayWnd::OnRButtonUp(UINT nFlags, CPoint point)
{
	if( FALSE == m_bLockEdit )
	{
		int nIndexClick = PointToIndex(&point);
		if ( nIndexClick >= 0 )
		{
			CPoint ptCurMouse;
			GetCursorPos(&ptCurMouse);
			CMenu menu;
			menu.LoadMenu(IDR_MENU1);
			int nMenuCmd = menu.GetSubMenu(0)->TrackPopupMenu(TPM_RETURNCMD,ptCurMouse.x,ptCurMouse.y,this);
			if (ID_ROOT_32771 == nMenuCmd )
			{
				DelMusic(nIndexClick);
			}
		}
		
	}
	CWnd::OnRButtonUp(nFlags, point);
}

void CMusicDisplayWnd::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);

	ReCreateMemDCIfNeed(cx,cy);
}

VOID CMusicDisplayWnd::ReCreateMemDCIfNeed(int cx, int cy)
{
	if (m_hMemDC)
	{
		::DeleteDC(m_hMemDC);
	}

	if (m_hMemBmp)
	{
		::DeleteObject(m_hMemBmp);
	}

	HDC hDC = GetDC()->m_hDC;
	m_hMemDC = CreateCompatibleDC(hDC);
	m_hMemBmp = CreateCompatibleBitmap(hDC,cx,cy);

	SelectObject(m_hMemDC,m_hMemBmp);
}

int CMusicDisplayWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	ReCreateMemDCIfNeed(lpCreateStruct->cx,lpCreateStruct->cy);

	return 0;
}

VOID CMusicDisplayWnd::LockEdit(bool bLock)
{
	m_bLockEdit = bLock;
	return VOID();
}


void CMusicDisplayWnd::OnSetFocus(CWnd* pOldWnd)
{
	CWnd::OnSetFocus(pOldWnd);

	// TODO: 在此处添加消息处理程序代码
}

void CMusicDisplayWnd::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值

	CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

BOOL CMusicDisplayWnd::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->hwnd == m_hWnd )
	{
		if ( WM_KEYDOWN == pMsg->message || WM_KEYUP == pMsg->message )
		{
			if (WM_KEYDOWN == pMsg->message)
			{
				if (VK_UP == pMsg->wParam )
				{
					m_nSelIndex--;
					if (m_nSelIndex < 0 )
					{
						m_nSelIndex = 0;
					}
						
					CRect rcClient;
					GetClientRect(&rcClient);
					InvalidateRect(&rcClient);
				}
				else if( VK_DOWN == pMsg->wParam )
				{
					m_nSelIndex++;
					if (m_nSelIndex >= m_lstButtons.size())
					{
						m_nSelIndex = m_lstButtons.size()-1;
					}
					CRect rcClient;
					GetClientRect(&rcClient);
					InvalidateRect(&rcClient);

				}
			}
			return TRUE;
		}
	}

	return CWnd::PreTranslateMessage(pMsg);
}
