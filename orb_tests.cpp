/** Unit tests */


//#include "glhack.h"
#include "persistent_containers.h"
#include "orb.h"
#include "orb_classwrap.h"
#include <string>
#include <functional>
#include <cassert>

using namespace std::placeholders;
#include "unittester.h"


int i;

template<class T>
bool expect_value(const orb::ValuePtr p, std::function<T(const orb::Value& v)> get, const T& comp, orb::Type expect_type)
{
    bool result = false;

    if(p->type == expect_type)
    {
        T tvalue = get(*p.get());
        if(comp == tvalue) result = true;
        else {
            std::ostringstream ostr;
            ostr << "expect_value: value mismatch. Expected:" << comp << " but got:" << *p.get() << std::endl;
            ORB_TEST_LOG(ostr.str());
        }
    }
    else
    {
        ORB_TEST_LOG(std::string("expect_value:type mismatch."));
    }

    return result;
}

template<class T>
bool compare_parsing(orb::Orb& m, const char* str, std::function<T(const orb::Value& v)> get, const T& comp, orb::Type expect_type)
{
    bool result = false;

    orb::orb_result r = orb::read_eval(m, str);

    if(r.valid()){

        result = expect_value(*r.as_value(), get, comp, expect_type);
    }
    else
    {
        ORB_TEST_LOG(r.message());
    }

    return result;
}

#define FAKE_CONTENTS "Fake!"
class FakeInputFile{
public:
    FakeInputFile(const std::string& path):path_(path){}
    ~FakeInputFile(){}
    std::istream& file(){}
    bool is_open(){
        return true;
    }
    void close(){}
    std::tuple<std::string, bool> contents_to_string(){
        return std::make_tuple(std::string(FAKE_CONTENTS), true);
    }

    std::string path_;

    class Printer{public: static std::string to_string(const FakeInputFile& f){return f.path_;}};
};

typedef orb::WrappedObject<FakeInputFile> WrappedInput;


orb::Value make_FakeInputFile(orb::Orb& m, orb::Vector& args, orb::Map& env){
    orb::VecIterator arg_start = args.begin();
    orb::VecIterator arg_end = args.end();

    std::string path;
    orb::ArgWrap(arg_start, arg_end).wrap(&path);

    orb::Value obj = orb::make_value_object(new WrappedInput(path));

    auto is_open = orb::wrap_member(&FakeInputFile::is_open);
    auto close = orb::wrap_member(&FakeInputFile::close);
    auto contents_to_string = orb::wrap_member(&FakeInputFile::contents_to_string);

    orb::FunMap fmap(m);
    fmap.add("is_open", is_open);
    fmap.add("close", close);
    fmap.add("contents_to_string", contents_to_string);

    orb::Value listv = orb::make_value_list(m);
    orb::List* l = orb::value_list(listv);

    *l = l->add(fmap.map());
    *l = l->add(obj);

    return listv;
}

UTEST(orb, object_interface_fake_input)
{
    using namespace orb;

    orb::Orb m;

    /*
         builder: return value, tahtn
    */

    orb::add_fun(m, "FakeInputFile", make_FakeInputFile);

    const char* src = "(def m (FakeInputFile \"input.txt\" ))"
                      "(def res (. 'is_open m))"
                      "(def contlist (. 'contents_to_string m))"
                      "(def contstring (first contlist))"
                      ;

    orb::orb_result evalresult = orb::read_eval(m, src);

    ASSERT_TRUE(evalresult.valid(), "Unsuccesfull parsing");

    auto res = orb::get_value(m, "res");

    ASSERT_TRUE(orb::value_type(res) == orb::BOOLEAN, "Type is not boolean");
    ASSERT_TRUE(orb::value_boolean(*res), "Result was not true.");


    auto contstring = orb::get_value(m, "contstring");
    auto contlist = orb::get_value(m, "contlist");

    ASSERT_TRUE(orb::value_type(contstring) == orb::STRING, "Type is not string");
    ASSERT_TRUE(strcmp(orb::value_string(*contstring),FAKE_CONTENTS) == 0
        , "Result did not match expected.");
}




UTEST(orb, get_value)
{
    using namespace orb;

    orb::Orb m;

    const char* def1 = "";

    ASSERT_TRUE(compare_parsing<orb::Number>(m, "1", orb::value_number, orb::Number::make(1), orb::NUMBER), "value mismatch");
    ASSERT_TRUE(compare_parsing<std::string>(m, "\"foo\"", orb::value_string, std::string("foo"), orb::STRING), "value mismatch");
    ASSERT_TRUE(compare_parsing<std::string>(m, "(def fooname \"foo\") fooname", orb::value_string, std::string("foo"), orb::STRING), "value mismatch");
}

UTEST(orb, simple_evaluations)
{
    using namespace orb;

    orb::Orb m;
    ASSERT_TRUE(compare_parsing<orb::Number>(m, "1", orb::value_number, orb::Number::make(1), orb::NUMBER), "value mismatch");
    ASSERT_TRUE(compare_parsing<std::string>(m, "\"foo\"", orb::value_string, std::string("foo"), orb::STRING), "value mismatch");
    ASSERT_TRUE(compare_parsing<std::string>(m, "(def fooname \"foo\") fooname", orb::value_string, std::string("foo"), orb::STRING), "value mismatch");

}

UTEST(orb, simple_parsing)
{
    using namespace orb;

    orb::Orb m;

    auto parsestr = [&m](const char* str)
    {
        orb::orb_result a = orb::string_to_value(m, str);

        if(a.valid())
        {
            const orb::Value* ptr = (*a).get();
            std::string value_str = orb::value_to_typed_string(ptr);
            //ORB_TEST_LOG(value_str);
        }
    };

    parsestr("1");
    parsestr("0xf");
    parsestr("0b101");

    parsestr("\"foo\"");
    parsestr("(\"foo\")");
    parsestr("'(\"foo\" \"bar\")");
    parsestr("(+ 1 2)");
}


#if 0
class WrappedInStream{ public:
    virtual ~WrappedInStream(){}
    virtual std::string readline() = 0;
    virtual std::string readall() = 0;};

template<class T> class StreamIn : public WrappedInStream { public:
    T& stream_; StreamIn(T& stream):stream_(stream){} 
    virtual std::string readline() override {
        std::ostringstream os;
        stream_ 
    };
    virtual std::string readall() override {}
};

class WrappedOutStream{ public:
    virtual ~WrappedOutStream(){}
    virtual void write(const char* str) = 0;};

template<class T> class StreamOut : public WrappedOutStream{ public:
    T& stream_; StreamOut(T& stream):stream_(stream){}
    virtual void write(const char* str) override{stream_ << str;}
};
#endif