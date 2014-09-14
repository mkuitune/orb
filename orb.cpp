/** \file orb.cpp
    \author Mikko Kuitunen (mikko <dot> kuitunen <at> iki <dot> fi)
*/
#include "orb_classwrap.h"
#include "persistent_containers.h"
#include "iotools.h"

#include <regex>


#include<stack>
#include<cstring>
#include<algorithm>
#include<cstdlib>
#include<cctype>
#include<sstream>
#include<numeric>
#include<tuple>
#include<utility>
#include<limits>
#include<type_traits>

namespace {
void local_assert(const char* msg)
{
    assert(!msg);
}
}
namespace orb{

bool all_are_float(NumberArray& arr)
{
    bool result = true;
    for(auto n = arr.begin(); n != arr.end() && result; ++n) result = n->type == Number::FLOAT;
    return result;
}

bool all_are_int(NumberArray& arr)
{
    bool result = true;
    for(auto n = arr.begin(); n != arr.end() && result; ++n) result = n->type == Number::INT;
    return result;
}

void convert_to_float(NumberArray& arr)
{
    for(auto n = arr.begin(); n != arr.end(); ++n){double d = n->to_float(); n->set(d);}
}

void convert_to_int(NumberArray& arr)
{
    for(auto n = arr.begin(); n != arr.end(); ++n){int i = n->to_int(); n->set(i);}
}

struct Function{PrimitiveFunction fun;};

// ValuesAreEqual and ValueHash member implementations
bool ValuesAreEqual::compare(const Value& k1, const Value& k2){return k1 == k2;} 
uint32_t ValueHash::hash(const Value& h){return h.get_hash();}

Value::Value():type(NIL){}

void Value::dealloc()
{
    if((type == STRING || type == SYMBOL) && value.string)
    {
        delete value.string;
    }
    else if(type == LIST && value.list)      { delete value.list;}
    else if(type == MAP && value.map)        { delete value.map;}
    else if(type == OBJECT && value.object)  { delete value.object;}
    else if(type == VECTOR && value.vector)  { delete value.vector;}
    else if(type == FUNCTION && value.function)  { delete value.function;}
    else if(type == NUMBER_ARRAY && value.number_array)  { delete value.number_array;}
}

Value::~Value()
{
    dealloc();
}

bool Value::is_str(const char* str){return orb::any_of(type, STRING, SYMBOL) && strcmp(value.string->c_str(), str) == 0;}

template<class V>
V* copy_new(const V* v)
{
    V* result = 0;
    if(v)
    {
        result = new V(*v);
    }
    return result;
}

Value::Value(const Value& v)
{
    copy(v);
}

void Value::copy(const Value& v)
{
    type = v.type;

#define COPY_PARAM_V(param_name) value. param_name = copy_new(v.value. param_name)
    if(type == NUMBER) value.number.set(v.value.number);
    else if(type == SYMBOL || type == STRING) COPY_PARAM_V(string);
    else if(type == LIST)    COPY_PARAM_V(list);
    else if(type == MAP)     COPY_PARAM_V(map);
    else if(type == OBJECT) value.object = v.value.object->copy();
    else if(type == VECTOR)  COPY_PARAM_V(vector);
    else if(type == FUNCTION) COPY_PARAM_V(function);
    else if(type == NUMBER_ARRAY) COPY_PARAM_V(number_array);
    else if(type == BOOLEAN) value.boolean = v.value.boolean;
    else if(type != NIL)
        {local_assert("Faulty param type.");}
#undef COPY_PARAM_V
}

void Value::movefrom(Value& v)
{
    type = v.type;
    void* that_value_ptr = reinterpret_cast<void*>(&v.value); 
    size_t value_size = sizeof(value);
    memcpy(reinterpret_cast<void*>(&value), that_value_ptr, sizeof(value));  
    memset(that_value_ptr, 0, value_size);
}

Value::Value(Value&& v)
{
    movefrom(v);
}

Value& Value::operator=(const Value& a)
{
    if(&a != this)
    {
        copy(a);
    }
    
    return *this;
}

Value& Value::operator=(Value&& v)
{
    if(&v != this)
    {
        movefrom(v);
    }

    return *this; 
}

void Value::alloc_str(const std::string& str)
{
    value.string = new std::string(str);
}

void Value::alloc_str(const char* str)
{
    value.string = new std::string(str);
}

void Value::alloc_str(const char* str, const char* str_end)
{
    value.string = new std::string(str, str_end);
}

bool Value::is_nil() const {return type == NIL;}

bool Value::operator==(const Value& v) const
{
    if(v.type != type) return false;
    bool result = false;

    if(type == NUMBER)            result = value.number == v.value.number;
    else if(type == NUMBER_ARRAY) result = (*value.number_array) == (*v.value.number_array);
    else if(type == STRING || type == SYMBOL) result = (*value.string) == (*v.value.string);
    else if(type == VECTOR) result = (*value.vector) == *(v.value.vector);
    else if(type == LIST) result = (*(value.list) ==  *(v.value.list));
    else if(type == MAP) result = (*(value.map) == *(v.value.map));
    else if(type == OBJECT)
    {
        // TODO - what to do.
    }
    else if(type == FUNCTION)
    {
        // TODO - what to do
    }
    else if(type == BOOLEAN) result = value.boolean == v.value.boolean;
    else if(type == NIL) result = true;

    return result;
}

uint32_t hash_of_number(const Number& n){return *((uint32_t*) &n);}
uint32_t accum_number_hash(const uint32_t& p, const Number& n){return p * hash_of_number(n);}
uint32_t accum_value_hash(const uint32_t& p, const Value& v){return p * v.get_hash();}

uint32_t Value::get_hash() const
{
    // For collections produce a product of the hashes of contained elements
    uint32_t h = 0;

    if(type == BOOLEAN) h = (uint32_t) value.boolean;
    else if(type == NIL) h = std::numeric_limits<uint32_t>::max();
    else if(type == NUMBER)  h = hash_of_number(value.number);
    else if(type == NUMBER_ARRAY){
        uint32_t orig = 0;
        h = orb::fold_left<uint32_t, NumberArray>(orig, accum_number_hash, *value.number_array);
    }
    else if(type == STRING || type == SYMBOL) h = hash32(*value.string);
    else if(type == VECTOR)
    {
        uint32_t orig = 0;
        h = orb::fold_left<uint32_t, Vector>(orig, accum_value_hash, *value.vector);
    }
    else if(type == LIST) 
    {
        uint32_t orig = 0;
        h = orb::fold_left<uint32_t, List>(orig, accum_value_hash, *value.list);
    }
    else if(type == MAP)
    {
        uint32_t accum = 0;
        Map::iterator i = value.map->begin();
        Map::iterator end = value.map->end();
        while(i != end)
        {
            accum = accum_value_hash(accum_value_hash(accum, i->first), i->second);
            ++i;
        }
        h = accum;
    }
    else if(type == OBJECT)
    {
        // TODO
    }
    else if(type == FUNCTION)
    {
        // TODO
    }

    return h;
}


void free_value(Value* v)
{
    if(v) delete v;
}

Value* new_value()
{
    return new Value();
}

template<class VI>
bool all_are_of_type(VI begin, VI end, Type t, size_t* out_size)
{
   size_t size = 0;
   while(begin != end) 
   {
       if(begin->type != t) return false;
       ++begin;
       ++size;
   }

   if(out_size) *out_size = size;

   return true;
}


inline Number value_number(const Value& v){return v.type == NUMBER ? v.value.number : Number::make(0);}

inline List* value_list(const Value& v){return v.type == LIST ? v.value.list : 0;}

inline Vector* value_vector(const Value& v){return v.type == VECTOR ? v.value.vector : 0;}

const char* value_string(const Value& v){
    return (v.type == STRING ||  v.type == SYMBOL) ? v.value.string->c_str() : 0;
}

bool value_boolean(const Value& v){
    return (v.type == BOOLEAN) ? v.value.boolean : false;
}

inline const Value* value_list_first(const Value& v)
{
    const Value* result = 0;
    List* l = value_list(v);
    if(l && !l->empty())
    {
        result = l->begin().data_ptr();
    }
    return result;
}

inline const Value* value_list_nth(const Value& v, size_t n)
{
    const Value* result = 0;
    List* l = value_list(v);

    if(l && !l->empty()){ 
        auto i = l->begin();
        auto e = l->end();
        while(i != e && n > 0) {++i; --n;}
        if(i != e) result = i.data_ptr();
    }
    return result;
}

inline const Value* value_list_second(const Value& v)
{
    return value_list_nth(v, 1);
}

inline const Value* value_list_third(const Value& v)
{
    return value_list_nth(v, 2);
}

inline const Value* value_list_fourth(const Value& v)
{
    return value_list_nth(v, 3);
}

Vector* value_vector(Value& v){return v.type == VECTOR ? v.value.vector : 0;}

NumberArray* value_number_array(Value& v){return v.type == NUMBER_ARRAY ? v.value.number_array : 0;}

Map* value_map(const Value& v){return v.type == MAP ? v.value.map : 0;}
IObject* value_object(const Value& v){return v.type == OBJECT ? v.value.object : 0;}

void append_to_value_stl_list(std::list<Value>& ext_value_list, const Value& v)
{
    ext_value_list.push_back(v);
}

///// Orb::Env //////


// Custom Garbage collection to remove dangling references.
namespace {

void value_increment_references(const Value& v);
void map_increment_references(Map& map);

void list_increment_references(List& list)
{
#ifdef PRINT_GC
    std::cout << "#Inc: List" << std::endl;
#endif
    list.increment_ref();
    auto e = list.end();
    for(auto i = list.begin(); i != e; ++i)
    {
        value_increment_references(*i);
    }
}

void value_increment_references(const Value& v)
{
    if(v.type == MAP)
    {
        map_increment_references(*value_map(v));
    }
    else if(v.type == LIST)
    {
        list_increment_references(*value_list(v));
    }
}

void map_increment_references(Map& map)
{
#ifdef PRINT_GC
    std::cout << "#Inc: Map" << std::endl;
#endif
    map.increment_ref();
    auto e = map.end();
    for(auto i = map.begin(); i != e; ++i)
    {
        value_increment_references(i->first);
        value_increment_references(i->second);
    }
}


void collect_map_and_list_pools_with_roots(MapPool& map_pool, ListPool& list_pool, Map& map)
{
    // Mark all cells that can be visited only through root node
    // #1 Set reference counts to zero for all roots.
    // #2 Follow all references through map. For any map or list root cells increment
    //    reference count.
    // #3 Run garbage collection 
    // Caveats: If the dereferenced cells contain data to which a pointer is stored in 
    // some of the remaining cells, and it is deleted, the pointer is now invalid.
    map_pool.clear_root_refcounts();
    list_pool.clear_root_refcounts();

    map_increment_references(map);

    map_pool.gc();
    list_pool.gc();
}

}
class Orb::Env
{
public:
    Env()
    {
        env_.reset(new Map(map_pool_.new_map()));
        load_default_env();
        out_ = &std::cout;
    }

