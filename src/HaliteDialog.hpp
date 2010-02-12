
//         Copyright Eoin O'Callaghan 2006 - 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#define HAL_NAME_STATIC                 15012
#define HAL_TRACKER_STATIC              15013
#define HAL_STATUS_STATIC               15014
#define HAL_TIME_STATIC                 15015
#define HAL_COMPLETED_STATIC            15016

#ifndef RC_INVOKED

#include "stdAfx.hpp"
#include "Aero.h"

#include "DdxEx.hpp"
#include "HaliteSortListViewCtrl.hpp"
#include "HaliteDialogBase.hpp"
#include "HaliteEditCtrl.hpp"
#include "halIni.hpp"

#include "halTorrent.hpp"
#include "halEvent.hpp"

class ui_signal;

class HaliteDialog :
	public WTL::CAeroDialogImpl<HaliteDialog>,
	public ATL::CAutoSizeWindow<HaliteDialog, false>,
	public WTLx::WinDataExchangeEx<HaliteDialog>,
	public CHaliteDialogBase<HaliteDialog>,
	private boost::noncopyable
{	
	typedef HaliteDialog thisClass;
	typedef WTL::CAeroDialogImpl<thisClass> baseClass;
	typedef ATL::CAutoSizeWindow<thisClass, false> autosizeClass;
	typedef CHaliteDialogBase<thisClass> dialogBaseClass;
		
	class DialogListView :
		public CHaliteSortListViewCtrl<DialogListView, std::wstring>,
		public hal::IniBase<DialogListView>,
		//public hal::update_lockable<CHaliteSortListViewCtrl<TBase, DataType> >,
		private boost::noncopyable
	{
	protected:
		typedef HaliteDialog::DialogListView thisClass;
		typedef hal::IniBase<thisClass> iniClass;
		typedef CHaliteSortListViewCtrl<DialogListView, std::wstring> listClass;
		typedef const hal::peer_detail pD;
	
		friend class listClass;

	public:	
		enum { 
			LISTVIEW_ID_MENU = 0,
			LISTVIEW_ID_COLUMNNAMES = HAL_DIALOGPEER_LISTVIEW_COS,
			LISTVIEW_ID_COLUMNWIDTHS = HAL_DIALOGPEER_LISTVIEW_COS_DEFAULTS
		};
	
		BEGIN_MSG_MAP_EX(thisClass)
			try
			{
			MSG_WM_DESTROY(OnDestroy)

			REFLECTED_NOTIFY_CODE_HANDLER(SLVN_SORTCHANGED, OnSortChanged)
			REFLECTED_NOTIFY_CODE_HANDLER(LVN_GETDISPINFO, OnGetDispInfo)
			}
			HAL_ALL_EXCEPTION_CATCH(L"in DialogListView MSG_MAP")

			CHAIN_MSG_MAP(listClass)
			DEFAULT_REFLECTION_HANDLER()
		END_MSG_MAP()
	
		DialogListView() :
			iniClass("listviews/dialog", "DialogPeersList")
		{}
		
		void saveSettings()
		{
			GetListViewDetails();
			save_to_ini();
		}
		
		bool SubclassWindow(HWND hwnd)
		{
			if(!parentClass::SubclassWindow(hwnd))
				return false;
				
			InitialSetup();	
			
			std::vector<wstring> names;	
			wstring column_names = hal::app().res_wstr(LISTVIEW_ID_COLUMNNAMES);

			// "Peer;Country;Download;Upload;Type;Client,Status"
			boost::split(names, column_names, boost::is_any_of(L";"));
			
			array<int, 6> widths = {100,20,70,70,70,100};
			array<int, 6> order = {0,1,2,3,4,5};
			array<bool, 6> visible = {true,true,true,true,true,true};

			for (int i=0, e=6; i < e; ++i)
			{
				AddColumn(names[i].c_str(), i, visible[i], widths[i]);
			}	
						
			load_from_ini();		

			for (unsigned i=0, e = hal::peer_detail::client_e-hal::peer_detail::ip_address_e; i <= e; ++i)
				SetColumnSortType(i, i + (WTL::LVCOLSORT_LAST+1+hal::peer_detail::ip_address_e));	
				
			return true;
		}
		
		void OnDestroy()
		{
			saveSettings();
		}
		
		friend class boost::serialization::access;
		template<class Archive>
		void serialize(Archive& ar, const unsigned int version)
		{
			ar & boost::serialization::make_nvp("listview", 
				boost::serialization::base_object<listClass>(*this));
		}

		LRESULT OnGetDispInfo(int, LPNMHDR pnmh, BOOL&);
		LRESULT OnSortChanged(int, LPNMHDR pnmh, BOOL&);

		bool sort_list_comparison(std::wstring l,  std::wstring r, size_t index, bool ascending);
		
		void uiUpdate(const hal::torrent_details_manager& tD);
		
	private:
		hal::peer_details_vec peer_details_;
	};
	
public:
	enum { IDD = HAL_HALITEDLG };

	HaliteDialog(HaliteWindow& HalWindow);
	BOOL PreTranslateMessage(MSG* pMsg)	{ return this->IsDialogMessage(pMsg); }

	void saveStatus();

	BEGIN_MSG_MAP_EX(thisClass)
		try
		{
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_CLOSE(OnClose)
		MESSAGE_HANDLER_EX(WM_USER_HAL_EDITCHANGED, OnHalEditChanged)
		
		if(uMsg == WM_FORWARDMSG)
			if(PreTranslateMessage((LPMSG)lParam)) return TRUE;

		COMMAND_ID_HANDLER_EX(BTNPAUSE, OnPause)
		COMMAND_ID_HANDLER_EX(BTNREANNOUNCE, OnReannounce)
		COMMAND_ID_HANDLER_EX(BTNREMOVE, OnRemove)
		}
		HAL_ALL_EXCEPTION_CATCH(L"in HaliteDialog MSG_MAP")

		CHAIN_MSG_MAP(baseClass)
		CHAIN_MSG_MAP(dialogBaseClass)
		CHAIN_MSG_MAP(autosizeClass)
		REFLECT_NOTIFICATIONS()
	END_MSG_MAP()

	BEGIN_DDX_MAP(thisClass)
	END_DDX_MAP()

	static CWindowMapStruct* GetWindowMap();
	
	void uiUpdate(const hal::torrent_details_manager& allTorrents); 
	void focusChanged(const hal::torrent_details_ptr pT);
	
protected:
	LRESULT OnInitDialog(HWND, LPARAM);
	void OnClose();

	void OnPause(UINT, int, HWND);
	void OnReannounce(UINT, int, HWND);
	void OnRemove(UINT, int, HWND);

	LRESULT OnCltColor(HDC hDC, HWND hWnd);	
	LRESULT OnHalEditChanged(UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	WTL::CButton m_btn_start;
	DialogListView m_list;
	WTL::CProgressBarCtrl m_prog;
	
	CHaliteEditCtrl<int> totalConnections_;
	CHaliteEditCtrl<int> uploadConnections_;
	CHaliteEditCtrl<float> downloadRate_;
	CHaliteEditCtrl<float> uploadRate_;
	
	string current_torrent_name_;
};

#endif // RC_INVOKED
