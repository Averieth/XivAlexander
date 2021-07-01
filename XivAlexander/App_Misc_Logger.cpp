#include "pch.h"
#include "App_Misc_Logger.h"

std::weak_ptr<App::Misc::Logger> App::Misc::Logger::s_instance;

class App::Misc::Logger::Implementation {
	static const int MaxLogCount = 8192;

public:
	Logger& logger;
	const Utils::Win32::Closeable::Handle m_hLogDispatcherThread;
	std::condition_variable m_threadTrigger;

	bool m_bQuitting = false;
	std::mutex m_pendingItemLock;
	std::mutex m_itemLock;
	std::deque<LogItem> m_items, m_pendingItems;

	Implementation(Logger& logger)
		: logger(logger)
		, m_hLogDispatcherThread(CreateThread(nullptr, 0, [](PVOID p) -> DWORD { return static_cast<Implementation*>(p)->ThreadWorker(); }, this, CREATE_SUSPENDED, nullptr),
			Utils::Win32::Closeable::Handle::Null,
			"Logger::CreateThread(ThreadWorker)") {
		ResumeThread(m_hLogDispatcherThread);
	}

	~Implementation() {
		m_bQuitting = true;
		m_threadTrigger.notify_all();
		WaitForSingleObject(m_hLogDispatcherThread, INFINITE);
	}

	DWORD ThreadWorker() {
		Utils::Win32::SetThreadDescription(GetCurrentThread(), L"XivAlexander::Misc::Logger::Implementation::ThreadWorker");
		while (true) {
			std::deque<LogItem> pendingItems;
			{
				std::unique_lock lock(m_pendingItemLock);
				if (m_pendingItems.empty()) {
					m_threadTrigger.wait(lock);
					if (m_bQuitting)
						return 0;
				}
				pendingItems = std::move(m_pendingItems);
			}
			for (auto& item : pendingItems) {
				{
					std::lock_guard lock(m_itemLock);
					m_items.push_back(item);
					if (m_items.size() > MaxLogCount)
						m_items.pop_front();
				}
				logger.OnNewLogItem(m_items.back());
			}
		}
	}

	void AddLogItem(LogItem item) {
		std::lock_guard lock(m_pendingItemLock);
		m_pendingItems.push_back(std::move(item));
		while (m_pendingItems.size() > MaxLogCount)
			m_pendingItems.pop_front();
		m_threadTrigger.notify_all();
	}
};

SYSTEMTIME App::Misc::Logger::LogItem::TimestampAsLocalSystemTime() const {
	return Utils::EpochToLocalSystemTime(std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count());
}

class App::Misc::Logger::LoggerCreator : public Logger {
public:
	LoggerCreator() = default;
	~LoggerCreator() override = default;
};

App::Misc::Logger::Logger()
	: m_pImpl(std::make_unique<Implementation>(*this))
	, OnNewLogItem([this](const auto& cb) {
		std::lock_guard lock(m_pImpl->m_itemLock);
		std::for_each(m_pImpl->m_items.begin(), m_pImpl->m_items.end(), [&cb](LogItem& item) { cb(item); });
		}) {
}

std::shared_ptr<App::Misc::Logger> App::Misc::Logger::Acquire() {
	auto r = s_instance.lock();
	if (!r) {
		static std::mutex mtx;
		std::lock_guard lock(mtx);

		r = s_instance.lock();
		if (!r)
			s_instance = r = std::make_shared<LoggerCreator>();
	}
	return r;
}

App::Misc::Logger::~Logger() = default;

void App::Misc::Logger::Log(LogCategory category, const char* s, LogLevel level) {
	Log(category, std::string(s));
}

void App::Misc::Logger::Log(LogCategory category, const char8_t* s, LogLevel level) {
	Log(category, reinterpret_cast<const char*>(s));
}

void App::Misc::Logger::Log(LogCategory category, const std::string & s, LogLevel level) {
	m_pImpl->AddLogItem(LogItem{
		category,
		std::chrono::system_clock::now(),
		level,
		s,
		});
}

void App::Misc::Logger::Clear() {
	std::lock_guard lock(m_pImpl->m_itemLock);
	std::lock_guard lock2(m_pImpl->m_pendingItemLock);
	m_pImpl->m_items.clear();
	m_pImpl->m_pendingItems.clear();
}

std::deque<const App::Misc::Logger::LogItem*> App::Misc::Logger::GetLogs() const {
	std::deque<const LogItem*> res;
	std::lock_guard lock(m_pImpl->m_itemLock);
	std::for_each(m_pImpl->m_items.begin(), m_pImpl->m_items.end(), [&res](LogItem& item) { res.push_back(&item); });
	return res;
}
