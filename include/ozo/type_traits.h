#pragma once

#include <ozo/detail/pg_type.h>
#include <ozo/detail/float.h>
#include <ozo/detail/strong_typedef.h>

#include <libpq-fe.h>

#include <boost/hana/at_key.hpp>
#include <boost/hana/insert.hpp>
#include <boost/hana/string.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/type.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/optional.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/weak_ptr.hpp>

// Fusion adaptors support
#include <boost/fusion/support/is_sequence.hpp>
#include <boost/fusion/include/is_sequence.hpp>

#include <ozo/concept.h>
#include <ozo/optional.h>
#include <memory>
#include <string>
#include <list>
#include <vector>
#include <set>
#include <type_traits>

/**
 * @defgroup group-type_system Type system
 * @brief Database-related type system of the library.
 */

/**
 * @defgroup group-type_system-types Types
 * @ingroup group-type_system
 * @brief Database-related type system types.
 */

/**
 * @defgroup group-type_system-concepts Concepts
 * @ingroup group-type_system
 * @brief Database-related type system concepts.
 */

/**
 * @defgroup group-type_system-constants Constants
 * @ingroup group-type_system
 * @brief Database-related type system constants.
 */

/**
 * @defgroup group-type_system-functions Functions
 * @ingroup group-type_system
 * @brief Database-related type system functions.
 */

/**
 * @defgroup group-type_system-mapping Mapping
 * @ingroup group-type_system
 * @brief Types mapping C++ to PostgreSQL
 */
