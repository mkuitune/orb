/** \file pcontainers_tests.cpp
\author Mikko Kuitunen (mikko <dot> kuitunen <at> iki <dot> fi)
MIT licence.
*/

#include "persistent_containers.h"
#include<string>
#include "unittester.h"
#include<list>
#include <utility>

//ADD_GROUP(collections_pmap);


/////////// Collections ////////////

UTEST(collections, PList_test)
{
    using namespace orb;

    PListPool<int> pool;
    auto list_empty = pool.new_list();
    auto list_a = pool.new_list(list(1, 2, 3, 4));
    auto list_b = pool.new_list(list(5, 6, 7, 8));

    print_container(list_a);
    print_container(list_b);

    std::list<int> long_range = range_to_list(1, 1, 100);
    // test gc

    pool.gc();

    auto list_c = pool.new_list(list(9, 10, 11, 12));

    auto list_long = pool.new_list(long_range);

    print_container(list_a);
    print_container(list_b);
    print_container(list_c);
    print_container(list_long);
}

template<class M> void print_pmap(M& map)
{
    ut_test_out() << "Contents of persistent map:" << std::endl;
    for(auto i = map.begin(); i != map.end(); ++i)
    {
        ut_test_out() << i->first  << ":" << i->second << std::endl;
    }
}


typedef orb::PMapPool<std::string, int> SIMapPool;
typedef orb::PMapPool<std::string, int>::Map SIMap;
typedef std::map<std::string, int> StlSIMap;

/* Map tests:
Use cases: Insert, Access, Remove, Gc, Print (IARGP).


*/

void test_overwrite(SIMap map)
{
    auto map1 = map.add("One", 1);
    print_pmap(map);
    auto map1_b = map.add("One", 11);
    print_pmap(map);
    auto map1_c = map1_b.add("One", 111);
    print_pmap(map);
}

void write_random_elements(const int n, Random<int> rand, orb::cstring& prefix, StlSIMap& map)
{
    int i = 0;
    while(i++ < n)
    {
        int r = rand.rand();
        std::string str = prefix + std::to_string(r);
        map[str] = r;
    }
}


// Map test: write, and gc test TODO

// Map test: write, remove and gc test TODO

// Map test: write, create and destroy recursively n maps, check root values, gc, check root values TODO 


// Map tests: create and gc tests:
//
// WE: fill map by writing element  by element
// WM: fill map by instantiating it from an stl-map.
// Instantiation elements: e1 e2
//                                    Test matrix:
//                                    1   2  3  4      5,6,7,8
// create_gc_test 1: write map        WE WE WM WM  e1   -||-  e1
//                2: write map2       WE WM WM WE  e2   -||-  e1
//                3: verify map     
//                3: delete map2, gc
//                4: verify map

SIMap map_insert_elements(const StlSIMap& map, SIMapPool& pool, bool gc_at_each)
{
    SIMap out_map = pool.new_map();
    if(gc_at_each)
    {
        for(auto i = map.begin(); i != map.end(); ++i)
        {
            out_map = out_map.add(i->first, i->second);
            out_map.gc();
        }
    }
    else
    {
        for(auto i = map.begin(); i != map.end(); ++i)
        {
            out_map = out_map.add(i->first, i->second);
        }
    }
    return out_map;
}

typedef SIMap (*create_map_inserter)(const StlSIMap&, SIMapPool&, bool gc_at_each);

SIMap map_direct_instantiation(const StlSIMap& map, SIMapPool& pool,bool gc_at_each)
{
    SIMap out_map = pool.new_map(map);
    if(gc_at_each) pool.gc();
    return out_map;
}

SIMap map_remove_elements(const StlSIMap& map, SIMap pmap)
{
    for(auto i = map.begin(); i != map.end(); ++i)
    {
        pmap = pmap.remove(i->first);
    }
    return pmap;
}

/** Return true if all elements in reference are found in map.*/
bool verify_map_elements(const StlSIMap& reference, SIMap& map)
{
    bool found = true;

    // Verify elements both ways - from map to reference and from reference to map.

    // From reference to map - check are elements exist.
    for(auto i = reference.begin(); i != reference.end(); ++i)
    {
        auto v = map.try_get_value(i->first);
        if(! v.is_valid())
        {
            ORB_TEST_LOG("Persistent map did not contain all elements.");
            return false;
        }
    }

    //  from map to reference - check there are no extra elements.
    for(auto i = map.begin(); i != map.end(); ++i)
    {
        if(reference.count(i->first) < 1)
        {
            ORB_TEST_LOG("Persistent map contained unexpected elements.");
            return false;
        }
    }

    return found;
}

int g_mcagtb_counter = 0;

