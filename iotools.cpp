/** \file iotools.cpp
    \author Mikko Kuitunen (mikko <dot> kuitunen <at> iki <dot> fi)
*/
#include "iotools.h"
#include "shims_and_types.h"
#include <algorithm>


//#ifdef WIN32
#include "win32_dirent.h"
//#else
//#include <cdirent>
//#endif


namespace {
std::string fix_path_to_win32_path(const std::string& path)
{
    std::string out(path);
    for(auto& c : out) if(c == '/') c = '\\';
    return out;
}
std::string fix_path_to_posix_path(const std::string& path)
{
    std::string out(path);
    for(auto& c : out) if(c == '\\') c = '/';
    return out;
}
} // end anonymous namespace


static FilesystemReference::t typeof_entry(dirent *entry){

    if(entry->d_type == DT_REG)      return FilesystemReference::File;
    else if(entry->d_type == DT_DIR) return FilesystemReference::Directory;
    else                             return FilesystemReference::Unsupported;
}

std::vector<FilesystemReference> list_dir(const char* path)
{
    std::vector<FilesystemReference> contents;

    DIR *pdir     = 0;
    dirent *entry = 0;

    pdir = opendir(path); // opendir(".") for current directory

    if(pdir){
        while (entry = readdir (pdir)){

            if (!entry) continue;

            if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            const std::string name(entry->d_name);
            const std::string fullpath = path_join(std::string(path), name);
            const FilesystemReference::t type = typeof_entry(entry);

            if(type != FilesystemReference::Unsupported)
                contents.push_back(FilesystemReference(name, fullpath, type));
        }
    }

    return contents;
}

bool directory_exists(std::string dirpath){
    std::string platform_path = path_to_platform_string(dirpath);
    bool result = false;
    DIR* dir = opendir(platform_path.c_str());

    if(dir)
    {
        /* Directory exists. */
        closedir(dir);
        result = true;
    }
    return result;
}

std::tuple<std::string, bool> file_to_string(const char* path)
{
    InputFile file(path);
    return file.contents_to_string();
}

std::tuple<std::vector<uint8_t>, bool> file_to_bytes(const char* path)
{
    InputFile file(path);
    return file.contents_to_bytes();
}

bool string_to_file(const char* path, const char* string)
{
    bool result = false;
    OutputFile o(path);
    return o.write(string);
}

std::string path_join(const std::string& head, const std::string& tail)
{
    char separator = platform_separator();

    std::string normalized_head = path_to_platform_string(head);
    std::string normalized_tail = path_to_platform_string(tail);

    if(head.size() > 0){
        if(*head.rbegin() != separator) return normalized_head + separator + normalized_tail;
        else                          return normalized_head + normalized_tail;
    }
    else return normalized_tail;
}

std::vector<std::string> path_split(const std::string& path)
{
    std::vector<std::string> segments;
    char separator = platform_separator();
    bool start_new_string = true;

    for(auto i = path.begin(); i != path.end(); ++i)
    {
        if(*i != separator){
            if(start_new_string){
                segments.push_back(std::string());
                start_new_string = false;
            }

            segments.back().push_back(*i);
        } else {
            start_new_string = true;
        }
    }
    return segments;
}

char platform_separator(){
#ifdef WIN32
    return '\\';
#else
    return return '/';
#endif
}

std::string path_to_platform_string(const std::string& path)
{
//TODO Change conversion and output format to e.g. type SystemPathString or such that it works in all regions.
#ifdef WIN32
    return fix_path_to_win32_path(path);
#else
    return fix_path_to_posix_path(path);
#endif
}

//////////// InputFile //////////////////

InputFile::InputFile(const char* path):file_(path, std::ios::in|std::ios::binary){}

InputFile::~InputFile(){if(file_) file_.close();}

std::ifstream& InputFile::file(){return file_;}

bool InputFile::is_open(){if(file_) return true; else return false;}

void InputFile::close(){file_.close();}

std::tuple<std::string, bool> InputFile::contents_to_string()
{
    std::string contents;
    bool success = false;
    if(file_)
    {
        file_.seekg(0, std::ios::end);
        const size_t filesize = (size_t) file_.tellg();
        contents.resize(filesize);
        file_.seekg(0, std::ios::beg);
        file_.read(&contents[0], contents.size());
        success = true;
    }

    return std::make_tuple(contents, success);
}

std::tuple<std::vector<uint8_t>, bool> InputFile::contents_to_bytes()
{
    std::vector<uint8_t> contents;

    bool success = false;
    if(file_)
    {
        file_.seekg(0, std::ios::end);
        const size_t filesize = (size_t) file_.tellg();
        contents.resize(filesize);
        file_.seekg(0, std::ios::beg);
        file_.read(reinterpret_cast<char*>(&contents[0]), contents.size());
        success = true;
    }

    return std::make_tuple(contents, success);
}

//// OutputFile ////

OutputFile::OutputFile(const char* path)
{
    file_.open(path, std::ios::out | std::ios::binary);
}

OutputFile::OutputFile(const char* path, bool append)
{
    auto openflags = append ? std::ios::out | std::ios::app | std::ios::binary : std::ios::out | std::ios::binary;
    file_.open(path, openflags);
}

OutputFile::~OutputFile(){if(file_) file_.close();}

bool OutputFile::is_open(){if(file_) return true; else return false;}

void OutputFile::close(){file_.close();}

bool OutputFile::write_str(std::string& str)
{
    return write(str.c_str());
}

bool OutputFile::write(const char* str)
{
    bool result = false;
    if(is_open()){
        file_ << str;
        result = true;
    }
    return result;
}

std::ofstream& OutputFile::file(){return file_;}


#if 0
//
// Main to test directory tools.
//
template<class ST>
std::vector<std::string> split_stream_to_tokens(ST& stream_in)
{
    using namespace std;
    vector<string> tokens;
    copy(istream_iterator<string>(stream_in), istream_iterator<string>(), back_inserter(tokens));

    return tokens;
}

std::vector<std::string> split_to_tokens(const std::string& in)
{
    std::istringstream iss(in);
    return split_stream_to_tokens(iss);
}

void cmd_ls(const char* path)
{
    std::cout << "Directory contents:" << std::endl;

    auto dircontent = list_dir(path);
    for(auto d: dircontent){
        if(d.type_ == FilesystemReference::File){
            std::cout << d.name_ << std::endl;
        }
        else if (d.type_ == FilesystemReference::Directory){
            std::cout << d.name_ << "/" <<std::endl;
        }
    }

}

#define CASE(string_param, match_param)if(string_param == std::string(match_param))

void do_cmd(const std::string& cmd){
    CASE(cmd, "ls") cmd_ls(".");
}

void do_cmd(const std::string& cmd, const std::string& param){
    CASE(cmd, "ls") cmd_ls(param.c_str());
}

int main(int argc, char* argv[])
{
    std::string cmd;

    while(cmd != std::string("q"))
    {
        std::getline(std::cin, cmd);
        auto tokens = split_to_tokens(cmd);
       
        std::cout << "Tokens:";
        if(tokens.size() > 0){
            
            std::cout << tokens[0];
            auto begin = std::next(tokens.begin());
            std::for_each(begin, tokens.end(), 
                [](const std::string& str){std::cout << ", " << str;});
        }
        std::cout << std::endl;

        if(tokens.size() == 1) do_cmd(tokens[0]);
        if(tokens.size() == 2) do_cmd(tokens[0], tokens[1]);

    }
}


#endif