namespace ozo {

namespace hana = boost::hana;
using namespace hana::literals;

namespace fusion = boost::fusion;
/**
 * @brief PostgreSQL OID type - object identifier
 * @ingroup group-type_system-types
 */
using oid_t = ::Oid;

/**
 * @brief OID constant type.
 *
 * This type is like `std::bool_constant` - used to enable type level
 * resolving of constants. All `BuiltIn` types uses this template
 * specialization for their OID constants.
 *
 * @ingroup group-type_system-types
 * @tparam Oid --- OID value
 */
template <oid_t Oid>
struct oid_constant : std::integral_constant<oid_t, Oid> {};

/**
 * @brief Type for non initialized OID
 * @ingroup group-type_system-types
 */
using null_oid_t = oid_constant<0>;

/**
 * @brief Constant for empty OID
 * @ingroup group-type_system-constants
 */
constexpr null_oid_t null_oid;

template <typename T, typename Enable = void>
struct is_nullable : std::false_type {};

template <typename T>
struct is_nullable<boost::optional<T>> : std::true_type {};

#ifdef __OZO_STD_OPTIONAL
template <typename T>
struct is_nullable<__OZO_STD_OPTIONAL<T>> : std::true_type {};
#endif

template <typename T>
struct is_nullable<boost::scoped_ptr<T>> : std::true_type {};
template <typename T, typename Deleter>
struct is_nullable<std::unique_ptr<T, Deleter>> : std::true_type {};
template <typename T>
struct is_nullable<boost::shared_ptr<T>> : std::true_type {};
template <typename T>
struct is_nullable<std::shared_ptr<T>> : std::true_type {};
template <typename T>
struct is_nullable<boost::weak_ptr<T>> : std::true_type {};
template <typename T>
struct is_nullable<std::weak_ptr<T>> : std::true_type {};

/**
 * @brief Indicates if type meets nullable requirements.
 *
 * `Nullable` type has to:
 * * have a null-state,
 * * be `bool` convertable - `false` indicates null state,
 * * be dereferenceable via `operator *`,
 * * have `allocate_nullable()` specialization (see the function documentation for how to).
 *
 * These next types are `Nullable` out of the box:
 * * `boost::optional`,
 * * `std::optional`,
 * * `boost::scoped_ptr`,
 * * `std::unique_ptr`,
 * * `boost::shared_ptr`,
 * * `std::shared_ptr`,
 * * `boost::weak_ptr`,
 * * `std::weak_ptr`.
 *
 * @note Raw pointers are not supported as `Nullable` by default, because this library uses RAII model for objects and
 * no special deallocation functions are used. But if it is really needed and you want to deallocate the allocated
 * objects manually you can add them as described below.
 *
 * ### Example
 *
 * If you want to add nullable type to the library --- you have to specialize `ozo::is_nullable` struct and specify allocation function like this:
 *
 * @code
//Add raw pointer to nullables

namespace ozo {

// Mark raw pointer as nullable
template <typename T>
struct is_nullable<T*> : std::true_type {};

// Specify allocation function
template <typename T>
struct allocate_nullable_impl<T*> {
    template <typename Alloc>
    static void apply(T*& out, const Alloc&) {
        out = new T();
    }
};

}

 * @endcode
 *
 * @sa ozo::unwrap_nullable(), is_null(), allocate_nullable(), init_nullable(), reset_nullable()
 * @ingroup group-core-concepts
 * @hideinitializer
 */
template <typename T>
constexpr auto Nullable = is_nullable<std::decay_t<T>>::value;

template <typename T, typename = std::void_t<>>
struct unwrap_nullable_impl {
    template <typename T1>
    constexpr static decltype(auto) apply(T1&& v) noexcept(noexcept(*v)) {
        return *v;
    }
};

/**
 * @brief Dereference #Nullable argument or forward it
 *
 * This utility function gets value from #Nullable, and pass object as it is if it is not #Nullable
 *
 * @param value --- object to unwrap
 * @return dereferenced argument in case of #Nullable, pass as is if it is not.
 * @ingroup group-core-functions
 */
template <typename T>
constexpr decltype(auto) unwrap_nullable(T&& v) noexcept(
        noexcept(unwrap_nullable_impl<std::decay_t<T>>::apply(std::forward<T>(v)))) {
    return unwrap_nullable_impl<std::decay_t<T>>::apply(std::forward<T>(v));
}

template <typename T>
struct unwrap_nullable_impl<T, Require<!Nullable<T>>> {
    template <typename T1>
    constexpr static decltype(auto) apply(T1&& v) noexcept {
        return std::forward<T1>(v);
    }
};

template <typename T>
struct unwrap_nullable_impl<std::weak_ptr<T>> {
    template <typename T1>
    constexpr static decltype(auto) apply(T1&& v) noexcept(noexcept(*v.lock())) {
        return *v.lock();
    }
};

template <typename T>
struct unwrap_nullable_impl<boost::weak_ptr<T>> {
    template <typename T1>
    constexpr static decltype(auto) apply(T1&& v) noexcept(noexcept(*v.lock())) {
        return *v.lock();
    }
};

template <typename T>
inline auto is_null(const T& v) noexcept -> Require<Nullable<T>, bool> {
    return !v;
}

template <typename T>
inline auto is_null(const boost::weak_ptr<T>& v) noexcept {
    return is_null(v.lock());
}

template <typename T>
inline auto is_null(const std::weak_ptr<T>& v) noexcept {
    return is_null(v.lock());
}

#ifdef OZO_DOCUMENTATION
/**
 * @brief Indicates if value is in null state
 *
 * This utility function indicates if given value is in null state.
 * If argument is not #Nullable it always returns `false`
 *
 * @param value --- object to examine
 * @return `true` --- object is in null-state,
 * @return `false` --- overwise.
 * @ingroup group-core-functions
 */
template <typename T>
constexpr auto is_null(const T&) noexcept;
#endif

template <typename T>
constexpr auto is_null(const T&) noexcept -> Require<!Nullable<T>, std::false_type> {
    return {};
}

template <typename T, typename = std::void_t<>>
struct allocate_nullable_impl {
    static_assert(Emplaceable<T>, "default implementation uses emplace() method");
    template <typename Alloc>
    static void apply(T& out, const Alloc&) {
        out.emplace();
    }
};

template <typename T>
struct allocate_nullable_impl<std::unique_ptr<T>> {
    template <typename Alloc>
    static void apply(std::unique_ptr<T>& out, const Alloc&) {
        out = std::make_unique<T>();
    }
};

template <typename T>
struct allocate_nullable_impl<boost::scoped_ptr<T>> {
    template <typename Alloc>
    static void apply(boost::scoped_ptr<T>& out, const Alloc&) {
        out.reset(new T{});
    }
};

template <typename T>
struct allocate_nullable_impl<boost::shared_ptr<T>> {
    template <typename Alloc>
    static void apply(boost::shared_ptr<T>& out, const Alloc& a) {
        out = boost::allocate_shared<T, Alloc>(a);
    }
};

template <typename T>
struct allocate_nullable_impl<std::shared_ptr<T>> {
    template <typename Alloc>
    static void apply(std::shared_ptr<T>& out, const Alloc& a) {
        out = std::allocate_shared<T, Alloc>(a);
    }
};

template <typename T, typename Alloc>
inline void allocate_nullable(T& out, const Alloc& a) {
    static_assert(Nullable<T>, "T must be nullable");
    allocate_nullable_impl<std::decay_t<T>>::apply(out, a);
}

template <typename T, typename Alloc = std::allocator<char>>
inline void init_nullable(T& n, const Alloc& a = Alloc{}) {
    static_assert(Nullable<T>, "T must be nullable");
    if (is_null(n)) {
        allocate_nullable(n, a);
    }
}

template <typename T>
inline void reset_nullable(T& n) {
    static_assert(Nullable<T>, "T must be nullable");
    n = T{};
}

/**
 * @brief Type traits template forward declaration.
 * @ingroup group-type_system-types
 *
 * Type traits contains information
 * related to it's representation in the database. There are two different
 * kind of traits - built-in types with constant OIDs and custom types with
 * database depent OIDs. The functions below describe neccesary traits.
 * For built-in types traits will be defined there. For custom types user
 * must define traits.
 *
 * @tparam T --- type to examine
 * @sa `ozo::type_traits_helper`
 */
#ifdef OZO_DOCUMENTATION
template <typename T>
struct type_traits {
    using name = <implementation defined>; //!< `boost::hana::string` with name of the type in a database
    using oid = <implementation defined>; //!< `ozo::oid_constant` with Oid of the built-in type or null_oid_t for non built-in type
    using size = <implementation defined>; //!< `std::integral_constant` with size of the type object in bytes or `ozo::dynamic_size` in other case
};
#else

template <typename T, typename = std::void_t<>>
struct type_traits_default {};

template <typename T>
struct type_traits : type_traits_default<T> {};

template <typename T>
struct type_traits_default<T, Require<Nullable<T>>>
    : type_traits<unwrap_nullable_type<T>> {};
#endif
/**
* Helpers to make size trait constant
*     bytes - makes fixed size trait
*     dynamic_size - makes dynamic size trait
*/
template <std::size_t n>
using bytes = std::integral_constant<std::size_t, n>;
using dynamic_size = void;

template <typename T, typename Size>
struct type_size_match : std::bool_constant<static_cast<size_type>(sizeof(T)) == Size::value> {};

template <typename T>
struct type_size_match<T, dynamic_size> : std::true_type {};
/**
 * @brief Helper for the type traits definitions.
 * @ingroup group-type_system-types
 *
 * @tparam Name --- type which can be converted into a string representation
 * which contain the fully qualified type name in DB
 * @tparam Oid --- `ozo::oid_constant`-based oid type - provides type's oid in database, may be defined for
 * built-in types only; custom types have only dynamic oid, depended
 * from the current state of DB.
 * @tparam Size --- size type - provides information about type's object size, may
 *         be specified only if the type's object has fixed size.
 */
template <typename T, typename Name, typename Oid = null_oid_t, typename Size = dynamic_size>
struct type_traits_helper {
    using name = Name;
    using oid = Oid;
    using size = Size;
    static_assert(type_size_match<T, size>::value,
        "type size does not match to declared size");
};

template <typename T, typename = std::void_t<>>
struct has_type_traits : std::false_type {};

template <typename T>
struct has_type_traits<T, std::void_t<
    typename type_traits<T>::name,
    typename type_traits<T>::oid,
    typename type_traits<T>::size
>> : std::true_type {};

/**
 * @brief Condition indicates if type has corresponding type traits for PostrgreSQL
 *
 * @tparam T --- type to examine
 * @ingroup group-core-concepts
 * @hideinitializer
 */
template <typename T>
constexpr auto HasTypeTraits = has_type_traits<std::decay_t<T>>::value;

template <typename T, typename = std::void_t<>>
struct is_built_in : std::false_type {};

template <typename T>
struct is_built_in<T, std::enable_if_t<
    !std::is_same_v<typename type_traits<T>::oid, null_oid_t>>
> : std::true_type {};

/**
 * @brief Condition indicates if the specified type is built-in for PG
 * @ingroup group-type_system-concepts
 * @tparam T --- type to check
 * @hideinitializer
 */
template <typename T>
constexpr auto BuiltIn = is_built_in<std::decay_t<T>>::value;

template <typename T>
struct is_dynamic_size : std::is_same<typename type_traits<T>::size, dynamic_size> {};

template <typename T>
constexpr auto DynamicSize = is_dynamic_size<std::decay_t<T>>::value;

template <typename T>
constexpr auto StaticSize = !DynamicSize<T>;

/**
* @brief Function returns type name in Postgre SQL.
* @tparam T --- type
* @return `boost::hana::string` --- name of the type
* @ingroup group-type_system-functions
*/
template <typename T>
constexpr auto type_name() noexcept {
    static_assert(!std::is_void<typename type_traits<T>::name>::value,
        "no type_traits found for the type");
    constexpr auto name = typename type_traits<T>::name{};
    constexpr char const* retval = hana::to<char const*>(name);
    return retval;
}

/**
* @brief Function returns type name in Postgre SQL.
* @param T --- type
* @return `boost::hana::string` --- name of the type
* @ingroup group-type_system-functions
*/
template <typename T>
constexpr auto type_name(const T&) noexcept {return type_name<T>();}

/**
* Function returns object size.
*/
template <typename T>
constexpr auto size_of(T&&) noexcept  -> typename std::enable_if<
        !is_dynamic_size<std::decay_t<T>>::value,
        typename type_traits<std::decay_t<T>>::size>::type {
    return {};
}

template <typename T>
constexpr auto size_of(const T& v) noexcept -> typename std::enable_if<
        is_dynamic_size<std::decay_t<T>>::value,
        decltype(std::size(std::declval<T>()))>::type {
    return std::size(v) ? std::size(v) * size_of(*std::begin(v)) : 0;
}

/**
 * @brief Namespace for PostgreSQL specific types
 */
namespace pg {

OZO_STRONG_TYPEDEF(std::string, name)
OZO_STRONG_TYPEDEF(std::vector<char>, bytea)

} // namespace pg
} // namespace ozo

