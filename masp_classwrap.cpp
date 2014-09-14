/** \file masp_classwrap.cpp. Wrapper for classes.
    \author Mikko Kuitunen (mikko <dot> kuitunen <at> iki <dot> fi)
*/
#pragma once

#include "masp_classwrap.h"

namespace orb{

Value object_data_to_list(FunMap&fmap, Value& obj, Masp& m)
{
    orb::Value listv = orb::make_value_list(m);
    orb::List* l = orb::value_list(listv);

    *l = l->add(fmap.map());
    *l = l->add(obj);

    return listv;
}

}//Namespace masp

