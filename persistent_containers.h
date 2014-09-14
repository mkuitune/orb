/** \file persistent_containers.h Persistent containers.
 *
 * Since C++ does not contain a standard garbage collector and our intent is not to use the persistent
 * data structures universally, each datastructure has it's own allocator and implementation of
 * garbage collection interface that's required to call explicitly. 
 *
 * Datastructures that are implemented:
 *  - persistent list PList
 *      - a simple linked list with distinct heads nodes for reference counting
 *  - persistent map PMap
 *      - a simple persistent map with node copying
 *
 * TODO: Should be straightforward to modify chunkbuffer so it supports parallel deallocation. 
 *
 * I quickly sketched the parallel marking & collection algorithm below. Might be faulty. TODO: verify.
 *
 * For parallel marking, a portion of the chunks must be 'locked' so no new space can be allocated
 * from them prior to collecting. So, to parallellize:
 *
 * 1) divide the chunks in chunkbuffer to gc-groups.
 * 2) one gc-group is selected for marking and deallocation (mad)
 * 3) during mad the gc-group is locked - no new slots can be allocated from it's chunks
 *    (otherwise a slot could be reserved from a chunk that's alread gone through marking and would be
 *     erroneously collected even though it should be reachable)
 *  4) pass marked gc-group for deallocation
 *  5) once deallocated, release gc-group once more into complete allocation pool
 *
 *  There could be, say, two or three allocation groups? TODO: Needs more attention.
 *
 * Once locked, the marking phase and deallocation phase can run in separate threads.
 * 1) Main thread locks allocation group g for mad (markeds pushed to concurrent filo queue)
 * 2) marking thread gets the next item from marking queue, marks them, and adds them to concurrent cleanup -queue
 * 3) deallocation thread gets the next item from cleanup -queue, deallocates unused slots and unlocks the allocation group
 *
 * Should all types of chunkbuffers implement a collection -interface so two mark and deallocation queues can be used for all types of chunkbuffers.
 * The mark thread should contain different marking implementations for all data structures (i.e. how to navigate a hash array trie forest v.s. a list forest)
 * 
 * \author Mikko Kuitunen (mikko <dot> kuitunen <at> iki <dot> fi)
 * */
#pragma once


#include "math_tools.h"
#include "shims_and_types.h"
#include <cassert>
#include<type_traits>
#include<unordered_map>
#include<functional>
#include<sstream>
#include<new>
#include<algorithm>


#define CHUNK_BUFFER_SIZE 32

namespace orb{

/** Chunk. Can be used only for storing classes with parameterless constructor and a destructor.*/
template<class T>
struct Chunk
{
    typename std::aligned_storage <sizeof(T), std::alignment_of<T>::value>::type buffer[CHUNK_BUFFER_SIZE];

    /** Used for storing the allocation state of buffer, with array position matching bit position
     *  1<<i : buffer[i]
     *  i.e the expression
     *  used_elements = 1<<n means buffer[n] is allocated.*/
    uint32_t used_elements;
    uint32_t mark_field; // use for garbage collection
          
    Chunk*   next;

    Chunk(){
        used_elements = 0;
        mark_field = 0;
        next = 0;
        memset(buffer, 0 , CHUNK_BUFFER_SIZE * sizeof(T));
    }

    bool is_full(){return (used_elements == 0xffffffff);}

    T* get_new()
    {
        if(is_full()) return 0;
        uint32_t index = lowest_unset_bit(used_elements);
        used_elements = set_bit_on(used_elements, index);
        T* address = ((T*)buffer) + index;
        T* t = new(address)T;
        return t;
    }
   
    T* at(size_t index) 
    {
        return ((T*)buffer)[index];
    }

    T* get_new_array(const size_t count)
    {
        T* result = 0;

        if(count > CHUNK_BUFFER_SIZE) return result;

        // Return pointer to start of array if enough consecutive slots are found.
        // Try to find a position where there are count free bits-
 
        // First make a mask such as 000001111 where the number of set fields is equal to the
        // number of elements stored. 
        uint32_t mask = 0;
        uint32_t i = count;
        while(i--) mask |= (1 << (i));
        
        uint32_t positions_to_search = CHUNK_BUFFER_SIZE - count + 1;
        uint32_t array_start_index = 0;

        while(positions_to_search--)
        {
            if((used_elements & mask) == 0)
            {
                result = ((T*) buffer) + array_start_index;
                T* tmp;
                for(size_t i = 0; i  < count; ++i) tmp = new(result + i)T;
                (void) tmp;
                used_elements |= mask;
                break;
            }
            mask <<= 1;
            array_start_index++;
        }

        return result;
    }

    void set_marked(const T* ptr)
    {
        uint32_t index = ptr - ((T*)buffer);
        // Note: if ptr < buffer, then index will wrap (to a very large number >> CHUNK_BUFFER_SIZE)
        // and the following clause will be false.
        if(index < CHUNK_BUFFER_SIZE)
        {
            mark_field = set_bit_on(mark_field, index);
        }
    }

    T* begin(){return buffer;}
    T* end(){return buffer + CHUNK_BUFFER_SIZE;}

    bool contains(const T* ptr)
    {
        return ptr >= ((T*) buffer) &&
               ptr < (((T*) buffer) + CHUNK_BUFFER_SIZE);
    }

    bool set_marked_if_contains(const T* elem)
    {
        bool result = false;
        if(contains(elem))
        {
            set_marked(elem);
            result = true;
        }
        return result;
    }
    
    bool set_marked_if_contains_array(const T* start, const size_t count)
    {
        bool result = false;
        if(contains(start) && ((start - ((T*)buffer)) + count) <= CHUNK_BUFFER_SIZE)
        {
            for(size_t i = 0; i < count; ++i) set_marked(start + i);

            result = true;
        }
        return result;
    }

    /** For each used/marked bit */
    void collect_marked()
    {
        uint32_t is_used = (used_elements ^ mark_field) & used_elements;
                              // 1 ^ 0 = 1
        for(int index = 0; index < CHUNK_BUFFER_SIZE; ++index)
        {
            // If field is used but unmarked as active
            if((is_used >> index)&0x1)
            {
                // Set slot unused and call destructor on allocated memory
                used_elements = set_bit_off(used_elements, index);
                T* t = &((T*)buffer)[index];
                t->~T();
            }
        }
    }

    /** Return total size used. */
    size_t reserved_size_bytes() const {return sizeof(*this);}

