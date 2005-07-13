#include <windows.h>
#include <commctrl.h>
#include <math.h>

#include "winclient.h" // for utility functions like DB2VOL, etc
#include "chanmix.h"
#include "resource.h"

#define MIN_VOL 0.000001

ChanMixer::ChanMixer()
{
  m_nScrollPos_w=0;
  m_ww=0;
  m_w=0;
  m_maxpos_w=0;

  m_values_used=0;
  m_hwnd=0;
  memset(m_values,0,sizeof(m_values));
  memset(m_sliders,0,sizeof(m_sliders));
}

ChanMixer::~ChanMixer()
{
  if (m_hwnd && IsWindow(m_hwnd)) DestroyWindow(m_hwnd);
}


void ChanMixer::CreateWnd(HINSTANCE hInst, HWND parent)
{
  if (m_hwnd)
  {
    SetParent(m_hwnd,parent);
  }
  else
  {
    m_hwnd=CreateDialogParam(hInst,MAKEINTRESOURCE(IDD_MIXERDLG),parent,_DlgProc,(LPARAM)this);
    // create dialog
  }
}

void ChanMixer::LoadConfig(const char *str)
{
}

void ChanMixer::SaveConfig(WDL_String *str)
{
}

void ChanMixer::MixData(float **inbuf, int in_offset, int innch, int chidx, float *outbuf, int len)
{
  memset(outbuf,0,len*sizeof(float));
  if (m_values_used < innch) innch=m_values_used;
  int ch;
  for (ch = 0; ch < innch; ch ++)
  {
    if (m_values[ch] >= MIN_VOL) // -100dB or so
    {
      int l=len;
      float *in=inbuf[ch]+in_offset;
      float *out=outbuf;
      while (l--) *out++ += *in++;
    }
  }
}



#define MKDLGPROC(name) \
  BOOL WINAPI ChanMixer::_##name(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {  \
    ChanMixer *t; \
    if (uMsg == WM_INITDIALOG) SetWindowLong(hwndDlg,GWL_USERDATA,(LONG)(t=(ChanMixer *)lParam)); \
    else t=(ChanMixer*)GetWindowLong(hwndDlg,GWL_USERDATA); \
    return t?t->name(hwndDlg,uMsg,wParam,lParam):0;  \
  }

MKDLGPROC(DlgProc)
MKDLGPROC(DlgProc_scrollchild)


BOOL ChanMixer::DlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_INITDIALOG:
      m_childwnd=CreateDialogParam((HINSTANCE)GetWindowLong(hwndDlg,GWL_HINSTANCE),MAKEINTRESOURCE(IDD_EMPTY),hwndDlg,_DlgProc_scrollchild,(LPARAM)this);
      ShowWindow(m_childwnd,SW_SHOWNA);
    case WM_SIZE:
    case WM_USER+1:
      if (uMsg != WM_SIZE || wParam != SIZE_MINIMIZED)
      {     
        RECT r;
        GetWindowRect(hwndDlg,&r);
        m_ww=r.right-r.left;

        GetWindowRect(m_childwnd,&r);
        m_w=r.right-r.left;

        m_maxpos_w=m_w-m_ww;
        if (m_maxpos_w < 0) m_maxpos_w=0;

        {
          SCROLLINFO si={sizeof(si),SIF_RANGE|SIF_PAGE,0,m_w,};
          si.nPage=m_ww;
          if (m_nScrollPos_w+m_ww > m_w)
          {
            int np=m_w-m_ww;
            if (np<0)np=0;
            si.nPos=np;
            si.fMask |= SIF_POS;

            ScrollWindow(hwndDlg,m_nScrollPos_w-np,0,NULL,NULL);
            m_nScrollPos_w=np;
          }

          SetScrollInfo(hwndDlg,SB_HORZ,&si,TRUE);
        }

      }
    return 0;

    case WM_HSCROLL:
      {
        int nSBCode=LOWORD(wParam);
	      int nDelta=0;

	      int nMaxPos = m_maxpos_w;

	      switch (nSBCode)
	      {
          case SB_TOP:
            nDelta = - m_nScrollPos_w;
          break;
          case SB_BOTTOM:
            nDelta = nMaxPos - m_nScrollPos_w;
          break;
	        case SB_LINEDOWN:
		        if (m_nScrollPos_w < nMaxPos) nDelta = min(nMaxPos/100,nMaxPos-m_nScrollPos_w);
		      break;
	        case SB_LINEUP:
		        if (m_nScrollPos_w > 0) nDelta = -min(nMaxPos/100,m_nScrollPos_w);
          break;
          case SB_PAGEDOWN:
		        if (m_nScrollPos_w < nMaxPos) nDelta = min(nMaxPos/10,nMaxPos-m_nScrollPos_w);
		      break;
          case SB_THUMBTRACK:
	        case SB_THUMBPOSITION:
		        nDelta = (int)HIWORD(wParam) - m_nScrollPos_w;
		      break;
	        case SB_PAGEUP:
		        if (m_nScrollPos_w > 0) nDelta = -min(nMaxPos/10,m_nScrollPos_w);
		      break;
	      }
        if (nDelta) 
        {
          m_nScrollPos_w += nDelta;
	        SetScrollPos(hwndDlg,SB_HORZ,m_nScrollPos_w,TRUE);
	        ScrollWindow(hwndDlg,-nDelta,0,NULL,NULL);
        }
      }
    break; 
  
    case WM_GETMINMAXINFO:
      {
        LPMINMAXINFO p=(LPMINMAXINFO)lParam;
        p->ptMinTrackSize.x = 200;
        p->ptMinTrackSize.y = 300;
      }
    return 0;
    case WM_CLOSE:
      ShowWindow(hwndDlg,SW_HIDE);
    return 0;
  }
  return 0;
}

