///////////////////////////////////////////////////////////////////////////////////////////////////
////////                            //////// vector<T> ////////                            ////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// Custom implementation of the C++ STL std::vector.  kane::vector conforms fully to the most 
// recent working draft of C++17 (n4759), which is expected to be substantively identical to the 
// published standard.  In addition to the standard's requirements, kane::vector provides a number 
// of optimisations and additional features which are useful when performance is a consideration.
//
// Internally, vector intelligently optimises its memory operations when the value_type has trivial
// constructors or destructors.  A few non-standard extensions have also been included: take() and 
// take_back() work like erase() and pop_back(), except that they return the erased element as an 
// rvalue.  take(i,j) moves a range of elements to an output iterator and then erases them.  
// replace() is a much more efficient way of performing an insert(erase()) operation.  Fast 
// insertion functions allow inserting elements that don't already exist in the container, which 
// allows the compiler to optimise much more aggressively.  The pod_back_insert_iterator allows 
// for very fast (and surprisingly safe) insertion into the end of a container of POD types, and 
// direct manipulation of uninitialised memory.  duplicate() acts the same as insert(p,i,j), except 
// it allows i and j to point into the vector.
// 
// Some features are currently missing, but planned:
//   - TODO: checked iterators
//   - TODO: properly roll back inserts when an exception occurs
//   - TODO: strong exception guarantee of no side-effects on failed reallocation inserting at end
//
// And finally, some features might not work as expected:
//   - default-construction is routinely optimised away, even when you might not expect it: for 
//     instance, with a vector of ints, assign(20) may not give you a vector of 20 zeros; you'd 
//     need assign(20, 0) to guarantee that, or assign(20, T()) in the general case
//     (non-trivial default construction is NEVER optimised away, this only applies to POD types
//     and other types with trivial default constructors).
//     If this sounds strange, as it did to me when I came back to read it after a year, an 
//     explanation: often, particularly in debug builds, while "int x = int();" would set x to 0, 
//     the constructor is still considered trivial because "int x;" wouldn't initialise x to 
//     anything.  Other vector implementations define a function like assign(size_type) as 
//     assign(size_type count, T val = T()), so they only need to write the function once for both
//     the default-construct and copy-construct versions.  Because I split that into two functions,
//     trivial default constructors are never called.
//
// Further implementation details in the .inl file
#pragma once

// Common collection includes
#include <KaneLib/Collections/ContainerFwd.h>

// Crunchy implementation details for Kane::Vector
#include <KaneLib/Collections/VectorDetails.h>

namespace kane { 

///////////////////////////////////////
// use_simple_insert type trait
///////////////////////////////////////
// Specialise this trait to select the insert algorithm that std::vector (and possibly other array
// container types) will use.  If true, a copy of the element to be inserted will be made before
// progressing with the insert routine, and that copy will be move- or copy-constructed into place.
// If false, when a reallocation must be performed, the new memory will be allocated first, then 
// the element will be inserted in the correct location, and then the old contents of the vector 
// will be moved to the new memory in two steps.
// The standard requires that containers allow data which is contained within the container to be 
// passed to the insert, emplace, and push functions.  In a vector, because inserting in the middle
// requires moving the existing elements, this might invalidate reference or pointer arguments due
// to the aliasing problem.  For most types, this isn't a big deal, because copies are very fast.
// For certain, very large types, using the "complex" insert routine may be preferable.
template<typename T> struct use_simple_insert : public std::true_type { };

template<typename VectorType> class pod_back_insert_iterator;

template<typename T, typename Alloc = std::allocator<T>>
class vector : protected detail::vector_base<T, Alloc> {
private:
	typedef detail::vector_base<T, Alloc> my_base;
	typedef vector<T,Alloc> my_type;

public:
	///////////////////////////////////////////////////////////////////////////////////////////////
	// Typedefs
	///////////////////////////////////////////////////////////////////////////////////////////////
	// Typedefs are defined in the base class, if you want to look, but you can just trust that I
	// do it all as recommended by the standard unless otherwise specified.
	typedef typename my_base::allocator_type		allocator_type;
	typedef typename my_base::allocator_type_traits	allocator_type_traits;

