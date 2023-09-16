#ifndef DATA_DRIVEN_TEST_H
#define DATA_DRIVEN_TEST_H

#include <iostream>
#include <string>
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


// Value output

template <class T>
const T& vout(const T& v) { return v; }

const char* vout(bool v) {
	return v ? "true" : "false";
}

} // namespace

class DataDrivenTest {
public:
	class TestCase {
	public:
		TestCase(DataDrivenTest& test, const char* name)
			: m_test(test)
			, m_name(name)
			, m_success_count(0)
			, m_failure_count(0)
		{}
		TestCase(TestCase&& other)
			: m_test(other.m_test)
			, m_name(std::move(other.m_name))
			, m_success_count(other.m_success_count)
			, m_failure_count(other.m_failure_count)
		{}

		~TestCase() {
			if (m_failure_count) {
				m_test.m_fail_count++;
				// failure reported in report_failure()
			} else if (m_success_count) {
				m_test.m_pass_count++;
				if (m_test.m_show_passed)
					std::cerr << "[PASS] " << m_name << "\n";
			} else {
				// no tests executed
				std::cerr << "[----] " << m_name << "\n";
			}
		}

		template<class T>
		TestCase& assert_equal(const T& expected, const T& value, const char* value_name) {
			if (std::equal_to<T>()(value, expected)) {
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
			m_success_count++;
		}
		std::ostream& report_failure() {
			if (m_failure_count == 0) {
				std::cerr << "[FAILED] " << m_name << "\n";
			}
			m_failure_count++;
			return std::cerr;
		}

		DataDrivenTest& m_test;
		std::string m_name;
		int m_success_count;
		int m_failure_count;
	};

	// constructor
	DataDrivenTest()
		: m_pass_count(0)
		, m_fail_count(0)
		, m_show_passed(false)
		, m_debug_break(false)
	{}

	~DataDrivenTest() {
		int all_count = m_pass_count + m_fail_count;
		std::cerr << "\n";
		if (all_count) {
			if (m_pass_count)
				std::cerr << m_pass_count << " passing" << std::endl;
			if (m_fail_count)
				std::cerr << m_fail_count << " failing" << std::endl;
		} else {
			std::cerr << "No tests!" << std::endl;
		}
	}

	// config
	void config_show_passed(bool show) {
		m_show_passed = show;
	}
	void config_debug_break(bool on) {
		m_debug_break = on;
	}

	// test case
	TestCase test_case(const char* name) {
		return TestCase(*this, name);
	}

	template <class TestFun>
	void test_case(const char* name, TestFun test_fun) {
		TestCase tc(*this, name);
		test_fun(tc);
#if defined(DATA_DRIVEN_TEST_DEBUG_BREAK)
		// debug break;
		if (tc.is_failure() && m_debug_break && isDebuggerActive()) {
			DATA_DRIVEN_TEST_DEBUG_BREAK;
			test_fun(tc);
		}
#endif
	}

	// return value for main function
	int result() const {
		return m_fail_count ? 1 : 0;
	}

protected:
	int m_pass_count;
	int m_fail_count;
	// config
	bool m_show_passed;
	bool m_debug_break;
};

#endif // DATA_DRIVEN_TEST_H
