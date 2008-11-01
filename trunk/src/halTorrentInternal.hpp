
//         Copyright E�in O'Callaghan 2006 - 2008.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "halTorrentDefines.hpp"

#ifndef HAL_TORRENT_STATE_LOGGING
#	define TORRENT_STATE_LOG(s)
#else
#	include "../halEvent.hpp"
#	define TORRENT_STATE_LOG(msg) \
	hal::event_log.post(boost::shared_ptr<hal::EventDetail>( \
			new hal::EventMsg(msg, hal::event_logger::torrent_dev))) 
#endif

#pragma warning (push, 1)
#	include <libtorrent/file.hpp>
#	include <libtorrent/hasher.hpp>
#	include <libtorrent/storage.hpp>
#	include <libtorrent/file_pool.hpp>
#	include <libtorrent/alert_types.hpp>
#	include <libtorrent/entry.hpp>
#	include <libtorrent/bencode.hpp>
#	include <libtorrent/session.hpp>
#	include <libtorrent/ip_filter.hpp>
#	include <libtorrent/torrent_handle.hpp>
#	include <libtorrent/peer_connection.hpp>
#	include <libtorrent/extensions/metadata_transfer.hpp>
#	include <libtorrent/extensions/ut_pex.hpp>
#pragma warning (pop) 

#include <boost/tuple/tuple.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/tag.hpp>

#include <boost/statechart/event.hpp>
#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/simple_state.hpp>

#include "halIni.hpp"
#include "halTypes.hpp"
#include "halSignaler.hpp"

namespace hal 
{
class TorrentInternalOld;
class torrent_internal;
}

BOOST_CLASS_VERSION(hal::TorrentInternalOld, 9)
BOOST_CLASS_VERSION(hal::torrent_internal, 2)

namespace hal 
{

namespace libt = libtorrent;
namespace sc = boost::statechart;


inline
libt::entry haldecode(const wpath &file) 
{
	fs::ifstream ifs(file, fs::ifstream::binary);
	if (ifs.is_open()) 
	{
		ifs.unsetf(fs::ifstream::skipws);
		return libt::bdecode(std::istream_iterator<char>(ifs), std::istream_iterator<char>());
	}
	else return libt::entry();
}

inline
bool halencode(const wpath &file, const libt::entry &e) 
{
	fs::ofstream ofs(file, fs::ofstream::binary);

	if (!ofs.is_open()) 
		return false;
	
	libt::bencode(std::ostream_iterator<char>(ofs), e);
	return true;
}

inline path path_to_utf8(const wpath& wp)
{
	return path(to_utf8(wp.string()));
}

inline wpath path_from_utf8(const path& p)
{
	return wpath(from_utf8(p.string()));
}

inline
std::pair<std::string, std::string> extract_names(const wpath &file)
{
	if (fs::exists(file)) 
	{	
		libt::torrent_info info(path_to_utf8(file));

		std::string name = info.name();	
		std::string filename = name;

		if (!boost::find_last(filename, ".torrent")) 
				filename += ".torrent";
		
		event_log.post(shared_ptr<EventDetail>(new EventMsg(
			hal::wform(L"Loaded names: %1%, %2%") % from_utf8(name) % from_utf8(filename))));

		return std::make_pair(name, filename);
	}
	else
		return std::make_pair("", "");
}

inline libt::storage_mode_t hal_allocation_to_libt(bit::allocations alloc)
{
	switch (alloc)
	{
	case bit::full_allocation:
		return libt::storage_mode_allocate;
	case bit::compact_allocation:
		return libt::storage_mode_compact;
	case bit::sparse_allocation:
	default:
		return libt::storage_mode_sparse;
	}
}

class invalidTorrent : public std::exception
{
public:
	invalidTorrent(const wstring& who) :
		who_(who)
	{}
	
	virtual ~invalidTorrent() throw () {}

	wstring who() const throw ()
	{
		return who_;
	}       
	
private:
	wstring who_;	
};
	
template<typename T>
class transfer_tracker
{
public:
	transfer_tracker() :
		total_(0),
		total_offset_(0)
	{}
	
	transfer_tracker(T total) :
		total_(total),
		total_offset_(0)
	{}
	
	transfer_tracker(T total, T offset) :
		total_(total),
		total_offset_(offset)
	{}
	
	void reset(T total) const
	{
		total_ = total;
		total_offset_ = 0;
	}
	
	T update(T rel_total) const
	{
		total_ += (rel_total - total_offset_);
		total_offset_ = rel_total;
		
		return total_;
	}
	
	void setOffset(T offset) const
	{
		total_offset_ = offset;
	}
	
	operator T() const { return total_; }
	
	friend class boost::serialization::access;
	template<class Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		ar & boost::serialization::make_nvp("total", total_);
	}
	
private:
	mutable T total_;
	mutable T total_offset_;
};

class duration_tracker
{
public:
	duration_tracker() :
		total_(boost::posix_time::time_duration(0,0,0,0), 
			boost::posix_time::time_duration(0,0,0,0))
	{}
	