	typedef typename my_base::value_type			value_type;
	typedef typename my_base::reference				reference;
	typedef typename my_base::rvalue_reference		rvalue_reference;
	typedef typename my_base::const_reference		const_reference;
	typedef typename my_base::pointer				pointer;
	typedef typename my_base::const_pointer			const_pointer;
	typedef typename my_base::size_type				size_type;
	typedef typename my_base::difference_type		difference_type;

	typedef pointer									iterator;
	typedef const_pointer							const_iterator;
	typedef std::reverse_iterator<iterator>			reverse_iterator;
	typedef std::reverse_iterator<const_iterator>	const_reverse_iterator;

	// Specialised back_insert_iterator for when the value_type is a POD type.  Using a 
	// pod_back_insert_iterator in a container which doesn't hold POD types results in undefined 
	// behaviour.  (But we don't specifically disallow it because there are rare cases it could be 
	// useful.)  See the bottom of this header for implementation details
	friend class pod_back_insert_iterator<my_type>;
	typedef pod_back_insert_iterator<my_type>		pod_back_insert_iterator;

	///////////////////////////////////////////////////////////////////////////////////////////////
	// Constructors
	///////////////////////////////////////////////////////////////////////////////////////////////
	// Construct empty vector
	vector() noexcept(std::is_nothrow_default_constructible<allocator_type>::value);
	explicit vector(const Alloc& allocator) noexcept;
	// Construct empty vector with initial capacity (non-standard extension)
	explicit vector(kane::capacity_tag_t<size_type> cap);
	vector(kane::capacity_tag_t<size_type> cap, const Alloc& allocator);
	// Construct vector with initialSize default-constructed elements
	explicit vector(size_type initialSize);
	vector(size_type initialSize, const Alloc& allocator);
	// Construct vector with initialSize copies of given value
	vector(size_type initialSize, const_reference val);
	vector(size_type initialSize, const_reference val, const Alloc& allocator);
	// Construct vector containing elements in given iterator range
	template<typename InputIterator, typename = enable_if_iterator_t<InputIterator>> 
	vector(InputIterator first, InputIterator last);
	template<typename InputIterator, typename = enable_if_iterator_t<InputIterator>> 
	vector(InputIterator first, InputIterator last, const Alloc& allocator);
	// Copy construct from another vector
	vector(const vector& other);
	vector(const vector& other, const Alloc& allocator);
	// Move construct from another vector
	vector(vector&& other) noexcept;
	vector(vector&& other, const Alloc& allocator);
	// Initialiser list construction
	vector(std::initializer_list<T> init);
	vector(std::initializer_list<T> init, const Alloc& allocator);
	// Destructor
	~vector();
	
	///////////////////////////////////////////////////////////////////////////////////////////////
	// Assignment
	///////////////////////////////////////////////////////////////////////////////////////////////
	// Assignment operator
	vector& operator=(const vector& rhs);
	vector& operator=(vector&& rhs) 
		noexcept(std::allocator_traits<Alloc>::propagate_on_container_move_assignment::value || 
				 std::allocator_traits<Alloc>::is_always_equal::value);
	vector& operator=(std::initializer_list<value_type> il) {
		assign(il.begin(), il.end());
	}
	// Assign content
	void assign(const size_type newSize, const_reference elem);
	// Assign from iterator pair
	template<typename InputIterator, typename = enable_if_iterator_t<InputIterator>>
	void assign(InputIterator first, InputIterator last);
	// Assign from initialiser list
	void assign(std::initializer_list<value_type> il) {
		assign(il.begin(), il.end());
	}

	///////////////////////////////////////////////////////////////////////////////////////////////
	// Iterators
	///////////////////////////////////////////////////////////////////////////////////////////////
	// Forward iterators
	iterator               begin() noexcept;
	iterator               end()   noexcept;
	const_iterator         begin() const noexcept;
	const_iterator         end()   const noexcept;
	// Reverse iterators
	reverse_iterator       rbegin() noexcept;
	reverse_iterator       rend()   noexcept;
	const_reverse_iterator rbegin() const noexcept;
	const_reverse_iterator rend()   const noexcept;
	// Const iterators
	const_iterator         cbegin()  const noexcept;
	const_iterator         cend()    const noexcept;
	const_reverse_iterator crbegin() const noexcept;
	const_reverse_iterator crend()   const noexcept;
	
	// POD back inserters (non-standard extension)
	pod_back_insert_iterator pod_back_inserter() noexcept;