#define OZO__TYPE_NAME_TYPE(Name) decltype(Name##_s)
#define OZO__TYPE_ARRAY_NAME_TYPE(Name) decltype(Name##_s+"[]"_s)

#define OZO_PG_DEFINE_TYPE(Type, Name, Oid, Size) \
    namespace ozo {\
        template <>\
        struct type_traits<Type> : type_traits_helper<\
            Type, \
            OZO__TYPE_NAME_TYPE(Name), \
            oid_constant<Oid>, \
            Size\
        >{};\
    }

#define OZO_PG_DEFINE_TYPE_ARRAY(Type, Name, Oid) \
    namespace ozo {\
        template <>\
        struct type_traits<std::vector<Type>> : type_traits_helper<\
            std::vector<Type>, \
            OZO__TYPE_ARRAY_NAME_TYPE(Name), \
            oid_constant<Oid>, \
            dynamic_size\
        >{};\
    }

#ifdef __OZO_STD_OPTIONAL
#define OZO_PG_DEFINE_TYPE_ARRAY_OZO_OPTIONAL(Type, Name, Oid) \
    OZO_PG_DEFINE_TYPE_ARRAY(__OZO_STD_OPTIONAL<Type>, Name, Oid)
#else
#define OZO_PG_DEFINE_TYPE_ARRAY_OZO_OPTIONAL(Type, Name, Oid)
#endif