    /** Return size of referred storage used. */
    size_t live_size_bytes() const {return sizeof(T) * count_bits(used_elements);}

};


template<class T>
class ChunkBox
{
public:
    typedef Chunk<T>                        chunk_type;
    typedef std::list<chunk_type>           chunk_container;
    typedef typename std::list<chunk_type>::iterator iterator;
    
    ChunkBox()
    {
        free_chunks_ = new_chunk();
    }

    chunk_type* new_chunk()
    {
        chunks_.push_back(chunk_type());
        chunk_type* chunk = &chunks_.back();
        return chunk;
    }

    T* reserve_element()
    {
        T* elem = 0;

        if(free_chunks_)
        {
            elem = free_chunks_->get_new();
            chunk_type* chunk = free_chunks_;
            // Check if free chunks is still free or do we need new chunks
            if(chunk->is_full())
            {
                if(chunk->next)
                {
                    free_chunks_ = chunk->next;
                }
                else
                {
                    free_chunks_ = new_chunk();
                }
                chunk->next = 0;
            }
        }

        return elem;
    }

    /** Try to reserve element_count new consecutive elements. 
        @param element_count number of consecutive elements to reserve 
        @result First element in the allocated sequence or null*/
    T* reserve_consecutive_elements(const size_t element_count)
    {
        T* result = 0;
        if(element_count <= CHUNK_BUFFER_SIZE)
        {
            chunk_type* chunk = free_chunks_;
            chunk_type* first_chunk =  chunk;
            chunk_type* prev_chunk = 0;

            while(chunk)
            {
                if((result = chunk->get_new_array(element_count))) 
                {
                    // Check if chunk still has space left and if not maintain free node list
                    if(chunk->is_full())
                    {
                        if(!prev_chunk && !chunk->next)
                        {
                            free_chunks_ = new_chunk();
                        }
                        else if(!prev_chunk && chunk->next)
                        {
                            free_chunks_ = chunk->next;
                        }
                        else // prev_chunk && chunk->next := valid|0
                        {
                            prev_chunk->next = chunk->next;
                        }

                        chunk->next = 0;
                   }

                   break;
               }
               else
               {
                   prev_chunk = chunk;
                   chunk = chunk->next;
                   if(chunk == first_chunk)
                   {
                       assert(!"ChunkBox: Free chunk list is circular.");
                       break; //TODO add breakpoint, debug
                   }
               }
            }

            if(!chunk)
            {
                // Did not find a suitable chunk. Create a new chunk and add it to the front of the
                // free list if the array does not consume it completely.
                chunk_type* created_chunk = new_chunk();

                if(element_count < CHUNK_BUFFER_SIZE)
                {
                    created_chunk->next = free_chunks_;
                    free_chunks_ = created_chunk;
                }

                // At this point we know the operation cannot fail.
                result = created_chunk->get_new_array(element_count);
            }
        }

        return result;
    }

    void refresh_free_chunk_list()
    {
        free_chunks_ = 0;
        for(auto chunk = chunks_.begin(); chunk != chunks_.end(); ++chunk)
        {
            if(!chunk->is_full())
            {
                chunk->next = free_chunks_;
                free_chunks_ = &(*chunk);
            }
        }
    }

    void mark_all_empty()
    {
        for(auto c = chunks_.begin(); c != chunks_.end(); ++c)
        {
            c->mark_field = 0;
        }
    }

    /** After the chunks have been marked, collect the unused memory in all of them. If 
     * chunk has been full and has some memory freed move it to the free_chunks_ list.*/
    void collect_chunks()
    {
        free_chunks_ = 0;
        for(auto chunk = begin(); chunk != end(); ++chunk)
        {
            chunk->collect_marked();

            if(!chunk->is_full())
            {
                chunk->next = free_chunks_;
                free_chunks_ = &(*chunk);
            }
        }
    }

    void set_marked_if_contained(const T* ptr)
    {
        std::for_each(begin(), end(), [&ptr](chunk_type& c){c.set_marked_if_contains(ptr);});
    }
    
    void set_marked_if_contained_array(const T* ptr, size_t size)
    {
        // TODO replace for_each with a loop that break immediately whem a match is found.
        std::for_each(begin(), end(), [&ptr, &size](chunk_type& c){c.set_marked_if_contains_array(ptr, size);});
    }

    iterator begin(){return chunks_.begin();}
    iterator end(){return chunks_.end();}

    chunk_container& chunks(){return chunks_;}
    chunk_type* free_chunks(){return free_chunks_;}

    size_t reserved_size_bytes() const {
        return sizeof(chunk_type) * chunks_.size();
    }

    size_t live_size_bytes() const
    {
        auto collect = [](size_t r, const chunk_type& t)->size_t {return r + t.live_size_bytes();};
        size_t init = 0;
        return fold_left<size_t, chunk_container>(init, collect, chunks_);
    }

private:
    chunk_container chunks_;
    chunk_type*     free_chunks_;
};


/////////// Persistent list //////////////

/** Pool manager and collector for persistent lists. 
 *  Not a particularly efficient implementation. */
template<class T>
class PListPool
{
public:

    /** List node. */
    struct Node
    {
        Node* next;
        T     data;
    };

    /** Stores head to List */
    class List
    {
    public:
        struct iterator
        {
            Node* node;
            iterator():node(0){}
            iterator(Node* n):node(n){}
            const T& operator*() const {return node->data;}
            T& operator*() {return node->data;}
            const T* operator->() const {return &(node->data);}
            T* operator->() {return &(node->data);}
            T* data_ptr() {return &node->data;}
            void operator++(){if(node) node = node->next;}
            bool operator!=(const iterator& i) const {return node != i.node;}
            bool operator==(const iterator& i) const {return node == i.node;}
        };

    public:

        typedef T value_type;

        List(PListPool& pool, Node* head):pool_(pool), head_(head){if(head_) pool_.add_ref(head_);}
        ~List(){if(head_) pool_.remove_ref(head_);}
       
        List(const List& old_list):pool_(old_list.pool_), head_(old_list.head_)
        {
            if(head_) pool_.add_ref(head_);
        }

        List(List&& temp_list):pool_(temp_list.pool_), head_(temp_list.head_)
        {
            temp_list.head_ = 0;
        }

        List& operator=(const List& list)
        {
            if(this != &list)
            {
                head_ = list.head_;
                pool_ = list.pool_;
                if(head_) pool_.add_ref(head_);
            }
            return *this;
        }