	///////////////////////////////////////////////////////////////////////////////////////////////
	// Capacity and Size
	///////////////////////////////////////////////////////////////////////////////////////////////
	// Information
	size_type size()     const noexcept;
	bool      empty()    const noexcept;
	bool      full()     const noexcept;		// size() == capacity(), non-standard extension
	size_type max_size() const noexcept;
	size_type capacity() const noexcept;
	size_type available() const noexcept;	// capacity() - size(), non-standard extension
	// Capacity and size management
	void reserve(size_type neededSize);
	void resize(size_type newSize);
	void resize(size_type newSize, const_reference elem);
	void shrink_to_fit();
	// Allocator
	allocator_type get_allocator() const noexcept;
	void set_allocator(const allocator_type& newAlloc);

	///////////////////////////////////////////////////////////////////////////////////////////////
	// Element access
	///////////////////////////////////////////////////////////////////////////////////////////////
	// Const accessors
	const_reference operator[](size_type index) const;
	const_reference at(size_type index) const;
	const_reference cat(size_type index) const;
	const_reference front() const;
	const_reference back() const;
	// Non-const accessors
	reference operator[](size_type index);
	reference at(size_type index);
	reference front();
	reference back();
	// Data access
	// Returned pointer remains valid until the next reallocation
	      T* data() noexcept;
	const T* data() const noexcept;

	///////////////////////////////////////////////////////////////////////////////////////////////
	// Modifiers
	///////////////////////////////////////////////////////////////////////////////////////////////
	
	///////////////////////////////////
	// Push/Pop
	void push_back();
	void push_back(const_reference val);
	void push_back(rvalue_reference val);
	void pop_back();

	// Fast push (non-standard extension)
	// As push_back(), but val may not exist in the vector.  If val does exist in the vector, the 
	// results are undefined unless it comes before the specified position and the vector is not 
	// full.
	void xpush_back(const_reference val);
	void xpush_back(rvalue_reference val);

	///////////////////////////////////
	// Insert
	// All insert functions return an iterator to the first inserted element if any elements were 
	// inserted, or position if zero elements were inserted.  (If count or (last-first) == 0.)  
	// This is very silly, counter-intuitive, and frankly useless, but required by the standard.
	// Insert single element at position
	iterator insert(const_iterator position, const_reference val);
	iterator insert(const_iterator position, rvalue_reference rval);
	// Insert count copies of the exemplar value
	iterator insert(const_iterator position, size_type count, const_reference val);
	// Insert contents of initialiser list
	iterator insert(const_iterator position, std::initializer_list<value_type> il) {
		// TODO: Move this to definitions file?
		return insert(position, il.begin(), il.end());
	}
	// Insert range at specified position.
	template<typename InputIterator, typename = enable_if_iterator_t<InputIterator, iterator>> 
	iterator insert(const_iterator position, InputIterator first, InputIterator last);
	
	// Fast insert (non-standard extension)
	// As insert(), but val may not be a reference into the vector.  If val is a reference into the
	// vector, the results are undefined unless it comes before the specified position and the 
	// vector is not full.
	iterator xinsert(const_iterator position, const_reference val);
	iterator xinsert(const_iterator position, rvalue_reference rval);
	iterator xinsert(const_iterator position, size_type count, const_reference val);

	// TODO: Re-insert range (non-standard extension)
	// Insert range (potentially from this vector) at the specified position (non-standard extension)
	// According to the standard, v.insert(p,i,j) doesn't allow for i,j to be iterators into v, but 
	// sometimes we want this functionality, and we can implement it very efficiently.  duplicate()
	// reduces to insert() when the iterators aren't in this vector, otherwise it uses an efficient
	// algorithm for copying the range in.
	//template<typename InputIterator>
	//iterator duplicate(const_iterator position, InputIterator first, InputIterator last);

	///////////////////////////////////
	// Emplace
	// Default-construct a new element in-place in the vector.
	reference emplace_back();
	iterator emplace(const_iterator position);

	// Directly construct a new element in-place in the vector.
	template<typename... Args>
	reference emplace_back(Args&&... args);
	template<typename... Args>
	iterator emplace(const_iterator position, Args&&... args);

	///////////////////////////////////
	// Erase
	// Returns iterator to the element that followed the erased elements, or end() if no such.
	iterator erase(const_iterator position);
	iterator erase(const_iterator first, const_iterator last);

