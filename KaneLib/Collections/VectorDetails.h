///////////////////////////////////////////////////////////////////////////////////////////////////
////////                           //////// vector_base ////////                           ////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// Base class to manage the memory for kane::vector.  Derives from array_container_base to add and
// manage data members and add a handful of functions for handling reallocation of the array.  Also
// provides a few support functions that are reused by the vector insert routines.
//
// Currently, the way the member variables are implemented in vector_base is that m_data, m_size, 
// and m_capacity are all array pointers; m_data being the beginning of the array, m_size being the
// end of initialised space, and m_capacity being the end of the array.  This makes pointer-related
// functions (like end()) as well as many internal operations far more efficient, at the cost of 
// making size() and capacity() slightly less efficient.  In theory, because of the way most of 
// the internal functions are written, as long as deriving classes consistently use iend(), 
// ibegin(), uend(), and uend() to access and set data members, m_size and m_capacity could be 
// changed to size_type without any other changes.  However, there are still a few functions 
// (most notably, capacity() itself) which would need to be changed to support this.
//
// Because I, personally, find it more aesthetically pleasing, when allocator is non-empty, data 
// members are arranged in this order: [m_data, m_size, m_capacity, m_allocator].  This means that,
// assuming the allocator doesn't have special alignment requirements, &vec is the same address
// as vec.data().
//
// TODO: Add specialisations for vector_base that provide checked iterators
#pragma once

#include <KaneLib/Collections/ContainerFwd.h>
#include <KaneLib/Collections/ArrayContainerBase.h>

namespace kane { namespace detail { 

///////////////////////////////////////////////////////////////////////////////////////////////////
// vector_base_members
///////////////////////////////////////////////////////////////////////////////////////////////////
// Base class for storing vector member variables.  The only reason for this is to partially 
// simplify constructors and to ensure that the vector members come before the allocator in 
// memory (when the allocator is non-empty).
// 
// Like alloc_container, this class is purely a container for data members, and thus provides no 
// additional functionality.  The typedefs are marked private for this reason, so they don't leak 
// into the implementation of deriving classes.
// 
// To make the template parameters simpler, we get our pointer type directly from the pointer 
// typedef in array_container_base, which creates a tight linkage between vector_base_members and 
// array_container_base, which is unfortunate.  In the future, consider changing the class template
// to template<typename Pointer>.
template<typename T, typename Alloc>
class vector_base_members {
private:
	typedef typename array_container_base<T, Alloc>::pointer pointer;

protected:
	pointer m_data;
	pointer m_size;
	pointer m_capacity;

	KFINLINE vector_base_members() : m_data(NULL), m_size(NULL), m_capacity(NULL) { }
	KFINLINE vector_base_members(pointer const d, pointer const s, pointer const c) : m_data(d), m_size(s), m_capacity(c) { }
	KFINLINE vector_base_members(const vector_base_members& rhs) : m_data(rhs.m_data), m_size(rhs.m_size), m_capacity(rhs.m_capacity) { }
	// Experimental!
	KFINLINE vector_base_members(const kane::no_default_construct_t) { }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// vector_base
///////////////////////////////////////////////////////////////////////////////////////////////////
// Base class to manage the memory for kane::vector.  Derives from array_container_base to add and
// manage data members and add a handful of functions for handling reallocation of the array.  Also
// provides a few support functions that are reused by the vector insert routines.
template<typename T, typename Alloc>
class vector_base : public vector_base_members<T, Alloc>, public array_container_base<T, Alloc> {
private:
	// Easier way to reference base classes
	typedef array_container_base<T, Alloc> alloc_base;
	typedef vector_base_members<T, Alloc>  members_base;

public:
	///////////////////////////////////////////////////////////////////////////////////////////////
	// Typedefs
	///////////////////////////////////////////////////////////////////////////////////////////////
	// Typedefs are defined in the base class, if you want to look, but you can just trust that I
	// do it all as recommended by the standard unless otherwise specified.
	// They have to be redefined here to make IntelliSense happy.
	typedef typename alloc_base::allocator_type		   allocator_type;
	typedef typename alloc_base::allocator_type_traits allocator_type_traits;

	typedef typename alloc_base::value_type		  value_type;
	typedef typename alloc_base::reference		  reference;
	typedef typename alloc_base::rvalue_reference rvalue_reference;
	typedef typename alloc_base::const_reference  const_reference;
	typedef typename alloc_base::pointer		  pointer;
	typedef typename alloc_base::const_pointer	  const_pointer;
	typedef typename alloc_base::size_type		  size_type;
	typedef typename alloc_base::difference_type  difference_type;

