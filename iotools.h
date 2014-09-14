/** 
* \file iotools.h
* 
* All the paths should be given using / as the directory separator.
* 
* \author Mikko Kuitunen (mikko <dot> kuitunen <at> iki <dot> fi)
* MIT license.
*/
#pragma once
#include "orb_lib.h"

#include<list>
#include<string>
#include<fstream>
#include<tuple>
#include<vector>
#include <cstdint>

struct FilesystemReference{
    enum t{File, Directory, Unsupported};

    std::string name_;
    std::string fullpath_;
    t type_;

    FilesystemReference(std::string name, std::string fullpath, t type):
        name_(name), fullpath_(fullpath), type_(type)
    {}

};

std::vector<FilesystemReference> list_dir(const char* path);

bool directory_exists(std::string dir);

ORB_LIB std::tuple<std::string, bool>          file_to_string(const char* path);
std::tuple<std::vector<uint8_t>, bool> file_to_bytes(const char* path);

bool string_to_file(const char* path, const char* string);

// All internal path operations expect '/' separator for paths

/** Join two file system paths together */
std::string path_join(const std::string& head, const std::string& tail);

/** Split path on '/ ' - to segments.*/
std::vector<std::string> path_split(const std::string& path);

/** Return platform directory separator.*/
char platform_separator();

/** Normalize path expression to platform. */
std::string path_to_platform_string(const std::string& path);

class InputFile{
private:
    InputFile(const InputFile& i){}
public:
    InputFile(const char* path);
    ~InputFile();

    std::ifstream& file();
    bool is_open();
    void close();

    std::tuple<std::string, bool>       contents_to_string();
    std::tuple<std::vector<uint8_t>, bool> contents_to_bytes();

    std::ifstream file_;
};

class OutputFile{
private:
    OutputFile(const OutputFile& o){}
public:
    OutputFile(const char* path);
    OutputFile(const char* path, bool append);
    ~OutputFile();
    bool is_open();
    void close();
    bool write_str(std::string& str);
    bool write(const char* str);

    std::ofstream& file();

    std::ofstream file_;
};