        List& operator=(List&& list)
        {
            if(this != &list)
            {
                head_ = list.head_;
                pool_ = list.pool_;
                list.head_ = 0;
            }
            return *this;
        }

        /** Warning: Use only if you know what you are doing. */
        void increment_ref()
        {
            if(head_) pool_.add_ref(head_);
        }

        /** Find first element from list matching with predicate or return end. */ 
        iterator find(const List* list, std::function<bool(const T&)>& pred) const
        {
            Node* n = head_;
            while(n && ! pred(n->data)) n = n->next;
            return iterator(n);
        }

        /** Add element to list */
        List add(const T& data) const
        {
            Node* n = pool_.new_node(data);
            n->next = head_; // prepend new element to head
            return List(pool_, n);
        }

        /** Add iterator range to list.*/
        template<class I>
        List add_end(I ibegin, I iend)
        {
            return pool_.add(*this, ibegin, iend);
        }

        /** Remove element from list. */
        List remove(const iterator& i) const
        {
            // Neither head nor node to remove can be null.
            if(i.node != 0 && head_)
            {
                // If node is first, just return a list starting from next node.
                if(head_ == i.node) return List(pool_, head_->next);

                // Otherwise must duplicate nodes prior to node to remove
                // [a]->[b]->[c]->[d]
                //
                //[a']->[b']->[d]

                // find position
                Node* n        = head_;
                Node* new_head = pool_.copy(n);;
                Node* build    = new_head;

                n = n->next;

                while(n && n != i.node)
                {
                    build->next = pool_.copy(n);
                    build = build->next;
                    n = n->next;
                }
                
                // Finally tie to the next cell from to be removed
                build->next = i.node->next;

                return List(pool_, new_head);
            }
            else
            {
                return *this;
            }
        }

        /** Return list containing all but the first element or emtpy list. */
        List rest() const
        {
            return head_ ? List(pool_, head_->next) : List(pool_, 0);
        }

        /** Return list containing all but the two first elements or emtpy list. */
        List rrest() const
        {
            if(head_ && head_->next) return List(pool_, head_->next->next);
            else return List(pool_, 0);
        }

        /** Return list containing all but the three first elements or emtpy list. */
        List rrrest() const
        {
            if(head_ && head_->next && head_->next->next) return List(pool_, head_->next->next);
            else return List(pool_, 0);
        }

        /** Return reference to the first element of list or null.*/
        const T* first() const
        {
            return head_ ? &head_->data : 0;
        }

        /** Return the second element in the list or empty.*/ 
        const T* second() const
        {
            if(head_ && head_->next && head_->next->next)
                return &head_->next->next->data;
            else return 0;
        }

        bool empty() const {return head_ == 0;}

        bool has_rest() const {return head_ ? head_->next != 0 : false;}

        iterator begin() const {return iterator(head_);}
        iterator end() const {return iterator(0);}

        const size_t size() const
        {
            size_t s = 0;
            Node* n = head_;
            while(n){ n = n->next; s++;}
            return s;
        }

        bool operator==(const List& l) const
        {
            iterator i = begin(), last = end(), li = l.begin(), le = l.end();
            while(i != last && li != le)
            {
                if(!(*i == *li)) return false;
                ++i; ++li;
            }
            if(i != last || li != le) return false;
            return true;
        }

    private:
        PListPool& pool_;
        Node*      head_; 
    };

    typedef Chunk<Node>                    node_chunk;
    typedef ChunkBox<Node>                 node_chunk_box;

    typedef std::unordered_map<Node*, int> ref_count_map;
    
    /** Recycle all memory. */
    void kill()
    {
        // Deleting ListPool before the end of the lifetime of all heads will result
        // in undefined behaviour.
        // Call destructor on unused elements
        ref_count_.clear();
        gc(); 
    }

    PListPool()
    {
    }

    ~PListPool()
    {
        kill();
    }

    /** Create new empty list. */
    List new_list()
    {
        return List( *this, 0);
    }

    /** Remove reference to node */
    void remove_ref(Node* n)
    {
        int* ref_count = try_get_value(ref_count_, n);
        if(ref_count && (*ref_count > 0)) --(*ref_count);
    }

    /** Add reference to node*/
    void add_ref(Node* n)
    {
        if(has_key(ref_count_, n)) ref_count_[n]++;
        else ref_count_[n] = 1;
    }

    /** Create new list from stl compatible container. */
    template<class Cont>
    List new_list(const Cont& container)
    {
        if(container.size() == 0) return List(*this, 0);

        auto i = container.begin();
        Node* head = new_node(*i);
        Node* node = head;
        i++;

        for(;i != container.end(); ++i)
        {
            node->next = new_node(*i);
            node = node->next;
        }

        return List( *this, head);
    }
 
    template<class I>
    Node* add_elem(I i_begin, I i_end) 
    {
        if(i_begin == i_end) return 0;
        else
        {
            Node* n = new_node(*i_begin);
            ++i_begin;
            n->next = add_elem(i_begin, i_end);
            
            return n;
        }
    }

    /** Create new list by appending elements in iterator range to list. */
    template<class I>
    List add(const List& old, I i_begin, I i_end)
    {
        //if(i_begin == i_end) return List(*old); // TODO: need this?


        auto o_i = old.begin();
        auto o_end = old.end();

        Node* cpy_head = 0;
        Node* cpy_pos = 0;

        if(o_i == o_end){
            Node* new_head = add_elem(i_begin, i_end);
            return List( *this, new_head);
        }else{

            while(o_i != o_end)
            {
                Node* n = new_node(*o_i);

                if(!cpy_head)
                {
                    cpy_head = n;
                    cpy_pos = cpy_head;
                }
                else
                {
                    cpy_pos->next = n;
                    cpy_pos = n;
                }

                ++o_i;
            }

            cpy_pos->next = add_elem(i_begin, i_end);
            return List( *this, cpy_head);
        }
    }


    /** Create new list from a number of input values. */
    List new_list(const T& a)
    {
        Node* head = new_node(a);

        return List( *this, head);
    }

    /** Create new list from a number of input values. */
    List new_list(const T& a, const T& b)
    {
        Node* head = new_node(a);
        Node* node = head;

        node->next = new_node(b);

        return List( *this, head);
    }

    /** Create new list from a number of input values. */
    List new_list(const T& a, const T& b, const T& c)
    {
        Node* head = new_node(a);
        Node* node = head;

        node->next = new_node(b);
        node = node->next;

        node->next = new_node(c);

        return List( *this, head);
    }