    ~Env()
    {
        map_pool_.kill();
        list_pool_.kill();
    }

    size_t reserved_size_bytes()
    {
        return list_pool_.reserved_size_bytes() + map_pool_.reserved_size_bytes();
    }

    size_t live_size_bytes()
    {
        return list_pool_.live_size_bytes() + map_pool_.live_size_bytes();
    }

    void gc()
    {
        collect_map_and_list_pools_with_roots(map_pool_, list_pool_, *env_);
    }

    void add_fun(const char* name, PrimitiveFunction f);

    void def(const Value& key, const Value& value);

    void load_default_env();
    void load_default_extensions();

    Map& get_env(){return *env_;}

    // Locals
    MapPool              map_pool_;
    ListPool             list_pool_;
    std::unique_ptr<Map> env_;
    std::ostream*        out_;
};


inline List new_list(Orb& m){return m.env()->list_pool_.new_list();}

template<class Cont>
inline List new_list(Orb& m, const Cont& container){return m.env()->list_pool_.new_list(container);}

inline List* new_list_alloc(Orb& m)
{
    return new List(m.env()->list_pool_.new_list());
}

inline Map* new_map_alloc(Orb& m)
{
    return new Map(m.env()->map_pool_.new_map());
}

inline MapPool& map_pool(Orb& m){return m.env()->map_pool_;}

inline Map new_map(Orb& m)
{
    return m.env()->map_pool_.new_map();
}

// Value factories

Value make_value_number(const Number& num)
{
    Value a;
    a.value.number.set(num);
    a.type = NUMBER;
    return a;
}

Value make_value_number(int i)
{
    Value a;
    a.value.number.set(i);
    a.type = NUMBER;
    return a;
}

Value make_value_number(double d)
{
    Value a;
    a.value.number.set(d);
    a.type = NUMBER;
    return a;
}

Value make_value_string(const char* str)
{
    Value a;
    a.type = STRING;
    a.alloc_str(str);
    return a;
}

Value make_value_string(const std::string& str)
{
    Value a;
    a.type = STRING;
    a.alloc_str(str);
    return a;
}

Value make_value_string(const char* str, const char* str_end)
{
    Value a;
    a.type = STRING;
    a.alloc_str(str, str_end);
    return a;
}


Value make_value_symbol(const char* str)
{
    Value a;
    a.type = SYMBOL;
    a.alloc_str(str);
    return a;
}

Value make_value_symbol(const char* str, const char* str_end)
{
    Value a;
    a.type = SYMBOL;
    a.alloc_str(str, str_end);
    return a;
}

Value make_value_list(Orb& m)
{
    Value a;
    a.type = LIST;
    List* list_ptr = new_list_alloc(m);
    a.value.list = list_ptr;
    return a;
}

Value make_value_list(const List& oldlist)
{
    Value a;
    a.type = LIST;
    List* list_ptr = new List(oldlist);
    a.value.list = list_ptr;
    return a;
}


Value* make_value_list_alloc(Orb& m)
{
    Value* a = new Value();
    a->type = LIST;
    List* list_ptr = new_list_alloc(m);
    a->value.list = list_ptr;
    return a;
}

Value make_value_map(Orb& m)
{
    Value a;
    a.type = MAP;
    Map* map_ptr = new_map_alloc(m);
    a.value.map = map_ptr;
    return a;
}

Value make_value_map(const Map& oldmap)
{
    Value a;
    a.type = MAP;
    Map* map_ptr = new Map(oldmap);
    a.value.map = map_ptr;
    return a;
}

Value make_value_function(PrimitiveFunction f)
{
    Value v;
    v.type = FUNCTION;
    v.value.function = new Function();
    v.value.function->fun = f;
    return v;

}

Value make_value_object(IObject* alloced_object)
{
    Value a;
    a.type = OBJECT;
    a.value.object = alloced_object;
    return a;
}

Value make_value_vector()
{
    // TODO ?
    Value a;
    a.type = VECTOR;
    a.value.vector = new Vector;
    return a;
}

Value make_value_vector(Vector& old, Value& v)
{
    // TODO ?
    Value a;
    a.type = VECTOR;
    a.value.vector = new Vector(old);
    a.value.vector->push_back(v);
    return a;
}

template<class I>
Value make_value_vector(Vector& old, I app_begin, I app_end )
{
    // TODO ?
    Value a;
    a.type = VECTOR;
    a.value.vector = new Vector(old);
    while(app_begin != app_end)
    {
        a.value.vector->push_back(*app_begin);
        ++app_begin;
    }
    return a;
}

Value make_value_vector(Value& v, Vector& old)
{
    // TODO ?
    Value a;
    a.type = VECTOR;
    a.value.vector = new Vector(old);
    a.value.vector->push_front(v);
    return a;
}

template<class I>
Value make_value_vector(I begin, I end)
{
    // TODO ?
    Value a;
    a.type = VECTOR;
    a.value.vector = new Vector(begin, end);
    return a;
}


Value make_value_number_array()
{
    // TODO ?
    Value a;
    a.type = NUMBER_ARRAY;
    a.value.number_array = new NumberArray();
    return a;
}

Value make_value_boolean(bool b)
{
    Value v;
    v.type = BOOLEAN;
    v.value.boolean = b;
    return v;
}

bool type_is(Value& v, Type t){return v.type == t;}

///// Utility functions /////

static inline bool is_digit(char c){return isdigit(c) != 0;}
static inline bool is_space(char c){return (isspace(c) != 0) || (c == ',');}

const char* g_delimiters = "(){}[];'";

enum DelimEnum{
    LEFT_PAREN    = 0,
    RIGHT_PAREN   = 1,
    LEFT_BRACE    = 2,
    RIGHT_BRACE   = 3,
    LEFT_BRACKET  = 4,
    RIGHT_BRACKET = 5,
    SEMICOLON     = 6,
    QUOTE         = 7
};

typedef std::regex  regex_t;
typedef std::cmatch cmatch_t;

regex_t g_regfloat("^([-+]?[0-9]+(\\.[0-9]*)?|\\.[0-9]+)([eE][-+]?[0-9]+)?$");
regex_t g_regint("^(?:([-+]?[1-9][0-9]*)|(0)|(0[xX][0-9A-Fa-f]+)|0[bB]([01]+))$");

typedef enum ParseResult_t{PARSE_NIL, PARSE_INT, PARSE_FLOAT} ParseResult;

ParseResult parsenum(const char* num, const char* numend, int& intvalue, double& doublevalue)
{
    cmatch_t res; 
    ParseResult result = PARSE_NIL;

    if(std::regex_search(num, numend, res, g_regint))
    {
        std::string is = res[0].str();

        if(is[0] == '0')
        {
            if(is.size() == 1) intvalue = 0;
            else if(is[1] == 'x' ||is[1] == 'X') intvalue = strtol(is.c_str(), 0, 16);
            else if(is[1] == 'b' || is[1] == 'B') intvalue = strtol(res[3].str().c_str(), 0, 2); 
        }
        else
        {
            intvalue = atoi(is.c_str());
        }
        result = PARSE_INT;
    }
    else if(std::regex_search(num, numend, res, g_regfloat))
    {
        std::string sf = res[0].str();
        doublevalue = atof(sf.c_str());
        result = PARSE_FLOAT;
    }
    return result;

}

/** Return pointer either to the next newline ('\n') or to the end of the given range.
*/
const char* to_newline(const char* begin, const char* end)
{
    const char* c = begin;
    while(c != end && *c != '\n'){c++;}
    return c;
}

/** Return pointer to end of string. Returns the pointer to the last
 *  quote in strings delimited with " -characters.
 *
 *  @param begin Pointer to first element after the initial " in the string.
 *  @param end   Pointer to the null termination character for the string.
 *  @return      Pointer to the delimiting " in a string or to the end of the given range.
 */
static const char* last_quote_of_string(const char* begin, const char* end)
{
    const char* c = begin;

    while(c != end)
    {
        if(*c == '\\')
        {
            c++;
            if(c == end) return c;
        }
        else if(*c == '"') 
        {
            return c;
        }

        c++;
    }

    return c;
}

/** Match the pattern to the beginning of the given range.
 * @param pattern        Pattern to match
 * @param pattern_length Length of pattern (equalent to to strlen).
 * @param begin          Beginning of the range to scan.
 * @param end            End of the range to scan.
 * @return               True if the pattern matched the beginning of the range, otherwise false.
*/
static bool match_string(const char* pattern, const size_t pattern_length, const char* begin, const char* end)
{
    bool result = false;
    size_t length = end - begin;
    if(length >= pattern_length)
    {
        size_t i = 0;
        const char* c = begin;
        result = true;
        while(i++ < pattern_length && result){result = *pattern++ == *c++;}
    }
    return result;
}

/** Look for the character in the given null terminated string. If value found return index of character in string. */
static int pos_in_string(const char c, const char* str)
{
    const char* ptr = str;
    while(c != *ptr && *(++ptr)){}
    if(*ptr) return ptr - str;
    else return - 1;
}

static bool is_in_string(const char c, const char* str)
{
    return pos_in_string(c, str) >= 0;
}

static bool is_delimiter(const char c)
{
    return is_in_string(c, g_delimiters);
}

static bool equal(const char* str_1, const char* str_2)
{
    return (strcmp(str_1, str_2) == 0);
}

static bool string_value_is(const Value& v, const char* str)
{
    return (v.type == STRING) ? (strcmp(v.value.string->c_str(), str) == 0) : false;
}

static bool symbol_value_is(const Value& v, const char* str)
{
    return (v.type == SYMBOL) ? (strcmp(v.value.string->c_str(), str) == 0) : false;
}

static bool match_range(const char* begin, const char* end, const char*str)
{
    while(*str && begin != end && *str == *begin){str++;begin++;}
    if(*str || begin != end) return false;
    else return true;
}

struct ScopeError
{
    /* If scope left open, return character of scope opening and number of line where scope was opened.
       If scope scoped pre-emptively, return line of error and and the faulty closing symbol.
    */
    enum Result{SCOPE_LEFT_OPEN, FAULTY_SCOPE_CLOSING, OK};
    Result result;
    int line;
    char scope;
    int char_on_line;

    ScopeError():result(OK){}
    ScopeError(Result res,char c, int cn , int l):result(res), line(l), scope(c), char_on_line(cn){}

    bool success(){return result == OK;}

