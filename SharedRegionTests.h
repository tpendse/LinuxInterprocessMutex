#include <unistd.h>
#include <signal.h>
#include <iostream>
#include <memory>
#include <vector>
#include "SharedRegion.hpp"
#include "TestHelpers.h"

class SharegRegionTest
{
private:
	const std::string _name;
	std::unique_ptr<SharedRegion> Region;

public:
	SharegRegionTest(const char* name) : _name(name), Region(nullptr) { }

	void Wait() const { SleepFor(WAIT_TIME_1); }

	bool CreateSharedMemory() {
		Region.reset(new SharedRegion(this->_name.c_str()));
		return Region->Create();
	}

    bool DestroySharedMemory() {
        Region->Destroy();
    }

    unsigned int Counter() const {
        return Region->Get()->counter;
    }

    void SetCounter(unsigned int value) {
        Region->Get()->counter = value;
    }

    long Timestamp() const {
        return Region->Get()->timestamp;
    }

    void SetTimestamp(long value) {
        Region->Get()->timestamp = value;
    }

    void Unmap() {
        Region->Unmap();
    }

    bool IsRegionNull() {
        return Region->Get() == nullptr;
    }
};

//====================================================================================================
//====================================================================================================

void Test_SingleProcess_DefaultValues_Correct()
{
    logtest(__func__);

    SharegRegionTest test(SHARE_NAME_1);
    
    bool result = test.CreateSharedMemory();
    assert(result, "Could not create shared memory");

    compare<unsigned int>(test.Counter(), 0, "Default counter values incorrect");
    compare<long>(test.Timestamp(), 0, "Default timestamp values incorrect");
}

void Test_SingleProcess_SetCounts_Correct()
{
    logtest(__func__);

    SharegRegionTest test(SHARE_NAME_1);
    test.CreateSharedMemory();

    test.SetCounter(TEST_COUNT_1);
    compare<unsigned int>(test.Counter(), TEST_COUNT_1, "Set counter incorrect");

    test.SetTimestamp(TEST_TIMESTAMP);
    compare<long>(test.Timestamp(), TEST_TIMESTAMP, "Set timestamp incorrect");
}

//====================================================================================================

void Test_SingleProcess_SetCounts_ResetsOnDestroy()
{
    logtest(__func__);

    SharegRegionTest test(SHARE_NAME_1);
    test.CreateSharedMemory();
    test.SetCounter(TEST_COUNT_1);
    test.SetTimestamp(TEST_TIMESTAMP);
    test.DestroySharedMemory();

    test.CreateSharedMemory();
    compare<unsigned int>(test.Counter(), 0, "Counter not reset on destroy");
    compare<long>(test.Timestamp(), 0, "Timestamp not reset on destroy");
}

//====================================================================================================

void Test_SingleProcess_Unmap_FreesMemory()
{
    logtest(__func__);

    SharegRegionTest test(SHARE_NAME_1);
    test.CreateSharedMemory();
    test.Unmap();
    assert(test.IsRegionNull(), "Unmap should free shared memory");
}

//====================================================================================================

void Test_SingleProcess_NoDestroy_RetainsOldValues()
{
    logtest(__func__);
    
    // Scope 1
    {
        SharegRegionTest test(SHARE_NAME_1);
        test.CreateSharedMemory();
        test.SetCounter(TEST_COUNT_1);
        test.SetTimestamp(TEST_TIMESTAMP);
        
        // Skip destroy here
    }

    // Scope 2
    {
        SharegRegionTest test(SHARE_NAME_1);
        test.CreateSharedMemory();

        compare<unsigned int>(test.Counter(), TEST_COUNT_1, "Old values not retained");
        compare<long>(test.Timestamp(), TEST_TIMESTAMP, "Old values not retained");

        test.DestroySharedMemory();
    }
}

//====================================================================================================

