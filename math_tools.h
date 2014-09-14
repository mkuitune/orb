/** \file math_tools.h Usefull wrappers and tools for numbers.
    \author Mikko Kuitunen (mikko <dot> kuitunen <at> iki <dot> fi)
*/
#pragma once

#include<cstdint>
#include<string>
#include "tinymt32.h"
#include <list>
//
//// Disable Eigen's alignment and vectorization, leave it to compiler
//#define EIGEN_DONT_ALIGN 
//#include <Eigen/Dense>
//

////#include "tinymt64.h"
//#define _USE_MATH_DEFINES
//#include <cmath>
//#include <cstdint>
//#include <list>
//#include<array>
//#include<map>
//#include<cstdarg>
//#include<vector>
//#include<limits>
//
//#define PIf 3.141592846f
//

/////////////// Hash functions //////////////

uint32_t hash32(const char* data, int len);

uint32_t hash32(const std::string&  string);

template<class T>
uint32_t hash32(const T& hashable)
{
    int len = sizeof(T);
    return hash32((char*)&hashable, len);
}


///////////////// Bit operations //////////////

/** Count bits in field */
inline const uint32_t count_bits(uint32_t field)
{
    uint32_t count = 0;
    for(; field; count++) field &= field - 1;
    return count;
}

/** Return index of lowest unset bit*/
inline const uint32_t lowest_unset_bit(uint32_t field)
{
    if(field == 0xffffffff) return 32; //> not found
    uint32_t index = 0;
    for(; (field & 0x1); field >>= 1, index++){}

    return index;
}

/** Set nth bit in field. Return result. */
inline uint32_t set_bit_on(const uint32_t field, uint32_t index)
{
    return index < 32 ? (field | 1 << index) : field;
}

/** Unset nth bit in field. Return result. */
inline uint32_t set_bit_off(uint32_t field, uint32_t index)
{
    return index < 32 ? (field & ~(1 << index)) : field;
}

/** Return true if bit is on */
inline bool bit_is_on(const uint32_t field, const uint32_t bit)
{
    return  (field & 1 << bit) != 0;
}

///////////// Generators: Random number ///////////

#define GLH_RAND_SEED 7894321

// Minimal PRNG of Park & Miller
inline uint32_t minrandu(uint32_t a) {
    return ((uint64_t)a * 279470273UL) % 4294967291UL;
}
inline int32_t minrand(int32_t a) {
    return (int32_t) minrandu(*((uint32_t*)&a));
}

/** PRN generator. */
template<class T>
struct Random{
    Random();
    Random(T seed);
    T rand();
};

template<>
struct Random<int32_t>
{
    tinymt32_t state;
    void init(int seed)
    {
        tinymt32_init(&state, seed);
    }

    Random(int seed){init(seed);}
    Random(){init(GLH_RAND_SEED);}

    int32_t rand()
    {
        uint32_t result = tinymt32_generate_uint32(&state);
        return *((int32_t*) &result);
    }
};

template<>
struct Random<float>
{
    tinymt32_t state;
    void init(int seed)
    {
        tinymt32_init(&state, seed);
    }

    Random(int seed){init(seed);}
    Random(){init(GLH_RAND_SEED);}

    float rand(){return tinymt32_generate_float(&state);}
};

/** Random range generates numbers in inclusive range(start, end)*/
template<class T>
struct RandomRange
{
    RandomRange(T start, T end);
    T rand();
};

template<>
struct RandomRange<int32_t>
{
    Random<float> random;
    int32_t start; //> Inclusive range start.
    int32_t end; //> Inclusive range end.
    float offset;

    void init()
    {
        offset = (float) (end - start);
    }

    RandomRange(int32_t start_, int32_t end_):start(start_), end(end_){init();}
    RandomRange(int32_t start_, int32_t end_, int seed):random(seed),start(start_), end(end_){init();}

    int32_t rand()
    {
        const float f = random.rand();
        return start + (int32_t) (floor(offset * f + 0.5f));
    }
};