	boost::posix_time::time_duration update() const
	{
		if (start_.is_not_a_date_time()) 
			start_ = boost::posix_time::second_clock::universal_time();

		if (static_cast<boost::posix_time::time_duration>(total_).is_special()) 
			total_.setOffset(boost::posix_time::time_duration(0,0,0,0));
		
		return total_.update(boost::posix_time::second_clock::universal_time() - start_);
	}
	
	void reset() const
	{
		total_.setOffset(boost::posix_time::time_duration(0,0,0,0));
		start_ = boost::posix_time::second_clock::universal_time();
	}
	
	friend class boost::serialization::access;
	template<class Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		ar & boost::serialization::make_nvp("total", total_);
	}
	
	operator boost::posix_time::time_duration() const { return total_; }
	
private:
	transfer_tracker<boost::posix_time::time_duration> total_;	
	mutable boost::posix_time::ptime start_;		
};
	
struct signalers
{
	signaler<> torrent_finished;

	boost::signal<void ()> torrent_paused;
	boost::signal<void ()> resume_data;
};

class torrent_internal;
typedef shared_ptr<torrent_internal> torrent_internal_ptr;

struct torrent_standalone :
	public hal::IniBase<torrent_standalone>
{
	typedef torrent_standalone thisClass;
	typedef hal::IniBase<thisClass> iniClass;

	torrent_standalone() :
		iniClass("torrent")
	{}

	torrent_standalone(torrent_internal_ptr t) :
		iniClass("torrent"),
		torrent(t),
		save_time(pt::second_clock::universal_time())
	{}

	torrent_internal_ptr torrent;
	pt::ptime save_time;

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
		ar & boost::serialization::make_nvp("torrent", torrent);
		ar & boost::serialization::make_nvp("save_time", save_time);
    }
};