void Test_TwoProcesses_SetValues_Shared()
{
    logtest(__func__);

    pid_t childPid = fork();
    assert(childPid >= 0, "Process fork failed");

    if (childPid == 0) 
    {
        /* 
        * Child Process -- don't do assertions here!
        */
       SharegRegionTest test(SHARE_NAME_1);
       test.CreateSharedMemory();
       test.SetCounter(TEST_COUNT_1);
       test.SetTimestamp(TEST_TIMESTAMP);
       test.Wait();

       test.DestroySharedMemory();
       
       // Don't proceed till killed by parent
       while(1) SleepFor(CHILD_SLEEP_TIME);
    }
    else
    {
        /*
         * Parent Process -- assert only after child process stopped!
         */
        SharegRegionTest test(SHARE_NAME_1);
        test.CreateSharedMemory();
        test.Wait();

        const auto counter = test.Counter();
        const auto timestamp = test.Timestamp();

        // Kill the child process here
        kill(childPid, SIGTERM);

        compare<unsigned int>(counter, TEST_COUNT_1, "Shared values incorrect");
        compare<long>(timestamp, TEST_TIMESTAMP, "Shared values incorrect");
    }
}

//====================================================================================================

void Test_TwoProcesses_RunSynchronous_GetDefaultValues()
{
    logtest(__func__);

    pid_t childPid = fork();
    assert(childPid >= 0, "Process fork failed!");

    if (childPid == 0)
    {
        /*
         * Child Process -- don't do assertions here!
         */
        SharegRegionTest test(SHARE_NAME_1);
        test.CreateSharedMemory();
        test.SetCounter(TEST_COUNT_1);
        test.SetTimestamp(TEST_TIMESTAMP);
        test.DestroySharedMemory();

        // Don't proceed till killed by parent
        while (1) SleepFor(CHILD_SLEEP_TIME);
    }
    else
    {
        /*
         * Parent Process -- assert only after child process stopped!
         */
        // Wait for child to do it's thing then kill it
        SleepFor(WAIT_TIME_1 * 1.25);
        kill(childPid, SIGTERM);
        
        SharegRegionTest test(SHARE_NAME_1);
        test.CreateSharedMemory();

        compare<unsigned int>(test.Counter(), 0, "Synchronous process values incorrect");
        compare<long>(test.Timestamp(), 0, "Synchronous process values incorrect");
    }
}

//====================================================================================================

void Test_TwoProcesses_WhenOneUnlinks_OtherUnaffected()
{
    logtest(__func__);

    pid_t childPid = fork();
    assert(childPid >= 0, "Process fork failed!");

    if (childPid == 0)
    {
        /*
         * Child Process -- don't do assertions here!
         */
        SharegRegionTest test(SHARE_NAME_1);
        test.CreateSharedMemory();
        test.Wait();
        test.DestroySharedMemory();

        // Don't proceed till killed by parent
        while (1) SleepFor(CHILD_SLEEP_TIME);
    }
    else
    {
        /*
         * Parent Process -- assert only after child process stopped!
         */
        SharegRegionTest test(SHARE_NAME_1);
        test.CreateSharedMemory();
        test.SetCounter(TEST_COUNT_1);
        test.SetTimestamp(TEST_TIMESTAMP);
        test.Wait();
        test.Wait();

        const auto counter = test.Counter();
        const auto timestamp = test.Timestamp();

        kill(childPid, SIGTERM);

        compare<unsigned int>(counter, TEST_COUNT_1, "Synchronous process values incorrect");
        compare<long>(timestamp, TEST_TIMESTAMP, "Synchronous process values incorrect");
    }
}

//====================================================================================================
//====================================================================================================

static std::vector<TEST_TYPE> GetSharedRegionTests()
{
    return std::vector<TEST_TYPE> {
        &Test_SingleProcess_DefaultValues_Correct,
        &Test_SingleProcess_SetCounts_Correct,
        &Test_SingleProcess_SetCounts_ResetsOnDestroy,
        &Test_SingleProcess_Unmap_FreesMemory,
        &Test_SingleProcess_NoDestroy_RetainsOldValues,
        &Test_TwoProcesses_SetValues_Shared,
        &Test_TwoProcesses_RunSynchronous_GetDefaultValues,
        &Test_TwoProcesses_WhenOneUnlinks_OtherUnaffected,
    };
}