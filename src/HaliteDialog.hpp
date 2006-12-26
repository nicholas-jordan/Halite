
#pragma once
 
#include "stdAfx.hpp"
#include "DdxEx.hpp"
#include "Halite.hpp"

class ui_signal;
class single_selection_manager;

class HaliteDialog :
	public CDialogImpl<HaliteDialog>,
	public CDialogResize<HaliteDialog>,
	public CWinDataExchangeEx<HaliteDialog>,
	private noncopyable
{
protected:
	typedef HaliteDialog thisClass;
	typedef CDialogImpl<HaliteDialog> baseClass;
	typedef CDialogResize<HaliteDialog> resizeClass;

public:
	enum { IDD = IDD_HALITEDLG };
	
	HaliteDialog(ui_signal& ui_sig, single_selection_manager& single_sel);	
	BOOL PreTranslateMessage(MSG* pMsg)	{ return this->IsDialogMessage(pMsg); }
	
	void saveStatus(); // ****** NEEDS TO BE ANOTHER SIGNAL CLASS ******
  	
	BEGIN_MSG_MAP(thisClass)
		MSG_WM_INITDIALOG(onInitDialog)
		MSG_WM_CLOSE(onClose)	
		COMMAND_RANGE_CODE_HANDLER_EX(IDC_EDITTLU, IDC_EDITNCU, EN_KILLFOCUS, OnEditKillFocus)
		
		if(uMsg == WM_FORWARDMSG)
			if(PreTranslateMessage((LPMSG)lParam)) return TRUE;

		COMMAND_ID_HANDLER_EX(BTNPAUSE, onPause)		
		COMMAND_ID_HANDLER_EX(BTNREANNOUNCE, onReannounce)
		COMMAND_ID_HANDLER_EX(BTNREMOVE, onRemove)
		
	//	MSG_WM_CTLCOLORSTATIC(OnCltColor)
		
		CHAIN_MSG_MAP(resizeClass)
	END_MSG_MAP()
	
	BEGIN_DDX_MAP(thisClass)
        DDX_EX_INT_POSITIVE_LIMIT(IDC_EDITNCD, NoConnDown, 2, true)
        DDX_EX_INT_POSITIVE_LIMIT(IDC_EDITNCU, NoConnUp, 2, true)
        DDX_EX_FLOAT_POSITIVE(IDC_EDITTLD, TranLimitDown)
        DDX_EX_FLOAT_POSITIVE(IDC_EDITTLU, TranLimitUp)
    END_DDX_MAP()

	BEGIN_DLGRESIZE_MAP(thisClass)
		DLGRESIZE_CONTROL(BTNPAUSE, (DLSZ_MOVE_X))
		DLGRESIZE_CONTROL(BTNREANNOUNCE, (DLSZ_MOVE_X))
		DLGRESIZE_CONTROL(BTNREMOVE, (DLSZ_MOVE_X))
		
		DLGRESIZE_CONTROL(IDC_TL, (DLSZ_MOVE_X))
		DLGRESIZE_CONTROL(IDC_NC, (DLSZ_MOVE_X))
		DLGRESIZE_CONTROL(IDC_TLD, (DLSZ_MOVE_X))
		DLGRESIZE_CONTROL(IDC_TLU, (DLSZ_MOVE_X))
		DLGRESIZE_CONTROL(IDC_NCU, (DLSZ_MOVE_X))
		DLGRESIZE_CONTROL(IDC_NCD, (DLSZ_MOVE_X))
		
		DLGRESIZE_CONTROL(IDC_EDITNCD, (DLSZ_MOVE_X))
		DLGRESIZE_CONTROL(IDC_EDITTLD, (DLSZ_MOVE_X))
		DLGRESIZE_CONTROL(IDC_EDITTLU, (DLSZ_MOVE_X))
		DLGRESIZE_CONTROL(IDC_EDITNCU, (DLSZ_MOVE_X))
		
		DLGRESIZE_CONTROL(TORRENTPROG, (DLSZ_SIZE_X))
		
		DLGRESIZE_CONTROL(IDC_NAME, (DLSZ_SIZE_X))
		DLGRESIZE_CONTROL(IDC_TRACKER, (DLSZ_SIZE_X))
		DLGRESIZE_CONTROL(IDC_STATUS, (DLSZ_SIZE_X))
		DLGRESIZE_CONTROL(IDC_COMPLETE, (DLSZ_SIZE_X))
		
		DLGRESIZE_CONTROL(LISTPEERS, (DLSZ_SIZE_X | DLSZ_SIZE_Y))
		DLGRESIZE_CONTROL(IDC_DETAILS_GROUP, (DLSZ_SIZE_X | DLSZ_SIZE_Y))
	END_DLGRESIZE_MAP()
	
protected:	
	LRESULT onInitDialog(HWND, LPARAM);	
	void onClose();	
	
	void onPause(UINT, int, HWND);	
	void onReannounce(UINT, int, HWND);	
	void onRemove(UINT, int, HWND);
	
	LRESULT OnEditKillFocus(UINT uCode, int nCtrlID, HWND hwndCtrl);
	LRESULT OnCltColor(HDC hDC, HWND hWnd);
	
	void selectionChanged(const string& torrent_name);	
	void updateDialog();

private:
	CButton m_btn_start;
	CListViewCtrl m_list;
	CContainedWindow m_wndNCD;
	CProgressBarCtrl m_prog;
	
	int NoConnDown, NoConnUp;
	float TranLimitDown, TranLimitUp;
	
	ui_signal& ui_;
	single_selection_manager& single_selection_;
};