class torrent_internal :
	public boost::enable_shared_from_this<torrent_internal>,
	private boost::noncopyable
{
	friend class bit_impl;	
	friend class bit::torrent::exec_around_ptr::proxy;

private:
	struct out_of_session;
	struct in_the_session;

	struct torrent_state_machine : sc::state_machine<torrent_state_machine, out_of_session> {};

	struct out_of_session : sc::simple_state<out_of_session, torrent_state_machine> {};

	struct paused;
	struct active;

	struct in_the_session : sc::simple_state<in_the_session, torrent_state_machine, paused> 
	{
		in_the_session();
		~in_the_session();
	};

	struct paused : sc::simple_state<paused, in_the_session>
	{
		paused();
		~paused();
	};

	struct active : sc::simple_state<active, in_the_session>
	{
		active();
		~active();
	};

public:
	#define TORRENT_INTERNALS_DEFAULTS \
		originalFilename_(L""), \
		transferLimit_(std::pair<float, float>(-1, -1)), \
		connections_(-1), \
		uploads_(-1), \
		ratio_(0), \
		resolve_countries_(true), \
		totalUploaded_(0), \
		totalBase_(0), \
		progress_(0), \
		startTime_(boost::posix_time::second_clock::universal_time()), \
		in_session_(false), \
		queue_position_(0)
		
	torrent_internal() :	
		TORRENT_INTERNALS_DEFAULTS,
		allocation_(bit::sparse_allocation),
		state_(torrent_details::torrent_stopped)
	{
		TORRENT_STATE_LOG(L"Torrent state machine initiate");
		machine_.initiate();
	}
	
		torrent_internal(wpath filename, wpath saveDirectory, bit::allocations alloc, wpath move_to_directory=L"") :
		TORRENT_INTERNALS_DEFAULTS,
		save_directory_(saveDirectory.string()),
		move_to_directory_(move_to_directory.string()),
		allocation_(alloc),	
		state_(torrent_details::torrent_stopped)
	{
		assert(the_session_);	
		
		TORRENT_STATE_LOG(L"Torrent state machine initiate");
		machine_.initiate();

		prepare(filename);
	}

	#undef TORRENT_INTERNALS_DEFAULTS
	
	torrent_details_ptr gettorrent_details_ptr()
	{	
		mutex_t::scoped_lock l(mutex_);

		try
		{

		if (in_session())
		{
			statusMemory_ = handle_.status();
			progress_ = statusMemory_.progress;

			queue_position_ = handle_.queue_position();
		}
		else
		{
			// Wipe these cause they don't make sense for a non-active torrent.
			
			statusMemory_.download_payload_rate = 0;
			statusMemory_.upload_payload_rate = 0;
			statusMemory_.next_announce = boost::posix_time::seconds(0);		
		}
		
		wstring state;
		
		switch (state_)
		{
		case torrent_details::torrent_paused:
			state = app().res_wstr(HAL_TORRENT_PAUSED);
			break;
			
		case torrent_details::torrent_pausing:
			state = app().res_wstr(HAL_TORRENT_PAUSING);
			break;
			
		case torrent_details::torrent_stopped:
			state = app().res_wstr(HAL_TORRENT_STOPPED);
			break;
			
		case torrent_details::torrent_stopping:
			state = app().res_wstr(HAL_TORRENT_STOPPING);
			break;
			
		default:
			switch (statusMemory_.state)
			{
			case libt::torrent_status::queued_for_checking:
				state = app().res_wstr(HAL_TORRENT_QUEUED_CHECKING);
				break;
			case libt::torrent_status::checking_files:
				state = app().res_wstr(HAL_TORRENT_CHECKING_FILES);
				break;
//			case libt::torrent_status::connecting_to_tracker:
//				state = app().res_wstr(HAL_TORRENT_CONNECTING);
//				break;
			case libt::torrent_status::downloading_metadata:
				state = app().res_wstr(HAL_TORRENT_METADATA);
				break;
			case libt::torrent_status::downloading:
				state = app().res_wstr(HAL_TORRENT_DOWNLOADING);
				break;
			case libt::torrent_status::finished:
				state = app().res_wstr(HAL_TORRENT_FINISHED);
				break;
			case libt::torrent_status::seeding:
				state = app().res_wstr(HAL_TORRENT_SEEDING);
				break;
			case libt::torrent_status::allocating:
				state = app().res_wstr(HAL_TORRENT_ALLOCATING);
				break;
			}	
		}
		
		pt::time_duration td(pt::pos_infin);
		
		if (statusMemory_.download_payload_rate != 0)
		{
			td = boost::posix_time::seconds(	
				long(float(statusMemory_.total_wanted-statusMemory_.total_wanted_done) / statusMemory_.download_payload_rate));
		}
		
		totalUploaded_ += (statusMemory_.total_payload_upload - totalBase_);
		totalBase_ = statusMemory_.total_payload_upload;
		
		uploaded_.update(statusMemory_.total_upload);
		payloadUploaded_.update(statusMemory_.total_payload_upload);
		downloaded_.update(statusMemory_.total_download);
		payloadDownloaded_.update(statusMemory_.total_payload_download);
		
		if (is_active())
		{
			activeDuration_.update();
			
			if (libt::torrent_status::seeding == statusMemory_.state)
				seedingDuration_.update();
		}	
		
		boost::tuple<size_t, size_t, size_t, size_t> connections = updatePeers();	

		return torrent_details_ptr(new torrent_details(name_, filename_, saveDirectory().string(), state, hal::from_utf8(statusMemory_.current_tracker), 
			std::pair<float, float>(statusMemory_.download_payload_rate, statusMemory_.upload_payload_rate),
			progress_, statusMemory_.distributed_copies, statusMemory_.total_wanted_done, statusMemory_.total_wanted, uploaded_, payloadUploaded_,
			downloaded_, payloadDownloaded_, connections, ratio_, td, statusMemory_.next_announce, activeDuration_, seedingDuration_, startTime_, finishTime_, queue_position_));

		}
		catch (const libt::invalid_handle&)
		{
			event_log.post(shared_ptr<EventDetail>(
				new EventInvalidTorrent(event_logger::critical, event_logger::invalidTorrent, to_utf8(name_), "gettorrent_details_ptr")));
		}
		catch (const std::exception& e)
		{
			event_log.post(shared_ptr<EventDetail>(
				new EventTorrentException(event_logger::critical, event_logger::torrentException, e.what(), to_utf8(name_), "gettorrent_details_ptr")));
		}
		
		return torrent_details_ptr(new torrent_details(name_, filename_, saveDirectory().string(), app().res_wstr(HAL_TORRENT_STOPPED), app().res_wstr(HAL_NA)));
	}

	void setTransferSpeed(float down, float up)
	{	
		mutex_t::scoped_lock l(mutex_);

		transferLimit_ = std::make_pair(down, up);
		
		applyTransferSpeed();
	}

	void setConnectionLimit(int maxConn, int maxUpload)		
	{
		mutex_t::scoped_lock l(mutex_);

		connections_ = maxConn;
		uploads_ = maxUpload;
		
		applyConnectionLimit();
	}

	std::pair<float, float> getTransferSpeed()
	{
		return transferLimit_;
	}

	std::pair<int, int> getConnectionLimit()
	{
		return std::make_pair(connections_, uploads_);
	}
	
	const wstring& name() const { return name_; }
	
	void set_ratio(float ratio) 
	{ 
		if (ratio < 0) ratio = 0;
		ratio_ = ratio; 
		
		apply_ratio();
	}
	
	float get_ratio()
	{
		return ratio_;
	}
	
	void add_to_session(bool paused = false)
	{
		try
		{

		mutex_t::scoped_lock l(mutex_);	
		assert(the_session_ != 0);

		HAL_DEV_MSG(hal::wform(L"add_to_session() paused=%1%") % paused);
		
		if (!in_session()) 
		{	
			libt::add_torrent_params p;

			string torrent_file = to_utf8((hal::app().get_working_directory()/L"torrents"/filename_).string());
			info_memory_.reset(new libt::torrent_info(torrent_file.c_str()));

			std::string resume_file = to_utf8((hal::app().get_working_directory()/L"resume" / (name_ + L".fastresume")).string());

			std::vector<char> buf;
			if (libt::load_file(resume_file.c_str(), buf) == 0)
			{
				HAL_DEV_MSG(L"Using resume data");
				p.resume_data = &buf;
			}

			p.ti = info_memory_;
			p.save_path = path_to_utf8(save_directory_);
			p.storage_mode = hal_allocation_to_libt(allocation_);
			p.paused = paused;
			p.duplicate_is_error = false;
			p.auto_managed = false;

			handle_ = the_session_->add_torrent(p);		
			assert(handle_.is_valid());
			in_session_ = true;
			
		//	clear_resume_data();
		//	handle_.force_reannounce();
		}	

		assert(in_session());
		HAL_DEV_MSG(L"Added to session");

		if (handle_.is_paused())
			state_ = torrent_details::torrent_paused;	

		}
		catch(std::exception& e)
		{
			hal::event_log.post(boost::shared_ptr<hal::EventDetail>(
				new hal::EventStdException(event_logger::critical, e, L"addToSession"))); 
		}
	}
	
	bool remove_from_session(bool write_data=true)
	{
		try
		{

		mutex_t::scoped_lock l(mutex_);
		assert(in_session());

		HAL_DEV_MSG(hal::wform(L"remove_from_session() writeData=%1%") % write_data);
		
		if (write_data)
		{
			HAL_DEV_MSG(L"requesting resume data");			
		
			signaler_wrapper<>* sig = new signaler_wrapper<>(bind(&torrent_internal::remove_from_session, this, false));
			signals().resume_data.connect(bind(&signaler_wrapper<>::operator(), sig));
			
			handle_.save_resume_data();

			return false;
		}
		else
		{		
			HAL_DEV_MSG(L"removing handle from session");
			the_session_->remove_torrent(handle_);
			in_session_ = false;

			assert(!in_session());	
			HAL_DEV_MSG(L"Removed from session!");

			return true;
		}

		}
		catch(std::exception& e)
		{
			hal::event_log.post(boost::shared_ptr<hal::EventDetail>(
				new hal::EventStdException(event_logger::critical, e, L"remove_fromSession"))); 
			return false;
		}
	}
	
	bool in_session() const
	{ 
		mutex_t::scoped_lock l(mutex_);

		return (in_session_ && the_session_ != 0 && handle_.is_valid());
	}
	
	void resume()
	{
		mutex_t::scoped_lock l(mutex_);

		if (state_ == torrent_details::torrent_stopped)
		{	
			add_to_session(false);
			assert(in_session());			
		}
		else
		{
			assert(in_session());
			handle_.resume();
		}	
		
		state_ = torrent_details::torrent_active;			
		//assert(!handle_.is_paused());
	}
	
	void pause()
	{
		mutex_t::scoped_lock l(mutex_);

		if (state_ == torrent_details::torrent_stopped)
		{	
			add_to_session(true);

			assert(in_session());
			assert(handle_.is_paused());
		}
		else
		{
			assert(in_session());

			HAL_DEV_MSG(hal::wform(L"pause() - handle_.pause()"));
			handle_.pause();

			signaler_wrapper<>* sig = new signaler_wrapper<>(bind(&torrent_internal::completed_pause, this));
			signals().torrent_paused.connect(bind(&signaler_wrapper<>::operator(), sig));

			state_ = torrent_details::torrent_pausing;	
		}			
	}
	
	void stop()
	{
		mutex_t::scoped_lock l(mutex_);

		if (state_ != torrent_details::torrent_stopped)
		{
			if (state_ == torrent_details::torrent_active)
			{
				assert(in_session());

				signaler_wrapper<>* sig = new signaler_wrapper<>(bind(&torrent_internal::completed_stop, this));
				signals().torrent_paused.connect(bind(&signaler_wrapper<>::operator(), sig));
				
				HAL_DEV_MSG(hal::wform(L"stop() - handle_.pause()"));
				handle_.pause();

				state_ = torrent_details::torrent_stopping;
			}
			else if (state_ == torrent_details::torrent_paused)
			{			
				remove_from_session();
				state_ = torrent_details::torrent_stopped;				
			}
		}
	}

	void set_state_stopped()
	{
		state_ = torrent_details::torrent_stopped;
	}

	void force_recheck()
	{
		mutex_t::scoped_lock l(mutex_);		
		HAL_DEV_MSG(L"force_recheck()");

		switch (state_)
		{
		case torrent_details::torrent_stopped:
			clear_resume_data();
			resume();
			break;

		case torrent_details::torrent_stopping:
		case torrent_details::torrent_pausing:
//			signals().torrent_paused.disconnect_all_once();

		case torrent_details::torrent_active:
//			signals().torrent_paused.disconnect_all_once();
//			signals().torrent_paused.connect_once(bind(&torrent_internal::handle_recheck, this));
			handle_.pause();
			state_ = torrent_details::torrent_pausing;
			break;

		default:
			assert(false);
		};
	}
	
	void write_resume_data(const libt::entry& ent)
	{					
		HAL_DEV_MSG(L"write_resume_data()");

		wpath resume_dir = hal::app().get_working_directory()/L"resume";
		
		if (!exists(resume_dir))
			create_directory(resume_dir);

		boost::filesystem::ofstream out(resume_dir/(name_ + L".fastresume"), std::ios_base::binary);
		out.unsetf(std::ios_base::skipws);
		bencode(std::ostream_iterator<char>(out), ent);

		HAL_DEV_MSG(L"Written!");
	}
	
	void clear_resume_data()
	{
		wpath resume_file = hal::app().get_working_directory()/L"resume"/filename_;
		
		if (exists(resume_file))
			remove(resume_file);

//		resumedata_ = libt::entry();
	}

	const wpath get_save_directory()
	{
		return save_directory_;
	}

	void set_save_directory(wpath s, bool force=false)
	{
		if (in_session() && !is_finished() &&
				s != path_from_utf8(handle_.save_path()))
		{
			handle_.move_storage(path_to_utf8(s));
			save_directory_ = s;
		}
		else if (!in_session() && force)
		{
			save_directory_ = s;
		}
	}

	const wpath get_move_to_directory()
	{
		return move_to_directory_;
	}
	
	void set_move_to_directory(wpath m)
	{
		if (is_finished() && !m.empty())
		{
			if (m != path_from_utf8(handle_.save_path()))
			{
				handle_.move_storage(path_to_utf8(m));
				save_directory_ = move_to_directory_ = m;
			}
		}
		else
		{
			move_to_directory_ = m;
		}
	}

	bool is_finished()
	{
		if (in_session())
		{
			libt::torrent_status::state_t s = handle_.status().state;

			return (s == libt::torrent_status::seeding ||
						s == libt::torrent_status::finished);
		}
		else return false;
	}
	
	void finished()
	{
		if (finishTime_.is_special())
			finishTime_ = boost::posix_time::second_clock::universal_time();

		if (is_finished())
		{
			if (!move_to_directory_.empty() && 
					move_to_directory_ !=  path_from_utf8(handle_.save_path()))
			{
				handle_.move_storage(path_to_utf8(move_to_directory_));
				save_directory_ = move_to_directory_;
			}
		}
	}
	
	bool is_active() const { return state_ == torrent_details::torrent_active; }
	
	unsigned state() const { return state_; }
	
	void setTrackerLogin(wstring username, wstring password)
	{
		trackerUsername_ = username;
		trackerPassword_ = password;
		
		applyTrackerLogin();
	}	
	
	std::pair<wstring, wstring> getTrackerLogin() const
	{
		return make_pair(trackerUsername_, trackerPassword_);
	}
	
	const wstring& filename() const { return filename_; }
	
	const wstring& originalFilename() const { return originalFilename_; }
	
	const libt::torrent_handle& handle() const { return handle_; }

	void resetTrackers()
	{
		if (in_session())
		{
			handle_.replace_trackers(torrent_trackers_);		
			trackers_.clear();
		}
	}
	
	void setTrackers(const std::vector<tracker_detail>& tracker_details)
	{
		trackers_.clear();
		trackers_.assign(tracker_details.begin(), tracker_details.end());
		
		applyTrackers();
	}
	
	const std::vector<tracker_detail>& getTrackers()
	{
		if (trackers_.empty() && info_memory_)
		{
			std::vector<libt::announce_entry> trackers = info_memory_->trackers();
			
			foreach (const libt::announce_entry& entry, trackers)
			{
				trackers_.push_back(
					tracker_detail(hal::from_utf8(entry.url), entry.tier));
			}
		}		
		return trackers_;
	}
	
	void setFilePriorities(std::vector<int> fileIndices, int priority)
	{
		if (!filePriorities_.empty())
		{
			foreach(int i, fileIndices)
				filePriorities_[i] = priority;
				
			applyFilePriorities();
		}
	}

	const wpath& saveDirectory() { return save_directory_; }
	
    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
		using boost::serialization::make_nvp;

		if (version > 1) {
			ar & make_nvp("transfer_limits", transferLimit_);
			ar & make_nvp("connection_limits", connections_);
			ar & make_nvp("upload_limits", uploads_);	

			ar & make_nvp("name", name_);
			ar & make_nvp("filename", filename_);	

			ar & make_nvp("ratio", ratio_);	
			ar & make_nvp("progress", progress_);
			ar & make_nvp("state", state_);
//			ar & make_nvp("compact_storage", compactStorage_);	
			ar & make_nvp("allocation_type", allocation_);	
			ar & make_nvp("resolve_countries", resolve_countries_);	

			ar & make_nvp("tracker_username", trackerUsername_);
			ar & make_nvp("tracker_password", trackerPassword_);
			ar & make_nvp("trackers", trackers_);

			ar & make_nvp("save_directory", save_directory_);
			ar & make_nvp("move_to_directory", move_to_directory_);
			
			ar & make_nvp("payload_uploaded", payloadUploaded_);
			ar & make_nvp("payload_downloaded", payloadDownloaded_);
			ar & make_nvp("uploaded", uploaded_);
			ar & make_nvp("downloaded", downloaded_);			
					
			ar & make_nvp("file_priorities", filePriorities_);
			
			ar & make_nvp("start_time", startTime_);
			ar & make_nvp("finish_time", finishTime_);
			ar & make_nvp("active_duration", activeDuration_);
			ar & make_nvp("seeding_duration", seedingDuration_);
					
		} 
		else 
		{
		    ar & make_nvp("transferLimit", transferLimit_);
			ar & make_nvp("connections", connections_);
			ar & make_nvp("uploads", uploads_);			
			ar & make_nvp("filename", filename_);	

			wstring s;
			ar & make_nvp("saveDirectory", s);
			save_directory_ = s;

			if (version == 2) {
				wstring m;
				ar & make_nvp("moveToDirectory", m);
				move_to_directory_ = m;
			} else {
				move_to_directory_ = save_directory_;
			}
			
			ar & make_nvp("payloadUploaded_", payloadUploaded_);
			ar & make_nvp("payloadDownloaded_", payloadDownloaded_);
			ar & make_nvp("uploaded_", uploaded_);
			ar & make_nvp("downloaded_", downloaded_);	
			ar & make_nvp("ratio", ratio_);	
			ar & make_nvp("trackerUsername", trackerUsername_);
			ar & make_nvp("trackerPassword", trackerPassword_);
			
			ar & make_nvp("state", state_);
			ar & make_nvp("trackers", trackers_);
			
			ar & make_nvp("resolve_countries", resolve_countries_);
			
			ar & make_nvp("file_priorities", filePriorities_);
			
			ar & make_nvp("startTime", startTime_);
			ar & make_nvp("activeDuration", activeDuration_);
			ar & make_nvp("seedingDuration", seedingDuration_);
			
			ar & make_nvp("name", name_);
			ar & make_nvp("compactStorage", compactStorage_);
			ar & make_nvp("finishTime", finishTime_);
			
			ar & make_nvp("progress", progress_);
	}
    }

	void setEntryData(boost::intrusive_ptr<libt::torrent_info> metadata, libtorrent::entry resumedata)
	{		
		info_memory_ = metadata;
//		resumedata_ = resumedata;
	}

	std::vector<libt::peer_info>& peers() { return peers_; }
	
	boost::tuple<size_t, size_t, size_t, size_t> updatePeers()
	{
		if (in_session())
			handle_.get_peer_info(peers_);
		
		size_t totalPeers = 0;
		size_t peersConnected = 0;
		size_t totalSeeds = 0;
		size_t seedsConnected = 0;
		
		foreach (libt::peer_info& peer, peers_) 
		{
			float speedSum = peer.down_speed + peer.up_speed;
			
			if (!(peer.flags & libt::peer_info::seed))
			{
				++totalPeers;
				
				if (speedSum > 0)
					++peersConnected;
			}
			else
			{
				++totalSeeds;
				
				if (speedSum > 0)
					++seedsConnected;
			}
		}	
		
		return boost::make_tuple(totalPeers, peersConnected, totalSeeds, seedsConnected);
	}
	
	void getPeerDetails(PeerDetails& peerDetails) const
	{
		if (in_session())
		{
			foreach (libt::peer_info peer, peers_) 
			{
				peerDetails.push_back(peer);
			}	
		}
	}

	void getFileDetails(FileDetails& fileDetails)
	{
		if (fileDetailsMemory_.empty())
		{
			boost::intrusive_ptr<libt::torrent_info> info = info_memory();
			std::vector<libt::file_entry> files;
			
			std::copy(info->begin_files(), info->end_files(), 
				std::back_inserter(files));					
				
			if (filePriorities_.size() != files.size())
			{
				filePriorities_.clear();
				filePriorities_.assign(files.size(), 1);
			}
			
			for(size_t i=0, e=files.size(); i<e; ++i)
			{
				wstring fullPath = hal::from_utf8(files[i].path.string());
				boost::int64_t size = static_cast<boost::int64_t>(files[i].size);
				
				fileDetailsMemory_.push_back(FileDetail(fullPath, size, 0, filePriorities_[i], i));
			}	
		}		
		
		if (in_session())
		{			
			std::vector<libt::size_type> fileProgress;			
			handle_.file_progress(fileProgress);
			
			for(size_t i=0, e=fileDetailsMemory_.size(); i<e; ++i)
				fileDetailsMemory_[i].progress =  fileProgress[i];			
		}

		for(size_t i=0, e=fileDetailsMemory_.size(); i<e; ++i)
			fileDetailsMemory_[i].priority =  filePriorities_[i];
		
		fileDetails = fileDetailsMemory_;
	}
	
	void prepare(wpath filename)
	{
		mutex_t::scoped_lock l(mutex_);
		
		if (fs::exists(filename)) 
			info_memory_ = new libt::torrent_info(path_to_utf8(filename));
		
		extractNames(info_memory());			
		
		const wpath resumeFile = hal::app().get_working_directory()/L"resume"/filename_;
		const wpath torrentFile = hal::app().get_working_directory()/L"torrents"/filename_;
		
		event_log.post(shared_ptr<EventDetail>(new EventMsg(
			hal::wform(L"File: %1%, %2%.") % resumeFile % torrentFile)));
		
	//	if (exists(resumeFile)) 
	//		resumedata_ = haldecode(resumeFile);

		if (!exists(hal::app().get_working_directory()/L"torrents"))
			create_directory(hal::app().get_working_directory()/L"torrents");

		if (!exists(torrentFile))
			copy_file(filename.string(), torrentFile);

		if (!fs::exists(save_directory_))
			fs::create_directory(save_directory_);

		if (state_ == torrent_details::torrent_stopping)
			state_ = torrent_details::torrent_stopped;
		else if (state_ == torrent_details::torrent_pausing)
			state_ = torrent_details::torrent_paused;
	}
	
	void extractNames(boost::intrusive_ptr<libt::torrent_info> metadata)
	{
		mutex_t::scoped_lock l(mutex_);
				
		name_ = hal::from_utf8_safe(metadata->name());
		
		filename_ = name_;
		if (!boost::find_last(filename_, L".torrent")) 
				filename_ += L".torrent";
		
		event_log.post(shared_ptr<EventDetail>(new EventMsg(
			hal::wform(L"Loaded names: %1%, %2%") % name_ % filename_)));
	}
	
	boost::intrusive_ptr<libt::torrent_info> info_memory()
	{
		if (!info_memory_) 
			info_memory_ = 
				boost::intrusive_ptr<libt::torrent_info>(new libt::torrent_info(path_to_utf8(filename())));
		
		return info_memory_;
	}
	
	signalers& signals()
	{
		mutex_t::scoped_lock l(mutex_);
		return signals_;
	}

