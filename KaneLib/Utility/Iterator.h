///////////////////////////////////////////////////////////////////////////////////////////////////
// Iterator utilities
///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

// Standard KaneLib configutation
#include <KaneLib/Config.h>
// For BOOST_MSVC
//#include <boost/config.hpp>
// STL iterator utilities
#include <iterator>
// Boost TMP utilities
//#include <boost/type_traits/is_base_of.hpp>
//#include <boost/utility/enable_if.hpp>
//#include <boost/iterator/iterator_traits.hpp>
//#include <boost/iterator/iterator_adaptor.hpp>

namespace Kane {

///////////////////////////////////////////////////////////////////////////////////////////////////
// Unchecked() and Rechecked()
///////////////////////////////////////////////////////////////////////////////////////////////////
// Some STL implementations (like MSVC's Dinkumware one, which is the only one I use in practice)
// use checked iterators, and sometimes provide a mechanism of retrieving an unchecked iterator for
// use in performance-critical library functions.  In some cases, like std::vector<>, this means 
// turning std::vector::iterator into a raw pointer, which allows for memory copy optimisations.
// These templates are provided for the cases when there is a way to get unchecked iterators,
// and to allow for generic portable code if I ever add my own checked iterators or find a way to 
// ditch MSVC and use GCC in Visual Studio.

// Generic version that uses the library's implementation if it exists, or just returns the identity
// otherwise.
// Custom iterators overload this for their own iterator type.
//#ifdef BOOST_MSVC
//template<typename UncheckedIterator, typename CheckedIterator> 
//__forceinline UncheckedIterator Unchecked(CheckedIterator it) {
//	// Use MSVC's generic unchecked getter 
//	return std::_Unchecked(it);
//}
//#else
//template<typename UncheckedIterator, typename CheckedIterator> 
//__forceinline UncheckedIterator Unchecked(CheckedIterator it) {
//	return it;
//}
//#endif

}

namespace kane {

///////////////////////////////////////////////////////////////////////////////////////////////////
// Custom version of std::advance()
///////////////////////////////////////////////////////////////////////////////////////////////////
// Because I much prefer returning a result than passing by reference
template<typename Iterator, typename Difference>
inline Iterator advance(Iterator i, const Difference diff) {
	std::advance(i, diff);
	return i;
}

///////////////////////////////////////
// Iterator Type Traits Helpers
///////////////////////////////////////

// Determine if type is iterator
template<typename Itr, typename Nope = void> struct is_iterator : public std::false_type { };
template<typename Itr> struct is_iterator<Itr, std::void_t<typename std::iterator_traits<Itr>::iterator_category>> : public std::true_type { };
	
template<typename Itr>
constexpr bool is_iterator_v = is_iterator<Itr>::value;

// Get iterator category
template<typename Itr>
using iterator_category = typename std::iterator_traits<Itr>::iterator_category;

// Get iterator category or void
template<typename Itr, typename Nope = void> struct iterator_category_or_void { typedef void type; };
template<typename Itr> struct iterator_category_or_void<Itr, std::void_t<typename std::iterator_traits<Itr>::iterator_category>> { typedef iterator_category<Itr> type; };
template<typename Itr> 
using iterator_category_or_void_t = typename iterator_category_or_void<Itr>::type;

// Determine if input iterator
template<typename Itr>
using is_input_iterator = std::is_base_of<std::input_iterator_tag, iterator_category_or_void<Itr>>;
template<typename Itr>
constexpr bool is_input_iterator_v = is_input_iterator<Itr>::value;

// Determine if only input iterator
// (Note that this isn't the same as "not forward iterator," because of output iterators and non-iterators.)
template<typename Itr>
using is_exactly_input_iterator = std::is_same<std::input_iterator_tag, iterator_category_or_void<Itr>>;
template<typename Itr>
constexpr bool is_exactly_input_iterator_v = is_exactly_input_iterator<Itr>::value;

// Determine if at least forward iterator
template<typename Itr>
using is_forward_iterator = std::is_base_of<std::forward_iterator_tag, iterator_category_or_void<Itr>>;
template<typename Itr>
constexpr bool is_forward_iterator_v = is_forward_iterator<Itr>::value;

// enable_if is iterator shortcut
template<typename Itr, typename V = void>
using enable_if_iterator = std::enable_if<is_iterator<Itr>::value, V>;
template<typename Itr, typename V = void>
using enable_if_iterator_t = typename enable_if_iterator<Itr, V>::type;

template<typename Itr, typename V = void>
using disable_if_iterator = std::enable_if<!is_iterator<Itr>::value, V>;
template<typename Itr, typename V = void>
using disable_if_iterator_t = typename disable_if_iterator<Itr, V>::type;

template<typename Itr, typename V = void>
using enable_if_exactly_input_iterator = std::enable_if<is_exactly_input_iterator<Itr>::value, V>;
template<typename Itr, typename V = void>
using enable_if_exactly_input_iterator_t = typename std::enable_if<is_exactly_input_iterator<Itr>::value, V>::type;

template<typename Itr, typename V = void>
using enable_if_forward_iterator = std::enable_if<is_forward_iterator<Itr>::value, V>;
template<typename Itr, typename V = void>
using enable_if_forward_iterator_t = typename std::enable_if<is_forward_iterator<Itr>::value, V>::type;

template<typename Itr, typename Category, typename V = void>
using enable_if_iterator_category = std::enable_if<std::is_base_of<Category, iterator_category_or_void_t<Itr>>::value, V>;
template<typename Itr, typename Category, typename V = void>
using enable_if_iterator_category_t = typename enable_if_iterator_category<Itr, Category, V>::type;

/*

// Returns the size of a range described by an iterator pair, or zero if Iterator doesn't provide
// the multi-pass guarantee.
// For non-iterator types (namely, integral types), returns kane::no_default_construct.  (Because 
// this is only being used in my internal library stuff for now, and that's what I need in those
// cases.)
template<typename Iterator>
KFINLINE typename boost::disable_if<is_iterator<Iterator>, const no_default_construct_t&>::type
distance_if_multipass(const Iterator&, const Iterator&) { 
	return ::kane::no_default_construct;
}
template<typename Iterator> 
KFINLINE typename enable_if_exactly_input_iterator<Iterator, std::size_t>::type
distance_if_multipass(const Iterator&, const Iterator&) {
	return 0;
}

template<typename Iterator> 
KFINLINE typename enable_if_forward_iterator<Iterator, std::size_t>::type
distance_if_multipass(const Iterator& first, const Iterator& last) {
	return std::distance<Iterator>(first, last);
}*/

}