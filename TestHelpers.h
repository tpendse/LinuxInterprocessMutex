#pragma once

#include <string>
#include <iostream>
#include <chrono>
#include <thread>

constexpr const char *divider1 = "======================================";
constexpr const char *divider2 = "--------------------------------------";
constexpr const char *divider3 = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
std::string LastError = "empty";


constexpr long          CHILD_SLEEP_TIME = 10 * 60 * 1000; /* 10 minutes */
constexpr const char*   SHARE_NAME_1     = "SHARE_1";
constexpr long          WAIT_TIME_1      = 500;  /* 500 ms */
constexpr long          WAIT_TIME_2      = 750;  /* 750 ms */
constexpr unsigned int  TEST_COUNT_1     = 1234;
constexpr long          TEST_TIMESTAMP   = 999999;


typedef void (*TEST_TYPE)();

static void logtest(const char *testname)
{
	std::cout << "Running: " << testname << std::endl;
}

static void handle_error(const std::string &message)
{
	std::cerr
		<< "Fail!" << std::endl
		<< message << std::endl
		<< divider2 << std::endl;
}

bool runtest(TEST_TYPE test)
{
	try
	{
		test();
		return true;
	}
	catch (...)
	{
		handle_error(LastError);
		return false;
	}
}

static void assert(bool flag, const std::string &message)
{
	if (!flag)
	{
		LastError = message;
		throw std::runtime_error(message.c_str());
	}
}

template <typename T>
static void compare(const T &expected, const T &actual, const std::string &message)
{
	const bool flag = expected == actual;
	if (!flag)
	{
		std::cerr << expected << " != " << actual << std::endl;
		assert(flag, message);
	}
}

static void SleepFor(const long milliseconds) {
	std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}