    // Return duplicate of existing node sans the links
    Node* copy(Node* n)
    {
        Node* c = 0;
        if(n)
        {
           c = new_node(n->data); 
        }
        return c; 
    }

    Node* new_node(const T& data)
    {
        Node* n = chunks_.reserve_element();
        n->data = data;
        n->next = 0;
        return n;
    }

    void mark_referenced(Node* node)
    {
        auto chunks_end = chunks_.end();
        auto chunk_iterator = chunks_end;

        // Follow referenced nodes
        while(node)
        {
            // Store reference to previous chunk visited. If there is not too much
            // fragmentation there is a fair chance that nodes have been reserved
            // linearly and that the previous chunk contains this node.

            if(chunk_iterator != chunks_end && chunk_iterator->set_marked_if_contains(node))
            {
                node = node->next;
                continue;
            }

            // Find the chunk that contains the node and label used.
            for(chunk_iterator = chunks_.begin(); chunk_iterator != chunks_end; ++chunk_iterator)
            {
                if(chunk_iterator->set_marked_if_contains(node)) break;
            }

            node = node->next;
        }   
    }
    
    /** Return number of bytes used by the chunk pool in total. */
    size_t reserved_size_bytes()
    {
        size_t ref_map_size = ref_count_.size() * (sizeof(Node*) + sizeof(int));
        size_t total = sizeof(*this) + ref_map_size + chunks_.reserved_size_bytes(); 
        return total;
    }

    size_t live_size_bytes()
    {
        size_t ref_map_size = ref_count_.size() * (sizeof(Node*) + sizeof(int));
        size_t total = sizeof(*this) + ref_map_size +  chunks_.live_size_bytes();
        return total;
    }

    // Collect all slots taken by unvisitable nodes //TODO: 
    void gc()
    {
        // Currently bit expensive - the cost is 
        //    constant * block_count (free all nodes) + m * block_count/2 (mark all visited nodes)
        // where m is the number of live nodes in all the lists

        // First mark all as empty
        chunks_.mark_all_empty();

        // Then clean up unused references, visit all heads and mark visited nodes as active
        ref_count_ = copyif(ref_count_, [](const std::pair<Node*, int>& p){return p.second > 0;});

        // Now ref_count_ contains as keys the head nodes of active lists.
        for(auto r = ref_count_.begin(); r != ref_count_.end(); ++r) mark_referenced(r->first);

        // Lastly, go through the blocks, deallocate free's slots and move chunks to free list
        // if space became available on a full one
        chunks_.collect_chunks();
    }

    /** Clear refcounts. Warning: use only if you know what you are doing. */
    void clear_root_refcounts()
    {
        ref_count_.clear();
    }

private:
    node_chunk_box        chunks_;
    ref_count_map         ref_count_;   //> Head node reference counts

};


/////////// Persistent map //////////////

#if 1
/** A persistent, self garbage-collecting version of Bagwell's persistent hash tries.
    Hash key: uint32_t

    The key is split to five 5-bit length fields and one 2-bit length field as

level  0     1     2     3     4     5   6
    |aaaaa|bbbbb|ccccc|ddddd|eeeee|fffff|gg
     0-31  0-31  0-31  0-31  0-31  0-31  0-3

    where each level give the index to use at the child-node array at the given level.


    Node: used_field: uint32_t
          ref_array = Ref * [huffmanlength(used_field)]

    Each node contains 32 slots for children.

    The ref_array is allocated only to the huffman length of the used_field (i.e. numbers of bits 1
    in the field).

    Each element in ref-array stores either: a) A reference to a child-node, b) A reference to the stored element or c) A reference to a list of stored elements in
    case the hash-key is overwritten.

    The datum for the element is not stored inside the node, only a reference to it.

    The storage used by a node _without the stored elements_ is 

    4 bytes (used_field) + 8 bytes (ref_array pointer) + elem_count(1 to 32) * 8 (ref_array contents) -> 20 to 268 bytes

    The hash trie is used in a persistent manner - each modifications return a new root-node
    with the path to the changed node re-allocated. The stored elements themselves are not
    copied, only the references to them.

    Each non-value type (i.e. that encompass more memory than just sizeof(T), std::string etc,
    stored in the map must have it's own implementation of the hash32 function.

*/

template<class K>
class AreEqual { public:
    static bool compare(const K& k1, const K& k2){return k1 == k2;} 
};

template<class H>
class MapHash { public:
    static uint32_t hash(const H& h){return get_hash32(h);}
};

template<class K, class V, class Compare = AreEqual<K>, class HashFun = MapHash<K>>
class PMapPool
{
public:

    /** Key-value pair. */
    struct KeyValue{uint32_t hash; K first; V second;};
   
    static const V* keyvalue_match_get(const KeyValue& kv, const K& key, const uint32_t hash) 
    {
        if(kv.hash == hash && Compare::compare(key, kv.first))
            return &kv.second;
        else return 0;
    }

    /* Data structure types. */
    
    typedef          PListPool<const KeyValue*>       KeyValueListPool;
    typedef typename PListPool<const KeyValue*>::List KeyValueList;

    struct RefCell;

    struct Node
    {
        struct Ref
        {
            Node* node;
            Node& operator*(){return *node;}
            Node& operator->(){return *node;}
        };

        enum NodeType{EmptyNode, ValueNode, CollisionNode};

        uint32_t used;
        Ref*     child_array; //> Pointer to an allocated child sequence
       
        /** For ValueNodes the keyvalue field is referenced. For
         *  CollisionNodes the collision_list field is referenced. */ 
        union
        {
            const KeyValue*     keyvalue;
            KeyValueList* collision_list; 
            //Before end of the trie tree hash collisions are just proapagated downwards.
            //The collided elements are always only keyvalues.
        }value;

        NodeType type; 

        Node():used(0), child_array(0){}

        ~Node()
        {
            if(type == CollisionNode && value.collision_list) delete value.collision_list;
        }

        /** Return number of elements stored */
        size_t size(){ return count_bits(used);}

        /** Return pointer to hash value if type is not EmptyNode */
        const uint32_t* hash_value()
        {
            const uint32_t* result = 0;
            if(type == CollisionNode)
            {
                typename KeyValueList::iterator kvi = value.collision_list->begin();
                const KeyValue* kv(*kvi.data_ptr());
                result = &(kv->hash);

            }
            else if(type == ValueNode)
            {
                result = &(value.keyvalue->hash);
            }
            return result;
        }