	// TODO: Make sure this actually works
	static constexpr size_type first_capacity_increment = next_capacity(0);
	static constexpr size_type second_capacity_increment = next_capacity(first_capacity_increment);

	///////////////////////////////////////////////////////////////////////////////////////////
	// Traits for TMP
	///////////////////////////////////////////////////////////////////////////////////////////
	// Provided in base class, but I'll leave them here, commented out, to use as a reference
	// value_type traits
	//static constexpr bool value_has_trivial_construct = std::is_trivially_constructible_v<value_type>;
	//static constexpr bool value_has_trivial_destroy = std::is_trivially_destructible_v<value_type>;
	// allocator_type traits
	//static constexpr bool alloc_propagate_copy = allocator_type_traits::propagate_on_container_copy_assignment();
	//static constexpr bool alloc_propagate_move = allocator_type_traits::propagate_on_container_move_assignment();
	//static constexpr bool alloc_propagate_swap = allocator_type_traits::propagate_on_container_swap();
	//static constexpr bool alloc_is_always_equal = allocator_type_traits::is_always_equal();

	///////////////////////////////////////////////////////////////////////////////////////////////
	// Constructors
	///////////////////////////////////////////////////////////////////////////////////////////////
	// Note: Constructors that take an allocator should take an Alloc, because that's the type the 
	// user specified.  Internally, we're using a rebound allocator, but it's fine, because 
	// allocators are required to support copy and move construction from any rebindable type.
	// Construct empty vector with default-constructed allocator
	vector_base();
	// Construct empty vector with specified allocator
	vector_base(const Alloc& a);
	// Construct empty vector with specified initial capacity
	vector_base(const size_type sz);
	// Construct empty vector with specified initial capacity and allocator
	vector_base(const size_type sz, const Alloc& a);
	// Copy-construct from another vector, using a copy of other vector's allocator
	vector_base(const vector_base& other);
	// Copy-construct from another vector, using specified allocator
	vector_base(const vector_base& other, const Alloc& a);
	// Construct by moving vector and allocator
	vector_base(vector_base&& rv);
	// Construct with allocator and move content from other vector; allocators definitely equal
	vector_base(vector_base&& rv, const Alloc& a, const std::true_type );
	// Construct with allocator and move content from other vector; allocators possibly different
	vector_base(vector_base&& rv, const Alloc& a, const std::false_type);

	// Default-construct allocator, but leave other members uninitialised
	vector_base(const kane::no_default_construct_t);
	// Use specified allocator, but leave other members uninitialised
	vector_base(const kane::no_default_construct_t, const Alloc& a);

	///////////////////////////////////////////////////////////////////////////////////////////////
	// Members Helpers
	///////////////////////////////////////////////////////////////////////////////////////////////
	// Size and capacity of storage
	size_type size     () const;		// same as iend() - ibegin()
	size_type capacity () const;		// same as uend() - ibegin()
	size_type available() const;		// same as capacity() - size()
	   bool   empty    () const;		// same as ibegin() == iend(), or size() == 0
	   bool   full     () const;		// same as m_size == m_capacity, or iend() == uend()
	// Pointers to beginning and end of uninitialised space
	// PONDER: These next four functions could be made const, couldn't they?
	      pointer ibegin();				// same as m_data
	      pointer iend  ();				// same as ibegin() + size()
	      pointer ubegin();				// same as ibegin() + size()
	      pointer uend  ();				// same as ibegin() + capacity()
	const_pointer ibegin() const;		// ...and repeat...
	const_pointer iend  () const;
	const_pointer ubegin() const;
	const_pointer uend  () const;
	// Set size
	void size  (const size_type newSize);	// same as iend(ibegin() + newSize)
	void iend  (pointer const newEnd);		// same as size(newEnd - ibegin())
	void ubegin(pointer const newEnd);		// same as iend(newEnd) above
	// Resets data members...
	void reset();													// to (NULL, NULL, NULL)
	void reset(pointer const ds, pointer const c);					// to (ds, ds, c)
	void reset(pointer const ds, const size_type cap);				// to (ds, ds, ds + cap)
	void reset(pointer const d, pointer const s, pointer const c);	// to (d, s, c)
	void reset(pointer const d, pointer const s, const size_type c);// to (d, s, d + c)

	// Joke (also useful) helpers
	bool many(const size_type sz) const; // same as (size() + sz) >  capacity(), or !few(count)
	bool few(const size_type sz) const;  // same as (size() + sz) <= capacity(), or !many(count)