template<>
struct RandomRange<float>
{
    Random<float> random;
    float start; //> Inclusive range start.
    float end; //> Inclusive range end.
    float offset;

    void init(){offset = end - start;}

    RandomRange(float start_, float end_):start(start_), end(end_){init();}
    RandomRange(float start_, float end_, int seed):random(seed),start(start_), end(end_){init();}

    float rand()
    {
        return start + offset * tinymt32_generate_float(&random.state);
    }
};

//////////////////// Combinatorial stuff /////////////////////
//
///** From list of elements {a} return all the pairs generated from the sequence {a_i * a_j} */
template<class V>
std::list<std::pair<typename V::value_type, typename V::value_type>> all_pairs(V& seq)
{
    typedef std::pair<typename V::value_type, typename V::value_type> pair_elem;
    std::list<pair_elem> result;
    for(auto i = seq.begin(); i != seq.end(); ++i)
    {
        for(auto j = seq.begin(); j != seq.end(); ++j)
        {
            result.push_back(pair_elem(*i,*j));
        }
    }

    return result;
}

////////////////////// Generators ///////////////////////////
//

template<class T>
T from_int(int in){ return (T) in; }
/** Generator for a start-inclusive an end-exclusive range := (start, end].*/
template<class T> class Range {
public:



    struct iterator {
        T current_;
        T increment_;
        bool increasing_;

        // A bit abusive for boolean operations but semantically conforms
        // to common iterator usage where != signals wether to continue iteration
        // or not.
        bool operator!=(const iterator& i){
            return increasing_ ? current_ <  i.current_ : current_ >  i.current_ ;
        }

        const T& operator*(){return current_;}

        void operator++(){current_ += increment_;}

        iterator(T val, T increment):current_(val), increment_(increment){
            increasing_ = increment_ > from_int<T>(0);
        }
    };

    T range_start_;
    T range_end_;
    T increment_;

    Range(T range_start, T range_end):
        range_start_(range_start), range_end_(range_end), increment_(from_int<T>(1)){}

    Range(T range_start, T increment, T range_end):
        range_start_(range_start), range_end_(range_end), increment_(increment)
    {
        if((range_end < range_start) && increment_ > from_int<T>(0))
            increment_ *= from_int<T>(-1);
    }

    iterator begin() const {return iterator(range_start_, increment_);}
    iterator end() const {return iterator(range_end_, increment_);}
};

template<class T>
Range<T> make_range(T begin, T end){return Range<T>(begin, end);}