        /** Return true if the index signified by the index is used.*/
        bool index_in_use(uint32_t index){return bit_is_on(used, index);}

        /** Return index matching the bitfield. The bitfield must reflect an actually used bit.*/
        uint32_t index(uint32_t bitfield){return count_bits(used & (bitfield - 1));}

        /** Return element corresponding to the hash-key.*/
        Node* get_child_by_fieldindex(uint32_t parsed_index)
        {
            if(index_in_use(parsed_index))
            {
                uint32_t child_index = index(1 << parsed_index);
                return child_array[child_index].node;
            }
            else
            {
                return 0;
            }
        }

        Node* get_child_by_hash_and_depth(const uint32_t hash, const uint32_t depth)
        {
            return get_child_by_fieldindex((hash >> (depth * 5)) & 0x1f);
        }

        /** Return reference matching the bit pattern or null if array is not large enough.*/
        Ref* get(uint32_t key, uint32_t level)
        {
            Ref* result = 0;
            uint32_t local_index = (key >> (level*5)) & 0x1f;
            uint32_t child_index = index(1 << local_index);
            if(child_index < size()) result = child_array + child_index;
            return result;
        }

        Ref* begin(){return child_array;}
        Ref* end(){return child_array + size();}
    };

    static bool node_has_children(Node* n) {return n->child_array != 0;}
    static bool node_has_keyvalues(Node* n) {return n->type != Node::EmptyNode;}

    /**
     * Iterator for the values in one node.
     */
    struct NodeValueIterator
    {
        Node* node;
        typename KeyValueList::iterator iter;
        typename KeyValueList::iterator end;
        
        const KeyValue* keyvalue;
        bool      iteration_ongoing;


        NodeValueIterator():node(0), keyvalue(0), iteration_ongoing(false){}

        NodeValueIterator(Node* node_in):node(node_in), keyvalue(0), iteration_ongoing(false)
        {
            if(node->type == Node::CollisionNode) end = node->value.collision_list->end();
        }

        /** Returns true as long as the iterator is ongoing. Once the iteration is complete
         *  the internal reference to keyvalue is in undefined state. */
        bool move_next()
        {
            bool result = false;
            switch(node->type)
            {
                case Node::ValueNode:
                {
                    if(!keyvalue)
                    {
                        // Can visit the only value only once.
                        keyvalue = node->value.keyvalue;
                        result = true;
                    }
                    break;
                }
                case Node::CollisionNode:
                {
                    if(iteration_ongoing)
                    {
                        ++iter;
                        if(iter != end)
                        {
                            keyvalue = *iter;
                            result = true;
                        }
                    }
                    else
                    {
                        iteration_ongoing = true;
                        // The list has at least two elements. No need to check for nil head.
                        iter = node->value.collision_list->begin();
                        keyvalue = *iter;
                        result = true;
                    }
                    break;
                }
                case Node::EmptyNode:
                default:
                    break;
            }
            return result;
        }

        const KeyValue* current() const {return keyvalue;}
    };

    // Itearator for children inside node.
    struct NodeChildIterator
    {
        Node* node;
        typename Node::Ref* iter;  
        typename Node::Ref* end;  
        bool iterating;
        bool node_has_children;

        NodeChildIterator(Node* node_in):node(node_in), iterating(false), node_has_children(false)
        {
            end = node->end();
            if(node_in->size() > 0) node_has_children = true;
        }

        bool move_next()
        {
            if(!node_has_children) return false;

            bool result = false;
            if(iterating) 
            {
                iter++;
            }
            else
            {
                iter = node->begin();
                iterating = true;
            }

            if(iter != end) result = true;

            return result;
        }

        Node* current(){return iter->node;}
    };
    
    typedef Chunk<KeyValue>  keyvalue_chunk;
    typedef Chunk<Node>      node_chunk;
    typedef Chunk<typename Node::Ref> ref_chunk;

    typedef ChunkBox<KeyValue>  keyvalue_chunk_box;
    typedef ChunkBox<Node>      node_chunk_box;
    typedef ChunkBox<typename Node::Ref> ref_chunk_box;

    typedef std::unordered_map<Node*, int> refcount_map;

    // Unordered iterator to map nodes
    //
    // The purpose is to support stl -like iteration.
    class node_iterator
    {
        // Use stack of node child iterators to keep track of state.
        // Algorithm:
        //  i)  Go to the leftmost node foundable
        //  ii) iterate it's values
        //  ii) go to node sibling, 
        
        FixedStack<NodeChildIterator, 8> iter_stack; //Max tree depth is 7 plus root.
        NodeValueIterator value_iter;

        const KeyValue* current;

#define CURRENT_NODE iter_stack.top()->current()

        // For each node:
        // Iterate over each value in array. If value is a node, go into node.
        public:

        node_iterator(Node* node):current(0)
        {
            if(node && node_has_children(node))
            {
                push_node(node);
            }
        }

        // Latch onto the children of the node - make sure the node has children prior to pushing it
        void push_node(Node* parent_node)
        {
            iter_stack.push(NodeChildIterator(parent_node));
            NodeChildIterator* ni = iter_stack.top();
            ni->move_next(); // Must succeed as node has children - now have a valid node

            Node* node = ni->current();
            if(node_has_keyvalues(node))
            {
                value_iter = NodeValueIterator(node);
                advance(); // Move value_iter to valid value
            }
            else
            {
                // Push until a node with values is found.
                push_node(node);
            }
        }

        void advance()
        {
            if(value_iter.move_next())
            {
                current = value_iter.current();
            }
            else
            {
                NodeChildIterator* ni = iter_stack.top();
                Node* node =ni->current();
                // Ran out of values - find next node.
                if(node_has_children(node))
                {
                    push_node(node);
                }
                else
                {
                    // Move to next sibling node or if none left pop.
                    if(ni->move_next())
                    {
                        Node* next_node = ni->current();
                        value_iter = NodeValueIterator(next_node);
                        advance(); // Move value_iter to valid value
                    }
                    else
                    {
                        // No siblings left - pop stack
                        pop();
                    }
                }
            }
        }

        void pop()
        {
            iter_stack.pop();

            if(iter_stack.depth() > 0)
            {
                NodeChildIterator* ni = iter_stack.top();
                if(ni->move_next())
                {
                    Node* node = ni->current();
                    if(node_has_keyvalues(node))
                    {
                        value_iter = NodeValueIterator(node);
                        advance();
                    }
                    else
                    {
                        push_node(node);
                    }
                }
                else
                {
                    pop();
                }
            }
            else
            {
                // Stack emtpy, nothing left to do - terminate iteration.
                current = 0;
            }
        }

