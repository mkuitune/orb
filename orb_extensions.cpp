/** \file orb_extensions.cpp
    \author Mikko Kuitunen (mikko <dot> kuitunen <at> iki <dot> fi)
    MIT licence.
*/

#include "orb_extensions.h"
#include "orb_classwrap.h"
#include "iotools.h"

namespace orb{

orb::Value make_InputFile(Orb& m, Vector& args, Map& env){
    typedef WrappedObject<InputFile> WrappedInput;

    VecIterator arg_start = args.begin();
    VecIterator arg_end = args.end();

    std::string path;
    ArgWrap(arg_start, arg_end).wrap(&path);

    orb::Value obj = make_value_object(new WrappedInput(path.c_str()));

    FunMap fmap(m);
    fmap.add("is_open",            wrap_member(&InputFile::is_open));
    fmap.add("close",              wrap_member(&InputFile::close));
    fmap.add("contents_to_string", wrap_member(&InputFile::contents_to_string));

    return object_data_to_list(fmap, obj, m);
}

void outputfile_functions(orb::FunMap& fmap)
{
    fmap.add("is_open", wrap_member(&OutputFile::is_open));
    fmap.add("close",   wrap_member(&OutputFile::close));
    fmap.add("write",   wrap_member(&OutputFile::write));
}

orb::Value make_OutputFile(orb::Orb& m, orb::Vector& args, orb::Map& env){
    typedef WrappedObject<OutputFile> WrappedOutput;

    orb::VecIterator arg_start = args.begin();
    orb::VecIterator arg_end = args.end();

    std::string path;
    orb::ArgWrap(arg_start, arg_end).wrap(&path);

    orb::Value obj = orb::make_value_object(new WrappedOutput(path.c_str()));

    orb::FunMap fmap(m);
    outputfile_functions(fmap);

    return object_data_to_list(fmap, obj, m);
}

orb::Value make_OutputFileApp(orb::Orb& m, orb::Vector& args, orb::Map& env){
    typedef WrappedObject<OutputFile> WrappedOutput;

    orb::VecIterator arg_start = args.begin();
    orb::VecIterator arg_end = args.end();

    std::string path;
    bool        app;
    orb::ArgWrap(arg_start, arg_end).wrap(&path, &app);

    orb::Value obj = orb::make_value_object(new WrappedOutput(path.c_str(), app));

    orb::FunMap fmap(m);
    outputfile_functions(fmap);

    return object_data_to_list(fmap, obj, m);
}

void load_orb_unsafe_extensions(orb::Orb& m)
{
    orb::add_fun(m , "InputFile", make_InputFile);
    orb::add_fun(m , "OutputFile", make_OutputFile);
    orb::add_fun(m , "OutputFileApp", make_OutputFileApp);
}

}
