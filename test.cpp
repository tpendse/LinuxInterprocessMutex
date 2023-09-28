#include "TestHelpers.h"
#include "SharedRegionTests.h"
#include "SharedMutexTests.h"

void runAllTest(const std::vector<TEST_TYPE>& tests)
{
	unsigned int failcount = 0;
	for (auto test : tests)
	{
		if (!runtest(test))
			++failcount;
	}

	if (failcount == 0)
		std::cout << "All " << tests.size() << " tests passed" << std::endl;
	else
		std::cerr << "Error: " << failcount << " tests out of " << tests.size() << " failed!" << std::endl;
}


int main(void)
{
	std::cout << divider1 << std::endl << "Starting Shared Region Tests ..." << std::endl;
	auto sharedRegionTests = GetSharedRegionTests();
	runAllTest(sharedRegionTests);

	std::cout << divider1 << std::endl << "Starting Shared Mutex Tests ..." << std::endl;
	auto sharedMutexTests = GetSharedMutexTests();
	runAllTest(sharedMutexTests);
	
	return 0;
}