#define OZO_PG_DEFINE_TYPE_ARRAY_NULLABLES(Type, Name, Oid) \
    OZO_PG_DEFINE_TYPE_ARRAY(boost::optional<Type>, Name, Oid) \
    OZO_PG_DEFINE_TYPE_ARRAY_OZO_OPTIONAL(Type, Name, Oid) \
    OZO_PG_DEFINE_TYPE_ARRAY(boost::scoped_ptr<Type>, Name, Oid) \
    OZO_PG_DEFINE_TYPE_ARRAY(std::unique_ptr<Type>, Name, Oid) \
    OZO_PG_DEFINE_TYPE_ARRAY(boost::shared_ptr<Type>, Name, Oid) \
    OZO_PG_DEFINE_TYPE_ARRAY(std::shared_ptr<Type>, Name, Oid)

/**
 * @brief Helper macro to define type mapping
 * @ingroup group-type_system-mapping
 * In general type mapping is provided via `ozo::type_traits` specialization.
 * But for a single type you need to define a type, an array of the type, an
 * optional of the type (to support null), shared_ptr... and many other boilerplate.
 * To reduce such things this macro is made.
 *
 * @note This macro can be called in the global namespace only
 *
 * @param Type --- C++ type to be mapped to database type
 * @param Name --- string with name of database type
 * @param Oid --- oid for built-in type and `ozo::null_oid_t` for custom type
 * @param ArrayOid --- oid for an array of built-in type and `ozo::null_oid_t` for custom type
 * @param Size --- `bytes<N>` for fixed-size type (like integer, bigint and so on),
 * there N - size of the type in database, `dynamic_type` for dynamic size types (like `text`
 * `bytea` and so on)
 *
 * ### Example
 *
 * E.g. a definition of `uuid` type looks like this:
@code
OZO_PG_DEFINE_TYPE_AND_ARRAY(boost::uuids::uuid, "uuid", UUIDOID, 2951, bytes<16>)
@endcode
 *
 * Definition of user defined composite type may look like this:
@code
BOOST_FUSION_DEFINE_STRUCT((smtp), message,
    (std::int64_t, id)
    (std::string, from)
    (std::vector<std::string>, to)
    (std::optional<std::string>, subject)
    (std::optional<std::string>, text)
);

//...

OZO_PG_DEFINE_TYPE_AND_ARRAY(smtp::message, "code.message", null_oid, null_oid, dynamic_size)
@endcode
 */