private:	
	signalers signals_;

	void applySettings()
	{		
		applyTransferSpeed();
		applyConnectionLimit();
		apply_ratio();
		applyTrackers();
		applyTrackerLogin();
		applyFilePriorities();
		applyResolveCountries();
	}
	
	void applyTransferSpeed()
	{
		mutex_t::scoped_lock l(mutex_);
		if (in_session())
		{
			int down = (transferLimit_.first > 0) ? static_cast<int>(transferLimit_.first*1024) : -1;
			handle_.set_download_limit(down);
			
			int up = (transferLimit_.second > 0) ? static_cast<int>(transferLimit_.second*1024) : -1;
			handle_.set_upload_limit(up);

			HAL_DEV_MSG(hal::wform(L"Applying Transfer Speed %1% - %2%") % down % up);
		}
	}

	void applyConnectionLimit()
	{
		mutex_t::scoped_lock l(mutex_);
		if (in_session())
		{
			handle_.set_max_connections(connections_);
			handle_.set_max_uploads(uploads_);

			HAL_DEV_MSG(hal::wform(L"Applying Connection Limit %1% - %2%") % connections_ % uploads_);
		}
	}
	
	void apply_ratio()
	{ 
		mutex_t::scoped_lock l(mutex_);
		if (in_session())
		{
			handle_.set_ratio(ratio_);

			HAL_DEV_MSG(hal::wform(L"Applying Ratio %1%") % ratio_);
		}
	}
	
	void applyTrackers()
	{
		mutex_t::scoped_lock l(mutex_);
		if (in_session())
		{
			if (torrent_trackers_.empty())
				torrent_trackers_ = handle_.trackers();
			
			if (!trackers_.empty())
			{
				std::vector<libt::announce_entry> trackers;
				
				foreach (const tracker_detail& tracker, trackers_)
				{
					trackers.push_back(
						libt::announce_entry(hal::to_utf8(tracker.url)));
					trackers.back().tier = tracker.tier;
				}
				handle_.replace_trackers(trackers);
			}
			
			HAL_DEV_MSG(L"Applying Trackers");
		}
	}
	
	void applyTrackerLogin()
	{
		mutex_t::scoped_lock l(mutex_);
		if (in_session())
		{
			if (trackerUsername_ != L"")
			{
				handle_.set_tracker_login(hal::to_utf8(trackerUsername_),
					hal::to_utf8(trackerPassword_));
			}

			HAL_DEV_MSG(hal::wform(L"Applying Tracker Login User: %1%, Pass: %2%") % trackerUsername_ % trackerPassword_ );
		}
	}
	
	void applyFilePriorities()
	{		
		mutex_t::scoped_lock l(mutex_);
		if (in_session()) 
		{
			if (!filePriorities_.empty())
				handle_.prioritize_files(filePriorities_);
			
			HAL_DEV_MSG(L"Applying File Priorities");
		}
	}	
	
	void applyResolveCountries()
	{
		mutex_t::scoped_lock l(mutex_);
		if (in_session())
		{
			handle_.resolve_countries(resolve_countries_);
			
			HAL_DEV_MSG(hal::wform(L"Applying Resolve Countries %1%") % resolve_countries_);
		}
	}
	
	bool completed_pause()
	{
		mutex_t::scoped_lock l(mutex_);
		assert(in_session());
		assert(handle_.is_paused());	

		HAL_DEV_MSG(L"completed_pause()");
				
		state_ = torrent_details::torrent_paused;	

		return true;
	}

	bool completed_stop()
	{
		mutex_t::scoped_lock l(mutex_);
		assert(in_session());
		assert(handle_.is_paused());			
		
		if (remove_from_session())
		{
			assert(!in_session());
			HAL_DEV_MSG(L"completed_stop()");
		}

		state_ = torrent_details::torrent_stopped;

		return true;
	}

	void handle_recheck()
	{
		mutex_t::scoped_lock l(mutex_);
		state_ = torrent_details::torrent_stopped;

		remove_from_session(false);
		assert(!in_session());

		clear_resume_data();

		resume();
		assert(in_session());

		HAL_DEV_MSG(L"handle_recheck()");
	}
		
	static libt::session* the_session_;
	
	mutable mutex_t mutex_;

	torrent_state_machine machine_;
	
	std::pair<float, float> transferLimit_;
	
	unsigned state_;
	int connections_;
	int uploads_;
	bool in_session_;
	float ratio_;
	bool resolve_countries_;
	
	wstring filename_;
	wstring name_;
	wpath save_directory_;
	wpath move_to_directory_;
	wstring originalFilename_;
	libt::torrent_handle handle_;	
	