    std::string report()
    {
        if(result == OK) return std::string("Scope ok");
        else if(result == SCOPE_LEFT_OPEN)
        {
            std::ostringstream os;
            os << "Scope "  << scope << "at character " << char_on_line << " at line " << line << " not closed.";
            return os.str();
        }
        else if(result == FAULTY_SCOPE_CLOSING)
        {
            std::ostringstream os;
            os << "Excess scope closing "  << scope << " at character " << char_on_line << " at line " << line  << ".";
            return os.str();
        }

        return std::string("");
    }
};

/** Verify that given scope delimiters balance out.
 *  The scope start and scope end arrays must be of equal length and contain start and end characters symmetrical
 *  positions such that scope_start[i] and scope_end[i] contain a character pair for a particular scope.
 *  @param scope_start string of scope begin characters.
 *  @param scope_end   string of scope end characters.
 *  @return ScopeError containing the result of the check.
*/

static ScopeError check_scope(const char* begin, const char* end, const char* comment, const char* scope_start, const char* scope_end)
{
    using namespace std;

    const char* c              = begin;
    const char* line_start_c   = c;
    size_t      comment_length = strlen(comment);
    int         line_number    = 0;

    std::stack<std::pair<int, std::pair<int, int>>> expected_closing; // Scope index; line number

    while(c < end)
    {
        if(*c == '\n') line_start_c = c;

        if(match_string(comment, comment_length, c, end))
        {
            c = to_newline(c, end);
            line_start_c = c;
            line_number++;
        }

        int scope_start_index = pos_in_string(*c, scope_start);
        int scope_end_index = pos_in_string(*c, scope_end);

        if(*c == '"')
        {
            c = last_quote_of_string(c+1, end);
        }
        else if(scope_start_index >= 0)
        {
            expected_closing.push(std::make_pair(scope_start_index, std::make_pair(c- line_start_c, line_number)));
        }
        else if(scope_end_index >= 0)
        {
            if(expected_closing.size() > 0 && expected_closing.top().first == scope_end_index)
            {
                expected_closing.pop();
            }
            else return ScopeError(ScopeError::FAULTY_SCOPE_CLOSING, *c, c - line_start_c, line_number);
        }

        c++;
    }

    if(expected_closing.size() > 0)
    {
        int scp; std::pair<int,int> col_line;
        std::tie(scp, col_line) = expected_closing.top();
        return ScopeError(ScopeError::SCOPE_LEFT_OPEN, scope_start[scp] , col_line.first , col_line.second);
    }

    return ScopeError();
}

/** Parse string to atom. Use one instance of AtomParser per string/atom pair. */
class ValueParser
{
public:

    Orb& orb_;

    ValueParser(Orb& orb):orb_(orb)
    {
        reading_string = false;
    }

    bool reading_string;

    // TODO: Write explanation of the parsing sequence
    //
    // a. Unless reading string symbols are broken on ', whitespace, (, ) and ;;
    // b. if reading string string is broken only when reading matching quote as on start
    //    \- will escape the following quote

    typedef const char* charptr;

    charptr c_;
    charptr end_;

    bool is(char ch){return *c_ == ch;}

    bool at_end(){return c_ >= end_;}

    void init(const char* start, const char* end){c_ = start; end_ = end;}

    const char* next(){return c_ + 1;}

    void move_forward(){c_ = c_ + 1;}

    void set(const char* chp){c_ = chp;}

    void to_newline()
    {
        while(c_ != end_ && *c_ != '\n'){c_++;}
    }

    const char* parse_string()
    {
        return last_quote_of_string(next(),end_);
    }

    bool parse_number(Number& out)
    {
        bool result = false;

        const char* c = c_;
        bool is_prefix = (*c == '+' || *c == '-');
        bool next_is_num = (c_ != end_ && (c_ + 1) != end_) && is_digit(c[1]);

        const char* begin = c_;
        const char* end = 0;

        if(is_digit(*c) || (is_prefix && next_is_num))
        {
            int intvalue;
            double floatvalue;

            end = c_ + 1;
            while(!is_space(*end) && (! is_delimiter(*end)) && *end) end++;

            ParseResult r = parsenum(begin, end, intvalue, floatvalue);
            result = r != PARSE_NIL;

            if(r == PARSE_INT) out.set(intvalue);
            else if(r == PARSE_FLOAT) out.set(floatvalue);
        }

        if(result) c_ = end - 1;

        return result;
    }

    bool parse_symbol(const char** begin, const char** end)
    {
        bool result = false;
        *begin = c_;
        const char* last = c_ + 1;
        while(!is_space(*last) && (! is_delimiter(*last)) && *last) last++;
        *end = last;
        result = true;
        return result;
    }

    std::string format_string(const char* begin, const char* end)
    {
        std::string result;
        size_t len = end - begin + 1;
        result.resize(len);
        auto i = std::begin(result);
        bool prevskip = false;

        while(begin != end){
            char c = *begin++;

            if(prevskip){
                     if(c == 'n') *i++ = '\n';
                else if(c == 'r') *i++ = '\r';
                else if(c == 't') *i++ = '\t';
                else if(c == 'b') *i++ = '\b';
                else if(c == 'f') *i++ = '\f';
                else *i++ = c;
                prevskip = false;
            } else {
                if(c == '\\') prevskip = true;
                else *i++ = c;
            }
        }
        result.erase(i, result.end());
        //*i = '\0';
        return result;
    }

    Value get_value()
    {
        Number tmp_number;
        const char* tmp_string_begin;
        const char* tmp_string_end;

        if(is('"')) //> string
        {
            const charptr first = c_ + 1;
            const charptr last = parse_string();
            set(last + 1);
            std::string formatted_string = format_string(first, last);
            return make_value_string(formatted_string);
        }
        else if(is('(')) // Enter list
        {
            Value result = make_value_list(orb_);
            move_forward();
            recursive_parse(result);
            return result;
        }
        else if(is('[')) // Enter vector
        {
            Value result = make_value_list(orb_);
            move_forward();
            recursive_parse(result);
            *result.value.list = result.value.list->add(make_value_symbol("make-vector"));
            return result;
        }
        else if(is('{')) // Enter map
        {
            Value result = make_value_list(orb_);
            move_forward();
            recursive_parse(result);
            *result.value.list = result.value.list->add(make_value_symbol("make-map"));
            return result;
        }
        else if(parse_number(tmp_number))
        {
            move_forward();
            return make_value_number(tmp_number);
        }
        else if(parse_symbol(&tmp_string_begin, &tmp_string_end))
        {
            set(tmp_string_end);
            if(match_range(tmp_string_begin, tmp_string_end, "nil")) return Value(); 
            else if(match_range(tmp_string_begin, tmp_string_end, "true")) return make_value_boolean(true);
            else if(match_range(tmp_string_begin, tmp_string_end, "false")) return make_value_boolean(false);
            else return make_value_symbol(tmp_string_begin, tmp_string_end);
        }
        else
        {
            throw EvaluationException("get_value: Orb parse error.");
            return Value();
        }
    }

    void rewrite_defn(std::list<Value>& build_list, List* list_ptr)
    {
            if(build_list.size() < 4) throw EvaluationException("recursive_parse: defn must contain at least 3 params.");

            // Decompose build list to parts
            auto build_iter = build_list.begin(); //build_list := (defn iname ilambda_params)
            auto iname = build_iter; ++iname;
            auto ilambda_params = iname; ++ilambda_params;
            auto iend = build_list.end();

            std::list<Value> rewritten_list;
            rewritten_list.push_back(make_value_symbol("def"));
            rewritten_list.push_back(*iname);

            // Compose lambda_expressions
            std::list<Value> lambda_list;
            lambda_list.push_back(make_value_symbol("fn"));
            lambda_list.insert(lambda_list.end(), ilambda_params, iend);

            // Alloc storage for value containing lambda expression
            rewritten_list.push_back(make_value_list(orb_));
            List* lambda_list_ptr = value_list(rewritten_list.back());
            *lambda_list_ptr = new_list(orb_, lambda_list);

            *list_ptr = new_list(orb_, rewritten_list);
    }

    void rewrite_member_call(std::list<Value>& build_list, List* list_ptr)
    {
        // (. fun obj params) :=  (((fnext obj) fun) (first obj) params)
        //                                 map       sym
        //                                    function
        if(build_list.size() < 3) throw EvaluationException("recursive_parse: member call must contain at least 3 params.");

        // Decompose build list to parts
        auto build_iter = build_list.begin(); //build_list := (. funname obj params)
        auto funname = build_iter; ++funname;
        auto obj = funname; ++obj;
        auto params = obj; ++params;
        auto iend = build_list.end();

        // TODO: Add 'verify object' call somewhere in order not to make object
        // evaluation errors so inscrutable.

        // Populate outer list.
        std::list<Value> rewritten_list;

        rewritten_list.push_back(make_value_list(orb_));
        List* outermaplist = value_list(*rewritten_list.rbegin());
        rewritten_list.push_back(make_value_list(orb_));
        List* symcalllist = value_list(*rewritten_list.rbegin());
        rewritten_list.insert(rewritten_list.end(), params, iend);

        // Populate first inner list
        *outermaplist = outermaplist->add(*funname);
        Value innermaplist = make_value_list(orb_);
        List* innermap = value_list(innermaplist);
        *innermap = innermap->add(*obj);
        *innermap = innermap->add(make_value_symbol("fnext"));
        *outermaplist = outermaplist->add(innermaplist);

        // Populate second inner list
        *symcalllist =  symcalllist->add(*obj);
        *symcalllist =  symcalllist->add(make_value_symbol("first"));

        *list_ptr = new_list(orb_, rewritten_list);
    }

    void recursive_parse(Value& root)
    {
        if(orb::is_not(root.type, LIST))
            throw EvaluationException("recursive_parse: root type is not LIST.");

        std::list<Value> build_list;

        bool next_is_quoted = false;;

        while(! at_end())
        {
            if(is(';'))  //> Comment, go to newline
            {
                to_newline();
                if(c_ != end_) move_forward();
            }
            else if(is(')') || is(']') || is('}')) // Exit container - scope was checked before this
            {
                move_forward();
                break;
            }
            else if(is_space(*c_))
            {
                move_forward();
                // Skip whitespace
                // After this we know that the input is either number or symbol
            }
            else if(is('\''))
            {
                next_is_quoted = true;
                move_forward();
            }
            else
            {
                // append to root  
                // push_to_value(root, get_value());
                Value v = get_value();
                if(next_is_quoted)
                {
                    Value quote_sym = make_value_symbol("quote");
                    Value outer = make_value_list(orb_);
                    *(outer.value.list) = outer.value.list->add(v);
                    *(outer.value.list) = outer.value.list->add(quote_sym);
                    append_to_value_stl_list(build_list, outer);
                    next_is_quoted = false;
                }
                else
                {
                    append_to_value_stl_list(build_list, v);
                }
            }

            if(at_end()) break;
        }

        if(next_is_quoted) // Quoted flag was not used.
        {
            throw EvaluationException("Quote cannot be empty.");
        }


        List* list_ptr = value_list(root);

        // Term rewritings that we prefer to do in parsing rather than evaluation stage ("poor mans macro system").
        bool list_occupied = !build_list.empty();
        if(list_occupied && build_list.front().is_str("defn"))
        {
            rewrite_defn(build_list, list_ptr);
        }
        else if(list_occupied && build_list.front().is_str("."))
        {
            rewrite_member_call(build_list, list_ptr);
        }
        else
        {
            *list_ptr = new_list(orb_, build_list);
        }
    }

