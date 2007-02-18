
#include "stdAfx.hpp"

#include <iostream>
#include <fstream>
#include <iterator>
#include <iomanip>
#include <map>
#include <algorithm>
#include <string>
#include <vector>

#include <libtorrent/file.hpp>
#include <libtorrent/hasher.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/ip_filter.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/peer_connection.hpp>

#include "halEvent.hpp"
#include "global/string_conv.hpp"

#define foreach BOOST_FOREACH

namespace hal
{

std::wstring Event::eventLevelToStr(eventLevel event)
{
	switch (event)
	{
	case debug:
		return hal::app().load_res_wstring(HAL_EVENTDEBUG);
	case info:
		return hal::app().load_res_wstring(HAL_EVENTINFO);
	case warning:
		return hal::app().load_res_wstring(HAL_EVENTINFO);
	case critical:
		return hal::app().load_res_wstring(HAL_EVENTCRITICAL);
	case fatal:
		return hal::app().load_res_wstring(HAL_EVENTCRITICAL);
	default:
		return hal::app().load_res_wstring(HAL_EVENTNONE);
	}
}

Event& event()
{
	static Event e;
	return e;
}

} // namespace hal
