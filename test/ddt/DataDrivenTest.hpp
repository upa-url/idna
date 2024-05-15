/*
Copyright (c) 2017-2023 Rimas Misevičius

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef DATA_DRIVEN_TEST_H
#define DATA_DRIVEN_TEST_H

#include <iostream>
#include <string>
#include <type_traits>
#include <utility>

namespace {

// Debugger suppor

#if defined(_MSC_VER)

extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent();

inline bool isDebuggerActive() { return IsDebuggerPresent() != 0; }

#define DATA_DRIVEN_TEST_DEBUG_BREAK	__debugbreak()

#else

inline bool isDebuggerActive() { return false; }

#endif


// To check name type is string

template<typename T>
struct is_string : std::integral_constant<bool,
	std::is_same<char*, typename std::decay<T>::type>::value ||
	std::is_same<const char*, typename std::decay<T>::type>::value> {};

template <typename Traits, typename Allocator>
struct is_string<std::basic_string<char, Traits, Allocator>> : std::true_type {};


// Value output

template <class T>
inline const T& vout(const T& v) {
	return v;
}

inline const char* vout(bool v) {
	return v ? "true" : "false";
}

} // namespace

class DataDrivenTest {
public:
	class TestCase {
	public:
		template <typename StrT, typename std::enable_if<is_string<StrT>::value, int>::type = 0>
		TestCase(DataDrivenTest& test, StrT const& name)
			: m_test(test)
			, m_name(name)
		{}
		TestCase(TestCase&& other) noexcept = default;

		~TestCase() {
			if (m_failure_count) {
				++m_test.m_fail_count;
				// failure reported in report_failure()
			} else if (m_success_count) {
				++m_test.m_pass_count;
				if (m_test.m_show_passed)
					std::cout << "[PASS] " << m_name << "\n";
			} else {
				// no tests executed
				std::cout << "[----] " << m_name << "\n";
			}
		}

		template<class T, class ExpT, typename StrT, typename std::enable_if<is_string<StrT>::value, int>::type = 0>
		TestCase& assert_equal(const ExpT& expected, const T& value, StrT const& value_name) {
			if (value == expected) {
				report_success();
			} else {
				report_failure()
					<< value_name << ":\n"
					<< " - actual + expected\n"
					<< "  -" << vout(value) << "\n"
					<< "  +" << vout(expected) << std::endl;
			}
			return *this;
		}

		template <class TestFun, typename StrT, typename std::enable_if<is_string<StrT>::value, int>::type = 0>
		TestCase& assert_throws(TestFun test_fun, StrT const& name) {
			try {
				test_fun();
			}
			catch (...) {
				report_success();
				return *this;
			}
			report_failure() << name << " did not throw exception\n";
			return *this;
		}

		template <class Exception, class TestFun, typename StrT, typename std::enable_if<is_string<StrT>::value, int>::type = 0>
		TestCase& assert_throws(TestFun test_fun, StrT const& name) {
			try {
				test_fun();
			}
			catch (const Exception&) {
				report_success();
				return *this;
			}
			catch (...) {
				try {
					throw;
				}
				catch (const std::exception& ex) {
					report_failure() << name << " threw not expected EXCEPTION: " << ex.what() << "\n";
				}
				catch (...) {
					report_failure() << name << " threw not expected UNKNOWN EXCEPTION\n";
				}
				return *this;
			}
			report_failure() << name << " did not throw exception\n";
			return *this;
		}

		void success() {
			report_success();
		}
		std::ostream& failure() {
			return report_failure();
		}

		bool is_failure() const {
			return m_failure_count != 0;
		}
	protected:
		void report_success() {
			++m_success_count;
		}
		std::ostream& report_failure() {
			if (m_failure_count == 0) {
				std::cout << "[FAILED] " << m_name << "\n";
			}
			++m_failure_count;
			return std::cout;
		}

		DataDrivenTest& m_test;
		std::string m_name;
		int m_success_count = 0;
		int m_failure_count = 0;
	};

	// constructor
	DataDrivenTest() = default;

	~DataDrivenTest() {
		const int all_count = m_pass_count + m_fail_count;
		std::cout << "\n";
		if (all_count) {
			if (m_pass_count)
				std::cout << m_pass_count << " passing" << std::endl;
			if (m_fail_count)
				std::cout << m_fail_count << " failing" << std::endl;
		} else {
			std::cout << "No tests!" << std::endl;
		}
	}

	// config
	void config_show_passed(bool show) noexcept {
		m_show_passed = show;
	}
	void config_debug_break(bool on) noexcept {
		m_debug_break = on;
	}

	// test case
	template <typename StrT, typename std::enable_if<is_string<StrT>::value, int>::type = 0>
	TestCase test_case(StrT const& name) {
		return TestCase(*this, name);
	}

	template <class TestFun, typename StrT, typename std::enable_if<is_string<StrT>::value, int>::type = 0>
	void test_case(StrT const& name, TestFun test_fun) {
		TestCase tc(*this, name);
#if defined(DATA_DRIVEN_TEST_DEBUG_BREAK)
		bool was_dbg_break = false;
		while (true) {
#endif
			try {
				test_fun(tc);
			}
			catch (const std::exception& ex) {
				tc.failure() << "Test case threw EXCEPTION: " << ex.what() << "\n";
			}
			catch (...) {
				tc.failure() << "Test case threw UNKNOWN EXCEPTION\n";
			}
#if defined(DATA_DRIVEN_TEST_DEBUG_BREAK)
			// debug break;
			if (!was_dbg_break && tc.is_failure() && m_debug_break && isDebuggerActive()) {
				was_dbg_break = true;
				DATA_DRIVEN_TEST_DEBUG_BREAK;
			} else
				break;
		}
#endif
	}

	// return value for main function
	int result() const noexcept {
		return m_fail_count ? 1 : 0;
	}

protected:
	int m_pass_count = 0;
	int m_fail_count = 0;
	// config
	bool m_show_passed = false;
	bool m_debug_break = false;
};

#endif // DATA_DRIVEN_TEST_H