    orb_result parse(const char* str)
    {
        size_t size = strlen(str);
        init(str, str + size);

        ScopeError scope_result = check_scope(c_, end_, ";", "({[", ")}]");

        if(!scope_result.success())
        {
            return orb_fail(scope_result.report());
        }

        Value* root = make_value_list_alloc(orb_);

        try{
            recursive_parse(*root);
        }
        catch(EvaluationException& e){
            return orb_fail(e.get_message());
        }

        List* root_list = value_list(*root);
        *root_list = root_list->add(make_value_symbol("begin"));

        return orb_result(ValuePtr(root, ValueDeleter()));
    }

};

////// Orb ///////

Orb::Orb()
{
    env_ = new Env();
}

Orb::~Orb()
{
    delete env_;
}

Orb::Env* Orb::env(){return env_;}

Map& Orb::env_map(){
    return env_->get_env();
}

void Orb::gc(){env_->gc();}

size_t Orb::reserved_size_bytes(){return env_->reserved_size_bytes();}

size_t Orb::live_size_bytes(){return env_->live_size_bytes();}

void Orb::set_output(std::ostream* os)
{
    if(env_) env_->out_ = os;
}

void Orb::set_args(int argc, char* argv[])
{
    Value argmap = make_value_map(*this);
    Map* map     = value_map(argmap);

    std::list<std::string> fixed_args;

    for(int i = 0; i < argc; ++i){
        std::string arg(argv[i]);
        size_t pos = std::string::npos;
        pos = arg.find('\\');
        while(pos != std::string::npos){
            if((arg.size() > (pos + 1)) && arg[pos + 1] == '\\'){
                pos = pos + 1;
            } else {
                arg = arg.replace(pos, 1, "/");
                pos = pos + 1;
            }
            pos = arg.find('\\', pos);
        } 
        fixed_args.push_back(arg);
    }

    int order = 0;
    for(auto &f : fixed_args){
        *map = map->add(make_value_number(order++), make_value_string(f));
    }

    env_->def(make_value_symbol("sys/args"), argmap);

    //std::cout << "Args set" << std::endl;

}

/** Get handle to output stream.*/
std::ostream& Orb::get_output()
{
    if(env_) return *(env_->out_);
    else return std::cout;
}

///// Evaluation utilities /////

orb_result orb_fail(const char* str){
    return orb_result(std::string(str));
}

orb_result orb_fail(const std::string& str){
    return orb_result(str);
}

orb_result string_to_value(Orb& m, const char* str)
{
    ValueParser parser(m);

    return parser.parse(str);
}

typedef std::string (*PrefixHelper)(const Value& v);

static void value_to_string_helper(std::ostream& os, const Value& v, PrefixHelper prfx)
{
    auto out = [&]()->std::ostream& {
        if(prfx) os << prfx(v) << " ";
        return os;
    };

    switch(v.type)
    {
        case NIL:{out() << "nil"; break;} // TODO: Do we need to print nill?
        case BOOLEAN:
        {
            if(v.value.boolean) out() << "true";
            else out() << "false";
            break;
        }
        case NUMBER:
        {
            if(v.value.number.type == Number::INT)
            {
                out() << v.value.number.to_int();
            }
            else
            {
                out() << v.value.number.to_float();
            }
            break;
        }
        case SYMBOL:
        {
            out() << *(v.value.string);
            break;
        }
        case STRING:
        {
            std::string esc("\"");
            out() << esc << *(v.value.string) << esc;
            break;
        }
        case LIST:
        {
            out() << "(";
            List* lst_ptr = v.value.list;
            for(auto i = lst_ptr->begin(); i != lst_ptr->end(); ++i)
            {
                value_to_string_helper(os, *i, prfx);
                os << " ";
            }
            os << ")";
            break;
        }
        case MAP:
        {
            Map* map_ptr = v.value.map;
            out() << "{";
            auto mend = map_ptr->end();
            for(auto m = map_ptr->begin(); m != mend; ++m)
            {
                value_to_string_helper(os, m->first, prfx);
                os << " ";
                value_to_string_helper(os, m->second, prfx);
                os << " ";
            }
            os << "}";
            break;
        }
        case VECTOR:
        {
            Vector* vec_ptr = v.value.vector;
            out() << "[";
            for(auto i = vec_ptr->begin(); i != vec_ptr->end(); ++i)
            {
                value_to_string_helper(os, *i, prfx);
                os << " ";
            }
            os << "]";
            break;
        }
        case FUNCTION:
        {
            //const std::type_info& funtype(v.value.function->fun.target_type());
            //const char* fun_type_name = funtype.name();

            out() << "<function>" ; // TODO: add function name to Function member.
            break;
        }
        case OBJECT:
        {
            //const std::type_info& funtype(v.value.function->fun.target_type());
            //const char* fun_type_name = funtype.name();

            out() << "<object>" ; // TODO: add function name to Function member.
            break;
        }
        // TODO: Number array
        default:
        {
            local_assert("Implement output for type");
        }
    }
}

std::string value_to_string(const Value& v)
{
    std::ostringstream stream;
    value_to_string_helper(stream, v, 0);
    return stream.str();
}

std::ostream& operator<<(std::ostream& os, const Value& v){
    os << value_to_string(v);
    return os;
}

std::string value_type_to_string(const Value& v);

std::string value_to_typed_string(const Value* v)
{
    std::ostringstream stream;
    value_to_string_helper(stream, *v, value_type_to_string);
    return stream.str();
}


std::string value_type_to_string(const Value& v)
{
    switch(v.type)
    {
        case NUMBER:
        {
            if(v.value.number.type == Number::INT) return std::string("NUMBER:INT");
            else                                   return std::string("NUMBER:FLOAT");
        }
        case STRING:        return std::string("STRING");
        case BOOLEAN:       return std::string("BOOLEAN");
        case NIL:           return std::string("");
        case SYMBOL:        return std::string("SYMBOL");
        case VECTOR:        return std::string("VECTOR");
        case LIST:          return std::string("LIST");
        case MAP:           return std::string("MAP");
        case OBJECT:        return std::string("OBJECT");
        case NUMBER_ARRAY:  return std::string("NUMBER ARRAY");
        case FUNCTION:           return std::string("FUNCTION");
    }
    return "";
}


// Evaluation utils.
// More or less straightforward translation of the simple Evaluator 
// in 'Structure and Interpretation of Computer Programs' (Steele 1996)
// Section 4.1.1 'The Core of the Evaluator'

namespace {

Value eval(const Value& v, Map& env, Orb& orb);
Value apply(const Value& v, VRefIterator args_begin, VRefIterator args_end, Map& env, Orb& orb);

bool is_self_evaluating(const Value& v)
{
    return v.type == NUMBER || v.type == STRING || v.type == MAP || v.type == NIL || v.type == BOOLEAN ||
        v.type == NUMBER_ARRAY || v.type == VECTOR || v.type == FUNCTION;
}

bool is_tagged_list(const Value& v, const char* symname)
{
    bool result = false;
    const Value* first = value_list_first(v);
    if(first) result = symbol_value_is(*first, symname);
    return result;
}

bool is_quoted(const Value& v){ return is_tagged_list(v,"quote");}
bool is_assignment(const Value& v){ return is_tagged_list(v,"def");}
bool is_function_assignment(const Value& v){ return is_tagged_list(v,"defn");}
bool is_reassignment(const Value& v){ return is_tagged_list(v,"set");}
bool is_if(const Value& v){return is_tagged_list(v, "if");} 
bool is_lambda(const Value& v){return is_tagged_list(v, "fn");} 
bool is_begin(const Value& v){return is_tagged_list(v, "begin");} 
bool is_cond(const Value& v){return is_tagged_list(v, "cond");} 
bool is_else(const Value& v){return is_tagged_list(v, "else");}
bool is_application(const Value& v){return v.is(LIST);}

Value begin_actions(const Value& v)
{
    List* vlist = value_list(v);
    if(vlist)
    {
        return make_value_list(vlist->rest());  
    }
    else
    {
        return Value();
    }
}

bool is_true(const Value& v)
{
    bool v_is_false = v.type == NIL || (v.type == BOOLEAN && (!v.value.boolean));
    return !v_is_false;
}

const Value* assignment_var(const Value& v){return value_list_second(v);}
const Value* assignment_value(const Value& v){return value_list_third(v);}

Value eval_sequence(const List& expressions, Map& env, Orb& orb)
{
    if(expressions.empty())
        throw EvaluationException(std::string("eval_sequence: Trying to evaluate empty sequence"));

    if(!expressions.has_rest()) 
    {
        return eval(*expressions.first(), env, orb);
    }
    else
    {
        eval(*expressions.first(), env, orb);
        return eval_sequence(expressions.rest(), env, orb);
    }
}

Value sequence_exp(const List& action)
{
    if(action.empty())
        return make_value_list(action);
    else if(!action.has_rest())
        return *action.first();
    else // make begin
        return make_value_list(action.add(make_value_symbol("begin")));
}

// cond: expand-clauses
Value expand_clauses(const List& clauses, Orb& orb)
{
    if(clauses.empty()) return Value();
    else
    {
        const Value* first = clauses.first();
        List rest = clauses.rest();

        if(!first) throw EvaluationException(std::string("expand_clauses: Error interpreting cond clause"));

        if(is_else(*first))
        {
            if(rest.empty())
            {
                // Sequence-exp
                List ca = value_list(*first)->rest(); // cond-actions
                return sequence_exp(ca);
            }
            else
            {
                throw EvaluationException(std::string("expand_clauses: ELSE clause isn't last - COND->iF"));
            }
        }
        else
        {
            if(first->type != LIST) throw EvaluationException(std::string("expand_clauses: element is not a list as expected but:") + value_to_string(*first));

            if(rest.empty())
            {
                throw EvaluationException(std::string("expand_clauses: Final case in cond is not an else case. Cond expression requires for else case to be final expression: (cond (... ...) (else ...))."));
            }

            // make-if

            Value v_iflist = make_value_list(orb);
            List* iflist = value_list(v_iflist);

            // first : (pred ca)

            const Value* pred = value_list_first(*first); 
            const List     ca = value_list(*first)->rest(); // cond-actions : (pred ca) -> ca
            Value         seq = sequence_exp(ca); // (foo bar) -> (begin foo bar) OR (foo) -> foo

            Value     clauses = expand_clauses(rest, orb); // (if foo bar (if ...))

            *iflist = iflist->add(clauses);
            *iflist = iflist->add(seq);
            *iflist = iflist->add(*pred);
            *iflist = iflist->add(make_value_symbol("if"));
            // iflist := (if pred seq clauses)

            return v_iflist;
        }
    }
}

// cond: cond->if
Value convert_cond_to_if(const Value& v, Orb& orb)
{
    return expand_clauses(value_list(v)->rest(), orb); 
}

Value eval(const Value& v, Map& env, Orb& orb)
{
    if(is_self_evaluating(v)) return v;
    else if(v.type == SYMBOL)
    {
        orb::ConstOption<Value> result = env.try_get_value(v);
        if(!result.is_valid()){
                throw EvaluationException(std::string("eval: Symbol not found. Input:") + *v.value.string);
        }
        return *result;
    }
    else if(is_quoted(v))
    {
        const Value* ref_result = value_list_second(v);
        if(!ref_result) throw EvaluationException(std::string("eval: Quote was not followed by an element. Input:") + value_to_string(v));
        return *ref_result;
    }
    else if(is_assignment(v))
    {
        const Value *asgn_var = assignment_var(v);
        const Value *asgn_val = assignment_value(v);
        if(asgn_var && asgn_val)
        {
            if(asgn_var->type != SYMBOL)
                throw EvaluationException(std::string("eval: Value to assign to was not symbol. Input:") + value_to_string(v));
            if(is_self_evaluating(*asgn_val))
                env = env.add(*asgn_var, *asgn_val);
            else
                env = env.add(*asgn_var, eval(*asgn_val, env, orb));
        }
        else
        {
            throw EvaluationException(std::string("eval:Did not find anything to assign to. Input:") + value_to_string(v));
        }
        return Value();
    }
    else if(is_reassignment(v))
    {
        const Value *asgn_var = assignment_var(v);
        const Value *asgn_val = assignment_value(v);
        bool result = false;
        if(asgn_var && asgn_val)
        {
            if(asgn_var->type != SYMBOL)
                throw EvaluationException(std::string("eval: Value to assign to was not symbol. Input:") + value_to_string(v));

            if(is_self_evaluating(*asgn_val))
                result = env.try_replace_value(*asgn_var, *asgn_val);
            else
                result = env.try_replace_value(*asgn_var, eval(*asgn_val, env, orb));
        }
        else
        {
            throw EvaluationException(std::string("eval:Did not find anything to set to. Input:") + value_to_string(v));
        }

        if(!result) throw EvaluationException(std::string("eval:Set value failed. Probably missing key. Input:") + value_to_string(v));

        return Value();
    }
    else if(is_if(v))
    {
        const Value* if_predicate = value_list_second(v);
        if(if_predicate)
        {
            if(is_true(eval(*if_predicate, env, orb)))
            {
                const Value* if_then = value_list_third(v);
                if(if_then)
                {
                    if(is_self_evaluating(*if_then)) return *if_then;
                    else return eval(*if_then, env, orb);
                }
                else throw EvaluationException(std::string("eval: Did not find 'fst' in expected form (if pred fst snd). Input:") + value_to_string(v));
            }
            else
            {
                const Value* if_else = value_list_nth(v, 3);
                if(if_else)
                {
                    if(is_self_evaluating(*if_else))
                        return *if_else;
                    else
                        return eval(*if_else, env, orb);
                }
                else
                {
                    return Value();
                }
            }
        }
        else  throw EvaluationException(std::string("Did not find 'pred' in expected form (if pred fst snd). Input:") + value_to_string(v));
    }
    else if(is_lambda(v))
    {
        List* l = value_list(v);
        const Value* lambda_parameters = value_list_second(v);

        if(lambda_parameters && l)
        {
            std::list<Value> lambda_list = orb::list(make_value_symbol("procedure"),
                    *lambda_parameters,
                    make_value_list(l->rrest()), // lambda body
                    make_value_map(env));
            return make_value_list(new_list(orb, lambda_list));
        }
        else
        {
            throw EvaluationException(std::string("Could not find one or more of 'params' 'body' in (lambda params body) expression. Input:")  + value_to_string(v));
        }
    }
    else if(is_begin(v))
    {
        return eval_sequence(value_list(v)->rest(), env, orb);
    }
    else if(is_cond(v))
    {
        return eval(convert_cond_to_if(v, orb), env, orb);
    }
    else if(is_application(v) && (!value_list(v)->empty())) // Is application
    {
        // Get operator
        const Value* first = value_list_first(v);
        Value op;

        if(!is_self_evaluating(*first)) 
            op = eval(*first, env, orb);
        else
            op = *first;

        List operands = value_list(v)->rest(); // TODO: eval each operand prior to application. Store result in temp. container or list.

        return apply(op, operands.begin(), operands.end(), env, orb);
    }

    throw EvaluationException(std::string("Could not find evaluable value. Input:") + value_to_string(v));

    return Value();
}

bool is_primitive_procedure(const Value& v){return v.type == FUNCTION;}
bool is_compound_procedure(const Value& v){return is_tagged_list(v, "procedure");}

PrimitiveFunction value_function(const Value& v)
{
    return v.value.function->fun;
}

Vector eval_list_to_vector(VRefIterator args_begin, VRefIterator args_end, Map& env, Orb& orb)
{
    Vector v;
    while(args_begin != args_end)
    {
        v.push_back(eval(*args_begin, env, orb));
        ++args_begin;
    }
    return v;
}

/** Attempts to assign addresses to elements accessible through iterator range. 
 *  @return false if range is shorter than the number of reference addresses.*/
template<class IT, class PPTR>
bool range_decompose(IT begin, IT end, PPTR a, PPTR b)
{
    bool result = true;

    if(begin != end){ *a = &(*begin); ++begin;} else result = false;
    if(begin != end){ *b = &(*begin); ++begin;} else result = false;

    return result;
}

/** Attempts to read as many entries from the list into input parameters.*/
void list_decompose(const List& l, const Value** first, const Value** second, const Value** third, const Value** fourth)
{
    auto i = l.begin();
    auto e = l.end();

    if(first)  *first = 0;
    if(second) *second = 0;
    if(third)  *third = 0;
    if(fourth) *fourth = 0;

    if(i != e && first) *first = i.data_ptr();

    if(i != e) ++i; else return;
    if(i != e && second) *second = i.data_ptr();

    if(i != e) ++i; else return;
    if(i != e && third) *third = i.data_ptr();

    if(i != e) ++i; else return;
    if(i != e && fourth) *fourth = i.data_ptr();

}

Value eval_compound_procedure(const Value& v, Vector& params, Orb& orb)
{
        // Eval sequence. #1 : Extract params, body and env from procedure list.
        const Value* proc_params = 0;
        const Value* proc_body   = 0;
        const Value* proc_env    = 0;

        List* l = value_list(v);

        list_decompose(*l, 0, &proc_params, &proc_body, &proc_env);

        if(!proc_params) throw EvaluationException(std::string("apply: Malformed compound procedure. Could not find procedure parameters."));
        if(!proc_body) throw EvaluationException(std::string("apply: Malformed compound procedure. Could not find procedure body."));
        if(!proc_env) throw EvaluationException(std::string("apply: Malformed compound procedure. Could not find procedure environment."));

        List* params_list = value_list(*proc_params);
        List* body_list   = value_list(*proc_body);
        Map* proc_env_map = value_map(*proc_env);

        if(proc_params->type != LIST)
            throw EvaluationException(std::string("apply: Malformed compound procedure. Proc_params was not a list but:") + value_to_string(*proc_params));
        
        if(proc_body->type != LIST)
            throw EvaluationException(std::string("apply: Malformed compound procedure. Proc_body was not a list but:") + value_to_string(*proc_body));

        if(proc_env->type != MAP)
            throw EvaluationException(std::string("apply: Malformed compound procedure. Proc_env was not a map but:") + value_to_string(*proc_env));

        if(!params_list) throw EvaluationException(std::string("apply: params_list is null."));
        if(!body_list) throw EvaluationException(std::string("apply: body_list is null."));
        if(!proc_env_map) throw EvaluationException(std::string("apply: env_map is null."));

        Map seq_env = proc_env_map->add(params_list->begin(), params_list->end(), params.begin(), params.end());

        return eval_sequence(*value_list(*proc_body), seq_env, orb);
}

Value apply(const Value& v, VRefIterator args_begin, VRefIterator args_end, Map& env, Orb& orb)
{
    Vector params = eval_list_to_vector(args_begin, args_end, env, orb);

    if(is_primitive_procedure(v))
    {
        return value_function(v)(orb, params, env);
    }
    else if(is_compound_procedure(v))
    {
        return eval_compound_procedure(v, params, orb);

#if 0
        // Eval sequence. #1 : Extract params, body and env from procedure list.
        const Value* proc_params = 0;
        const Value* proc_body   = 0;
        const Value* proc_env    = 0;

        List* l = value_list(v);

        list_decompose(*l, 0, &proc_params, &proc_body, &proc_env);

        if(!proc_params) throw EvaluationException(std::string("apply: Malformed compound procedure. Could not find procedure parameters."));
        if(!proc_body) throw EvaluationException(std::string("apply: Malformed compound procedure. Could not find procedure body."));
        if(!proc_env) throw EvaluationException(std::string("apply: Malformed compound procedure. Could not find procedure environment."));

        List* params_list = value_list(*proc_params);
        List* body_list   = value_list(*proc_body);
        Map* proc_env_map = value_map(*proc_env);

        if(proc_params->type != LIST)
            throw EvaluationException(std::string("apply: Malformed compound procedure. Proc_params was not a list but:") + value_to_string(*proc_params));
        
        if(proc_body->type != LIST)
            throw EvaluationException(std::string("apply: Malformed compound procedure. Proc_body was not a list but:") + value_to_string(*proc_body));

        if(proc_env->type != MAP)
            throw EvaluationException(std::string("apply: Malformed compound procedure. Proc_env was not a map but:") + value_to_string(*proc_env));

        if(!params_list) throw EvaluationException(std::string("apply: params_list is null."));
        if(!body_list) throw EvaluationException(std::string("apply: body_list is null."));
        if(!proc_env_map) throw EvaluationException(std::string("apply: env_map is null."));

        Map seq_env = proc_env_map->add(params_list->begin(), params_list->end(), params.begin(), params.end());

        return eval_sequence(*value_list(*proc_body), seq_env, orb);
#endif
    }
    else if(v.type == MAP)
    {
        Map* m = value_map(v);

        if(params.begin() != params.end())
        {
            orb::ConstOption<Value> result = m->try_get_value(*params.begin());
            if(!result.is_valid()) return Value();
            return *result;
        }
        else
        {
            throw EvaluationException(std::string("apply: Attempting to apply map without key to search for."));
        }
    }
    else if(v.type == VECTOR)
    {
        Vector* vec = value_vector(v);
        size_t param_size = params.size();
        if(param_size != 1)
        {
            throw EvaluationException(std::string("apply: Vector: Invalid number of arguments:") + orb::to_string(param_size));
        }

        if(params.begin()->type != NUMBER || params.begin()->value.number.type != Number::INT) 
            throw EvaluationException(std::string("apply: Vector: Index parameter must be integer. Was:") + value_to_string(*params.begin()));

        int index = params.begin()->value.number.to_int();
        int vec_size = vec->size();
        if(index < 0 || index >= vec_size)
            throw EvaluationException(std::string("apply: Vector: Index parameter out of range:") + orb::to_string(index));

        return (*vec)[index];
    }
    else
    {
        throw EvaluationException(std::string("apply: Attempting to apply non-procedure. Input:") + value_to_string(v));
        return Value();
    }
}

} // empty namespace

orb_result eval(Orb& m, const Value* v)
{
    ValuePtr result(new Value(), ValueDeleter());

    try
    {
        *result = eval(*v, m.env()->get_env(), m);
    }catch(const EvaluationException& e)
    {
        return orb_fail(e.get_message());
    }catch(const std::exception& e)
    {
        return orb_fail(e.what());
    }
    catch(...)
    {
        return orb_fail("Unknown error.");
    }

    return orb_result(result);
}

orb_result read_eval(Orb& m, const char* str){
    orb_result parse_result = string_to_value(m, str);
    if(parse_result.valid()){
        return eval(m, parse_result.as_value()->get());
    }
    else{
        return parse_result;
    }
}

const Value* get_value(Orb& m, const char* pathstr)
{
    const Value* result = 0;

    if(pathstr)
    {
        const Value* tmpresult = 0;
        std::string path(pathstr);
        Map* map = &m.env()->get_env();

        if(path.find('/') != std::string::npos)
        {
            auto lines = orb::string_split(pathstr, "/");

            for(auto &l : lines){
                Value key =  make_value_string(l.string);
                orb::ConstOption<Value> value = map->try_get_value(key);

                if(value.is_valid())
                {
                    tmpresult = value.get();
                    if(&l != &*lines.rbegin())
                    {
                        map = value_map(*tmpresult);
                        if(!map){
                            tmpresult = 0;
                            break;
                        }
                    }
                }
            }
        }
        else{
            //Value mapv;
            //mapv.type = MAP;
            //mapv.value.map = map;
            //auto mapstr = value_to_string(mapv);
            //mapv.value.map = 0;
            //mapv.type = NIL;

            //std::cout << mapstr << std::endl;

            // Check if root env contains the value
            Value key =  make_value_symbol(path.c_str());
            orb::ConstOption<Value> value = map->try_get_value(key);
            if(value.is_valid()) tmpresult = value.get();
        }

        result = tmpresult;
    }

    return result;
}

Type value_type(const Value* v)
{
    return v ? v->type : NIL;
}

std::string  get_value_string(const Value* v)
{
    std::string result;
    if(v->is(STRING) || v->is(SYMBOL)) result = *v->value.string;
    return result;
}

Number get_value_number(const Value* v)
{
    Number result = Number::make(0);
    if(v->is(NUMBER)) result = v->value.number;
    return result;
}


//////////// Native operators ////////////


namespace {

#define OPDEF(name_param, i_start_param, i_end_param) Value name_param(Orb& m, Vector& args, Map& env){\
            VecIterator i_start_param = args.begin(); VecIterator i_end_param = args.end();