	// TODO: Will this work?  Probably!
	//pointer allocate(const size_type sz) { return my_base::allocate(sz, m_data); }
	// It *doesn't* work with kane::no_default_construct, though
	// TODO: Fix this if we ever need the hint

	///////////////////////////////////////////////////////////////////////////////////////////////
	// Uninitialised Memory Operations
	///////////////////////////////////////////////////////////////////////////////////////////////
	// Note that for value_types with trivial destructors, copy/move assignment is always the same
	// as copy/move construction.  (At least, in terms of the result of the operation.)

	///////////////////////////////////
	// Unchecked Moves
	///////////////////////////////////
	// Move a block of memory [position,iend()) forward N spaces and update size.  There must be
	// sufficient capacity to perform the move.  No checking is done to make sure that this is
	// the case--these functions are intended for use in algorithms that have already ensured
	// the necessary conditions.  See the checked moves below for helpers that DO provide some 
	// safety.
	//
	// When the destination and source ranges do not overlap, the gap created will consist of an
	// initialised segment followed by a (possibly empty) uninitialised segment.  Functions for 
	// which this is a possibility will notify the user where the uninitialised segment begins,
	// if it does.

	// Moves the block of memory forward one space.  Nothing is returned because the uninitialised 
	// segment is guaranteed to be empty.
	void move_forward_1(pointer const position);

	// Move the block of memory [position,iend()) forward N spaces and update size.  There must be
	// sufficient capacity to perform the move.  If the destination range does not overlap the 
	// source range, the latter part of the empty space is uninitialised, while the former part
	// is initialised.  
	// The returned pointer is to the one-past-the-end of the initialised segment.  If there is 
	// no uninitialised segment, this is position + sz, otherwise it's the original iend(), before 
	// it was adjusted forward.
	pointer move_forward_n(pointer const position, const size_type sz);

	///////////////////////////////////
	// Checked Moves
	///////////////////////////////////
	// As unchecked moves, but will reallocate if there isn't sufficient capacity to perform the
	// move.

	// Creates a gap of size 1.  Return value is the same as position if no allocation was required,
	// otherwise a pointer into the new array.  If no allocation was required, the return value 
	// points to an initialised element, otherwise it points to an uninitialised element.
	// (So, if result == position, use assignment, otherwise use construction.)
	pointer make_gap_1(pointer const position);

	// Creates a gap of size N.  Return value is a pair of pointers to the initialised and 
	// uninitialised beginnings of the gap.  The initialised begin always precedes the 
	// uninitialised begin if it exists.
	std::pair<pointer, pointer> make_gap_n(pointer const position, const size_type sz);
	
	///////////////////////////////////////////////////////////////////////////////////////////////
	// Vector Operations
	///////////////////////////////////////////////////////////////////////////////////////////////
	// Determines the next capacity (currently, 2 if capacity == 0, otherwise capacity * 2).
	size_type next_capacity() const;
	// Next capacity increment given the specified current capacity.
	constexpr size_type next_capacity(const size_type sz);
	// Determines the best next capacity large enough to contain the specified size.
	// Currently, this is the larger of needed and next_capacity()
	size_type best_capacity(const size_type needed) const;

	// Reallocate the internal array to the next_capacity()
	void reallocate();
	// Reallocate the internal array to the specified capacity if greater than the current capacity.
	void reallocate(const size_type newCapacity);
	// Internal function for actually handling the reallocation.
	// (No sanity checks are performed on the inputs.  This should normally only be called from 
	// inside reallocate().  At the very least, requires 0 <= oldCapacity < newCapacity.)
	void really_reallocate(const size_type oldCapacity, const size_type newCapacity);

	///////////////////////////////////////////////////////
	// InputIterator Helper
	///////////////////////////////////////////////////////
	// Returns an array of sufficient capacity to contain the specified size, plus all of the 
	// elements in the input sequence, and with the input sequence constructed beginning at the 
	// specified index.  The return value is:
	struct horrible_insert_helper {
		pointer newData;		// The array itself
		pointer newSize;		// One past the last element inserted from the input iterators
		pointer newCapacity;	// One past the end of the array
	};
	// All elements of the array except the elements inserted from the input iterator are 
	// uninitialised.
	template<typename InputIterator>
	horrible_insert_helper insert_horrible(const size_type index, const size_type oldSize, const size_type oldCapacity,
											InputIterator first, const InputIterator last);
	// Implementation details can be found by the function definition.  tl;dr: handles inserting 
	// an InputIterator range with at most O(2N) copy or move constructions.
};

} }

#include <KaneLib/Collections/VectorDetails.inl>