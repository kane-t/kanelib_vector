///////////////////////////////////////////////////////////////////////////////////////////////////
// Miscellaneous utility junk
///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <KaneLib/Config.h>
#include <string>
#include <iostream>
//#include <boost/iostreams/concepts.hpp>
//#include <boost/iostreams/stream.hpp>

namespace kane {

///////////////////////////////////////////////////////////////////////////////
// kane::no_default_construct
///////////////////////////////////////////////////////////////////////////////
// Tag to allow overloaded constructors which don't default-initialise their members, instead 
// leaving them uninitialised (at least, for those types which aren't automatically initialised by
// the language).
// When C++11 support finally hits Microsoft, this'll be a constexpr.
// TODO: Check if it's better to pass this by value or by reference
struct no_default_construct_t { };
static const no_default_construct_t no_default_construct = no_default_construct_t();

///////////////////////////////////////////////////////////////////////////////
// Initial capacity initialiser
///////////////////////////////////////////////////////////////////////////////
// Kind of a trivial addition, but I seem to do this a lot:
//  vector<T> v; v.reserve(count);
template<typename T, typename Enable = typename std::is_integral<T>::type>
struct capacity_tag_t {
	capacity_tag_t() : value(0) { }

	// Only enabled if U is convertible to T
	template<typename U, typename = std::enable_if_t<std::is_convertible_v<U,T>>> 
	capacity_tag_t(const capacity_tag_t<U>& other) : value(other.value) { }

	// Only enabled if U is convertible to T
	template<typename U>
	std::enable_if_t<std::is_convertible_v<U, T>, capacity_tag_t<T>&> 
	operator=(const capacity_tag_t<U>& rhs) { value = rhs.value; return *this; }

	T value;
};

// TODO: Consider changing this from "capacity" to "initial_capacity," so it's more clear and less 
// likely to cause a name conflict.  Though, since it's in the kane namespace, that seems unlikely.
template<typename T> KFINLINE capacity_tag_t<T> capacity(const T value) { 
	capacity_tag_t<T> result; 
	result.value = value; 
	return result; 
}

///////////////////////////////////////////////////////////////////////////////////////////////////
                                      // String Utilities //                                       
///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// kane::cstring
///////////////////////////////////////////////////////////////////////////////
// Simple wrapper for C strings to allow them to be easily hashed or compared 
// with other strings
struct cstring {
	cstring() : psz(NULL) { }
	cstring(const char* psz) : psz(psz) { }
	cstring(const cstring& other) : psz(other.psz) { }
	cstring& operator=(const cstring& other) { psz = other.psz; return *this; }

	bool empty() const { return !psz || *psz == '\0'; }
	std::size_t size() const { return empty() ? 0 : std::strlen(psz); }
	void clear() { psz = NULL; }

	// SO UNCHECKED
	const char& operator[](std::size_t i) const { return psz[i]; }
	const char& at(std::size_t i) const { return psz[i]; }
	const char* c_str() const { return psz; }
	const char* data() const { return psz; }

	const char* psz;
};

inline std::ostream& operator<<(std::ostream& stream, const cstring& c) {
	if(!c.empty()) { stream << c.data(); }
	return stream;
}

inline bool operator==(const cstring& lhs, const cstring& rhs) { 
	return lhs.empty() ? rhs.empty() : (!rhs.empty() && strcmp(lhs.psz, rhs.psz) == 0);
}
inline bool operator==(const cstring& lhs, const std::string& rhs) { 
	return lhs.empty() ? rhs.empty() : lhs.psz == rhs;
}
inline bool operator==(const std::string& lhs, const cstring& rhs) { 
	return rhs.empty() ? lhs.empty() : lhs == rhs.psz;
}

inline bool operator!=(const cstring& lhs, const cstring& rhs) { return !(lhs == rhs); }
inline bool operator!=(const cstring& lhs, const std::string& rhs) { return !(lhs == rhs); }
inline bool operator!=(const std::string& lhs, const cstring& rhs) { return !(lhs == rhs); }

///////////////////////////////////////////////////////////////////////////////
// kane::substring
///////////////////////////////////////////////////////////////////////////////

// Non-mutable substring view of std::string.  Substring just wraps an iterator pair, so anything 
// that invalidates iterators also invalidates a Substring. 
class substring { 
public:
	typedef std::string::const_iterator const_iterator;

	// Constructors/Assignment
	// Substring() is the empty substring
	substring() : b(), e() { }
	substring(const std::pair<const_iterator, const_iterator>& range) : b(range.first), e(range.second) { }
	substring(const substring& other) : b(other.b), e(other.e) { }
	substring(const const_iterator& b, const const_iterator& e) : b(b), e(e) { }
	substring(const std::string& str, unsigned int start, unsigned int size) : b(str.begin() + start), e(str.begin() + start + size) { }
	substring& operator=(const substring& rhs) { b = rhs.b; e = rhs.e; return *this; }