void map_create_and_gc_test_body(const StlSIMap& first_elements,
                                 const StlSIMap& second_elements,
                                 create_map_inserter first_insert,
                                 create_map_inserter second_insert, 
                                 bool first_gc,
                                 bool gc_at_each,
                                 bool remove_before_delete,
                                 bool& result)
{
    using namespace orb;
 
    result = false;

    SIMapPool pool;

    SIMap first_map = first_insert(first_elements, pool, gc_at_each);

    ASSERT_TRUE(verify_map_elements(first_elements, first_map), "First map invalid.");

    if(first_gc)
    { 
        pool.gc();
        ASSERT_TRUE(verify_map_elements(first_elements, first_map), "First map invalid.");
    }

    SIMap* second_map = new SIMap(second_insert(second_elements, pool, gc_at_each));

    ASSERT_TRUE(verify_map_elements(second_elements, *second_map), "Second map invalid.");
    ASSERT_TRUE(verify_map_elements(first_elements, first_map), "First map invalid.");

    pool.gc();

    ASSERT_TRUE(verify_map_elements(second_elements, *second_map), "Second map invalid.");
    ASSERT_TRUE(verify_map_elements(first_elements, first_map), "First map invalid.");

    delete(second_map);

    ASSERT_TRUE(verify_map_elements(first_elements, first_map), "First map invalid.");

    pool.gc();

    ASSERT_TRUE(verify_map_elements(first_elements, first_map), "First map invalid.");

    // Remove half of elements from first map, insert them to another map and back again and
    // verify the results.
    if(first_elements.size() > 1)
    {
        auto maps = split_container(first_elements, 2);
        const int removed = 1;
        const int kept = 0;

        for(auto i = maps[removed].begin(); i != maps[removed].end(); ++i)
        {
            first_map = first_map.remove(i->first);
        }

        if(gc_at_each) first_map.gc();

        ASSERT_TRUE(verify_map_elements( maps[kept], first_map), "Persistent map erase half verify failed.");
    }

    g_mcagtb_counter++; // Use counter to track specific runtimes for debugger attachment.
    result = true;
}

bool persistent_map_create_and_gc_body(const StlSIMap& first_elements, const StlSIMap& second_elements,
                                       bool gc_first,bool gc_at_each, bool remove_before_delete)
{
    using namespace orb;

    bool result = false;

    map_create_and_gc_test_body(first_elements, second_elements, map_insert_elements, map_insert_elements,
                                gc_first, gc_at_each, remove_before_delete, result);
    if(!result) return false;
    map_create_and_gc_test_body(first_elements, second_elements, map_insert_elements, map_direct_instantiation,
                                gc_first, gc_at_each,remove_before_delete,result);
    if(!result) return false;
    map_create_and_gc_test_body(first_elements, second_elements, map_direct_instantiation, map_insert_elements, 
                                gc_first, gc_at_each,remove_before_delete,result);
    if(!result) return false;
    map_create_and_gc_test_body(first_elements, second_elements, map_direct_instantiation, map_direct_instantiation,
                                gc_first, gc_at_each,remove_before_delete,result);
    if(!result) return false;
    map_create_and_gc_test_body(first_elements, first_elements, map_insert_elements, map_insert_elements, 
                                gc_first, gc_at_each,remove_before_delete, result);
    if(!result) return false;
    map_create_and_gc_test_body(first_elements, first_elements, map_insert_elements, map_direct_instantiation,
                                gc_first, gc_at_each,remove_before_delete, result);
    if(!result) return false;
    map_create_and_gc_test_body(first_elements, first_elements, map_direct_instantiation, map_insert_elements,
                                gc_first, gc_at_each,remove_before_delete, result);
    if(!result) return false;
    map_create_and_gc_test_body(first_elements, first_elements, map_direct_instantiation, map_direct_instantiation,
                                gc_first, gc_at_each,remove_before_delete, result);
    return result;
}

UTEST(collections_pmap, PMap_twosource_collect)
{
    using namespace orb;
    bool gc_first             =false;
    bool gc_at_each           = true;
    bool remove_before_delete = false;
    bool result = false;

    StlSIMap first_elements;
    StlSIMap second_elements;

    Random<int> rand;

    int first_count = 1;
    int second_count = 2;

    write_random_elements(first_count, rand, std::string(""), first_elements);
    write_random_elements(second_count, rand, std::string(""), second_elements);

    map_create_and_gc_test_body(first_elements, second_elements, map_insert_elements, map_insert_elements,
                                gc_first, gc_at_each, remove_before_delete, result);

    ASSERT_TRUE(result, "Persistent map twosource collect failed.");
}

