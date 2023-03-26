#ifndef TIMEOUTCACHE_H
#define TIMEOUTCACHE_H

#include <asio.hpp>
#include <unordered_set>
#include <chrono>
#include <shared_mutex>

template <typename T>
class TimeoutCache
{
public:
	TimeoutCache(asio::io_context& IOC, const size_t timeout_ms):
		IOC(IOC),
		timeout(timeout_ms)
	{}
	void Add(const T& key)
	{
		std::shared_lock<std::shared_mutex> lck(mtx);
		auto timer_it = Cache.find(key);
		if(timer_it != Cache.end()) //reset the existing timer
		{
			timer_it->second.expires_from_now(timeout);
		}
		else //add a new timer
		{
			lck.unlock(); //temporarily unlock shared to get unique
			{
				std::unique_lock<std::shared_mutex> write_lck(mtx);
				Cache.insert({key,asio::steady_timer(IOC,timeout)});
			}
			lck.lock();
		}
		Cache.at(key).async_wait([this,key](asio::error_code err)
		{
			if(err)
				return;
			std::unique_lock<std::shared_mutex> lck(mtx);
			Cache.erase(key);
		});
	}
	const std::vector<const T> Keys() const
	{
		std::shared_lock<std::shared_mutex> lck(mtx);
		std::vector<const T> keys;
		for(auto& [key,t] : Cache)
			keys.emplace_back(key);
		return keys;
	}

private:
	asio::io_context& IOC;
	const std::chrono::milliseconds timeout;
	std::unordered_map<T,asio::steady_timer> Cache;
	mutable std::shared_mutex mtx;
};

#endif // TIMEOUTCACHE_H