	// Size stuff
	std::size_t size() const   { return e - b; }
	std::size_t length() const { return size(); }
	bool empty() const         { return b == e; }
	void clear()               { b = e = const_iterator(); }

	// Iterator stuff
	const_iterator begin() const { return b; }
	const_iterator end() const   { return e; }

	// Element access
	const char& operator[](const std::size_t pos) const { return b[pos]; }
	const char& at(const std::size_t pos) const { 
		const const_iterator i = b + pos; 
		if(i < e) { return *i; } 
		else      { throw std::out_of_range("invalid string position"); } 
	}
	
	// Data access
	const char* data() const { return &(*b); }

	// Hash and comparison
	bool operator==(const substring& rhs) const { 
		if(empty()) { return rhs.empty(); }
		else if(rhs.size() == size()) { return std::equal(b, e, rhs.begin()); }
		else { return false; }
	}
	bool operator==(const std::string& rhs) const {
		if(empty()) { return rhs.empty(); }
		else if(rhs.size() == size()) { return std::equal(b, e, rhs.begin()); }
		else { return false; }
	}

protected:
	const_iterator b;
	const_iterator e;
};

inline std::ostream& operator<<(std::ostream& stream, const substring& substr) {
	if(stream && !substr.empty()) { stream.write(substr.data(), substr.length()); }
	return stream;
}

__forceinline bool operator==(const std::string& lhs, const substring& rhs) { return rhs == lhs; }

///////////////////////////////////////////////////////////////////////////////
// kane::stringstream
///////////////////////////////////////////////////////////////////////////////

//class stringsink : public boost::iostreams::device<boost::iostreams::output_seekable> {
//public:
//	// Constructors
//	stringsink()                       : m_buffer(), m_position(std::string::npos) { }
//	stringsink(const stringsink& rhs)  : m_buffer(rhs.m_buffer), m_position(rhs.m_position) { }
//	stringsink(      stringsink&& rhs) : m_buffer(std::move(rhs.m_buffer)), m_position(rhs.m_position) { rhs.m_position = std::string::npos; }
//	stringsink(const std::string& s)   : m_buffer(s), m_position(std::string::npos) { }
//	stringsink(      std::string&& s)  : m_buffer(std::move(s)), m_position(std::string::npos) { }
//	// Assignment
//	stringsink& operator=(const stringsink& rhs)   { m_buffer = rhs.m_buffer; m_position = rhs.m_position; return *this; }
//	stringsink& operator=(      stringsink&& rhs)  { m_buffer = rhs.m_buffer; m_position = rhs.m_position; rhs.m_position = std::string::npos; *this; }
//	stringsink& operator=(const std::string& rhs)  { m_buffer = rhs; m_position = std::string::npos; return *this; }
//	stringsink& operator=(      std::string&& rhs) { m_buffer = rhs; m_position = std::string::npos; return *this; }
//
//	// Iostreams requirements
//	std::streamsize write(const char* s, const std::streamsize n0) { 
//		// Fuck it, just do this to avoid the bloody warnings
//		const std::string::size_type n = (std::string::size_type)n0;
//		if(m_position != std::string::npos) {
//			if(n < (m_buffer.size() - m_position)) {
//				m_buffer.replace(m_position, n, s, n);
//				m_position += n;
//				return n;
//			} else {
//				m_buffer.resize(m_position);
//				m_position = std::string::npos;
//			}
//		}
//		
//		m_buffer.append(s, (std::string::size_type)n);
//
//		return n;
//	}
//
//	boost::iostreams::stream_offset seek(boost::iostreams::stream_offset off, std::ios_base::seekdir dir) {
//		// Determine origin
//		std::string::size_type origin;
//		if(dir == std::ios_base::beg) { 
//			origin = 0;
//		} else if(dir == std::ios_base::end || m_position == std::string::npos) {
//			origin = m_buffer.size();
//		} else { //if(dir == std::ios_base::cur) {
//			origin = m_position;
//		}
//		
//		// Check for out of bounds
//		if(-off > origin) { 
//			throw std::ios_base::failure("bad seek offset (seeking before start of sequence)");
//		}
//		
//		const std::string::size_type newPosition = std::string::size_type(origin + off);
//		if(newPosition > m_buffer.size()) { 
//			throw std::ios_base::failure("bad seek offset (seeking past end of sequence)");
//		}
//			
//		// No bounds problems, assign new position
//		if(newPosition == m_buffer.size()) { 
//			m_position = std::string::npos;
//		} else {
//			m_position = newPosition;
//		}
//	}
//
//	// Buffer control
//		  std::string& buffer()		  { return m_buffer; }
//	const std::string& buffer() const { return m_buffer; }
//	void clear() { m_buffer.clear(); }
//
//protected:
//	std::string m_buffer;
//	std::string::size_type m_position;
//};
//
//typedef boost::iostreams::stream<stringsink> ostringstream;

}