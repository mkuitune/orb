/** \file unittester.h
\author Mikko Kuitunen (mikko <dot> kuitunen <at> iki <dot> fi)
MIT licence.
*/

// This is a lightweight unittest suite.
//
// It contains two files - unittester.h and unittester.cpp. The latter implements the
// test entry point main -function.
//
// Include unittester.h in every file which implements tests and
// compile them with unittester.cpp into an executable.
//
//
// Usage
// -----
// The macro
//
// UTEST(<test_group>, <test_name>)
//
// declares a test after which the test body follows.
//
// The macros
//
// ASSERT_TRUE(<statement_in_c++>, <message string>)
// ASSERT_FALSE(<statement_in_c++>, <message string>)
//
// catch erroneus states, seize the execution of a particular test and signal the test suite of failure in a particular test.
//
// Error in one test does not terminate the execution of the entire test suite.
//
// Example:
//
// UTEST(string_tests, catenate)
// {
//  std::string a("foo");
//  std::string b("bar");
//  std::string c = a + b;
//  ASSERT_TRUE(c == std::string("foobar"), "String catenation using operator '+' failed.");
// }
//
//
// Complete example:
// ----------------
//
// (in own_tests.cpp)
//
// UTEST(string_tests, constructor)
// {
//  std::string a("foo");
//  ASSERT_TRUE(strcmp("foo", a.c_str() == 0, "String constructor failed.");
// }
//
// UTEST(string_tests, find)
// {
//  std::string a("foo");
//  ASSERT_TRUE(a.find('o') == 1, "String find failed.");
// }
//
// UTEST(list_tests, begin)
// {
//  std::list<int> list;
//  list.push_back(12);
//  ASSERT_TRUE(*(list.begin()) == 12 , "Acquiring iterator to begin of string failed.");
// }
//
//
// This code is in the public domain.
//
// The software is provided "as is", without warranty of any kind etc.
//
// Author: Mikko Kuitunen (mikko <dot> kuitunen <at> iki <dot> fi)
//
#pragma once


#include<list>
#include<map>
#include<functional>
#include<iostream> 
#include<string>
#include<memory>


/** Ouput stream for test logging. Usage: ut_test_out() << "Hello, you feisty tester's little helper!" */
std::ostream& ut_test_out();

void ut_test_err();

/** Use this macro to declare a test. */
#define UTEST(group_name, test_name)     \
class utestclass_##test_name {           \
    public: static void run();             \
};                                         \
UtTestAdd test_name##__add(TestCallback(utestclass_##test_name::run, #group_name, #test_name)); \
void utestclass_##test_name::run()

#define ORB_TEST_LOG(msg_param)do{ ut_test_out() << std::endl << "  " << msg_param << std::endl;}while(0)

// Asset that the stmnt_param evaluates to true, or signall failed test and log the message in msg_param. Test ends.
#define ASSERT_TRUE(stmnt_param, msg_param)do{if(!(stmnt_param)){ut_test_err(); ut_test_out() << std::endl << "  " << msg_param << std::endl;return;}}while(0)

// Asset that the stmnt_param evaluates to false, or signall failed test and log the message in msg_param. Test ends.
#define ASSERT_FALSE(stmnt_param, msg_param)do{if(stmnt_param){ut_test_err(); ut_test_out() << std::endl << "  " << msg_param << std::endl;return;}}while(0)


/////////// Test utilities ///////////

template<class T>
std::list<T> range_to_list(T start, T delta, T end)
{
    T value;
    std::list<T> result;

    for(value = start; value < end; value += delta) result.push_back(value);

    return result;
}

template<class T>
void print_container(T container)
{
    auto begin = container.begin();
    auto end = container.end();
    ut_test_out() << range_to_string(begin, end) << std::endl;
}


////////////// Implementation /////////////////

typedef std::function<void(void)> UtTestFun;

struct TestCallback{

    static void dummy(void){}

    UtTestFun callback; 
    std::string group;
    std::string name;
    TestCallback(UtTestFun f, const char* grp, const char* str):
        callback(f), group(grp), name(str){}
   TestCallback():callback(TestCallback::dummy), name(""){}
   bool names_match(const char* str){return (group.find(str) != std::string::npos) || (name.find(str) != std::string::npos);}
};

typedef std::map<std::string, TestCallback> TestGroup;
typedef std::map<std::string, TestGroup> TestSet;


class UtTestAdd
{
public:
    UtTestAdd(const TestCallback& callback);
};
