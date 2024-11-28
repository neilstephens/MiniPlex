/*	MiniPlex
 *
 *	Copyright (c) 2023: Neil Stephens
 *
 *	Licensed under the Apache License, Version 2.0 (the "License");
 *	you may not use this file except in compliance with the License.
 *	You may obtain a copy of the License at
 *
 *		http://www.apache.org/licenses/LICENSE-2.0
 *
 *	Unless required by applicable law or agreed to in writing, software
 *	distributed under the License is distributed on an "AS IS" BASIS,
 *	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *	See the License for the specific language governing permissions and
 *	limitations under the License.
 */

#ifndef TIMEOUTCACHE_H
#define TIMEOUTCACHE_H

#include <asio.hpp>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <list>

template <typename T>
class TimeoutCache
{
public:
	TimeoutCache(asio::io_service::strand& Strand, const size_t timeout_ms, std::function<void(const T& key)> timeout_handler = [](const T&){}):
		Strand(Strand),
		timeout(timeout_ms),
		timeout_handler(timeout_handler)
	{}
	void Clear()
	{
		Cache.clear();
		KeySequence.clear();
	}
	bool Add(const T& key)
	{
		auto key_entry_it = Cache.find(key);
		if(key_entry_it != Cache.end())
		{
			//Just bump the access time of the existing entry and return
			key_entry_it->second.AccessTime = std::chrono::steady_clock::now();
			return false;
		}

		//Add a new entry
		KeySequence.emplace_back(key);
		key_entry_it = Cache.insert({key,CacheEntry(Strand,timeout,--KeySequence.end())}).first;
		key_entry_it->second.Timer.async_wait(Strand.wrap([this,key](asio::error_code err)
		{
			TimerHandler(key,err);
		}));
		return true;
	}
	const std::list<T>& Keys() const
	{
		return KeySequence;
	}

private:
	void TimerHandler(const T& key, const asio::error_code& err)
	{
		if(err)
			return;

		auto now = std::chrono::steady_clock::now();
		auto& entry = Cache.at(key);
		auto time_to_expiry = (entry.AccessTime+timeout)-now;
		if(time_to_expiry > std::chrono::steady_clock::duration::zero())
		{
			entry.Timer.expires_from_now(time_to_expiry);
			entry.Timer.async_wait(Strand.wrap([this,key](asio::error_code err)
			{
				TimerHandler(key,err);
			}));
			return;
		}
		KeySequence.erase(entry.KeySequenceIterator);
		Cache.erase(key);
		Strand.context().post([this,key]{timeout_handler(key);});
	}
	struct CacheEntry
	{
		CacheEntry(asio::io_service::strand& Strand, const std::chrono::milliseconds& timeout, const std::list<T>::iterator& it):
			Timer(Strand.context(),timeout), AccessTime(std::chrono::steady_clock::now()), KeySequenceIterator(it)
		{}
		asio::steady_timer Timer;
		std::chrono::time_point<std::chrono::steady_clock> AccessTime;
		const std::list<T>::iterator KeySequenceIterator;
	};
	asio::io_service::strand& Strand;
	const std::chrono::milliseconds timeout;
	std::function<void(const T& key)> timeout_handler;
	std::unordered_map<T,CacheEntry> Cache;
	std::list<T> KeySequence;
};

#endif // TIMEOUTCACHE_H
