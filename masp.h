/** \file masp.h
    \author Mikko Kuitunen (mikko <dot> kuitunen <at> iki <dot> fi)
*/
#pragma once

#include "orb_lib.h"

#include "annotated_result.h"
#include "persistent_containers.h"
#include "math_tools.h"

#include<list>
#include<memory>
#include<cstdint>
#include<ostream>
#include<deque>
#include<functional>
#include<vector>

namespace orb{

enum Type{NIL, BOOLEAN, NUMBER, NUMBER_ARRAY, STRING, SYMBOL, VECTOR, LIST, MAP, OBJECT, FUNCTION};

struct Number{
    enum Type{INT, FLOAT};
    union
    {
        int    intvalue;
        double floatvalue;
    }value;
    Type type;

    Number& set(const int i){type = INT; value.intvalue = i; return *this;}
    Number& set(const double d){type = FLOAT; value.floatvalue = d; return *this;}
    Number& set(const Number& n){
        type = n.type;
        if(type == INT) value.intvalue = n.value.intvalue;
        else            value.floatvalue = n.value.floatvalue;
        return *this;
    }

    int to_int() const
    {
        if(type == INT) return value.intvalue;
        else return (int) value.floatvalue;
    }
    
    double to_float() const
    {
        if(type == INT) return (double) value.intvalue;
        else return value.floatvalue;
    }

    bool operator==(const Number& n) const{
        if(type != n.type) return false;
        else return type == INT ? (value.intvalue == n.value.intvalue) : (value.floatvalue == n.value.floatvalue);
    }

    Number& operator+=(const Number& n)
    {
        if(type == FLOAT || n.type == FLOAT) return set(to_float() + n.to_float()); 
        else return set(to_int() + n.to_int());
    }

    Number& operator-=(const Number& n)
    {
        if(type == FLOAT || n.type == FLOAT) return set(to_float() - n.to_float()); 
        else return set(to_int() - n.to_int());
    }

    Number& operator*=(const Number& n)
    {
        if(type == FLOAT || n.type == FLOAT) return set(to_float() * n.to_float()); 
        else return set(to_int() * n.to_int());
    }

    Number& operator/=(const Number& n)
    {
        if(type == FLOAT || n.type == FLOAT) return set(to_float() / n.to_float()); 
        else return set(to_int() / n.to_int());
    }

    bool operator<(const Number& n) const
    {
        if(type == FLOAT || n.type == FLOAT) return to_float() < n.to_float(); else return to_int() < n.to_int();
    }
    bool operator<=(const Number& n) const
    {
        if(type == FLOAT || n.type == FLOAT) return to_float() <= n.to_float(); else return to_int() <= n.to_int();
    }
    bool operator>=(const Number& n) const
    {
        if(type == FLOAT || n.type == FLOAT) return to_float() >= n.to_float(); else return to_int() >= n.to_int();
    }
    bool operator>(const Number& n) const
    {
        if(type == FLOAT || n.type == FLOAT) return to_float() > n.to_float(); else return to_int() > n.to_int();
    }

    static Number make(int i){Number n; n.set(i); return n;}
    static Number make(double f){Number n; n.set(f); return n;}

    friend std::ostream& operator<<(std::ostream& os, const Number& n){
        if(n.type == Number::INT) os << n.to_int(); else os << n.to_float();
        return os;
    }

};

typedef std::vector<Number> NumberArray;

class Value;

// define hash function for value

class ValuesAreEqual { public:
    static bool compare(const Value& k1, const Value& k2);
};

class ValueHash { public:
    static uint32_t hash(const Value& h);
};


typedef orb::PMapPool<Value, Value, ValuesAreEqual, ValueHash> MapPool;
typedef MapPool::Map   Map;

typedef orb::PListPool<Value>              ListPool;
typedef orb::PListPool<Value>::List        List;
typedef List::iterator VRefIterator;

/**  Object interface */

class IObject{
public:
    virtual ~IObject(){}
    virtual std::string to_string() = 0;
    virtual IObject* copy() = 0;
};

struct Function;

/** Masp value. */
class Value
{
public:
    Type type;

    typedef std::deque<Value> Vector;

    union
    {
        Number       number;
        std::string* string; //> Data for string | symbol
        List*        list;
        Map*         map;
        Vector*      vector;
        Function*    function;
        IObject*     object;
        NumberArray* number_array;
        bool         boolean;
    } value;

    Value();
    ~Value();
    Value(const Value& v);
    Value(Value&& v);
    Value& operator=(const Value& v);
    Value& operator=(Value&& v);
    void alloc_str(const char* str);
    void alloc_str(const std::string& str);
    void alloc_str(const char* str, const char* end);
    bool is_nil() const;
    bool is_str(const char* str);
    bool is(const Type t) const{return type == t;}
    bool operator==(const Value& v) const;
    uint32_t get_hash() const;