#ifdef OZO_DOCUMENTATION
#define OZO_PG_DEFINE_TYPE_AND_ARRAY(Type, Name, Oid, ArrayOid, Size)
#else
#define OZO_PG_DEFINE_TYPE_AND_ARRAY(Type, Name, Oid, ArrayOid, Size) \
    OZO_PG_DEFINE_TYPE(Type, Name, Oid, Size) \
    OZO_PG_DEFINE_TYPE_ARRAY(Type, Name, ArrayOid) \
    OZO_PG_DEFINE_TYPE_ARRAY_NULLABLES(Type, Name, ArrayOid)
#endif


#define OZO_PG_DEFINE_CUSTOM_TYPE(Type, Name, Size) \
    namespace ozo {\
        template <>\
        struct type_traits<Type> : type_traits_helper<\
            Type, \
            OZO__TYPE_NAME_TYPE(Name), \
            null_oid_t, \
            Size\
        >{};\
    }

/**
 * @brief Bool type mapping
 * @ingroup group-type_system-mapping
 */
OZO_PG_DEFINE_TYPE_AND_ARRAY(bool, "bool", BOOLOID, 1000, bytes<1>)
OZO_PG_DEFINE_TYPE_AND_ARRAY(char, "char", CHAROID, 1002, bytes<1>)
OZO_PG_DEFINE_TYPE_AND_ARRAY(ozo::pg::bytea, "bytea", BYTEAOID, 1001, dynamic_size)