	///////////////////////////////////
	// Swap and Clear
	void swap(vector<T,Alloc>& vec)
		noexcept(std::allocator_traits<Alloc>::propagate_on_container_swap::value || 
				 std::allocator_traits<Alloc>::is_always_equal::value);
	void clear() noexcept;

	///////////////////////////////////
	// Take (non-standard extension)
	// As erase and pop_back, except returns the removed element
	value_type take_back();
	value_type take(const_iterator position);
	// Moves the range [first,last) into the specified output iterator, then erases the range from
	// the vector.  The OutputIterator may be provided as a wrapped reference using boost::ref 
	// if you want to get the end of the output sequence back.
	template<typename OutputIterator>
	iterator take(const_iterator first, const_iterator last, OutputIterator out);

	///////////////////////////////////
	// Replace (non-standard extension)
	// Replaces a range of elements with elements from an input sequence.  Given iterator ranges
	// [first1, last1) in a vector V and an input sequence defined by [first2, last2),
	//   v.replace(first1, last1, first2, last2);
	// has an equivalent effect to, but usually more efficient than,
	//   v.insert(v.erase(first1, last1), first2, last2);
	// but the return value from replace() is an iterator pointing to one past the last inserted 
	// element, rather than insert()'s return value of an iterator pointing to the first inserted 
	// element.  This choice was made because insert()'s return value is highly objectionable.  
	// Replace may also use N default-constructed or copy-constructed elements as the input 
	// sequence.
	iterator replace(const_iterator first, const_iterator last, size_type count);
	iterator replace(const_iterator first, const_iterator last, size_type count, const_reference val);
	template<typename InputIterator, typename = enable_if_iterator_t<InputIterator>> 
	iterator replace(const_iterator first1, const_iterator last1, InputIterator first2, InputIterator last2);

	// Testing function
	// (Actually may not remove this.  Performs an iterator range insert as if by repeated calls to
	// push_back(), including all the reallocations that would entail.  May be useful if you want 
	// to insert a small range from an istream_iterator or something.)
	template<typename InputIterator>
	iterator simple_append(InputIterator first, const InputIterator last);

	// Logical operators
	// Most STL implementations I've seen make these non-member functions, and just use the vector's
	// iterators, but that seems a tad off to me.  Especially if you're using checked iterators.  
	// What's the benefit of providing a function the user could easily write themselves without 
	// any special access to your code?
	// The standard doesn't require us to allow comparisons between vectors with other value_types
	// or allocator_types, but I'm going to add that in, provided there's an operator==(T,U) or
	// operator<(T,U) available (as needed by the comparison).
	template<typename U, typename OtherAlloc> bool operator==(const vector<U,OtherAlloc>& rhs) const;
	template<typename U, typename OtherAlloc> bool operator!=(const vector<U,OtherAlloc>& rhs) const;
	template<typename U, typename OtherAlloc> bool operator< (const vector<U,OtherAlloc>& rhs) const;
	template<typename U, typename OtherAlloc> bool operator<=(const vector<U,OtherAlloc>& rhs) const;
	template<typename U, typename OtherAlloc> bool operator> (const vector<U,OtherAlloc>& rhs) const;
	template<typename U, typename OtherAlloc> bool operator>=(const vector<U,OtherAlloc>& rhs) const;

	// Write the contents of a vector to an ostream.  The elements are written separated by spaces,
	// with the entire sequence surrounded by square brackets.  (ie, [0 1 2 3 4])
	// Obviously, this requires an << operator for type T
	friend std::ostream& operator<<(std::ostream& stream, const vector& vec);

protected:
	///////////////////////////////////////////////////////////////////////////////////////////////
	// Iterator/Pointer Conversions
	///////////////////////////////////////////////////////////////////////////////////////////////
	// These will allow us to easily handle checked iterators
	pointer iterator_to_pointer(iterator i) const;
	pointer iterator_to_pointer(const_iterator i) const;
	iterator pointer_to_iterator(pointer i) const;
	iterator pointer_to_iterator(const_pointer i) const;

	///////////////////////////////////////////////////////////////////////////////////////////////
	// Helper Functions
	///////////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////
	// Construction Helpers
	///////////////////////////////////////////////////////
	// Construct with N default-constructed elements
	void do_construct(size_type initialSize);
	// Construct with N copies of exemplar element
	void do_construct(size_type initialSize, const_reference val);

