/** \file glh_regex.hi
 *  Wrapper for used regex implementation.
 * */

#ifndef GLH_USE_BOOST_REGEX

#include<regex>

typedef std::regex  regex_t;
typedef std::cmatch cmatch_t;
#define glh_regex_search std::regex_search

#else

#include<boost/regex.hpp>

typedef boost::regex  regex_t;
typedef boost::cmatch cmatch_t;
#define glh_regex_search boost::regex_search

#endif