//
//////////////// Numeric conversions /////////////
//
//template<class R, class P>
//R to_number(P in){return (R) in;}
//
//struct Dim{
//    enum coord_t{X = 0, Y = 1, Z = 2};
//    enum scale_t{SX = 0, SY = 1, SZ = 2};
//    enum euler_t{RX = 0, RY = 1, RZ = 2};
//};
//
//namespace orb{
//
///////////////// Types //////////////
//
//
//typedef Eigen::Vector4f vec4;
//typedef Eigen::Vector4i vec4i;
//typedef Eigen::Vector3f vec3;
//typedef Eigen::Vector2f vec2;
//typedef Eigen::Vector2i vec2i;
//
//typedef Eigen::Matrix4f mat4;
//
//typedef Eigen::Quaternion<float>  quaternion;
//typedef Eigen::Quaternion<double> quaterniond;
//
//typedef Eigen::Transform<float, 3, Eigen::Affine> transform3;
//
//struct Color{
//    float r;
//    float g;
//    float b;
//    float a;
//
//    Color():r(0.f),g(0.f), b(0.f),a(0.f){}
//    Color(float rr, float gg, float bb, float aa):r(rr), g(gg), b(bb), a(aa){}
//    Color(const vec4& v):r(v[0]), g(v[1]), b(v[2]), a(v[3]){}
//};
//
//
//template<class T>
//class Math
//{
//public:
//
//    typedef Eigen::Matrix<T, 4, 4> matrix4_t;
//    typedef Eigen::Matrix<T, 3, 1> vec3_t;
//    typedef Eigen::Quaternion<T>   quaternion_t;
//
//    class span_t {
//        T data[2];
//    public:
//        span_t(const T& first, const T& second){data[0] = first; data[1] = second;}
//        T& operator[](size_t ind){return data[ind];}
//        const T& operator[](size_t ind) const {return data[ind];}
//    };
//
//    static T max(){return std::numeric_limits<T>::max();}
//    static T min(){return std::numeric_limits<T>::min();}
//
//    static span_t null_span(){return span_t(T(0), T(0));}
//
//    template<class NUM>
//    static T to_type(const NUM& n){return static_cast<T>(n);}
//};
//
//inline uint8_t to_ubyte(const float f){return static_cast<uint8_t>(f);}
//inline float   to_float(const uint8_t u){return static_cast<float>(u);}
//inline float   to_float(const double d){return static_cast<float>(d);}
//inline int     to_int(const double d){return static_cast<int>(d);}
//
///** Approximate comparison of floats. */
//inline bool are_near(const float first, const float second, const float epsilon = 0.001f){
//    return fabsf((first - second)) < epsilon;
//}
//
///** Convenience class to store vectors as some types such as Eigen::Vector4f have strict alignment requirements. */
//template<class T, int N>
//struct ArrayN{
//
//    typedef Eigen::Matrix<T, N, 1> complement_t;
//
//    T data_[N];
//
//    ArrayN(T x){
//        set_zero();
//        data_[0] = x;}
//    ArrayN(T x, T y){
//        set_zero();
//        data_[0] = x; data_[1] = y;}
//    ArrayN(T x, T y, T z){
//        set_zero();
//        data_[0] = x; data_[1] = y; data_[2] = z;}
//    ArrayN(T x, T y, T z, T w){
//        set_zero();
//        data_[0] = x; data_[1] = y; data_[2] = z; data_[3] = w;}
//    ArrayN(){set_zero();}
//    ArrayN(const complement_t& in){
//        for(int i = 0; i < N; ++i) data_[i] = in[i];}
//    ArrayN(const ArrayN& in){
//        for(int i = 0; i < N; ++i) data_[i] = in[i];}
//    ArrayN(const T* data){
//        for(int i = 0; i < N; ++i) data_[i] = data[i];}
//
//    void set_zero(){memset(data_, sizeof(data_), 0);}
//
//    T& operator[](size_t i){return data_[i];}
//    const T& operator[](size_t i) const {return data_[i];}
//
//    ArrayN& operator=(const complement_t& in){
//        for(int i = 0; i < N; ++i) data_[i] = in[i];
//        return *this;}
//
//    ArrayN& operator=(const ArrayN& rhs){
//        for(int i = 0; i < N; ++i) data_[i] = rhs[i];
//        return *this;}
//
//    void fill(const T& val){
//        for(int i = 0; i < N; ++i) data_[i] = val;}
//
//    complement_t to_vec() const{
//        complement_t comp;
//        for(int i = 0; i < N; ++i) comp[i] = data_[i];
//        return comp;}
//
//    template<int M>
//    Eigen::Matrix<T, M, 1> change_dim() const {
//        static_assert(M <= N, "Only ouput of equal or reduced dimension supported");
//        Eigen::Matrix<T, M, 1> output;
//        for(int i = 0; i < M; i++) output[i] = data_[i];
//        return output;
//    }
//
//};
//
//typedef ArrayN<float, 4> array4;
//
//////////////////// Mappings /////////////////////
//
///** Given value within [begin, end], map it's position to range [0,1]. TODO:Make numerically more robust if used in critical sections. */
//template<class T> inline T interval_range(const T value, const T begin, const T end){ return (value - begin) / (end - begin); }
//
///** Constrain given value within [begin, end]. If it's outside these ranges then truncate to closest extrema. */
//template<class T> inline T constrain(const T value, const T begin, const T end){
//    return value > end ? end : (value < begin ? begin : value);
//}
//
//template<class T> inline bool in_range_inclusive(const T value, const T begin, const T end){
//    return (value >= begin) && (value <= end);
//}
//
///** Perform a numerically stable Kahan summation on the input numbers. */
//template<class T> inline T kahan_sum(const T& numbers){
//    typedef numbers::value_type t;
//    t sum = (t) 0;
//    t c = (t) 0;
//    for(auto& n : numbers){
//        t y = n - c;
//        t tally = sum + y;
//        c = (tally - sum) - y;
//        sum = tally;
//    }
//    return sum;
//}
//
//template<class T, int N> inline T kahan_average(const std::array<T, N>& numbers){
//    T mul = (T) 1.0 / N;
//    return mul * kahan_sum(numbers);
//}
//
//template<class T> inline T average(const T& fst, const T& snd){
//    return ((T) 0.5) * (fst + snd);
//}
//
///** Smoothstep polynomial between [0,1] range. */
//inline float smoothstep(const float x){ return (1.0f - 2.0f*(-1.0f + x))* x * x; }
//
//inline double smoothstep(const double x){ return (1.0 - 2.0*(-1.0 + x))* x * x; }
//
///** Linear interpolation between [a,b] range (range of x goes from 0 to 1). */
//template<class I, class G> inline G lerp(const I x, const G& a, const G& b){ return a + x * (b - a); }
//