UTEST(collections_pmap, PMap_write_and_find_elements)
{
    using namespace orb;

    StlSIMap elements_a;
    add(elements_a, "-1027699544", -1027699544);
    StlSIMap elements_b;
    add(elements_b, "-1027699544", -1027699544)("-1904646281", -1904646281);
    StlSIMap elements_c;
    add(elements_c, "-1027699544", -1027699544)("-1904646281", -1904646281)("-957781851", -957781851);
    StlSIMap elements_d;
    add(elements_d, "-1027699544", -1027699544)("-1904646281", -1904646281)("-957781851", -957781851)("511395623", 511395623);

    SIMapPool pool;
    bool gc_at_each = false;

    SIMap map = map_insert_elements(elements_a, pool, gc_at_each);
    ASSERT_TRUE(verify_map_elements(elements_a, map), "Map failed to store all elements.");

    map = map_insert_elements(elements_b, pool, gc_at_each);
    ASSERT_TRUE(verify_map_elements(elements_b, map), "Map failed to store all elements.");

    map = map_insert_elements(elements_c, pool, gc_at_each);
    ASSERT_TRUE(verify_map_elements(elements_c, map), "Map failed to store all elements.");

    map = map_insert_elements(elements_d, pool, gc_at_each);
    ASSERT_TRUE(verify_map_elements(elements_d, map), "Map failed to store all elements.");

}

#if 0
UTEST(collections_pmap, PMap_combinations)
{
    using namespace orb;
    std::list<int> sizes;
    add(sizes, 1)(2)(3)(4)(5)(6)(7)(11)(13);
    //add(sizes, 1)(2)(3)(4)(5)(6)(7)(11)(13)(17)(19)(23)(29)(31)(32)(33)(67)(135)(271)(543);
    //add(sizes, 1)(2)(3)(4)(5)(6)(7)(11)(13)(17)(19)(23)(29)(31)(32)(33)(67)(135);
    // TODO: Make gc faster. The current n^2 is really, really bad.
    auto size_pairs = all_pairs(sizes);
    typedef Random<int> Rand;
    std::vector<Rand> rands;
    add(rands,Rand(1))(Rand(7))(Rand(17))(Rand());

    size_t pair_index = 1;
    // First run all tests with different maps.
    ut_test_out() << std::endl;
    for(auto p = size_pairs.begin(); p != size_pairs.end(); ++p, pair_index++)
    {
        ut_test_out() << "Run pair " << pair_index << " of " << size_pairs.size() << "(" << p->first << ", "<< p->second << ")" << std::endl;
        int first_size = p->first;
        int second_size = p->second;

        // Test with different pseudorandom sequences.
        for(size_t ri = 0; ri != rands.size(); ++ri)
        {
            StlSIMap first_elements;
            StlSIMap second_elements;

            write_random_elements(first_size,  rands[ri], std::string(""), first_elements);
            write_random_elements(second_size, rands[ri], std::string(""), second_elements);

            // Go through each six combinations of the three bools insertable
            for(uint32_t bools = 0; bools < 6; ++bools)
            {
                bool gc_first             = bit_is_on(bools, 0);
                bool gc_at_each           = bit_is_on(bools, 1);
                bool remove_before_delete = bit_is_on(bools, 2);

                bool result = persistent_map_create_and_gc_body(first_elements, second_elements, gc_first, gc_at_each, 
                                                  remove_before_delete);
                ASSERT_TRUE(result, "Persistent_map_create_and_gc_body failed");

                // Run only with values from one map
                result = persistent_map_create_and_gc_body(first_elements, first_elements, gc_first, gc_at_each, 
                                                  remove_before_delete);
                ASSERT_TRUE(result, "Persistent_map_create_and_gc_body failed");
            }
        }
    }
}
#endif

#if 0
UTEST(collections_pmap, PMap_test)
{
    using namespace orb;


    auto visit_scope = [](SIMap& map, cstring& str, int i){
        auto map2 = map.add(str, i);
        print_pmap(map2);
    };

    PMapPool<std::string, int> pool;
    SIMap map = pool.new_map();

    ut_test_out() << "  #########" << std::endl;

    print_pmap(map);
    auto map1 = map.add("Foo", 300);
    print_pmap(map);
    print_pmap(map1);

    ut_test_out() << "  #########" << std::endl;

    test_overwrite(map);
    print_pmap(map);
    print_pmap(map1);
    
    ut_test_out() << "  #########" << std::endl;

    auto map2 = map1.add("Removethis", 6996);
    print_pmap(map1);
    print_pmap(map2);
    map2 = map2.remove("Removethis");
    print_pmap(map1);
    print_pmap(map2);
    map2.gc();
    print_pmap(map1);
    print_pmap(map2);
}
#endif