BOOL ChanMixer::DlgProc_scrollchild(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_INITDIALOG:
      int x;
      {
        int maxy=100,maxx=100;
        for (x = 0; x < m_values_used; x ++)
        {
          if (!m_sliders[x])
          {
            HWND h=CreateDialogParam((HINSTANCE)GetWindowLong(hwndDlg,GWL_HINSTANCE),MAKEINTRESOURCE(IDD_MIXERITEM),hwndDlg,_DlgProc_item,(LPARAM)x);
            m_sliders[x]=h;

            RECT r;
            GetClientRect(h,&r);
            if (r.bottom-r.top > maxy) maxy=r.bottom-r.top;
            SetWindowPos(h,0,maxx=x*(r.right-r.left),0,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
            ShowWindow(h,SW_SHOWNA);
          }
        }
        SetWindowPos(hwndDlg,NULL,0,0,maxx,maxy,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        for (; x < MAX_CHANMIX_CHANS; x ++)
        {
          if (m_sliders[x])
          {
            DestroyWindow(m_sliders[x]);
            m_sliders[x]=0;
          }
        }
        if (uMsg != WM_INITDIALOG) SendMessage(GetParent(hwndDlg),WM_USER+1,0,0);
      }
    return 0;
  }
  return 0;
}

BOOL WINAPI ChanMixer::_DlgProc_item(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  int chid;
  if (uMsg == WM_INITDIALOG) SetWindowLong(hwndDlg,GWL_USERDATA,(LONG)(chid=(int)lParam));
  else chid=(int)GetWindowLong(hwndDlg,GWL_USERDATA);

  ChanMixer *_this=(ChanMixer*)GetWindowLong(GetParent(hwndDlg),GWL_USERDATA);

  if (_this) switch (uMsg)
  {
    case WM_INITDIALOG:
      SendDlgItemMessage(hwndDlg,IDC_VOL,TBM_SETRANGE,FALSE,MAKELONG(0,100));
      SendDlgItemMessage(hwndDlg,IDC_VOL,TBM_SETTIC,FALSE,63);       
      SendDlgItemMessage(hwndDlg,IDC_VOL,TBM_SETPOS,TRUE,(LPARAM)DB2SLIDER(VAL2DB(_this->m_values[chid])));      

    break;
    case WM_VSCROLL:
      {
        double pos=(double)SendMessage((HWND)lParam,TBM_GETPOS,0,0);

        _this->m_values[chid] = (float)DB2VAL(SLIDER2DB(pos));
      }
    break;

  }
  if (uMsg == WM_INITDIALOG || uMsg == WM_VSCROLL)
  {
    char buf[128];
    sprintf(buf,"%.2f dB",DB2VAL(SLIDER2DB(_this->m_values[chid])));
    SetDlgItemText(hwndDlg,IDC_LABEL2,buf);      
  }
  return 0;
}