        void operator++(){advance();}

        bool operator==(const node_iterator& i){return current == i.current;}
        bool operator!=(const node_iterator& i){return current != i.current;}
        const KeyValue* data_ptr(){return current;}
        const KeyValue& operator*(){return *current;}
        const KeyValue* operator->(){return current;}
    };

    /** The map class.*/
    class Map
    {
    public:
        typedef node_iterator iterator;

        typedef K key_type;
        typedef V mapped_type;
        typedef KeyValue value_type;

        Map(PMapPool& pool, Node* root):pool_(pool), root_(root)
        {
            if(root_) pool_.add_ref(root_);
        }

        Map(const Map& map):pool_(map.pool_), root_(map.root_)
        {
            if(root_) pool_.add_ref(root_);
        }

        Map(Map&& map):pool_(map.pool_), root_(map.root_)
        {
            map.root_ = 0;
        }

        ~Map()
        {
            if(root_) pool_.remove_ref(root_);
        }

        Map& operator=(const Map& map)
        {
            if(this != &map)
            {
                if(root_) pool_.remove_ref(root_);
                pool_ = map.pool_;
                root_ = map.root_;
                pool_.add_ref(root_);
            }
            return *this;
        }

        Map& operator=(Map&& map)
        {
            if(this != &map)
            {
                if(root_) pool_.remove_ref(root_);
                pool_ = map.pool_;
                root_ = map.root_;
                map.root_ = 0;
            }
            return *this;
        }

        /** Warning: Use only if you know what you are doing. */
        void increment_ref()
        {
            if(root_) pool_.add_ref(root_);
        }

        ConstOption<V> try_get_value(const K& key) const
        {
            if(!root_) return ConstOption<V>(0);

            uint32_t hash = HashFun::hash(key);
            uint32_t level = 0; 
            const V* result = 0;
            Node* current = root_;

            for(;level < 8; level++) 
            {
                Node* n = current->get_child_by_hash_and_depth(hash, level);
                if(n)
                {     
                    if(n->type == Node::ValueNode)
                    {
                        result = keyvalue_match_get(*n->value.keyvalue, key, hash);
                    }
                    else if (n->type == Node::CollisionNode)
                    {
                        typename KeyValueList::iterator i = n->value.collision_list->begin();
                        typename KeyValueList::iterator end = n->value.collision_list->end();
                        for(;i != end && (result == 0); ++i)
                        {
                            result = keyvalue_match_get(**i, key, hash);
                        }
                    }

                    if(result) break;

                    current = n; // Just move on if match not found.
                }
                else
                {
                    // Trie path ran out. No value.
                    break;
                }
            }

            return ConstOption<V>(result);
        }

        /** Rewrite value held in existing key. In most instances avoid this if possible. */
        bool try_replace_value(const K& key, const V& value)
        {
            bool result = false;
            ConstOption<V> opt = try_get_value(key);
            if(opt.is_valid())
            {
                V* v = const_cast<V*>(opt.get());
                *v = value;
                result = true;
            }
            return result;
        }

        // TODO: template<class T> Map.add_seq keyvalue-pair list - add sequence of pairs in one update

        // TODO: map_add_diff -> map -> value -> diff that implements the element add operator
        // on a map and also returns a diff between the two versions of the maps.

        /** Add key and value to map.*/
        Map add(const K& key, const V& value)
        {
            return pool_.add(*this, key, value);
        }

        template<class KI, class VI>
        Map add(const KI i_key, const KI key_end, const VI i_value, const VI value_end) const
        {
            return pool_.add(*this, i_key, key_end, i_value, value_end);
        }

        // TODO: map_remove_diff -> map -> value -> diff that implements the element remove operator
        // on a map and also returns a diff between the two versions of the maps.

        /** Remove key entry from map.*/
        Map remove(const K& key)
        {
            if(try_get_value(key).is_valid())
            {
                // Remove root and branch containing the removed value. Collect all keyvalues not equal to missing key.
                std::list<const KeyValue*> kept_keys;

                Node* root = root_;
                Node* new_root = pool_.new_node();

                // Copy values first from old root to new as is.
                *new_root = *root;

                uint32_t hash = HashFun::hash(key);
                Node* removed_branch = root_->get_child_by_hash_and_depth(hash, 0);

                // Have to collect the keyvalue from node separately.
                NodeValueIterator node_val_iter(removed_branch);
                while(node_val_iter.move_next())
                {
                    const KeyValue* current = node_val_iter.current();
                    if(! keyvalue_match_get(*current, key, hash)) 
                    {
                        kept_keys.push_back(current);
                    }
                }

                // Visit the branch containing the keyvalue and collect all keyvalues except the one to remove.
                node_iterator iter(removed_branch);
                node_iterator end(0);

                for(;iter != end; ++iter)
                {
                    const KeyValue* current = iter.data_ptr();
                    if(! keyvalue_match_get(*current, key, hash)) 
                    {
                        kept_keys.push_back(current);
                    }
                }

                // Remove the branch holding the sought out keyvalue from the new_root.
                size_t old_size = new_root->size();
                size_t new_size = old_size - 1;

                if(new_size > 0)
                {
                    uint32_t invalid_index = (hash) & 0x1f;
                    new_root->used = set_bit_off(new_root->used, invalid_index);

                    typename Node::Ref* new_child_array = pool_.ref_chunks_.reserve_consecutive_elements(new_size);
                    new_root->child_array = new_child_array;
                    size_t i = 0;
                    size_t child_index = 0;
                    for(i = 0; i < old_size; ++i)
                    {
                        if(root_->child_array[i].node != removed_branch) new_child_array[child_index++] = root_->child_array[i];
                    }
                }
                else
                {
                    new_root->used = 0;
                    new_root->child_array = 0;
                }

                // Init new map root and child array - add the kept keyvalues from the removed branch.

                Map out_map(pool_, new_root);

                for(auto i = kept_keys.begin(); i != kept_keys.end(); ++i)
                {
                    out_map = pool_.add(out_map, *i);
                }

                return out_map;
            }
            else
            {
                // Nothing to remove, just return a new instance of the current map.
                return *this;
            }
        }

        // Run garbage collector on the root pool.
        void gc(){pool_.gc();}

        iterator begin() const {return iterator(root_);}
        iterator end() const {return iterator(0);}
      
