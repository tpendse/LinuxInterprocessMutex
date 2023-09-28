#include <vector>
#include <memory>
#include <unistd.h>
#include <signal.h>
#include "SharedRegion.hpp"
#include "SharedMutex.hpp"
#include "TestHelpers.h"

constexpr long TIMESTAMP_DELTA = 500; /* 500 milliseconds */

class SharedMutexTest
{
private:
	const std::string _name;
	std::unique_ptr<LinuxSharedMutex> Mutex;
	std::unique_ptr<SharedRegion> Region;

public:
	SharedMutexTest(const char* name) : _name(name), Mutex(nullptr) {}
	virtual ~SharedMutexTest() { 
		if (Mutex)
			Mutex->Release();
	 }

	void WaitOne() const { SleepFor(WAIT_TIME_1); }
	void WaitTwo() const { SleepFor(WAIT_TIME_2); }

	bool CreateSharedMutex() {
		Mutex.reset(new LinuxSharedMutex(this->_name.c_str()));
		return true;
	}

	LinuxSharedMutex* GetMutex() {
		return Mutex.get();
	}

	SharedRegion* GetSharedRegion() {
		if (Region == nullptr) {
			Region.reset(new SharedRegion( this->_name.c_str() ));
			Region->Create();
		}
		return Region.get();
	}

	bool HasFile() {
		std::string shm_path = "/dev/shm/";
		shm_path += this->_name;
		return access(shm_path.c_str(), F_OK) == 0;
	}
};