OZO_PG_DEFINE_TYPE_AND_ARRAY(boost::uuids::uuid, "uuid", UUIDOID, 2951, bytes<16>)

OZO_PG_DEFINE_TYPE_AND_ARRAY(int64_t, "int8", INT8OID, 1016, bytes<8>)
OZO_PG_DEFINE_TYPE_AND_ARRAY(int32_t, "int4", INT4OID, INT4ARRAYOID, bytes<4>)
OZO_PG_DEFINE_TYPE_AND_ARRAY(int16_t, "int2", INT2OID, INT2ARRAYOID, bytes<2>)

OZO_PG_DEFINE_TYPE_AND_ARRAY(ozo::oid_t, "oid", OIDOID, OIDARRAYOID, bytes<4>)

OZO_PG_DEFINE_TYPE_AND_ARRAY(double, "float8", FLOAT8OID, 1022, bytes<8>)
OZO_PG_DEFINE_TYPE_AND_ARRAY(float, "float4", FLOAT4OID, FLOAT4ARRAYOID, bytes<4>)

OZO_PG_DEFINE_TYPE_AND_ARRAY(std::string, "text", TEXTOID, TEXTARRAYOID, dynamic_size)

OZO_PG_DEFINE_TYPE_AND_ARRAY(ozo::pg::name, "name", NAMEOID, 1003, dynamic_size)

namespace ozo {

/**
 * @brief OidMap implementation type.
 * @ingroup group-type_system-types
 *
 * This type implements OidMap concept based on `boost::hana::map`.
 */
template <typename ImplT>
struct oid_map_t {
    using impl_type = ImplT;

