

#pragma once

#define WINVER 0x0500
#define _WIN32_WINNT 0x0500
#define _WIN32_IE 0x0500
#define _RICHEDIT_VER 0x0200
#define VC_EXTRALEAN

#define HALITE_GUID L"HALITE-{E3A8BF7D-962F-476E-886B-FECEDD2F0FC7}"
#define WMU_ARE_YOU_ME_STRING  L"WMU_ARE_YOU_ME_HALITE-{E3A8BF7D-962F-476E-886B-FECEDD2F0FC7}"
#define HALITE_SENDING_CMD 68816889

#include <atlbase.h>
#include <atlsocket.h>
#include <atlapp.h>

extern CAppModule _Module;
#define _ATL_USE_DDX_FLOAT

#include <atlwin.h>
#include <atlframe.h>
#include <atlmisc.h>
#include <atlcrack.h>
#include <atldlgs.h>
#include <atlsplit.h>
#include <atlctrls.h>
#include <atlctrlw.h>
#include <atlctrlx.h>
#include <atlddx.h>

#include "global/wtl_app.hpp"

#include "..\res\resource.h"

// Include very common C++ and Boost libraries

#include <string>
#include <vector>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/array.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/noncopyable.hpp>

using std::string;
using std::wstring;
using std::pair;
using std::make_pair;

using boost::lexical_cast;
using boost::array;
using boost::format;
using boost::wformat;
using boost::bind;
using boost::thread;
using boost::shared_ptr;
using boost::scoped_ptr;
using boost::filesystem::path;
using boost::filesystem::wpath;
using boost::noncopyable;

#define foreach BOOST_FOREACH