    // Arithmetic operators

    OPDEF(op_add, arg_start, arg_end)

        Number n = Number::make(0);
        if(!all_are_of_type(arg_start, arg_end, NUMBER, 0)) throw EvaluationException("op_add: value's type is not NUMBER");
        while(arg_start != arg_end)
        {
            n += value_number(*arg_start);
            ++arg_start;
        }

        return make_value_number(n);
    }

    OPDEF(op_sub, arg_start, arg_end)

        Number n = Number::make(0);
        size_t size = 0;
        if(!all_are_of_type(arg_start, arg_end, NUMBER, &size)) throw EvaluationException("op_sub: value's type is not NUMBER");

        if(size == 1)
        {
            n -= value_number(*arg_start);
        }
        else if (size > 1)
        {
            n.set( value_number(*arg_start));
            ++arg_start;
            while(arg_start != arg_end)
            {
                n -= value_number(*arg_start);
                ++arg_start;
            }
        }

        return make_value_number(n);
    }

    OPDEF(op_mul, arg_start, arg_end)

        Number n = Number::make(1);
        if(!all_are_of_type(arg_start, arg_end, NUMBER, 0)) throw EvaluationException("op_mul: value's type is not NUMBER");
        while(arg_start != arg_end)
        {
            n *= value_number(*arg_start);
            ++arg_start;
        }

        return make_value_number(n);
    }

    OPDEF(op_div, arg_start, arg_end)

        Number n = Number::make(1);
        size_t size = 0;
        if(!all_are_of_type(arg_start, arg_end, NUMBER, &size)) throw EvaluationException("op_sub: value's type is not NUMBER");

        if(size == 1)
        {
            n /= value_number(*arg_start);
        }
        else if (size > 1)
        {
            n.set( value_number(*arg_start));
            ++arg_start;
            while(arg_start != arg_end)
            {
                n /= value_number(*arg_start);
                ++arg_start;
            }
        }

        return make_value_number(n);
    }

    OPDEF(op_make_range, arg_start, arg_end)

        ArgWrap wrapper(arg_start, arg_end); 

        size_t count = args.size();
        
        Number start;
        Number end;
        Number increment;
         
        if(args.size() == 1){
            wrapper.wrap(&end);
            if(end.type == Number::FLOAT){start.set(0.0); increment.set(1.0);}
            else                         {start.set(0); increment.set(1);}
        }
        else if(args.size() == 2){
            wrapper.wrap(&start, &end);
            if(orb::any_of(Number::FLOAT, start.type, end.type)){
                start.set(start.to_float());
                end.set(end.to_float());
                increment.set(1.0);
            }
            else {
                increment.set(1);
            }
        }
        else if(args.size() == 3){
            wrapper.wrap(&start, &increment, &end);
            if(orb::any_of(Number::FLOAT, start.type, end.type, increment.type)){
                start.set(start.to_float());
                end.set(end.to_float());
                increment.set(increment.to_float());
            }
        }
        else throw EvaluationException("op_make_range: need 1 - 3 numeric arguments.");

        Range<Number> range(start, increment, end);
        
        std::vector<Value> rangeinstance;

        for(auto r : range){
            rangeinstance.push_back(make_value_number(r));
        }

        Value result = make_value_list(m);
        List* res_list = value_list(result);
        *res_list = new_list(m, rangeinstance);
        
        return result;
    }

    // Booleans