long millisecondsNow() {
	auto now = std::chrono::system_clock::now();
	return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

long staleTimestamp() {
	return millisecondsNow() - 1 * 60 * 60 * 1000; // 1 hour ago
}

long validTimestamp() {
	return millisecondsNow() - 20 * 1000; // 20 seconds
}

//====================================================================================================
//====================================================================================================

void Test_SingleMutex_Create_ValuesCorrect()
{
	logtest(__func__);

	auto expected_timestamp = millisecondsNow();

	SharedMutexTest test(SHARE_NAME_1);
	test.CreateSharedMutex();

	auto counter = test.GetSharedRegion()->Get()->counter;
	auto timestamp = test.GetSharedRegion()->Get()->timestamp;

	assert(counter == 1, "Default counter values incorrect");
	auto diff = abs(timestamp - expected_timestamp);
	assert(diff < TIMESTAMP_DELTA, "Default timestamp values incorrect");
}

void Test_SingleMutex_Locking_Correct()
{
	logtest(__func__);

	SharedMutexTest test(SHARE_NAME_1);
	test.CreateSharedMutex();
	test.GetMutex()->TryLock(0);
	auto isLocked1 = test.GetMutex()->IsLocked();
	test.GetMutex()->Unlock();
	auto isLocked2 = test.GetMutex()->IsLocked();

	assert(isLocked1, "Mutex should be locked");
	assert(isLocked2 == false, "Mutex should be unlocked");
}

void Test_SingleMutex_CreateValidTimestamp_ReusesSharedRegion()
{
	logtest(__func__);

	SharedMutexTest test(SHARE_NAME_1);
	auto sharedRegion = SharedRegion(SHARE_NAME_1);
	sharedRegion.Create();
	auto expected_timestamp = millisecondsNow();
	sharedRegion.Get()->timestamp = expected_timestamp;
	sharedRegion.Get()->counter = TEST_COUNT_1;

	test.CreateSharedMutex();
	
	auto sharedRegion2 = SharedRegion(SHARE_NAME_1);
	sharedRegion2.Create();
	auto timestamp = sharedRegion2.Get()->timestamp;
	auto counter = sharedRegion2.Get()->counter;

	sharedRegion2.Destroy();

	compare<long>(timestamp, expected_timestamp, "Timestamp values incorrect");
	compare<long>(counter, TEST_COUNT_1 + 1, "Counter values incorrect");
}

void Test_SingleMutex_CreateStaleTimestamp_CreatesNewShare()
{
	logtest(__func__);

	SharedMutexTest test(SHARE_NAME_1);
	auto sharedRegion = test.GetSharedRegion();
	sharedRegion->Get()->timestamp = staleTimestamp();
	sharedRegion->Get()->counter = TEST_COUNT_1;

	auto expected_timestamp = millisecondsNow();
	test.CreateSharedMutex();

	auto sharedRegion2 = SharedRegion(SHARE_NAME_1);
	sharedRegion2.Create();
	auto timestamp = sharedRegion2.Get()->timestamp;

	auto diff = abs(timestamp - expected_timestamp);
	auto counter = sharedRegion2.Get()->counter;

	sharedRegion2.Destroy();

	assert(diff < TIMESTAMP_DELTA, "Default timestamp values incorrect");
	assert(counter == 1, "Default counter values incorrect");
}

void Test_SingleMutex_OnRelease_UnlocksMutex()
{
	logtest(__func__);

	SharedMutexTest test(SHARE_NAME_1);
	test.CreateSharedMutex();
	test.GetMutex()->TryLock(0);
	auto sharedRegion = test.GetSharedRegion();
	test.GetMutex()->Release();

	auto isLocked = sharedRegion->Get()->mutex.try_lock();
	sharedRegion->Get()->mutex.unlock();

	assert(isLocked, "Mutex should be free");
}

void Test_SingleMutex_OnRelease_DeletesShare()
{
	logtest(__func__);

	SharedMutexTest test(SHARE_NAME_1);
	test.CreateSharedMutex();
	auto hasFileBefore = test.HasFile();
	test.GetMutex()->Release();
	auto hasFileAfter = test.HasFile();

	assert(hasFileBefore, "Shared memory file should exist");
	assert(hasFileAfter == false, "Shared memory file should be deleted");
}

void Test_SingleMutex_CounterIsTwo_OnRelease_DoesNotDeleteShare()
{
	logtest(__func__);

	SharedMutexTest test(SHARE_NAME_1);
	test.CreateSharedMutex();
	test.GetSharedRegion()->Get()->counter++;
	test.GetMutex()->Release();
	auto hasFile = test.HasFile();
	test.GetSharedRegion()->Destroy();

	assert(hasFile, "Shared memory file should not be deleted");
}

void Test_TwoMutexes_LockFirst_SecondFails_TillFirstRelease()
{
	logtest(__func__);

	pid_t childPid = fork();
	assert(childPid >= 0, "Process fork failed");

	if (childPid == 0)
	{
		/*
		* Child Process -- don't do assertions here!
		*/
		SharedMutexTest test(SHARE_NAME_1);
		test.CreateSharedMutex();
		test.GetMutex()->TryLock(0);
		test.WaitTwo();
		test.GetMutex()->Release();

		// Don't proceed till killed by parent
		while(1) SleepFor(CHILD_SLEEP_TIME);
	}
	else
	{
		/*
		 * Parent Process -- assert only after child process stopped! 
		*/
		SharedMutexTest test(SHARE_NAME_1);
		test.CreateSharedMutex();
		test.WaitOne();
		auto lockFirst = test.GetMutex()->TryLock(0);
		test.WaitOne();
		auto lockSecond = test.GetMutex()->TryLock(0);
		test.GetMutex()->Release();

		// Kill child proc
		kill(childPid, SIGTERM);

		assert(lockFirst == false, "Shared mutex should be locked");
		assert(lockSecond, "Shared mutex after release should be unlocked");
	}
}

void Test_TwoMutexes_LockFirstForShortPeriod_TryLockSecond_SucceedsBeforeTimeout()
{
	logtest(__func__);

	pid_t childPid = fork();
	assert(childPid >= 0, "Process fork failed");

	if (childPid == 0)
	{
		/*
		* Child Process -- don't do assertions here!
		*/
		SharedMutexTest test(SHARE_NAME_1);
		test.CreateSharedMutex();
		test.GetMutex()->TryLock(0);
		test.WaitTwo();
		test.GetMutex()->Release();

		// Don't proceed till killed by parent
		while(1) SleepFor(CHILD_SLEEP_TIME);
	}
	else
	{
		/*
		* Parent Process -- assert only after child process stopped!
		*/
		SharedMutexTest test(SHARE_NAME_1);
		test.WaitOne();
		test.CreateSharedMutex();
		auto success = test.GetMutex()->TryLock(WAIT_TIME_2);
		test.WaitTwo();
		test.GetMutex()->Release();

		// Kill child proc
		kill(childPid, SIGTERM);
		
		assert(success, "Mutex could not lock before timeout");
	}
}

void Test_TwoMutexes_LockFirstForLongPeriod_TryLockSecond_FailsAfterTimeout()
{
	logtest(__func__);

	pid_t childPid = fork();
	assert(childPid >= 0, "Process fork failed");

	if (childPid == 0)
	{
		/*
		 * Child Process -- don't do assertions here!
		 */
		SharedMutexTest test(SHARE_NAME_1);
		test.CreateSharedMutex();
		test.GetMutex()->TryLock(0);
		test.WaitTwo();
		test.WaitTwo();
		test.GetMutex()->Release();

		// Don't proceed till killed by parent
		while (1) SleepFor(CHILD_SLEEP_TIME);
	}
	else
	{
		/*
		 * Parent Process -- assert only after child process stopped!
		 */
		SharedMutexTest test(SHARE_NAME_1);
		test.WaitOne();
		test.CreateSharedMutex();
		auto success = test.GetMutex()->TryLock(WAIT_TIME_1);
		test.WaitTwo();
		test.WaitTwo();
		test.GetMutex()->Release();

		// Kill child proc
		kill(childPid, SIGTERM);

		assert(success == false, "Mutex lock should timeout");
	}
}

//====================================================================================================
//====================================================================================================

static std::vector<TEST_TYPE> GetSharedMutexTests() {
	// Check no existing shared memory mapped files exist
	SharedMutexTest test(SHARE_NAME_1);
	if (test.HasFile())
	{
		std::cout
			<< divider3 << std::endl
			<< "!! ERROR !!" << std::endl
			<< "Shared memory file already exists at /run/shm/" << SHARE_NAME_1 << std::endl
			<< "Manually delete this file and re-run the tests" << std::endl
			<< divider3 << std::endl;

		return {};
	}

	return std::vector<TEST_TYPE> {
		&Test_SingleMutex_Create_ValuesCorrect,
		&Test_SingleMutex_Locking_Correct,
		&Test_SingleMutex_CreateValidTimestamp_ReusesSharedRegion,
		&Test_SingleMutex_CreateStaleTimestamp_CreatesNewShare,
		&Test_SingleMutex_OnRelease_UnlocksMutex,
		&Test_SingleMutex_OnRelease_DeletesShare,
		&Test_SingleMutex_CounterIsTwo_OnRelease_DoesNotDeleteShare,
		&Test_TwoMutexes_LockFirst_SecondFails_TillFirstRelease,
		&Test_TwoMutexes_LockFirstForShortPeriod_TryLockSecond_SucceedsBeforeTimeout,
		&Test_TwoMutexes_LockFirstForLongPeriod_TryLockSecond_FailsAfterTimeout,
	};
}