        bool operator==(const Map& m) const
        {
            iterator i = begin(), last = end();
            while(i != last)
            {
                ConstOption<V> opt = m.try_get_value(i->first);
                if(! (opt.is_valid() && (*opt) == i->second)) return false;
                ++i;
            }
            return true;
        }

        const size_t size() const
        {
            return orb::iterator_range_length(begin(), end());
        }

        friend class PMapPool;

    private:
        PMapPool& pool_;
        Node* root_;
    };

    /** Recycle all memory. */
    void kill()
    {
        // Must call destructor on all used cells.
        // TODO: Should this be the responsibility of individual containers?
        ref_count_.clear();
        gc();
    }

    ~PMapPool()
    {
        kill();
    }

    /** Remove reference to node */
    void remove_ref(Node* n)
    {
        int* ref_count = try_get_value(ref_count_, n);
        if(ref_count && (*ref_count > 0)) --(*ref_count);
    }

    /** Add reference to node*/
    void add_ref(Node* n)
    {
        if(has_key(ref_count_, n)) ref_count_[n]++;
        else ref_count_[n] = 1;
    }

    /** Clear refcounts. Warning: use only if you know what you are doing. */
    void clear_root_refcounts()
    {
        ref_count_.clear();
    }

    /** Create new empty map */
    Map new_map()
    {
        Map m(*this, 0);
        return m;
    }
    
    /** Create a copy from existing map. TODO: Remove this?*/
    template<class M>
    Map new_map(const M& map_in)
    {
        Node* root = 0;

        for(auto i = map_in.begin(); i != map_in.end(); ++i)
        {
            KeyValue* kv = new_keyvalue(i->first, i->second);
            root = instantiate_tree_path(root, kv);
        }

        Map m(*this, root);

        return m;
    }

    /** Create new map with one element*/
    Map new_map(const K& key, const V& value)
    {
        KeyValue* kv = new_keyvalue(key, value);
        Node* root = instantiate_tree_path(0, kv);

        Map m(*this, root);
        return m;
    }

    /** Create a new map by adding an element to existing map*/
    Map add(const Map& old, const K& key, const V& value)
    {
        KeyValue* kv = new_keyvalue(key, value);
        Node* new_root = instantiate_tree_path(old.root_, kv);
        return Map(*this, new_root);
    }

    /** Create a new map by adding an elements and values to an existing map*/
    template<class KI, class VI>
    Map add(const Map& old, KI i_key, KI key_end, VI i_value, VI value_end)
    {
        Node* new_root = old.root_;

        while((i_key != key_end) && 
              (i_value != value_end))
        {
            KeyValue* kv = new_keyvalue(*i_key, *i_value);

            new_root = instantiate_tree_path(new_root, kv);

            ++i_key;
            ++i_value;
        }

        return Map(*this, new_root);
    }

    /** Create a new map by adding an elements and values to an existing map. Usable in case key and 
    *   value types are the same.*/
    template<class KVI>
    Map add(const Map& old, KVI i_elems, KVI elems_end)
    {
        Node* new_root = old.root_;

        KVI first;

        while((i_elems != elems_end))
        {
            first = i_elems;
            ++i_elems;
            if(i_elems != elems_end)
            {
                KeyValue* kv = new_keyvalue(*first, *i_elems);
                new_root = instantiate_tree_path(new_root, kv);
            }
            
            ++i_elems;
        }

        return Map(*this, new_root);
    }

    /** Create a new map by adding an allocated keyvalue instance to an existing map.*/
    Map add(const Map& old, const KeyValue* keyvalue)
    {
        Node* new_root = instantiate_tree_path(old.root_, keyvalue);
        return Map(*this, new_root);
    }

    KeyValue* new_keyvalue(const K& k, const V& v)
    {
        KeyValue* kv = keyvalue_chunks_.reserve_element();
        kv->first  = k;
        kv->second = v;
        kv->hash   = HashFun::hash(k);
        return kv;
    }

    /** Replace the existing reference array at node with a new array that has the newnode inserted at loc. */
    void node_insert_replace_refarray(Node* parent, Node* newnode, const uint32_t local_index, const uint32_t child_level)
    {
        size_t   old_count       = parent->size();
        size_t   new_count       = old_count + 1;
        uint32_t local_index_bit = 1 << local_index;

        typename Node::Ref* child_array = ref_chunks_.reserve_consecutive_elements(new_count);
        typename Node::Ref* old_children = parent->child_array;

        // Create new child array for current node and mark new index used.
        parent->child_array = child_array;
        parent->used |= local_index_bit;

        uint32_t array_index = parent->index(local_index_bit); 
        // TODO: Check all child_array references that they use explicitly ref.
        child_array[array_index].node = newnode;

        auto child_ref = child_array + array_index;

        // Copy all other children to new places in new child array
        for(auto oi = old_children; oi != old_children + old_count; ++oi)
        {
            const uint32_t* child_hash = oi->node->hash_value();
            assert(child_hash);
            auto ref = parent->get(*child_hash, child_level);
            assert(ref != child_ref);
            ref->node = oi->node;
        }
    }