#define ITER_TO_PTR(iter_param, end_param) ((iter_param != end_param) ? (&(*iter_param)) : 0)

    OPDEF(op_equal, arg_start, arg_end)

        bool Result = true;

        const Value* first;
        const Value* second;

        second = ITER_TO_PTR(arg_start, arg_end);

        while(Result && arg_start != arg_end)
        {
            first = second;
            ++arg_start;
            second = ITER_TO_PTR(arg_start, arg_end);
            if(first && second)
            {
                Result &= ValuesAreEqual::compare(*first, *second);
            }
        }

        return make_value_boolean(Result);
    }

    Value op_not_equal(Orb& m, Vector& args, Map& env){
        VecIterator arg_start = args.begin();
        VecIterator arg_end = args.end();
        bool Result = true;

        const Value* first;
        const Value* second;

        second = ITER_TO_PTR(arg_start, arg_end);

        while(Result && arg_start != arg_end)
        {
            first = second;
            ++arg_start;
            second = ITER_TO_PTR(arg_start, arg_end);
            if(first && second)
            {
                Result &= (!ValuesAreEqual::compare(*first, *second));
            }
        }

        return make_value_boolean(Result);
    }

    class NumGt{public: static bool op(const Number& first, const Number& second){return first > second;} };
    class NumLess{public: static bool op(const Number& first, const Number& second){return first < second;} };
    class NumLeq{public: static bool op(const Number& first, const Number& second){return first <= second;} };
    class NumGeq{public: static bool op(const Number& first, const Number& second){return first >= second;} };

    template<class OP> bool num_op_loop(VecIterator arg_start, VecIterator arg_end)
    {
       bool Result = true;

        const Value* first;
        const Value* second;

        second = ITER_TO_PTR(arg_start, arg_end);

        while(Result && arg_start != arg_end)
        {
            first = second;
            ++arg_start;
            second = ITER_TO_PTR(arg_start, arg_end);

            if(first && second && (first->type == NUMBER && second->type == NUMBER ))
            {
                Result &= OP::op(first->value.number, second->value.number);
            }
            else if((first && first->type != NUMBER) || second){Result = false;}
        }

        return Result;
    }

    Value op_less_or_eq(Orb& m, Vector& args, Map& env){
        VecIterator arg_start = args.begin();
        VecIterator arg_end = args.end();
        bool Result = num_op_loop<NumLeq>(arg_start, arg_end);
        return make_value_boolean(Result);
    }

    Value op_less(Orb& m, Vector& args, Map& env){
        VecIterator arg_start = args.begin();
        VecIterator arg_end = args.end();
        bool Result = num_op_loop<NumLess>(arg_start, arg_end);
        return make_value_boolean(Result);
    }

    Value op_gt(Orb& m, Vector& args, Map& env){
        VecIterator arg_start = args.begin();
        VecIterator arg_end = args.end();
        bool Result = num_op_loop<NumGt>(arg_start, arg_end);
        return make_value_boolean(Result);
    }

    Value op_gt_or_eq(Orb& m, Vector& args, Map& env){
        VecIterator arg_start = args.begin();
        VecIterator arg_end = args.end();
        bool Result = num_op_loop<NumGeq>(arg_start, arg_end);
        return make_value_boolean(Result);
    }

    ////////// List functions

    Value next_of_value(const Value* v)
    {
        if(v->type == LIST)
        {
            return make_value_list(value_list(*v)->rest());
        }
        else if(v->type == VECTOR)
        {
            Vector* vec = value_vector(*v);

            if(vec->size() > 0)
            {
                auto i = vec->begin();
                ++i;
                return make_value_vector(i , vec->end());
            }
            else
            {
                return make_value_vector();
            }
        }
        return Value();
    }

    const Value* first_of_value(const Value* v)
    {
        const Value* first = 0;
        if(v->type == LIST)
        {
            first = value_list(*v)->first();
        }
        else if(v->type == VECTOR)
        {
            Vector* vec = v->value.vector;
            if(vec->size() > 0) first = &(*vec)[0];
        }
        return first;
    }

    Value op_first(Orb& m, Vector& args, Map& env){
        VecIterator arg_start = args.begin();
        VecIterator arg_end = args.end();
        const Value* first = 0;
        if(arg_start != arg_end)
        {
            const Value* v = &(*arg_start);
            first = first_of_value(v);
        }
        return first ? *first : Value();
    }

    Value op_next(Orb& m, Vector& args, Map& env){
        VecIterator arg_start = args.begin();
        VecIterator arg_end = args.end();
        if(arg_start != arg_end)
        {
            Value* v =  &(*arg_start);
            return next_of_value(v);
        }
        return Value();
    }

    Value op_fnext(Orb& m, Vector& args, Map& env){
        VecIterator arg_start = args.begin();
        VecIterator arg_end = args.end();
        if(arg_start != arg_end)
        {
            if(arg_start->type == LIST)
            {
                List* l = value_list(*arg_start);
                auto b = l->begin();
                auto e = l->end();
                if(b != e) ++b;
                if(b != e) return *b;
            }
            else if(arg_start->type == VECTOR)
            {
                Vector* vec = value_vector(*arg_start);
                auto i = vec->begin();
                auto e = vec->end();
                if(i != e) ++i;
                if(i != e) return *i;
            }
        }
        return Value();
    }

    Value op_nnext(Orb& m, Vector& args, Map& env){
        VecIterator arg_start = args.begin();
        VecIterator arg_end = args.end();
        if(arg_start != arg_end)
        {
            if(arg_start->type == LIST)
            {
                return make_value_list(value_list(*arg_start)->rrest());
            }
            else if(arg_start->type == VECTOR)
            {
                Vector* vec = value_vector(*arg_start);

                if(vec->size() > 1)
                {
                    auto i = vec->begin();
                    ++i;++i;
                    return make_value_vector(i , vec->end());
                }
                else
                {
                    return make_value_vector();
                }
            }
        }
        return Value();
    }

    Value op_nfirst(Orb& m, Vector& args, Map& env){
        VecIterator arg_start = args.begin();
        VecIterator arg_end = args.end();
        const Value* first = 0;
        if(arg_start != arg_end)
        {
            if(arg_start->type == LIST)
            {
                first = value_list(*arg_start)->first();
            }
            else if(arg_start->type == VECTOR)
            {
                Vector* vec = arg_start->value.vector;
                if(vec->size() > 0) first = &(*vec)[0];
            }

            if(first)
            {
                return next_of_value(first);
            }
        }
        return Value();
    }

    Value op_ffirst(Orb& m, Vector& args, Map& env){
        VecIterator arg_start = args.begin();
        VecIterator arg_end = args.end();
        const Value* ffirst = 0;
        if(arg_start != arg_end)
        {
            const Value* v = &(*arg_start);
            const Value* first = first_of_value(v);
            if(first) ffirst = first_of_value(first);
        }

        return ffirst ? *ffirst : Value();
    }

    // Type query operations