//
////////////////////// Linear algebra etc. ///////////////////////////
//
//template<class T, int N, int M>
//Eigen::Matrix<T, N, M> component_wise_product(const Eigen::Matrix<T, N, M>& first, const Eigen::Matrix<T, N, M>& second)
//{
//    Eigen::Matrix<T, N, M> res = Eigen::Matrix<T, N, M>::Zero();
//    for(int j = 0; j < M; ++j) for(int i = 0; i < N; i++) res[i][j] = first[i][j] * second[i][j];
//    return res;
//}
//
//template<class T, int N>
//Eigen::Matrix<T, N+1, 1> increase_dim(const Eigen::Matrix<T, N, 1>& v){
//    Eigen::Matrix<T, N+1, 1> out;
//    for(int i = 0; i < N; ++i){out[i] = v[i];}
//    out[N] = Math<T>::to_type(1);
//    return out;
//}
//
//template<class T, int N>
//Eigen::Matrix<T, N + 1, 1> increase_dim(const Eigen::Matrix<T, N, 1>& v, const T& default_value){
//    Eigen::Matrix<T, N + 1, 1> out;
//    for(int i = 0; i < N; ++i){ out[i] = v[i]; }
//    out[N] = default_value;
//    return out;
//}
//
//template<class T, int N>
//Eigen::Matrix<T, N-1, 1> decrease_dim(const Eigen::Matrix<T, N, 1>& v){
//    Eigen::Matrix<T, N-1, 1> out;
//    for(int i = 0; i < N - 1; ++i){out[i] = v[i];}
//    return out;
//}
//
//
///** Calculate transform matrix m := m_loc * m_rot * m_scale */
//template<class T>
//Eigen::Transform<T,3, Eigen::Affine> generate_transform(Eigen::Matrix<T, 3, 1>& loc, Eigen::Quaternion<T>& rot, Eigen::Matrix<T, 3, 1>& scale)
//{
//    Eigen::Translation<T, 3> translation(loc);
//    Eigen::Transform<T, 3, Eigen::Affine> t = translation * rot * Eigen::Scaling(scale);
//    return t;
//}
//
//template<class T>
//struct ExplicitTransform{
//    // TODO FIXME: Store rotations as euler angles
//
//    typename Math<T>::vec3_t       position_;
//    typename Math<T>::vec3_t       scale_;
//    typename Math<T>::quaternion_t rotation_;
//
//    ExplicitTransform(){initialize();}
//
//    static ExplicitTransform ExplicitTransform::position(typename const Math<T>::vec3_t& pos){
//        Transform t;
//        t.position_ = pos;
//        return t;
//    }
//
//    void initialize(){
//        position_ = Math<T>::vec3_t(0.f, 0.f, 0.f);
//        scale_    = Math<T>::vec3_t(1.f, 1.f, 1.f);
//        rotation_ = Math<T>::quaternion_t(1.f, 0.f, 0.f, 0.f);
//    }
//
//    void add_to_each_dim(const ExplicitTransform& e){
//        // TODO FIXME: Store rotations as euler angles
//        position_ += e.position_;
//        scale_ += e.scale_;
//        rotation_ =  e.rotation_ * rotation_;
//        rotation_.normalize();
//    }
//
//    typename Math<T>::matrix4_t matrix(){return generate_transform<T>(position_, rotation_, scale_).matrix();}
//};
//
////////////////////// Geometric entities ///////////////////////////
//
///** Edge. */
//template<class T, int N>
//struct Edge{
//    typedef Eigen::Matrix<T, N, 1> vec_t;
//
//    vec_t first_;
//    vec_t second_;
//
//    Edge(const vec_t& first, const vec_t& second):first_(first), second_(second){}
//
//};
//
//typedef Edge<float, 2> Edge2;
//
///** All points in box are min + s * (max - min) where 0 <= s <= 1*/
//template<class T, int N>
//struct Box
//{
//    typedef Eigen::Matrix<T, N, 1>   vec_t;
//    typedef Eigen::Matrix<T, N+1, 1> vec_p;
//
//    vec_t min_;
//    vec_t max_;
//
//    Box(const vec_t& min, const vec_t& max):min_(min), max_(max){}
//    Box(){
//        for(int i = 0; i < N; ++i){
//            min_[i] = Math<T>::to_type(0);
//            max_[i] = Math<T>::to_type(0);
//        }
//    }
//
//    vec_t size() const {return max_ - min_;}
//
//    /** In 2D returns area, in 3D volume, etc. */
//    T measure() const {
//        vec_t s = size();
//        T res = Math<T>::to_type(1);
//        for(int i = 0; i < N; ++i) res *= s[i];
//        return res;
//    }
//};
//
//typedef Box<int, 2>   Box2i;
//typedef Box<float, 2> Box2;
//typedef Box<float, 3> Box3;
//
//template<class T>
//Box<T, 2> make_box2(T xlow, T ylow, T xhigh, T yhigh){return Box<T, 2>(Box<T, 2>::vec_t(xlow, ylow), Box<T, 2>::vec_t(xhigh, yhigh));}
//
///** Return this boxes bounds transformed by the given N+1 dimensional matrix as a new box. */
//template<class T, int N, class M>
//Box<T,N> transform_box(M& tr, const Box<T,N>& box){
//    Box<T,N>::vec_p minp = increase_dim(box.min_);
//    Box<T,N>::vec_p maxp = increase_dim(box.max_);
//
//    minp = tr * minp;
//    maxp = tr * maxp;
//
//    Box<T,N>::vec_t new_min = decrease_dim(minp);
//    Box<T,N>::vec_t new_max = decrease_dim(maxp);
//
//    return Box<T,N>(new_min, new_max);
//}
//
///** If s is in range (a,b) return true. */
//template<class T>
//inline bool in_range(const T& a, const T& b, const T& s)
//{
//    return (s >= a) && (s <= b);
//}
//
///* check intersections of spans (a[0],a[1]) and (b[0],b[1])
// * */
//
//template<class T>
//typename Math<T>::span_t intersect_spans(const typename Math<T>::span_t& a, const typename Math<T>::span_t& b)
//{
//    typedef typename Math<T>::span_t span_t;
//    span_t span(0,0); // This is the null-span
//
//    if(b[0] < a[0])
//    {
//        if(a[1] < b[1]) span = span_t(a[0], a[1]);
//        else
//        {
//            if(a[0] < b[1]) span = span_t(a[0],b[1]);
//        }
//    }
//    else
//    {
//        if(b[1] < a[1]) span = span_t(b[0], b[1]);
//        else
//        {
//            if(b[0] < a[1]) span = span_t(b[0], a[1]);
//        }
//    }
//
//    return span;
//}
//
///* Return span that covers both spans (a[0],a[1]) and (b[0],b[1])
// * */
//template<class T>
//typename Math<T>::span_t cover_spans(const typename Math<T>::span_t& a, const typename Math<T>::span_t& b)
//{
//    T lower  = std::min(a[0], b[0]);
//    T higher = std::max(a[1], b[1]);
//
//    Math<T>::span_t covering_span(lower, higher);
//
//    return covering_span;
//}
//
//template<class T>
//bool span_is_empty(const typename Math<T>::span_t& span){return span[0] >= span[1];}
//
///**
// * @return (box, boxHasVolume) - in case of n = 2 this means the area and whether it is larger than 0.
// */
//template<class T, int N>
//std::tuple<Box<T,N>, bool> intersect(const Box<T,N>& a, const Box<T,N>& b)
//{
//    typedef typename Math<T>::span_t span_t;
//
//    bool has_volume = true;
//    Box<T,N> box;
//
//    for(int i = 0; i < N; ++i)
//    {
//        span_t spanA(a.min_[i], a.max_[i]);
//        span_t spanB(b.min_[i], b.max_[i]);
//
//        span_t span = intersect_spans<T>(spanA, spanB);
//
//        if(span_is_empty<T>(span)) has_volume = false;
//
//        box.min_[i] = span[0];
//        box.max_[i] = span[1];
//
//    }
//
//    return std::make_tuple(box, has_volume);
//}
//
///**
// * @return box that is the smallest box containing a and b.
// */
//template<class T, int N>
//Box<T,N> cover(const Box<T,N>& a, const Box<T,N>& b)
//{
//    typedef typename Math<T>::span_t span_t;
//
//    Box<T,N> box;
//
//    for(int i = 0; i < N; ++i)
//    {
//        span_t spanA(a.min_[i], a.max_[i]);
//        span_t spanB(b.min_[i], b.max_[i]);
//
//        span_t span = cover_spans<T>(spanA, spanB);
//
//        box.min_[i] = span[0];
//        box.max_[i] = span[1];
//    }
//
//    return box;
//}
//
///** Return the bounding box for the input vertices. */
//template<class T, int N>
//Box<T, N> cover_points(const std::vector<Eigen::Matrix<T, N, 1>>& points){
//    Eigen::Matrix<T, N, 1> min = points[0];
//    Eigen::Matrix<T, N, 1> max = points[0];
//    for(auto&p : points){
//        for(int i = 0; i < N; ++i){
//            min[i] = std::min(min[i], p[i]);
//            max[i] = std::max(max[i], p[i]);
//        }
//    }
//    return Box<T, N>(min, max);
//}
//
///** @return Returns true if point is inside box. */
//template<class T, int N>
//bool inside(const Box<T, N>& box, const Eigen::Matrix<T, N, 1>& point){
//    bool is_inside = true;
//    for(int i = 0; i < N; ++i)
//        is_inside &= in_range_inclusive(point[i], box.min_[i], box.max_[i]);
//    
//    return is_inside;
//}
//
///** @return Returns true if edge is inside box. */
//template<class T, int N>
//bool inside(const Box<T, N>& box, const Edge<T, N>& edge){
//    bool is_inside = true;
//    for(int i = 0; i < N; ++i)
//        is_inside &= in_range_inclusive(edge.first_[i], box.min_[i], box.max_[i]) && 
//                     in_range_inclusive(edge.second_[i], box.min_[i], box.max_[i]);
//
//    return is_inside;
//}
//
///** @return Returns true if box is inside outer box. */
//template<class T, int N>
//bool inside(const Box<T, N>& outer_box, const Box<T, N>& inner_box){
//    return inside(outer_box, inner_box.min_) && inside(outer_box, inner_box.max_);
//}
//
//