//	boost::intrusive_ptr<libt::torrent_info> metadata_;
//	boost::shared_ptr<libt::entry> resumedata_;
	
	wstring trackerUsername_;	
	wstring trackerPassword_;
	
	boost::int64_t totalUploaded_;
	boost::int64_t totalBase_;
	
	transfer_tracker<boost::int64_t> payloadUploaded_;
	transfer_tracker<boost::int64_t> payloadDownloaded_;
	transfer_tracker<boost::int64_t> uploaded_;
	transfer_tracker<boost::int64_t> downloaded_;
	
	pt::ptime startTime_;
	pt::ptime finishTime_;
	duration_tracker activeDuration_;
	duration_tracker seedingDuration_;
	
	std::vector<tracker_detail> trackers_;
	std::vector<libt::announce_entry> torrent_trackers_;
	std::vector<libt::peer_info> peers_;	
	std::vector<int> filePriorities_;
	
	float progress_;
	
	boost::intrusive_ptr<libt::torrent_info> info_memory_;
	libt::torrent_status statusMemory_;
	FileDetails fileDetailsMemory_;
	
	int queue_position_;
	bool compactStorage_;
	bit::allocations allocation_;
};

typedef std::map<std::string, TorrentInternalOld> TorrentMap;
typedef std::pair<std::string, TorrentInternalOld> TorrentPair;