    void movefrom(Value& v);

private:
    void dealloc();
    void copy(const Value& v);

};

class Masp;

typedef Value::Vector Vector;
typedef Vector::iterator VecIterator;
typedef std::function<Value(Masp& m, Vector& args, Map& env)> PrimitiveFunction;


void free_value(Value* v);

class ValueDeleter{
public:
    void operator()(Value* v){free_value(v);}
};

typedef std::shared_ptr<Value> ValuePtr;

/** Script environment. */
class ORB_LIB Masp
{
public:

#define MASP_VERSION 0.01


    Masp();
    ~Masp();

    // TODO: replace std::List with plist
    // TODO: Eval : place value of atom to applied list
    // TODO: Apply: fun -> list -> value

    // Environment
    class Env;

    /** Return pointer to the environment instance. */
    Env* env();

    /** Return reference to current env map. */
    Map& env_map();

    /** Garbage collect the used data structures.*/
    void gc();

    /** Number of bytes used by the state.*/
    size_t reserved_size_bytes();

    /** Number of bytes marked used.*/
    size_t live_size_bytes();

    /** Set output stream for messages. */
    void set_output(std::ostream* os);

    /** Get handle to output stream.*/
    std::ostream& get_output();

    void set_args(int argc, char* argv[]);

private:

    Env* env_;
};


typedef orb::AnnotatedResult<ValuePtr> masp_result;

masp_result masp_fail(const char* str);
masp_result masp_fail(const std::string& str);

/** Parse string to value data structure.*/
ORB_LIB masp_result string_to_value(Masp& m, const char* str);

/** Evaluate the datastructure held within the atom in the context of the Masp env. Return result as atom.*/
ORB_LIB masp_result eval(Masp& m, const Value* v);

/** Parse string and evaluate result */
masp_result read_eval(Masp& m, const char* str);

/** Parse and evaluate contents of file and return the result as a value data structure. */
// TODO: masp_result readfile(Masp& m, const char* file_path);

/** Return string representation of value. */
ORB_LIB std::string value_to_string(const Value& v);

std::ostream& operator<<(std::ostream& os, const Value& v);

/** Return string representation of value annotated with type. */
ORB_LIB std::string value_to_typed_string(const Value* v);

/** Return type of value as string.*/
const char* value_type_to_string(const Value* v);


void add_fun(Masp& m, const char* name, PrimitiveFunction f);

/// State accessors

/** Try to access value of name 'valpath' from m root env. Recursive access from maps is supported through
*   URI paths, ie foo/bar will attempt to access the value in key 'bar' in map 'foo'. And "cat/hat/rat" will
*   attempt to access the value by the key 'rat' held in the map stored in the map cat by the key 'hat' etc.
*   If the value is not found, a null pointer is returned.*/
const Value* get_value(Masp& m, const char* path);

/** Get type of value. @param v Value to query @return Type of v. If v is null returns NIL. */
Type         value_type(const Value* v);


// object_factory = return list (value:IObject, value:map{name, objfun, name, objfun})
// objfun : void foo(Masp& m, VecIterator arg_start, VecIterator arg_end, Map& env)
// {obj = value_object(&*arg_start); if(obj){inst = dynamic_cast<inst_type>(obj)}}
// TOOD: add shorthand (. fun obj params) :=  (((fnext obj) fun) (first obj) params) = 
//                           

Vector*      value_vector(Value& v);
NumberArray* value_number_array(Value& v);
Map*         value_map(const Value& v);
IObject*     value_object(const Value& v);
Number       value_number(const Value& v);
List*        value_list(const Value& v);
const char*  value_string(const Value& v);
bool         value_boolean(const Value& v);

const Value* value_list_first(const Value& v);
const Value* value_list_nth(const Value& v, size_t n);

// Value factories
Value make_value_number(const Number& num);
Value make_value_number(int i);
Value make_value_number(double d);

Value make_value_string(const char* str);
Value make_value_string(const std::string& str);
Value make_value_string(const char* str, const char* str_end);

Value make_value_symbol(const char* str);
Value make_value_symbol(const char* str, const char* str_end);
Value make_value_list(Masp& m);
Value make_value_list(const List& oldlist);

Value make_value_map(Masp& m);
Value make_value_map(const Map& oldmap);

Value make_value_function(PrimitiveFunction f);

Value make_value_object(IObject* alloced_object);

Value make_value_vector();
Value make_value_vector(Vector& old, Value& v);
Value make_value_vector(Value& v, Vector& old);

Value make_value_number_array();
Value make_value_boolean(bool b);

/** Evaluation errors will throw a EvaluationException. */ 
class EvaluationException 
{
public:

    EvaluationException(const char* msg):msg_(msg){}
    EvaluationException(const std::string& msg):msg_(msg){}
    ~EvaluationException(){}

    std::string get_message() const {return msg_;}

    std::string msg_;
};

}//Namespace orb

//template<class R, class P>
//R to_number(P in){return (R) in;}
template<> inline orb::Number from_int<orb::Number>(int i){return orb::Number::make(i);}
//template<> inline orb::Number from_int<orb::Number,double>(double d){return orb::Number::make(d);}

