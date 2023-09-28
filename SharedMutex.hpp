#pragma once

#include <memory>
#include <string>

class SharedRegion;

class LinuxSharedMutex
{
private:
	const std::string _name;
	std::unique_ptr<SharedRegion> _share;
	bool _locked;

public:
	LinuxSharedMutex(const char* name);
	virtual ~LinuxSharedMutex();

	virtual std::string_view Name() const;
	virtual bool TryLock(int timeout);
	virtual void Unlock();
	virtual bool IsLocked() const;
	virtual void Release();

private:
	bool HasValidTimestamp() const;
	static long MillisecondsNow();
};