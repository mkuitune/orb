/** \file shims_and_types.cpp Implementation of the shims_and_types.h
   \author Mikko Kuitunen (mikko <dot> kuitunen <at> iki <dot> fi)
*/

#include "shims_and_types.h"

namespace orb {
/////// String utilities /////////

TextLine make_text_line(const char* buffer, int begin, int end, int line)
{
    TextLine t(std::string(buffer + begin, buffer + end), line);
    return t;
}

/** Split string to lines according to one or more delimiter characters in delim */
std::list<TextLine> string_split(const char* str, const char* delim)
{
    std::list<TextLine> lines_out;
    const char* buffer     = str;
    int         delim_size  = strlen(delim);
    int         buffer_size = strlen(str);

    auto        addLine = [&] (const char* buffer, int start, int end, int& line)
    {
        lines_out.push_back(make_text_line(buffer, start, end, line));
        line++;
    };

    int line_start = 0;
    int lines = 0;
    int i = 0;
    while(i < buffer_size)
    {
        bool is_delim = strchr(delim, *(buffer + i)) != 0;

        if(is_delim)
        {
            addLine(buffer, line_start, i , lines);
            i += 1;
            line_start = i;
        }
        else
        {
            i++;
        }
    }

    if(line_start < buffer_size)
    {
        addLine(buffer, line_start, i , lines);
    }
    return lines_out;
}

bool elements_are_ordered(const std::string& first, const std::string& second){
    return first.compare(second) <= 0;
}

//////////////////////// Hashing /////////////////////////////


uint32_t get_hash32(cstring& string)
{
    const char* data = string.c_str();
    int len = string.size();
    return hash32(data, len);
}

}