	// Select the appropriate base-class capacity for the insert-range construct
	// TODO: Remove the enabler condition and, possibly, make this a static function?
	// For forward iterators or better, it's the distance; for input iterators, it's the vector's
	// second capacity increment (14).
	template<typename InputIterator, typename = enable_if_iterator_t<InputIterator>>
	size_type select_capacity_from_range(InputIterator first, InputIterator last);

	///////////////////////////////////////////////////////
	// Assignment Helpers
	///////////////////////////////////////////////////////
	// Move-assign from vector
	void do_move_assign(vector&& rhs);

	///////////////////////////////////////////////////////
	// Insert-At-End Helpers
	///////////////////////////////////////////////////////
	// Append range from input iterators.  Uses the Horrible Insert Algorithm.
	template<typename InputIterator>
	pointer append_range(InputIterator first, InputIterator last, const std::input_iterator_tag);

	// Append range from forward (or better) iterators.  Calculates the distance and appends optimally.
	template<typename ForwardIterator>
	pointer append_range(ForwardIterator first, ForwardIterator last, const std::forward_iterator_tag);

	// Append elements from an iterator range
	template<typename InputIterator>
	pointer append_range(InputIterator first, InputIterator last);

	// Append N default-constructed elements
	pointer append_defaults(size_type sz);

	///////////////////////////////////////////////////////
	// Erase Helpers
	///////////////////////////////////////////////////////
	pointer erase_internal(pointer first, pointer last);
	pointer truncate_internal(pointer newSize);

	///////////////////////////////////////////////////////
	// Iterator Tag Dispatches
	///////////////////////////////////////////////////////
	// For the most part, each function uses the same set of algorithms, based on the iterator 
	// category.  Unless otherwise specified, the following can be assumed:
	//   With InputIterators: uses the Horrible Insert Algorithm.
	//   With all other iterator types: std::distance() is used to determine the size of the input
	//     sequence and the necessary space is allocated before copying, so only one reallocation
	//     is required.
	//     With RandomAccessIterators, std::distance() is an O(1) operation, so only one pass
	//     through the input sequence is required.  Otherwise, std::distance() performs one pass 
	//     and then a second pass copies the data in.

	///////////////////////////////////
	// Construct Dispatches
	///////////////////////////////////
	// Slow implementation of range construct for input iterators.  Uses the Horrible Insert Algorithm.
	template<typename InputIterator>
	void do_construct_range(InputIterator first, InputIterator last, const std::input_iterator_tag);

	// Fast implementation of range construct for forward iterators.
	template<typename ForwardIterator> 
	void do_construct_range(ForwardIterator first, ForwardIterator last, const std::forward_iterator_tag);

	// Construct vector from the elements of an iterator range
	template<typename InputIterator>
	void do_construct_range(InputIterator first, InputIterator last);

	///////////////////////////////////
	// Assign Dispatches
	///////////////////////////////////
	// Slow implementation of assign for input iterators.  Uses the Horrible Insert Algorithm.
	template<typename InputIterator> 
	void do_assign(InputIterator first, InputIterator last, const std::input_iterator_tag);

	// Fast implementation of assign for forward iterators.
	template<typename ForwardIterator>
	void do_assign(ForwardIterator first, ForwardIterator last, const std::forward_iterator_tag);

	// TODO: Make sure this is unnecessary, then remove it
	template<typename InputIterator> 
	void do_assign(InputIterator first, InputIterator last);

	///////////////////////////////////
	// Insert Dispatches
	///////////////////////////////////
	// Slow implementation of insert for input iterators.
	template<typename InputIterator> 
	pointer insert_range(pointer position, InputIterator first, InputIterator last, const std::input_iterator_tag);

	// Fast implementation of insert for forward iterators.
	template<typename ForwardIterator> 
	pointer insert_range(pointer position, ForwardIterator first, ForwardIterator last, const std::forward_iterator_tag);

	// Insert elements of range at position
	template<typename InputIterator> 
	pointer insert_range(pointer position, InputIterator first, InputIterator last);

	///////////////////////////////////
	// Replace Dispatches
	///////////////////////////////////
	// Slow version of replace with range for input iterators
	template<typename InputIterator> 
	iterator do_replace_range(pointer first1, pointer last1, InputIterator first2, InputIterator last2, const std::input_iterator_tag);

	// Fast implementation of replace with range for forward iterators
	template<typename InputIterator> 
	iterator do_replace_range(pointer first1, pointer last1, InputIterator first2, InputIterator last2, const std::forward_iterator_tag);