class TorrentManager : 
	public hal::IniBase<TorrentManager>
{
	typedef TorrentManager thisClass;
	typedef hal::IniBase<thisClass> iniClass;

	struct TorrentHolder
	{
		mutable torrent_internal_ptr torrent;
		
		wstring filename;
		wstring name;		
		
		TorrentHolder()
		{}
		
		explicit TorrentHolder(torrent_internal_ptr t) :
			torrent(t), filename(torrent->filename()), name(torrent->name())
		{}
						
		friend class boost::serialization::access;
		template<class Archive>
		void serialize(Archive& ar, const unsigned int version)
		{
			using boost::serialization::make_nvp;

			ar & make_nvp("torrent", torrent);
			ar & make_nvp("filename", filename);
			ar & make_nvp("name", name);
		}
	};
	
	struct byFilename{};
	struct byName{};
	
	typedef boost::multi_index_container<
		TorrentHolder,
		boost::multi_index::indexed_by<
			boost::multi_index::ordered_unique<
				boost::multi_index::tag<byFilename>,
				boost::multi_index::member<
					TorrentHolder, wstring, &TorrentHolder::filename> 
				>,
			boost::multi_index::ordered_unique<
				boost::multi_index::tag<byName>,
				boost::multi_index::member<
					TorrentHolder, wstring, &TorrentHolder::name> 
				>
		>
	> TorrentMultiIndex;
	
public:
	typedef TorrentMultiIndex::index<byFilename>::type torrentByFilename;
	typedef TorrentMultiIndex::index<byName>::type torrentByName;
	
	TorrentManager(ini_file& ini) :
		iniClass("bittorrent", "TorrentManager", ini)
	{}

	std::pair<torrentByName::iterator, bool> insert(const TorrentHolder& h)
	{
		return torrents_.get<byName>().insert(h);
	}
	
	std::pair<torrentByName::iterator, bool> insert(torrent_internal_ptr t)
	{
		return insert(TorrentHolder(t));
	}

	torrent_internal_ptr getByFile(const wstring& filename)
	{
		torrentByFilename::iterator it = torrents_.get<byFilename>().find(filename);
		
		if (it != torrents_.get<byFilename>().end() && (*it).torrent)
		{
			return (*it).torrent;
		}
		
		throw invalidTorrent(filename);
	}
	
	torrent_internal_ptr get(const wstring& name)
	{
		torrentByName::iterator it = torrents_.get<byName>().find(name);
		
		if (it != torrents_.get<byName>().end() && (*it).torrent)
		{
			return (*it).torrent;
		}
		
		throw invalidTorrent(name);
	}
	
	torrentByName::iterator erase(torrentByName::iterator where)
	{
		return torrents_.get<byName>().erase(where);
	}
	
	size_t size()
	{
		return torrents_.size();
	}
	
	size_t erase(const wstring& name)
	{
		return torrents_.get<byName>().erase(name);
	}
	
	bool exists(const wstring& name)
	{
		torrentByName::iterator it = torrents_.get<byName>().find(name);
		
		if (it != torrents_.get<byName>().end())
			return true;
		else
			return false;
	}
	
	torrentByName::iterator begin() { return torrents_.get<byName>().begin(); }
	torrentByName::iterator end() { return torrents_.get<byName>().end(); }
	
	friend class boost::serialization::access;
	template<class Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		ar & boost::serialization::make_nvp("torrents", torrents_);
	}	
	
private:
	TorrentMultiIndex torrents_;
};

} // namespace hal

BOOST_CLASS_VERSION(hal::TorrentManager::TorrentHolder, 1)