    impl_type impl;
};


template <typename T>
struct is_oid_map : std::false_type {};

template <typename T>
struct is_oid_map<oid_map_t<T>> : std::true_type {};

/**
 * @brief Map of C++ types to corresponding PostgreSQL types OIDs
 *
 * @ingroup group-type_system-concepts
 * `OidMap` is needed to store information about C++ types and corresponding
 * custom database types' OIDs. For PostgreSQL built-in types no mapping is needed since their
 * `OID`s are defined in PostgreSQL sources. For custom types their `OID`s are defined
 * in a database.
 *
 * ###OidMap Definition
 *
 * `OidMap` `map` is an object for which these next statements are valid:
 *
 @code
 oid_t oid;
 //...
 set_type_oid<T>(map, oid);
 @endcode
 * Sets oid for type T in the map.
 *
 @code
 oid_t oid = type_oid<T>(map);
 @endcode
 * Returns oid value for type T from the map.
 *
@code
bool res = empty(map);
@endcode
 * Returns true if map has no types OIDs.
 *
 * @hideinitializer
 * @sa oid_map_t, register_types(), set_type_oid(), type_oid(), accepts_oid()
 */
template <typename T>
constexpr auto OidMap = is_oid_map<std::decay_t<T>>::value;

namespace detail {

template <typename ... T>
constexpr decltype(auto) register_types() noexcept {
    return hana::make_map(
        hana::make_pair(hana::type_c<T>, null_oid()) ...
    );
}

} // namespace detail

/**
 * @brief Provides #OidMap implementation for user-defined types.
 *
 * @ingroup group-type_system-functions
 * This function have to be used to provide information about custom types are being used
 * within requests for a #ConnectionSource.
 *
 * ###Example
 *
 * @code
// User defined type
struct custom_type;

//...

// Providing type information and corresponding database type
OZO_PG_DEFINE_TYPE_AND_ARRAY(custom_type, "code.custom_type", null_oid, null_oid, dynamic_size)

//...
// Creating ConnectionSource for futher requests to a database
const auto conn_source = ozo::make_connection_info("...", regiter_types<custom_type>());
 * @endcode
 *
 * @return `oid_map_t` object.
 */
template <typename ... T>
constexpr decltype(auto) register_types() noexcept {
    using impl_type = decltype(detail::register_types<T ...>());
    return oid_map_t<impl_type> {detail::register_types<T ...>()};
}

/**
 * @brief Type alias for empty #OidMap
 * @ingroup group-type_system-types
 */
using empty_oid_map = std::decay_t<decltype(register_types<>())>;

/**
* Function sets oid for type in #OidMap.
* @ingroup group-type_system-functions
* @tparam T --- type to set oid for.
* @param map --- #OidMap to modify.
* @param oid --- OID to set.
*/
template <typename T, typename MapImplT>
inline void set_type_oid(oid_map_t<MapImplT>& map, oid_t oid) noexcept {
    static_assert(!is_built_in<T>::value, "type must not be built-in");
    map.impl[hana::type_c<T>] = oid;
}

/**
* @brief Function returns oid for type from #OidMap.
* @ingroup group-type_system-functions
* @tparam T --- type to get OID for.
* @param map --- #OidMap to get OID from.
* @return oid_t --- OID of the type
*/
#ifdef OZO_DOCUMENTATION
template <typename T, typename OidMap>
oid_t type_oid(const OidMap& map) noexcept;
#else
template <typename T, typename MapImplT>
inline auto type_oid(const oid_map_t<MapImplT>& map) noexcept
        -> Require<!BuiltIn<T>, oid_t> {
    return map.impl[hana::type_c<T>];
}

template <typename T, typename MapImplT>
constexpr auto type_oid(const oid_map_t<MapImplT>&) noexcept
        -> Require<BuiltIn<T>, oid_t> {
    return typename type_traits<T>::oid();
}
#endif

/**
* @brief Function returns oid for type from #OidMap.
* @ingroup group-type_system-functions
* @param map --- #OidMap to get OID from.
* @param v --- object for which type's OID will return.
* @return oid_t --- OID of the type
*/
#ifdef OZO_DOCUMENTATION
template <typename T, typename OidMap>
oid_t type_oid(const OidMap& map, const T& v) noexcept
#else
template <typename T, typename MapImplT>
inline auto type_oid(const oid_map_t<MapImplT>& map, const T&) noexcept{
    return type_oid<std::decay_t<T>>(map);
}
#endif

/**
* Function returns true if type can be obtained from DB response with
* specified OID.
* @ingroup group-type_system-functions
* @tparam T --- type to examine
* @param map --- #OidMap to get type OID from
* @param oid --- OID to check for compatibility
*/
template <typename T, typename MapImplT>
inline bool accepts_oid(const oid_map_t<MapImplT>& map, oid_t oid) noexcept {
    return type_oid<T>(map) == oid;
}

/**
* Function returns true if type can be obtained from DB response with
* specified OID.
* @ingroup group-type_system-functions
* @param map --- #OidMap to get type OID from
* @param const T& --- type to examine
* @param oid --- OID to check for compatibility
*/
template <typename T, typename MapImplT>
inline bool accepts_oid(const oid_map_t<MapImplT>& map, const T& , oid_t oid) noexcept {
    return accepts_oid<std::decay_t<T>>(map, oid);
}

/**
 * Checks if #OidMap contains no items.
 * @ingroup group-type_system-functions
 *
 * ### Example
 *
 * @code
static_assert(empty(ozo::empty_oid_map{}));
 * @endcode
 * @param map --- #OidMap to check
 * @return `true` --- if map contains no items
 * @return `false` --- if contains items.
 */
template <typename MapImplT>
inline constexpr bool empty(const oid_map_t<MapImplT>& map) noexcept {
    return hana::length(map.impl) == hana::size_c<0>;
}

} // namespace ozo
