#include "pch.h"
#include "Utils_Win32_Handle.h"

HANDLE Utils::Win32::Handle::DuplicateHandleNullable(HANDLE src) {
	if (!src)
		return nullptr;
	DWORD flags;
	if (!GetHandleInformation(src, &flags))
		throw Error("GetHandleInformation");
	const auto bInherit = (flags & HANDLE_FLAG_INHERIT) ? TRUE : FALSE;
	if (HANDLE dst; DuplicateHandle(GetCurrentProcess(), src, GetCurrentProcess(), &dst, 0, bInherit, DUPLICATE_SAME_ACCESS))
		return dst;
	throw Error("DuplicateHandle");
}

Utils::Win32::Handle::Handle(Handle&& r) noexcept
	: Closeable(std::move(r)) {
}

Utils::Win32::Handle::Handle(const Handle& r)
	: Closeable(r.m_bOwnership ? DuplicateHandleNullable(r.m_object) : r.m_object, r.m_bOwnership) {
}

Utils::Win32::Handle& Utils::Win32::Handle::operator=(Handle&& r) noexcept {
	if (&r == this)
		return *this;

	Clear();
	m_object = r.m_object;
	m_bOwnership = r.m_bOwnership;
	r.Detach();
	return *this;
}

Utils::Win32::Handle& Utils::Win32::Handle::operator=(const Handle& r) {
	if (&r == this)
		return *this;

	const auto newHandle = r.m_bOwnership ? DuplicateHandleNullable(r.m_object) : r.m_object;
	if (m_object)
		CloseHandle(m_object);
	m_object = newHandle;
	m_bOwnership = r.m_bOwnership;
	return *this;
}

Utils::Win32::Handle::~Handle() = default;

void Utils::Win32::Handle::Seek(int64_t offset, DWORD dwMoveMethod) const {
	if (!SetFilePointerEx(m_object, { .QuadPart = offset }, nullptr, dwMoveMethod))
		throw Error("SetFilePointerEx");
}