    /** Create new nodes on map for the path for the given hash value
     *  @param hash hash value in.
     *  @returns RootValue for new tree
     */
    Node* instantiate_tree_path(const Node* old_root, const KeyValue* kv)
    {
        Node* newroot = new_node();// This will be the root of the duplicate map.

        if(old_root) *newroot = *old_root; // At first just copy children.

        uint32_t level = 0;
        uint32_t hash = kv->hash;
        Node* current = newroot;

        // Go all the way down once we find an empty node. Then assign it as the value.
        for(;true;level++)
        {
            // Calculate the array index for this level of hash and the corresponding bit pattern.
            uint32_t local_index = (hash >> (level*5)) & 0x1f; // |0-31|

            if(! current->index_in_use(local_index))
            {     
                // Assign new node positioned at index

                Node* newnode = new_node(); // This node will contain the value.
                newnode->type =  Node::ValueNode;
                newnode->value.keyvalue = kv;

                node_insert_replace_refarray(current, newnode, local_index, level);

                break; 
            }
            else
            {
                // Index was in use.

                size_t count = current->size();

                typename Node::Ref* child_array = ref_chunks_.reserve_consecutive_elements(count);
                typename Node::Ref* old_children = current->child_array;
                unsafe_copy(old_children, old_children + count, child_array);

                current->child_array = child_array;

                uint32_t array_index = current->index(1 << local_index); 

                Node* newnode = new_node();
                *newnode = *child_array[array_index].node;
                child_array[array_index].node = newnode;

                // No suitable slot found yet.
                if(level < 6)
                {
                    // Check if node value matches. If they do, replace current. 
                    const V* v = keyvalue_match_get(*newnode->value.keyvalue, kv->first, hash);
                    if(v)
                    {
                        // Replace matching key
                        newnode->value.keyvalue = kv;
                        break;
                    }

                    // If not matching key, just continue onwards.
                    current = newnode;
                }
                else
                {
                    // Ran out of level. Need a collision list or replace existing value.
                    // Need collision list.
                    if(newnode->type == Node::CollisionNode)
                    {
                        // Already have a list. First check if value exists, then just replace it.
                        // Otherwise append to list.

                        const KeyValueList* oldlist = newnode->value.collision_list;
                        typename KeyValueList::iterator i    = oldlist->begin();
                        typename KeyValueList::iterator end  = oldlist->end();

                        bool replace = false;
                        for(;i != end; ++i)
                        {
                            const KeyValue* ikv = *i;
                            const V* v = keyvalue_match_get(*ikv, kv->first, hash);
                            if(v)
                            {
                                replace = true;
                                break;
                            }
                        }

                        if(replace)
                        {
                            KeyValueList tmp = oldlist->remove(i);
                            *(newnode->value.collision_list) = tmp.add(kv);
                        }
                        else
                        {
                            *(newnode->value.collision_list) = oldlist->add(kv);
                        }
                    }
                    else
                    {
                        // Replace single value with a list or replace existing value.
                        const V* v = keyvalue_match_get(*newnode->value.keyvalue, kv->first, hash);
                        if(v)
                        {
                            // Replace matching key
                            newnode->value.keyvalue = kv;
                            break;
                        }
                        else
                        {
                            //Need collision list
                            newnode->type = Node::CollisionNode;
                            const KeyValue* current_kv = newnode->value.keyvalue;
                            newnode->value.collision_list = new KeyValueList(collided_list_pool_, 0);
                            *(newnode->value.collision_list) = collided_list_pool_.new_list(current_kv, kv);
                        }
                    }

                    break;
                }
            }
        }

        return newroot;
    }

    // TODO all absolutely non-member functions to static
    void recursive_mark(Node* node)
    {
        node_chunks_.set_marked_if_contained(node); 
        size_t size = node->size();
        if(size > 0)
        {
            ref_chunks_.set_marked_if_contained_array(node->child_array, size);
        }

        if(node->type == Node::ValueNode)
        {
            keyvalue_chunks_.set_marked_if_contained(node->value.keyvalue);
        }
        else if(node->type == Node::CollisionNode)
        {
            auto iter = node->value.collision_list->begin();
            auto end = node->value.collision_list->end();
            for(;iter != end; ++iter)
            {
                keyvalue_chunks_.set_marked_if_contained(*iter);
            }
        }

        for(auto ref = node->begin(); ref != node->end(); ++ref)
            recursive_mark(ref->node);

    };

    /** Mark all found entries when iterating from this node. Maximum child depth is
     * seven so stack should not be terribly wasted. */
    void mark_referenced(Node* node)
    {
        recursive_mark(node);
    }

    /** Garbage collection for map.*/
    void gc()
    {
        // Must cleanup keyvalues, nodes, refs and gc collided_list_pool_
        // Mark all empty

        // Visit all heads (iterate through map)
        // For each node: mark node, mark refs, go through all keyvalues and mark them
        // Then, collect all
        // Finally gc collided_list_pool_
        
        // TODO: Make gc more efficient.
        // i) collect all live references to an array and sort them.
        // ii) Sort all chunks in order of their first address
        // Now the two lists can be passed in linear fashion. 
        // Takes space (on 64 bit system) 8 * Chunk bytes + 8 * reference bytes.
        // i.e. for million references ~9MB.


        // Clean mark fields.
        keyvalue_chunks_.mark_all_empty();
        node_chunks_.mark_all_empty();
        ref_chunks_.mark_all_empty();

        // Then clean up unused references, visit all heads and mark visited nodes as active
        ref_count_ = copyif(ref_count_, [](const std::pair<Node*, int>& p){return p.second > 0;});

        // Go through each head.
        for(auto r = ref_count_.begin(); r!= ref_count_.end(); ++r) mark_referenced(r->first);

        // Lastly, collect unused slots.
        keyvalue_chunks_.collect_chunks();
        node_chunks_.collect_chunks();
        ref_chunks_.collect_chunks();

        // After all used slots are collected garbage collect collided list pool (unused heads
        // have been released in the possible destructors of the list heads).
        collided_list_pool_.gc();

    }

    /** Allocate new empty node.*/
    Node* new_node()
    {
        Node* n = node_chunks_.reserve_element();
        n->type = Node::EmptyNode;
        return n;
    }

    /** Allocate new node with one value.*/
#if 0 // TODO remove?
    Node* new_node(const KeyValue& v)
    {
        Node* n = node_chunks_.reserve_element();
        *kv = v;

        n->type = Node::ValueNode;
        n->value.keyvalue = kv;

        return n;
    }
#endif

    /** Return number of bytes used by the chunk pool in total. */
    size_t reserved_size_bytes()
    {
        size_t ref_map_size = ref_count_.size() *  sizeof(typename refcount_map::value_type);
        size_t total = sizeof(*this) + ref_map_size +  keyvalue_chunks_.reserved_size_bytes() + 
                       node_chunks_.reserved_size_bytes() +  ref_chunks_.reserved_size_bytes() + collided_list_pool_.reserved_size_bytes();
        return total;
    }

    size_t live_size_bytes()
    {
        size_t ref_map_size = ref_count_.size() * sizeof(typename refcount_map::value_type);
        size_t total = sizeof(*this) + ref_map_size +  keyvalue_chunks_.live_size_bytes() + 
                       node_chunks_.live_size_bytes() +  ref_chunks_.live_size_bytes() + collided_list_pool_.live_size_bytes();
        return total;
    }
private:
    keyvalue_chunk_box keyvalue_chunks_;
    node_chunk_box     node_chunks_;
    ref_chunk_box      ref_chunks_;
    KeyValueListPool   collided_list_pool_;
    refcount_map       ref_count_; // Store references to root nodes
};

#endif

#if 0
/** Generic collection printer */
template<class T>
std::ostream& operator<<(std::ostream& os, PListPool<T>::List& list)
{
    each_elem_to_os(os, list.begin(), list.end());
    return os;
}
#endif

} //namespace orb

