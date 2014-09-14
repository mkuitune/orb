// See unittester.h for instructions.
//
// This code is in the public domain.
//
// The software is provided "as is", without warranty of any kind etc.
//
// Author: Mikko Kuitunen (mikko <dot> kuitunen <at> iki <dot> fi)
//
#include "unittester.h"

TestSet*                  g_tests = 0;

struct ActiveGroups
{
    ~ActiveGroups()
    {
        if(g_tests) delete g_tests;
    }
    void init()
    {
        if(!g_tests) g_tests = new TestSet();
    }
} g_ag;


UtTestAdd::UtTestAdd(const TestCallback& callback)
{
    g_ag.init();
    if((*g_tests).count(callback.group) == 0) (*g_tests)[callback.group] = TestGroup();

    (*g_tests)[callback.group][callback.name] = callback;
}


// Globals
bool g_exec_result;


void ut_test_err()
{
    ut_test_out() << "Error.";
    g_exec_result = false;
}

// Output stream mapping.
std::ostream* g_outstream = &std::cout;
std::ostream& ut_test_out(){return *g_outstream;}


/** Execute one test.*/
void run_test(const TestCallback& test)
{
    //ut_test_out() << "Run " << test.name << " ";
    g_exec_result = true;

    test.callback();

    if(g_exec_result) ut_test_out() << "\n[Test " << test.group << ":" << test.name << " passed.]" << std::endl;
    else              ut_test_out() << "\n[Test " << test.group << ":"<< test.name << " FAILED.]" << std::endl; 
}

/** Execute the test within one test group.*/
void run_group(TestSet::value_type& group)
{
    ut_test_out() << "Group:'" << group.first << "'" << std::endl;
    for(auto g = group.second.begin(); g != group.second.end(); ++g) run_test(g->second);
}

/** If the user has not defined any specific groups for testrun then this function executes all of the tests.*/
bool run_all_tests()
{
    bool result = true;
    for(auto t = g_tests->begin(); t != g_tests->end(); ++t) run_group(*t);
    return result;
}

int main(int argc, char* argv[])
{
    // Filter tests are run based on input: test names with any matching strings are run.
    // If there is no input, then all tests are run.
    if(argc < 2)
    {
        run_all_tests();
    }
    else
    {
        for(auto& test_set : *g_tests)
        {
            for(auto& test_group : test_set.second)
            {
                bool run = false;
                for(int i = 1; i < argc; ++i){
                    if(test_group.second.names_match(argv[i])) run = true;
                }
                if(run) run_test(test_group.second);
            }
        }
    }

    return 0;
}
