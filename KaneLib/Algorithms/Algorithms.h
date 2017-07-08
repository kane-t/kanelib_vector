#pragma once

#include <cmath>

namespace kane {

// Lexicographical compare for equal-sized ranges
template<typename InputIterator1, typename InputIterator2>
inline bool unchecked_lexicographical_compare(InputIterator1 first1, const InputIterator1 last1, InputIterator2 first2) {
	while(first1 != last1) { 
		if(*first1 < *first2) { 
			return true; 
		} else if(*first2 < *first1) {
			return false;
		}
		++first1;
		++first2;
	}
	return false;
}

// Lexicographical compare for equal-sized ranges
template<typename InputIterator1, typename InputIterator2, typename Predicate>
inline bool unchecked_lexicographical_compare(InputIterator1 first1, const InputIterator1 last1, InputIterator2 first2, Predicate p) {
	while(first1 != last1) { 
		if(p(*first1, *first2)) { 
			return true; 
		} else if(p(*first2, *first1)) {
			return false;
		}
		++first1;
		++first2;
	}
	return false;
}

template<typename InputIterator1, typename InputIterator2, typename T>
__forceinline T cumulative_difference(InputIterator1 first1, InputIterator1 last1, InputIterator2 first2, T init = T()) {
	while(first1 != last1) { init += std::abs(*first1 - *first2); ++first1; ++first2; }
	return init;
}

}