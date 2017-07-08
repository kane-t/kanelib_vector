///////////////////////////////////////////////////////////////////////////////////////////////////
// Common includes for collection classes
#pragma once

// Always need KaneLib/Config
#include <KaneLib/Config.h>

// Standard library includes
#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>
#include <iostream>
#include <type_traits>
#include <tuple>

// KaneLib utility includes
#include <KaneLib/Algorithms/Algorithms.h>
#include <KaneLib/Utility/Utility.h>
#include <KaneLib/Utility/Iterator.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
// alloc_container
///////////////////////////////////////////////////////////////////////////////////////////////////
// Base class for storing the allocator and (if possible) performing empty member optimisation.
// alloc_container shouldn't provide any more functionality than simply containing and allowing 
// access to an allocator.  Everything beyond that should be handled by a deriving class.  This 
// complicates some things and introduces some code reuse, but that's intentional.  alloc_container 
// should be completely interchangable with an actual literal allocator member named m_allocator().
//
// We actually provide two implementations of alloc_container: if the allocator is an empty class, 
// it's contained as a private base class to perform empty member optimisation; otherwise, it's a
// (private) member named m_alloc.  Either way, the only access to the allocator is through member
// functions m_allocator() and m_allocator() const.
//
// Because all container types ultimately need a base class like these, I'm going to put it here in
// the ContainerFwd header.  If we ultimately find ourselves building up a number of these generic, 
// shared types, maybe a new header will be called for.
namespace kane { namespace detail { 

template<typename Alloc, typename NotEmpty = void>
class alloc_container {
private:
	Alloc m_alloc;

	// Rebind helper
	template<typename U>
	using rebound_allocator = typename std::allocator_traits<Alloc>::template rebind_alloc<U>;

protected:
	typedef Alloc allocator_type;

	KFINLINE alloc_container()				 : m_alloc() { }
	KFINLINE alloc_container(const Alloc& a) : m_alloc(a) { }
	KFINLINE alloc_container(Alloc&& a)		 : m_alloc(std::move(a)) { }
	// Assign from compatible allocator type
	template<typename U>
	inline alloc_container(const rebound_allocator<U>& otherAlloc) : m_alloc(std::forward<U>(otherAlloc)) { }
	template<typename U>
	inline alloc_container(rebound_allocator<U>&& otherAlloc) : m_alloc(std::forward<U>(otherAlloc)) { }
	// This can *probably* be:
	// template<typename U> inline alloc_container(U&& otherAlloc) : m_alloc(std::forward<U>(otherAlloc)) { }
	// But I'm not 100% positive, so,
	// TODO: Test that
	// Experimental!
	KFINLINE alloc_container(kane::no_default_construct_t) { }
	
	KFINLINE const Alloc& m_allocator() const { return m_alloc; }
	KFINLINE Alloc& m_allocator()			  { return m_alloc; }
};

template<typename Alloc> 
class alloc_container<Alloc, std::enable_if_t<std::is_empty_v<Alloc>>> : private Alloc {
	typedef Alloc m_alloc;

	// Rebind helper
	template<typename U>
	using rebound_allocator = typename std::allocator_traits<Alloc>::template rebind_alloc<U>;

protected:
	typedef Alloc allocator_type;

	KFINLINE alloc_container()				 : m_alloc() { }
	KFINLINE alloc_container(const Alloc& a) : m_alloc(a) { }
	KFINLINE alloc_container(Alloc&& a)		 : m_alloc(std::move(a)) { }
	// Assign from compatible allocator type
	template<typename U>
	inline alloc_container(const rebound_allocator<U>& otherAlloc) : m_alloc(std::forward<U>(otherAlloc)) { }
	template<typename U>
	inline alloc_container(rebound_allocator<U>&& otherAlloc) : m_alloc(std::forward<U>(otherAlloc)) { }
	// This can *probably* be:
	// template<typename U> inline alloc_container(U&& otherAlloc) : m_alloc(std::forward<U>(otherAlloc)) { }
	// But I'm not 100% positive, so,
	// TODO: Test that
	// Experimental!
	KFINLINE alloc_container(kane::no_default_construct_t) { }

	KFINLINE const Alloc& m_allocator() const { return *this; }
	KFINLINE Alloc& m_allocator()			  { return *this; }
};

} }