#define OP_1_DEFN(opval_param, i_param)    Value opval_param(Orb& m, Vector& args, Map& env) \
    {VecIterator i_param = args.begin(); VecIterator arg_end = args.end(); if(i_param != arg_end){

    OP_1_DEFN(op_value_is_integer, vi)
        if(vi->type == NUMBER && vi->value.number.type == Number::INT) return make_value_boolean(true);
    } return make_value_boolean(false);}
    
    OP_1_DEFN(op_value_is_float, vi)
        if(vi->type == NUMBER && vi->value.number.type == Number::FLOAT) return make_value_boolean(true);
    } return make_value_boolean(false);}

    OP_1_DEFN(op_value_is_string, vi)
        if(vi->type == STRING) return make_value_boolean(true);
    } return make_value_boolean(false);}

    OP_1_DEFN(op_value_is_boolean, vi)
        if(vi->type == BOOLEAN) return make_value_boolean(true);
    } return make_value_boolean(false);}

    OP_1_DEFN(op_value_is_symbol, vi)
        if(vi->type == SYMBOL) return make_value_boolean(true);
    } return make_value_boolean(false);}

    OP_1_DEFN(op_value_is_map, vi)
        if(vi->type == MAP) return make_value_boolean(true);
    } return make_value_boolean(false);}

    OP_1_DEFN(op_value_is_vector, vi)
        if(vi->type == VECTOR) return make_value_boolean(true);
    } return make_value_boolean(false);}
    
    OP_1_DEFN(op_value_is_list, vi)
        if(vi->type == LIST) return make_value_boolean(true);
    } return make_value_boolean(false);}

    OP_1_DEFN(op_value_is_fn, vi)
        if(is_primitive_procedure(*vi) || is_compound_procedure(*vi)) return make_value_boolean(true);
    } return make_value_boolean(false);}

    OP_1_DEFN(op_value_is_object, vi)
        if(vi->type == OBJECT) return make_value_boolean(true);
    } return make_value_boolean(false);}

    // Container constructors

    OPDEF(op_make_map, arg_start, arg_end)

        Map map = new_map(m);
        MapPool* pool = &map_pool(m);
        return make_value_map(pool->add(map, arg_start, arg_end));
    }

    OPDEF(op_make_vector, arg_start, arg_end)

        return make_value_vector(arg_start, arg_end);
    }

    // Printers

    std::string value_iters_to_string(VecIterator i_start, VecIterator i_end, const char* spacer)
    {
        std::ostringstream os;
        for(;i_start != i_end;)
        {
            if (i_start->type == SYMBOL || i_start->type == STRING) os << *i_start->value.string;
            else os <<  value_to_string(*i_start);
            ++i_start;
            if(i_start != i_end) os << spacer;
        }
        return os.str();
    }

    OPDEF(op_str, arg_start, arg_end)

        return make_value_string(value_iters_to_string(arg_start, arg_end, ""));
    }

    OPDEF(op_println, arg_start, arg_end)

        m.get_output() << value_iters_to_string(arg_start, arg_end, " ");
        m.get_output() << "\n";
        return Value();
    }

    OPDEF(op_printf, arg_start, arg_end)

        m.get_output() << value_iters_to_string(arg_start, arg_end, " ");
        return Value();
    }

    // Container operations

    OPDEF(op_count, arg_i, arg_end)

        int count = 0;
        if(arg_i != arg_end)
        {
            if(arg_i->type == VECTOR)     {count = arg_i->value.vector->size();}
            else if(arg_i->type == LIST)  {count = arg_i->value.list->size();}
            else if(arg_i->type == MAP)   {count = arg_i->value.map->size();}
            else if(arg_i->type == STRING){count = arg_i->value.string->size();}
    } return make_value_number(Number::make(count));}

    OPDEF(op_cons, arg_i, arg_end) 

        Value* fst;
        Value* snd;

        if(range_decompose(arg_i, arg_end, &fst, &snd))
        {
            if(snd->type == LIST)
            {
                List* l = value_list(*snd);
                return make_value_list(l->add(*fst));
            }
            else if(snd->type == VECTOR)
            {
                Vector* v = value_vector(*snd);
                
                return make_value_vector(*fst, *v);
            }
            else throw EvaluationException("op_cons: value to append to must be LIST or VECTOR (was:" +  value_to_string(*snd) + ")."); 
        }

        throw EvaluationException("op_cons: bad syntax. Cons must be applied to two parameters: (cons param1 param2)."); 
        return Value();
    }

    OPDEF(op_conj, arg_i, arg_end) 

        Value* fst;
        Value* snd;

        if(range_decompose(arg_i, arg_end, &fst, &snd))
        {
            if(fst->type == LIST)
            {
                List* l = value_list(*fst);
                ++arg_i;
                return make_value_list(l->add_end(arg_i, arg_end));
            }
            else if(fst->type == VECTOR)
            {
                Vector* v = value_vector(*fst);
                ++arg_i; 
                return make_value_vector(*v, arg_i, arg_end);
            }
            else throw EvaluationException("op_conj: value to append to must be LIST or VECTOR (was:" +  value_to_string(*fst) + ")."); 
        }

        throw EvaluationException("op_conj: bad syntax. Cons must be applied to at least two parameters: (conj collection elem ... )."); 
        return Value();
    }

    // Add keyvalue pair to map
    OPDEF(op_insert_data, arg_i, arg_end)
        // Signature (add map key value key value key value ...)

        if(args.size() < 3 && args.size() % 2 == 0) throw EvaluationException("op_insert_data: wrong number of input arguments. Signature is (add map key value key value ...).");

        Map* map = value_map(args[0]);

        if(!map){
            std::string first_str = value_to_typed_string(&args[0]);
            throw EvaluationException("op_insert_data: first argument must be a map. You entered:" + first_str);
        }

        Value result = make_value_map(*map);

        ++arg_i;

        Map* resmap = value_map(result);

        while(arg_i != arg_end)
        {
            auto key = arg_i;
            ++arg_i;
            auto value = arg_i;
            ++arg_i;
            *resmap = resmap->add(*key, *value);
        }

        return result;
    }

    // Remove keyvalue pair from map
    OPDEF(op_remove_data, arg_i, arg_end) 
                // Signature (add map key value key value key value ...)
        if(args.size() < 2) throw EvaluationException("op_remove_data: wrong number of input arguments. Signature is (remove map key key...");
        Map* map = value_map(args[0]);
        if(!map){
            std::string first_str = value_to_typed_string(&args[0]);
            throw EvaluationException("op_remove_data: first argument must be a map. You entered:" + first_str);
        }

        Value result = make_value_map(*map);

        ++arg_i;

        Map* resmap = value_map(result);

        while(arg_i != arg_end)
        {
            auto key = arg_i;
            ++arg_i;
            *resmap = resmap->remove(*key);
        }

        return result;
    }

    OPDEF(op_map_keys, arg_i, arg_end)
        if(args.size() != 1) throw EvaluationException("op_map_keys: wrong number of input arguments. Signature is (keys map)");
        Map* map = value_map(*arg_i);
        if(!map){
            std::string value_type = value_type_to_string(*arg_i);
            throw EvaluationException("op_map_keys: argument must be a map. Type was:" + value_type  + ".");
        }

        Value result = make_value_list(m);
        List* lst = value_list(result);

        for(auto &kv : *map){
            *lst = lst->add(kv.first);
        }

        return result;
    }

    OPDEF(op_map_vals, arg_i, arg_end)
        if(args.size() != 1) throw EvaluationException("op_map_vals: wrong number of input arguments. Signature is (vals map)");
        Map* map = value_map(*arg_i);
        if(!map){
            std::string value_type = value_type_to_string(*arg_i);
            throw EvaluationException("op_map_vals: argument must be a map. Type was:" + value_type  + ".");
        }

        Value result = make_value_list(m);
        List* lst = value_list(result);

        for(auto &kv : *map){
            *lst = lst->add(kv.second);
        }

        return result;
    }

    struct IterContext{
        Vector& args;
        size_t count;
        size_t symcount;
        Value& collection;
        Value& fun;
         
        IterContext(Vector& arguments)
            :args(arguments), count(args.size()),symcount(count - 2), collection(args[count - 2]), fun(args[count - 1]){
        }

        Value apply(Vector& params, Map& env, Orb& orb)
        {
            if(is_primitive_procedure(fun))
            {
                return value_function(fun)(orb, params, env);
            }
            else if(is_compound_procedure(fun))
            {
                return eval_compound_procedure(fun, params, orb);
            }
            else throw EvaluationException("IterContext::apply: malformed call, attempting call non-callable value."); 
        }
    };

    template<class T>
    Value extract_apply(T begin, T end, IterContext& ic, Map& env, Orb& orb)
    {
        Vector args;
        bool done = begin == end;

        if(ic.symcount == 0) ic.symcount = 1;

        while(!done)
        {
            size_t load = 0;
            args.clear();
            while(begin != end && load < ic.symcount)
            {
                args.push_back(*begin); ++begin;
                ++load;
            }
            while(load < ic.symcount) args.push_back(Value());
            ic.apply(args, env, orb);

            if(begin == end) done = true;
        }

        return Value();
    }

    // TODO: raise exception or unify: iter for map takes only 2 parameters. Make explicit.
    Value extract_apply_map(Map::iterator begin, Map::iterator end, IterContext& ic, Map& env, Orb& orb)
    {
        if(ic.symcount != 0) throw EvaluationException("op_iter: map does not accept decomposition symbols. call as (map mapref fun)."); 

        Vector args;

        while(begin != end)
        {
            args.clear();
            args.push_back(begin->first);
            args.push_back(begin->second);
            ic.apply(args, env, orb);
            ++begin;
        }

        return Value();
    }

    Value do_iter_list(Orb& m, Vector& args, Map& env){
        IterContext ic(args);
        List* list = value_list(ic.collection);
        return extract_apply(list->begin(), list->end(), ic, env, m);
    }
    
    Value do_iter_vector(Orb& m, Vector& args, Map& env){
        IterContext ic(args);
        Vector* vector = value_vector(ic.collection);
        return extract_apply(vector->begin(), vector->end(), ic, env, m);
    }
    
    Value do_iter_map(Orb& m, Vector& args, Map& env){
        IterContext ic(args);
        Map* map = value_map(ic.collection);
       return extract_apply_map(map->begin(), map->end(), ic, env, m);
    }

    // iter: (iter <syms> collection f)
    OPDEF(op_iter, arg_start, arg_end)
        size_t count = args.size();

        if(count < 2)
            throw EvaluationException("op_iter: iter needs at least 2 parameters:(iter <syms> collection function) or (iter collection function) without decomposition."); 

        size_t symcount = count - 2; 

        bool all_are_syms = true;
        for(size_t i = 0; i < symcount; ++i)
            all_are_syms = all_are_syms && (args[i].type == SYMBOL);

        if(!all_are_syms)
            throw EvaluationException("op_iter: parameters prior to collection must be symbols."); 
      
        Value& collection(args[count - 2]);
        Value& fun(args[count - 1]);
        
        if(orb::none_of(collection.type, VECTOR, LIST, MAP))
            throw EvaluationException("op_iter: second to last parameter must be a collection (list, vector or map)."); 
       
        if(!(is_primitive_procedure(fun) || is_compound_procedure(fun)))
            throw EvaluationException("op_iter: last parameter must be a function."); 

        if(collection.type == VECTOR)    return do_iter_vector(m, args ,env);
        else if(collection.type == LIST) return do_iter_list(m, args ,env);
        else if(collection.type == MAP)  return do_iter_map(m, args ,env);

        return Value();
    }

    template<class COL, class T>
    Value extract_apply_collect(T begin, T end, IterContext& ic, Map& env, Orb& m)
    {
        Vector args;
        bool done = begin == end;

        Vector result_vec;

        if(ic.symcount == 0) ic.symcount = 1;

        while(!done)
        {
            size_t load = 0;

            args.clear();

            while(begin != end && load < ic.symcount)
            {
                args.push_back(*begin); ++begin;
                ++load;
            }

            while(load < ic.symcount) args.push_back(Value());

            result_vec.push_back(ic.apply(args, env, m));

            if(begin == end) done = true;
        }

        if(std::is_same<List, COL>::value){
            Value result = make_value_list(m);
            List* reslist = value_list(result);
            *reslist = reslist->add_end(result_vec.begin(), result_vec.end());
            return result;
        }
        else{
            return make_value_vector(result_vec.begin(), result_vec.end());
        }

        return Value();
    }

    // Raise exception or unify: iter for map takes only  2 parameters. Make explicit.
    Value extract_apply_map_collect(Map::iterator begin, Map::iterator end, IterContext& ic, Map& env, Orb& m)
    {
        if(ic.symcount != 0) throw EvaluationException("op_iter: iter for map does not accept decomposition symbols. call as (iter mapref fun) "); 

        Vector args;

        bool done = begin == end;

        Value result = make_value_map(m);
        Map* resmap = value_map(result);

        while(!done)
        {
            args.clear();

            args.push_back(begin->first);
            args.push_back(begin->second);
            ++begin;

            Value applied = ic.apply(args, env, m);

            if(applied.type == LIST)
            {
                List* lst = value_list(applied);
                const Value* key = lst->first();
                const Value* value = lst->second();
                if(key && value) *resmap = resmap->add(*key, *value);
                else throw EvaluationException("map :: map Result list did not contain two elements."); 
            }
            else if(applied.type == VECTOR)
            {
                Vector& vec(*value_vector(applied));
                size_t size = vec.size();
                if(size != 2) throw EvaluationException("map :: map Result vector did not contain 2 elements. "); 
                *resmap = resmap->add(vec[0], vec[1]);
            }
            else if(applied.type == MAP)
            {
                Map* map = value_map(applied);
                for(auto& kv : *map){
                    *resmap = resmap->add(kv.first, kv.second);
                }
            }
            else{
                std::string valstr = value_to_typed_string(&applied);
                throw EvaluationException("map :: map: Function does not return a mappable sequence (list, vector or map) but a " + valstr + "." ); 
            }

            if(begin == end) done = true;
        }

        return result;
    }

    Value do_map_list(Orb& m, Vector& args, Map& env){
        IterContext ic(args);
        List* list = value_list(ic.collection);
        return extract_apply_collect<List, List::iterator>(list->begin(), list->end(), ic, env, m);
    }
    
    Value do_map_vector(Orb& m, Vector& args, Map& env){
        IterContext ic(args);
        Vector* vector = value_vector(ic.collection);
        return extract_apply_collect<Vector, Vector::iterator>(vector->begin(), vector->end(), ic, env, m);
    }
    
    Value do_map_map(Orb& m, Vector& args, Map& env){
        IterContext ic(args);
        Map* map = value_map(ic.collection);
       return extract_apply_map_collect(map->begin(), map->end(), ic, env, m);
    }

    // TODO
    // map: (map <syms> collection f)
    OPDEF(op_map, arg_start, arg_end)
        size_t count = args.size();

        if(count < 2)
            throw EvaluationException("op_iter: Not enough parameters"); 

        size_t symcount = count - 2; 

        bool all_are_syms = true;

        for(size_t i = 0; i < symcount; ++i)
            all_are_syms = all_are_syms && (args[i].type == SYMBOL);

        if(!all_are_syms)
            throw EvaluationException("op_iter: parameters prior to collection must be symbols."); 
 
        Value& collection(args[count - 2]);
        Value& fun(args[count - 1]);
 
        if(orb::none_of(collection.type, VECTOR, LIST, MAP))
            throw EvaluationException("op_iter: second to last parameter must be a collection (list, vector or map)."); 
       
        if(!(is_primitive_procedure(fun) || is_compound_procedure(fun)))
            throw EvaluationException("op_iter: last parameter must be a function."); 

        if(collection.type == VECTOR)    return do_map_vector(m, args ,env);
        else if(collection.type == LIST) return do_map_list(m, args ,env);
        else if(collection.type == MAP)  return do_map_map(m, args ,env);

        return Value();
    }
    // System ops

    OPDEF(op_import_file, arg_i, arg_end)

        Value* fst = (arg_i != arg_end) ? &*arg_i : 0;

        if(fst && fst->type == STRING)
        {
            const char* path = value_string(*fst);

            std::string contents;
            bool        success;
            std::tie(contents, success) = file_to_string(path);

            if(success){
                orb_result res = read_eval(m, contents.c_str());

                if(res.valid()){ 
                    return *(res.as_value()->get());
                } else {
                    throw EvaluationException(res.message());
                }
            }
            else{
                std::string msg = std::string("op_import_file: Could not read in file:") +
                                  std::string(path);
                throw EvaluationException(msg);
            }
        }
        else throw EvaluationException("op_import_file: first value must be string"); 

        return Value();
    }

    // TODO:while  dot cross str
    // map filter range apply count zip