//
//// Interpolators
//template<class I, class G> class Lerp { public: 
//    static G interpolate(const I x, const G& a, const G& b){
//        return lerp(x,a,b);
//    }
//};
//// Interpolators
//template<class I, class G> class Smoothstep { public: 
//    static G interpolate(const I x, const G& a, const G& b){
//        return lerp(smoothstep(x),a,b);
//    }
//};
//
//// Interpolating map, useful for e.g. color gradients
//template<class Key, class Value, class Interp = Lerp<Key, Value>>
//class InterpolatingMap{
//public:
//    typedef Key                 key_t;
//    typedef Value               value_t;
//    typedef std::map<Key,Value> map_t;
//    
//
//    map_t map_;
//
//    void insert(const Key& key, const Value& value){
//        map_[key] = value;
//    }
//
//    Value interpolate(const Key& key) const {
//        // Three possibilities: key in [min_key, max_key], key < min_key, key > max_key
//
//        map_t::const_iterator upper = map_.lower_bound(key); // first equivalent or larger than key
//                                                             // if not found key is largest
//                                                             // if is begin then key is smallest
//        map_t::const_iterator end   = map_.end();
//        map_t::const_iterator begin   = map_.begin();
//
//        if(upper == end) {
//            // No elements after key
//            return map_.rbegin()->second;
//        } else if(upper == begin) {
//            // No elements before key 
//            return map_.begin()->second;
//        } else {
//            map_t::const_iterator lower = upper;
//            lower--;
//            const Value low(lower->second);
//            const Value high(upper->second);
//            Key interp = interval_range(key, lower->first, upper->first);
//            return Interp::interpolate(interp, low, high);
//        }
//    }
//};
//
//class InterpolationType{ public:
//    enum t{Nearest, Linear};
//};
//
///** 1D array of samples that can be accessed on the span [0,1] with chosen interpolation between samples. */
//template<class Value, class Interp = Lerp<double, Value>> class Sampler1D {
//public:
//    InterpolationType::t interpolation_technique_;
//    int                  sample_count_;
//    std::vector<Value>   samples_;
//
//    Sampler1D(int sample_count):sample_count_(sample_count), samples_(sample_count_),
//        interpolation_technique_(InterpolationType::Linear){}
//
//    Sampler1D():interpolation_technique_(InterpolationType::Linear), sample_count_(0){}
//
//    Sampler1D(Sampler1D&& sampler){
//        interpolation_technique_ = sampler.interpolation_technique_;
//        sample_count_            = sampler.sample_count_;
//        samples_                 = std::move(sampler.samples_);
//    }
//
//    void technique(InterpolationType::t t) {interpolation_technique_ = t;}
//
//    Value get(double key){
//        key = key > 1.0 ? 1.0 : ( key < 0.0 ? 0.0 : key);
//
//        double sample_coord = key * sample_count_;
//        double first_sample = floor(sample_coord);
//        double dx           = sample_coord - first_sample;
//        int    first        = to_int(sample_coord);
//
//        return (interpolation_technique_ == InterpolationType::Linear) ? 
//                    lerp(dx, samples_[first], samples_[first + 1]) :
//                    (dx > 0.5 ? samples_[first + 1] : samples_[first]);
//    }
//
//    Value get(float key){
//        key = key > 1.0f ? 1.0f : ( key < 0.0f ? 0.0f : key);
//
//        float sample_coord = key * sample_count_;
//        float first_sample = floorf(sample_coord);
//        float dx           = sample_coord - first_sample;
//        int   first        = to_int(sample_coord);
//
//        return (interpolation_technique_ == InterpolationType::Linear) ? 
//                    lerp(dx, samples_[first], samples_[first + 1]) :
//                    (dx > 0.5 ? samples_[first + 1] : samples_[first]);
//    }
//};
//
//template<class Key, class Value, class Interp>
//Sampler1D<Value> sample_interpolating_map(const InterpolatingMap<Key, Value, Interp>& map, const int sample_count)
//{
//    Sampler1D<Value> sampler(sample_count);
//
//    double value = 0.0;
//    double dv    = 1.0 / (sample_count - 1);
//
//    for(auto& s : sampler.samples_) {
//        s = map.interpolate(Math<Key>::to_type(value));
//        value += dv;
//    }
//    return sampler;
//}
//
//// TODO: Piecewise curve? Own section for polynomes, splines and piecewise curves?
//
////template<class T >
////class PiecewiseCurve{
////public:
////    std::vector<>
////};
//

//
//} // namespace orb
//