	///////////////////////////////////
	// Insert Operations
	///////////////////////////////////
	// Returns pointer to newly-inserted element.  position may not be iend()
	template<typename... Args>
	pointer emplace_internal(pointer position, Args&&... args);
	// Returns pointer to newly-inserted element
	template<typename... Args>
	pointer emplace_back_internal(Args&&... args);
	// Returns pointer to first of newly-inserted elements.  position may not be iend()
	template<typename... Args>
	pointer emplace_n_internal(pointer const position, size_type count, Args&&... args);
	// Returns pointer to first of newly-inserted elements
	template<typename... Args>
	pointer emplace_back_n_internal(size_type count, Args&&... args);
};

///////////////////////////////////
// POD back inserter
///////////////////////////////////
// The POD back inserter is a very efficient way to add elements to a vector with a POD 
// value_type.  The back inserter is essentially a normal iterator, except that it "points 
// into" the uninitialised memory at the end of the vector's elements.
//
// Dereferencing a pod_back_insert_iterator produces a reference to the next uninitialised 
// space in the vector's memory; incrementing the iterator "adds" the pointed-to off-the-end 
// element to the container (that is, it increments the container's size by one, so that now 
// the off-the-end element is considered part of the container's initialised space.)  Note: this
// means you must increment the iterator after creating each element.  The following will NOT add
// an element to the vector:
// auto i = v.pod_back_inserter(); i->a = 1; i->b = 3;    // ERROR: no ++i!
//
// The pod_back_insert_iterator is NOT safe for multithreaded applications, because that would 
// require checks that eliminate the performance benefits of using it.  Any changes to the 
// underlying vector's size (not just capacity) invalidate the insert iterator.  We could make it
// slightly more fail-safe, but I'd have to test it to be sure it wouldn't impact performance.
//
// Obviously, the pointed-to element is uninitialised, so relying on it containing any 
// particular data is unwise.
//
// The pod_back_insert_iterator checks if the vector is full when the -> and * operators are 
// called, and reallocates at that point if necessary.  This doesn't mean you should avoid calling 
// them repeatedly if you want the best performance: the check is written so a good compiler (like 
// MSVC or GCC) will optimise it out after the first test.  So, for instance, this only requires 
// one test, and performs better than calling push_back():
//    for(int x = 0; x < 10; x++, i++) { i->x = 5; i->y = 2; i->z = 1; }
template<typename VectorType>
class pod_back_insert_iterator : public std::iterator<std::forward_iterator_tag, typename VectorType::value_type> {
public:
	typedef typename VectorType::value_type value_type;
	typedef typename VectorType::pointer	pointer;
	typedef typename VectorType::reference	reference;
	typedef typename VectorType::iterator   normal_iterator;

	// Construct a back inserter pointing to the "end" of any container
	pod_back_insert_iterator();
	// Construct a back inserter for the specified vector
	pod_back_insert_iterator(VectorType& v);
	// Copy the specified back inserter
	pod_back_insert_iterator(const pod_back_insert_iterator& other);
	// Copy the specified back inserter
	pod_back_insert_iterator& operator=(const pod_back_insert_iterator& rhs);

	///////////////////////////////
	// Iterator functionality
	// Access the next uninitialised element in the container
	reference operator*();
	pointer operator->();
	// Insert the next uninitialised element into the container and move forward to the next one.
	pod_back_insert_iterator& operator++();
	// As pre-increment, but returns a normal iterator to the inserted element.
	normal_iterator operator++(int);
	// Test for equality/inequality
	bool operator==(const pod_back_insert_iterator& rhs) const;
	bool operator!=(const pod_back_insert_iterator& rhs) const;

	///////////////////////////////
	// TODO: Extended functionality
	// Create an end iterator
/*	pod_back_insert_iterator end() const;
	// Directly construct an element in-place (same as vector's emplace_back, but faster)
	template<typename... Args>
	typename VectorType::iterator emplace(Args&&... args);
*/
protected:
	void check_next();
	void grow_vector();

	VectorType* m_vector;
	pointer m_next;
};

template<typename T, typename Alloc>
pod_back_insert_iterator<vector<T,Alloc>> pod_back_inserter(vector<T,Alloc>& v);

}

// Implementation
#include <KaneLib\Collections\Vector.inl>