#undef OPDEF
#undef OP_1_DEFN
}

//////////// Load environment ////////////

void Orb::Env::add_fun(const char* name, PrimitiveFunction f)
{
    *env_ = env_->add(make_value_symbol(name), make_value_function(f));
}

void Orb::Env::def(const Value& key, const Value& value)
{
    *env_ = env_->add(key, value);
}

void Orb::Env::load_default_env()
{
    add_fun("+", op_add);
    add_fun("-", op_sub);
    add_fun("*", op_mul);
    add_fun("/", op_div);

    add_fun("range", op_make_range);

    add_fun("=", op_equal);
    add_fun("!=", op_not_equal);
    add_fun("<", op_less);
    add_fun(">", op_gt);
    add_fun("<=", op_less_or_eq);
    add_fun(">=", op_gt_or_eq);

    add_fun("first", op_first);
    add_fun("ffirst", op_ffirst);
    add_fun("next",  op_next);
    add_fun("fnext", op_fnext);
    add_fun("nnext", op_nnext);
    add_fun("nfirst",op_nfirst);

    add_fun("integer?", op_value_is_integer);
    add_fun("float?", op_value_is_float);
    add_fun("string?", op_value_is_string);
    add_fun("boolean?", op_value_is_boolean);
    add_fun("symbol?", op_value_is_symbol);
    add_fun("map?", op_value_is_map);
    add_fun("vector?", op_value_is_vector);
    add_fun("list?", op_value_is_list);
    add_fun("fn?", op_value_is_fn);
    add_fun("object?", op_value_is_object);

    add_fun("make-map", op_make_map);
    add_fun("make-vector", op_make_vector);

    add_fun("count", op_count); 
    add_fun("cons", op_cons);
    add_fun("conj", op_conj);
    add_fun("iter", op_iter);
    add_fun("map", op_map);

    add_fun("insert", op_insert_data);
    add_fun("remove", op_remove_data);
    add_fun("keys", op_map_keys);
    add_fun("vals", op_map_vals);

    // TODO fold

    add_fun("println", op_println);
    add_fun("printf", op_printf);
    add_fun("str", op_str);

    add_fun("read", wrap_function(file_to_string));
    add_fun("write", wrap_function(string_to_file));
    add_fun("import", op_import_file);
}

void add_fun(Orb& m, const char* name, PrimitiveFunction f) {m.env()->add_fun(name, f);}

} // Namespace orb ends