Utils::Win32::Handle Utils::Win32::Handle::FromCreateFile(
	_In_ const std::filesystem::path& path,
	_In_ DWORD dwDesiredAccess,
	_In_ DWORD dwShareMode,
	_In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	_In_ DWORD dwCreationDisposition,
	_In_ DWORD dwFlagsAndAttributes,
	_In_opt_ HANDLE hTemplateFile
) {
	const auto hFile = CreateFileW(path.wstring().c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
	if (hFile == INVALID_HANDLE_VALUE)
		throw Error("CreateFileW");
	return { hFile, true };
}

std::pair<Utils::Win32::Handle, Utils::Win32::Handle> Utils::Win32::Handle::FromCreatePipe(LPSECURITY_ATTRIBUTES lpPipeAttributes, DWORD nSize) {
	HANDLE hReadPipe, hWritePipe;
	if (!::CreatePipe(&hReadPipe, &hWritePipe, lpPipeAttributes, nSize))
		throw Error("CreatePipe");
	return {{hReadPipe, true}, {hWritePipe, true}};
}

size_t Utils::Win32::Handle::Read(uint64_t offset, void* buf, size_t len, PartialIoMode readMode) const {
	const uint64_t ChunkSize = 0x10000000UL;
	if (len > ChunkSize) {
		size_t totalRead = 0;
		for (size_t i = 0; i < len; i += 0x1000000) {
			const auto toRead = static_cast<DWORD>(std::min<uint64_t>(ChunkSize, len - i));
			const auto read = Read(
				offset + i,
				static_cast<char*>(buf) + i,
				toRead,
				readMode
			);
			totalRead += read;
			if (read != toRead)
				break;
		}
		return totalRead;
	} else {
		DWORD readLength;
		OVERLAPPED ov{};
		const auto hCompleteEvent = Event::Create();
		ov.hEvent = hCompleteEvent;
		ov.Offset = static_cast<DWORD>(offset);
		ov.OffsetHigh = static_cast<DWORD>(offset >> 32);
		if (!ReadFile(m_object, buf, static_cast<DWORD>(len), nullptr, &ov)) {
			const auto err = GetLastError();
			if (err != ERROR_IO_PENDING) {
				if (err == ERROR_HANDLE_EOF && readMode == PartialIoMode::AllowPartial)
					readLength = 0;
				else
					throw Error("ReadFile");
			}
		}
		if (!GetOverlappedResult(m_object, &ov, &readLength, TRUE)) {
			const auto err = GetLastError();
			if (err == ERROR_HANDLE_EOF && readMode == PartialIoMode::AllowPartial)
				readLength = 0;
			else
				throw Error(err, "GetOverlappedResult");
		}
		if (readMode == PartialIoMode::AlwaysFull && readLength != len)
			throw Error("ReadFile(readLength != len)");
		return readLength;
	}
}

size_t Utils::Win32::Handle::Write(uint64_t offset, const void* buf, size_t len, PartialIoMode writeMode) const {
	const uint64_t ChunkSize = 0x10000000UL;
	if (len > ChunkSize) {
		size_t totalRead = 0;
		for (size_t i = 0; i < len; i += 0x1000000) {
			const auto toRead = static_cast<DWORD>(std::min<uint64_t>(ChunkSize, len - i));
			const auto read = Write(
				offset + i,
				static_cast<const char*>(buf) + i,
				toRead,
				writeMode
			);
			totalRead += read;
			if (read != toRead)
				break;
		}
		return totalRead;
	} else {
		DWORD readLength;
		OVERLAPPED ov{};
		const auto hCompleteEvent = Event::Create();
		ov.hEvent = hCompleteEvent;
		ov.Offset = static_cast<DWORD>(offset);
		ov.OffsetHigh = static_cast<DWORD>(offset >> 32);
		if (!WriteFile(m_object, buf, static_cast<DWORD>(len), nullptr, &ov)) {
			const auto err = GetLastError();
			if (err == ERROR_HANDLE_EOF && writeMode == PartialIoMode::AllowPartial)
				readLength = 0;
			else
				throw Error("WriteFile");
		}
		if (!GetOverlappedResult(m_object, &ov, &readLength, TRUE)) {
			const auto err = GetLastError();
			if (err == ERROR_HANDLE_EOF && writeMode == PartialIoMode::AllowPartial)
				readLength = 0;
			else
				throw Error(err, "GetOverlappedResult");
		}
		if (writeMode == PartialIoMode::AlwaysFull && readLength != len)
			throw Error("WriteFile(readLength != len)");
		return readLength;
	}
}

uint64_t Utils::Win32::Handle::GetFileSize() const {
	LARGE_INTEGER fs;
	if (!GetFileSizeEx(m_object, &fs))
		throw Error("GetFileSizeEx");
	return fs.QuadPart;
}

std::filesystem::path Utils::Win32::Handle::GetPathName(bool bOpenedPath, bool bNtPath) const {
	std::wstring result;
	result.resize(PATHCCH_MAX_CCH);
	result.resize(GetFinalPathNameByHandleW(m_object, &result[0], static_cast<DWORD>(result.size()), 
		(bNtPath ? VOLUME_NAME_NT : VOLUME_NAME_DOS) |
		(bOpenedPath ? FILE_NAME_OPENED : FILE_NAME_NORMALIZED)
	));
	if (result.empty())
		throw Error("GetFinalPathNameByHandleW");

	if (bNtPath) {
		if (!result.starts_with(LR"(\Device\)"))
			throw std::runtime_error(std::format("Path unprocessable: {}", result));
		return LR"(\\?\)" + result.substr(8);
	}

	return { std::move(result) };
}

Utils::Win32::ActivationContext::ActivationContext(const ACTCTXW & actctx)
	: Closeable<HANDLE, ReleaseActCtx>(CreateActCtxW(&actctx), INVALID_HANDLE_VALUE, "CreateActCtxW") {
}

Utils::Win32::ActivationContext::~ActivationContext() = default;

Utils::CallOnDestruction Utils::Win32::ActivationContext::With() const {
	ULONG_PTR cookie;
	if (!ActivateActCtx(m_object, &cookie))
		throw Error("ActivateActCtx");
	return CallOnDestruction([cookie]() {
		DeactivateActCtx(DEACTIVATE_ACTCTX_FLAG_FORCE_EARLY_DEACTIVATION, cookie);
	});
}

Utils::Win32::Thread::Thread()
	: Handle() {
}

Utils::Win32::Thread::Thread(HANDLE hThread, bool ownership)
	: Handle(hThread, ownership) {
}

Utils::Win32::Thread::Thread(std::wstring name, std::function<DWORD()> body, LoadedModule hLibraryToFreeAfterExecution) {
	typedef std::function<DWORD(void*)> InnerBodyFunctionType;
	const auto pInnerBodyFunction = new InnerBodyFunctionType(
		[
			name = std::move(name),
			body = std::move(body),
			hLibraryToFreeAfterExecution = std::move(hLibraryToFreeAfterExecution)
		](void* pThisFunction) mutable {
		SetThreadDescription(GetCurrentThread(), name);
		const auto res = body();
		if (hLibraryToFreeAfterExecution.HasOwnership()) {
			const auto hModule = hLibraryToFreeAfterExecution.Detach();
			delete static_cast<InnerBodyFunctionType*>(pThisFunction);
			FreeLibraryAndExitThread(hModule, res);
		}

		delete static_cast<InnerBodyFunctionType*>(pThisFunction);
		return res;
	});
	const auto hThread = CreateThread(nullptr, 0, [](void* lpParameter) -> DWORD {
		return (*static_cast<decltype(pInnerBodyFunction)>(lpParameter))(lpParameter);
	}, pInnerBodyFunction, 0, nullptr);
	if (!hThread) {
		const auto err = GetLastError();
		delete pInnerBodyFunction;
		throw Error(err, "CreateThread");
	}
	m_bOwnership = true;
	m_object = hThread;
}

Utils::Win32::Thread::Thread(std::wstring name, std::function<void()> body, LoadedModule hLibraryToFreeAfterExecution)
	: Thread(std::move(name), std::function([body = std::move(body)]()->DWORD { body(); return 0; }), std::move(hLibraryToFreeAfterExecution)) {
}

Utils::Win32::Thread::Thread(Thread && r) noexcept
	: Handle(r.m_object, r.m_bOwnership) {
	r.m_object = nullptr;
	r.m_bOwnership = false;
}

Utils::Win32::Thread::Thread(const Thread & r)
	: Handle(r) {
}

Utils::Win32::Thread& Utils::Win32::Thread::operator=(Thread && r) noexcept {
	if (&r == this)
		return *this;

	m_object = r.m_object;
	m_bOwnership = r.m_bOwnership;
	r.Detach();
	return *this;
}

Utils::Win32::Thread& Utils::Win32::Thread::operator=(const Thread & r) {
	if (&r == this)
		return *this;

	Handle::operator=(r);
	return *this;
}

Utils::Win32::Thread& Utils::Win32::Thread::operator=(std::nullptr_t) noexcept {
	Clear();
	return *this;
}

Utils::Win32::Thread::~Thread() = default;

DWORD Utils::Win32::Thread::GetId() const {
	return GetThreadId(m_object);
}

void Utils::Win32::Thread::Terminate(DWORD dwExitCode, bool errorIfAlreadyTerminated) const {
	if (TerminateThread(m_object, dwExitCode))
		return;
	const auto err = GetLastError();
	if (!errorIfAlreadyTerminated && Wait(0) != WAIT_TIMEOUT)
		return;
	throw Error(err, "TerminateThread");
}

Utils::Win32::Semaphore Utils::Win32::Semaphore::Create(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes, LONG lInitialCount, LONG lMaximumCount, LPCWSTR lpName, DWORD dwDesiredAccess) {
	return Semaphore(CreateSemaphoreExW(lpSemaphoreAttributes, lInitialCount, lMaximumCount, lpName, 0, dwDesiredAccess), Null, "CreateSemaphoreExW");
}

LONG Utils::Win32::Semaphore::Release(LONG count) const {
	LONG prevCount = 0;
	if (!ReleaseSemaphore(m_object, count, &prevCount))
		throw Error("ReleaseSemaphore");
	return prevCount;
}

Utils::Win32::Event::~Event() = default;

void Utils::Win32::Handle::Wait() const {
	const auto res = WaitForSingleObject(m_object, INFINITE);
	if (res == WAIT_FAILED)
		throw Error("WaitForSingleObject");
}

DWORD Utils::Win32::Handle::Wait(DWORD dwMilliseconds) const {
	const auto res = WaitForSingleObject(m_object, dwMilliseconds);
	if (res == WAIT_FAILED)
		throw Error("WaitForSingleObject");
	return res;
}

DWORD Utils::Win32::Handle::Wait(bool bWaitAll, std::initializer_list<HANDLE> waitWith, DWORD dwMilliseconds, bool bAlertable) const {
	std::vector handles{ m_object };
	handles.insert(handles.end(), waitWith.begin(), waitWith.end());
	const auto res = WaitForMultipleObjectsEx(static_cast<DWORD>(handles.size()), handles.data(), bWaitAll, dwMilliseconds, bAlertable);
	if (res == WAIT_FAILED)
		throw Error("WaitForMultipleObjectsEx");
	return res;
}

Utils::Win32::ActivationContext::ActivationContext(ActivationContext && r) noexcept
	: Closeable(std::move(r)) {
}

Utils::Win32::ActivationContext& Utils::Win32::ActivationContext::operator=(ActivationContext && r) noexcept {
	Closeable<HANDLE, ReleaseActCtx>::operator=(std::move(r));
	return *this;
}

Utils::Win32::Event Utils::Win32::Event::Create(
	_In_opt_ LPSECURITY_ATTRIBUTES lpEventAttributes,
	_In_ BOOL bManualReset,
	_In_ BOOL bInitialState,
	_In_opt_ LPCWSTR lpName
) {
	return Event(CreateEventW(lpEventAttributes, bManualReset, bInitialState, lpName), Null, "CreateEventW");
}

void Utils::Win32::Event::Set() const {
	if (!SetEvent(m_object))
		throw Error("SetEvent");
}

void Utils::Win32::Event::Reset() const {
	if (!ResetEvent(m_object))
		throw Error("ResetEvent");
}

Utils::Win32::Semaphore::~Semaphore() = default;
