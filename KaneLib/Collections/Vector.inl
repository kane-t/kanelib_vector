///////////////////////////////////////////////////////////////////////////////////////////////////
////////                 //////// kane::vector<T> Implementation ////////                  ////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// *** SOME NOTES ON DATA MEMBERS ***
// A vector needs to keep track of several things, internally: 
//   a pointer to the beginning of the allocated memory (m_data)
//   the initialised range, [ibegin, iend)
//   the uninitialised range, [ubegin, uend)
//   the size and capacity
// There are a number of natural equivalencies between these properties:
//	 m_data == ibegin, iend == ubegin
//   iend == (ibegin + size) == (m_data + size)
//   size == (iend - ibegin) == (iend - m_data)
//   uend == m_data + capacity
//   capacity == (uend - m_data)
// Thus, the most reasonable way to implement a vector is to store m_data and either explicitly
// store the size and capacity or explicitly store iend and uend, and synthesize all the other 
// properties using the equivalencies described above.
//
// Whichever we choose, algorithms will naturally use both size/capacity and iend/uend, depending
// on what they're trying to do, so we provide getters and setters for all of these described 
// quantities (in VectorBase), so the pointer arithmetic doesn't need to be written inline into 
// the member functions, and so we can easily switch between the implementations if we want to.
//
// Currently, we store the pointers explicitly, not the sizes.  My suspicion is that this is more
// efficient in the most likely case.  Specifically, it's more efficient when push_back() and 
// emplace_back() are much more frequently called than size(), capacity(), reserve(), and resize().
// Internally, the names of the variables are always m_size and m_capacity, meaning that the 
// expression (m_size == m_capacity) is always valid for testing if the capacity is full, as is 
// (m_capacity - m_size) for determining the number of elements we can insert before needing to 
// reallocate.
//
// ALSO: There are two convenience functions I use frequently:
//   many(sz) == capacity() < (size() + sz)
//   few(sz) == capacity() >= (size() + sz)
// Basically, "many" means more additional elements than can fit in the current capacity, "few" 
// means the opposite.
#pragma once

namespace kane {

///////////////////////////////////////////////////////////////////////////////////////////////////
// Constructors
///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////
// Default constructor
///////////////////////////////////////

// Construct with zero capacity
template<typename T, typename Alloc> 
inline vector<T,Alloc>::vector() noexcept(std::is_nothrow_default_constructible<allocator_type>::value) 
	: my_base() { }

template<typename T, typename Alloc> 
inline vector<T,Alloc>::vector(const Alloc& a) noexcept 
	: my_base(a) { }

// Construct with specified capacity
template<typename T, typename Alloc> 
inline vector<T,Alloc>::vector(kane::capacity_tag_t<size_type> cap) 
	: my_base(cap.value) { }

template<typename T, typename Alloc> 
inline vector<T,Alloc>::vector(kane::capacity_tag_t<size_type> cap, const Alloc& a) 
	: my_base(cap.value, a) { }

///////////////////////////////////////
// Construct with size
///////////////////////////////////////

template<typename T, typename Alloc> 
inline vector<T,Alloc>::vector(size_type initialSize) 
	: my_base(initialSize) { do_construct(initialSize); }

template<typename T, typename Alloc> 
inline vector<T,Alloc>::vector(size_type initialSize, const Alloc& a) 
	: my_base(initialSize, a) { do_construct(initialSize); }

template<typename T, typename Alloc> 
inline vector<T,Alloc>::vector(size_type initialSize, const_reference value) 
	: my_base(initialSize) { do_construct(initialSize, value); }

template<typename T, typename Alloc> 
inline vector<T,Alloc>::vector(size_type initialSize, const_reference value, const Alloc& a) 
	: my_base(initialSize, a) { do_construct(initialSize, value); }

///////////////////////////////////////
// Construct from range
///////////////////////////////////////

template<typename T, typename Alloc>
template<typename InputIterator, typename> 
inline vector<T,Alloc>::vector(InputIterator first, InputIterator last) 
	: my_base(select_capacity_from_range(first, last)) { do_construct_range(first, last); }

template<typename T, typename Alloc> 
template<typename InputIterator, typename> 
inline vector<T,Alloc>::vector(InputIterator first, InputIterator last, const Alloc& a) 
	: my_base(select_capacity_from_range(first, last), a) { do_construct_range(first, last); }

///////////////////////////////////////
// Copy construction
///////////////////////////////////////

template<typename T, typename Alloc> 
inline vector<T,Alloc>::vector(const vector& other) 
	: my_base(other) { }

template<typename T, typename Alloc> 
inline vector<T,Alloc>::vector(const vector& other, const Alloc& a) 
	: my_base(other.size(), a) { do_construct_range(other.begin(), other.end()); }

///////////////////////////////////////
// Move construction
///////////////////////////////////////

template<typename T, typename Alloc> 
inline vector<T,Alloc>::vector(vector&& other) noexcept
	: my_base(std::move(other)) { }

template<typename T, typename Alloc> 
inline vector<T,Alloc>::vector(vector&& other, const Alloc& a) 
	: my_base(std::move(other), a, typename std::allocator_traits<Alloc>::is_always_equal()) { }

///////////////////////////////////////
// Initialiser list construction
///////////////////////////////////////
template<typename T, typename Alloc>
inline vector<T,Alloc>::vector(std::initializer_list<T> init) 
	: my_base(init.size()) { do_construct_range(init.begin(), init.end()); }

template<typename T, typename Alloc>
inline vector<T,Alloc>::vector(std::initializer_list<T> init, const Alloc& allocator) 
	: my_base(init.size(), allocator) { do_construct_range(init.begin(), init.end()); }


///////////////////////////////////////
// Destructor
///////////////////////////////////////

template<typename T, typename Alloc> inline vector<T,Alloc>::~vector() {
	if(m_data) {
		clear();
		deallocate(m_data, m_capacity);
		reset();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Assignment 
///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////
// Copy-Assignment Operator
///////////////////////////////////////////////////////////

template<typename T, typename Alloc> 
inline vector<T,Alloc>& vector<T,Alloc>::operator=(const vector& rhs) { 
	if(this != &rhs) {
		// First, check if we need to set a new allocator
		// Compiler should resolve these if conditions at compile-time, and be able to elide the 
		// impossible paths.
		if(alloc_propagate_copy) {
			// If allocators propagate on copy, we need to test the allocator for equality to see
			// if we need to reallocate.
			if(!alloc_is_always_equal && m_allocator() != rhs.m_allocator()) {
				// Fine, fine, need to clear and deallocate
				if(m_data) {
					clear();
					deallocate(m_data, m_capacity);
					reset();
				}
			}
			
			// Assign new allocator regardless of whether they're equal.  They may be equal for 
			// deallocation, and simply have different logic for allocating new storage.
			m_allocator() = rhs.m_allocator();
		}

		// If either side is empty, we can use fairly simple logic for this copy
		if(empty()) {
			// Only need to do anything if rhs has elements
			if(!rhs.empty()) {
				// lhs is empty, so we can just allocate the necessary space and copy in directly
				reserve(rhs.size());
				iend(copy_construct_from_range(iend(), rhs.ibegin(), rhs.iend()));
			}
		} else if(rhs.empty()) {
			// lhs is non-empty, rhs is empty, so we just need to clear
			clear();
		} else {
			// Neither side is empty, so there's no helping it, we'll have to use the ordinary
			// assign(it,it)
			assign(rhs.ibegin(), rhs.iend());
		}
	}

	return *this;
}

///////////////////////////////////////////////////////////
// Move-Assignment Operator
///////////////////////////////////////////////////////////

template<typename T, typename Alloc> 
inline vector<T,Alloc>& vector<T,Alloc>::operator=(vector&& rhs) 
noexcept(std::allocator_traits<Alloc>::propagate_on_container_move_assignment::value || 
		 std::allocator_traits<Alloc>::is_always_equal::value) {

	if(this != &rhs) { do_move_assign(std::move(rhs)); }
	return *this;
}

///////////////////////////////////////////////////////////
// Assign content
///////////////////////////////////////////////////////////

template<typename T, typename Alloc>
inline void vector<T,Alloc>::assign(size_type newSize, const_reference val) {
	// TODO: Consider removing this condition (may end up being more work than just running through
	// the default case).
	if(newSize == 0) { 
		clear(); 

	} else if(value_has_trivial_destroy || newSize > capacity()) {
		// If clearing is a no-op, or we need to reallocate anyway, clear, reserve, then 
		// copy-construct.
		clear();
		reserve(newSize);
		iend(construct_n(m_data, newSize, val));

	} else {
		// We have enough space, figure what the new m_size is going to be
		pointer const newEnd = m_data + newSize;
		// True if the new size is contained within the old size
		const bool sizeContained = newEnd <= iend();

		// First, try overwriting existing content
		pointer const position = assign_range(ibegin(), sizeContained ? newEnd : iend(), val);

		// If sizeContained, we may need to erase extra elements.  Otherwise, we just insert the 
		// remaining elements into uninitialised space
		if(sizeContained) {
			truncate_internal(position);
		} else {
			construct_range(iend(), newEnd, val);
			iend(newEnd);
		}
	}
}

template<typename T, typename Alloc>
template<typename InputIterator, typename>
__forceinline void vector<T,Alloc>::assign(InputIterator first, InputIterator last) {
	// Dispatch based on the iterator category
	using tag = kane::iterator_category<InputIterator>;
	do_assign(first, last, tag());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Iterators
///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////
// Normal iterators
///////////////////////////////////////

template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::iterator vector<T,Alloc>::begin() noexcept { return ibegin(); }
template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::iterator vector<T,Alloc>::end() noexcept { return iend(); }
template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::const_iterator vector<T,Alloc>::begin() const noexcept { return ibegin(); }
template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::const_iterator vector<T,Alloc>::end() const noexcept { return iend(); }

///////////////////////////////////////
// Reverse iterators
///////////////////////////////////////

template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::reverse_iterator vector<T,Alloc>::rbegin() noexcept { return reverse_iterator(end()); }
template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::reverse_iterator vector<T,Alloc>::rend() noexcept { return reverse_iterator(begin()); }
template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::const_reverse_iterator vector<T,Alloc>::rbegin() const noexcept { return const_reverse_iterator(end()); }
template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::const_reverse_iterator vector<T,Alloc>::rend() const noexcept { return const_reverse_iterator(begin()); }

///////////////////////////////////////
// Const iterators
///////////////////////////////////////

template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::const_iterator vector<T,Alloc>::cbegin() const noexcept { return begin(); }
template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::const_iterator vector<T,Alloc>::cend() const noexcept { return end(); }
template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::const_reverse_iterator vector<T,Alloc>::crbegin() const noexcept { return rbegin(); }
template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::const_reverse_iterator vector<T,Alloc>::crend() const noexcept { return rend(); }

///////////////////////////////////////
// POD back insert iterators
///////////////////////////////////////

template<typename T, typename Alloc>
inline typename vector<T,Alloc>::pod_back_insert_iterator vector<T,Alloc>::pod_back_inserter() noexcept { return pod_back_insert_iterator(*this); }

///////////////////////////////////////////////////////////////////////////////////////////////////
// Capacity and Size
///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////
// Information
///////////////////////////////////////

template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::size_type vector<T,Alloc>::size() const noexcept { return my_base::size(); }
template<typename T, typename Alloc> 
inline                              bool   vector<T,Alloc>::empty() const noexcept { return my_base::empty(); }
template<typename T, typename Alloc> 
inline                              bool   vector<T,Alloc>::full() const noexcept { return my_base::full(); }
template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::size_type vector<T,Alloc>::max_size() const noexcept { return my_base::max_size(); }
template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::size_type vector<T,Alloc>::capacity() const noexcept { return my_base::capacity(); }
template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::size_type vector<T,Alloc>::available() const noexcept { return my_base::available(); }

// Allocator
template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::allocator_type vector<T,Alloc>::get_allocator() const noexcept { return m_allocator(); }
template<typename T, typename Alloc> 
inline void vector<T,Alloc>::set_allocator(const allocator_type& newAlloc) { 
	// If our new allocator can handle our old memory, just set the new one
	if(alloc_is_always_equal || m_allocator() == newAlloc) {
		m_allocator() = newAlloc;

	} else if(empty()) {
		// Special case where we just need to deallocate
		deallocate();
		reset();
		m_allocator() = newAlloc;

	} else {
		// Otherwise, we need to allocate a new array and move our old content in.  We'll do this 
		// by creating a temporary vector with the new allocator, moving the old content to it, and
		// then swapping.  This will be slow if, for some reason, swapping can't be done in O(1), 
		// but in that case there's no way to make this function fast.

		// Because we know how our vector(i,j,a) constructor is built, we actually know that it's 
		// perfectly efficient for this specific case.  We could manually call 
		// vector_base::move_construct_from_range, of course, but the code would compile down to
		// the same thing.
		vector temp(std::make_move_iterator(ibegin()), std::make_move_iterator(iend()), newAlloc);
		// Then swap, and the old content will automatically be destroyed when temp goes out of 
		// scope.
		swap(temp);
	}
}

///////////////////////////////////////
// Capacity and size management
///////////////////////////////////////

// reallocate() checks for valid inputs on its own
template<typename T, typename Alloc> 
inline void vector<T,Alloc>::reserve(size_type neededSize) { reallocate(neededSize); }

template<typename T, typename Alloc> 
inline void vector<T,Alloc>::resize(size_type newSize, const_reference val) {
	const size_type currentSize = size();
	if(newSize < currentSize) { 
		truncate_internal(ibegin() + newSize);
	} else {
		const size_type elementsAdded = newSize - currentSize;
		if(elementsAdded != 0) {
			emplace_back_n_internal(elementsAdded, val);
		}
	}
}

template<typename T, typename Alloc> 
inline void vector<T,Alloc>::resize(size_type newSize) {
	const size_type currentSize = size();
	if(newSize < currentSize) { 
		truncate_internal(ibegin() + newSize);
	} else {
		const size_type elementsAdded = newSize - currentSize;
		if(elementsAdded != 0) {
			append_defaults(elementsAdded);
		}
	}
}

template<typename T, typename Alloc>
inline void vector<T,Alloc>::shrink_to_fit() { 
	if(m_size == m_data) {
		// Either already deallocated, or empty vector.  If initialised, deallocate.
		if(m_data) { deallocate(m_data, m_capacity); reset(); }

	} else if(m_size != m_capacity) {
		// Allocate a new, smaller array and move our elements over
		pointer const newData = allocate(size());
		pointer const newSize = move_construct_from_range(newData, ibegin(), iend());

		// Deallocate the current array
		destroy(ibegin(), iend());
		deallocate(m_data, m_capacity);

		// Set the new array
		reset(newData, newSize, newSize);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Element access
///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////
// Const accessors
///////////////////////////////////////

template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::const_reference vector<T,Alloc>::operator[](size_type index) const { return m_data[index]; }
template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::const_reference vector<T,Alloc>::at(size_type index) const { 
	if(m_data) { 
		// BUG: we *might* not be allowed to create an invalid index, for custom pointer types...
		const_pointer const position = m_data + index;
		if(position < iend()) { return *position; }
	} 
	
	throw std::out_of_range("Invalid index in vector<T>::at()");
}
// *snicker*
template<typename T, typename Alloc>
inline typename vector<T,Alloc>::const_reference vector<T,Alloc>::cat(size_type index) const {
	return at(index);
}
template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::const_reference vector<T,Alloc>::front() const { return *m_data; }
template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::const_reference vector<T,Alloc>::back() const { return *(iend() - 1); }

///////////////////////////////////////
// Non-const accessors
///////////////////////////////////////

template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::reference vector<T,Alloc>::operator[](size_type index) { return m_data[index]; }
template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::reference vector<T,Alloc>::at(size_type index) { 
	if(m_data) {
		// BUG: we *might* not be allowed to create an invalid index, for custom pointer types...
		pointer const position = m_data + index;
		if(position < iend()) { return *position; }
	}
	
	throw std::out_of_range();
}
template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::reference vector<T,Alloc>::front() { return *m_data; }
template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::reference vector<T,Alloc>::back() { return *(iend() - 1); }

///////////////////////////////////////
// Data access
///////////////////////////////////////

template<typename T, typename Alloc> inline T* vector<T,Alloc>::data() noexcept { return m_data; }
template<typename T, typename Alloc> inline const T* vector<T,Alloc>::data() const noexcept { return m_data; }

///////////////////////////////////////////////////////////////////////////////////////////////////
// Modifiers
///////////////////////////////////////////////////////////////////////////////////////////////////
	
///////////////////////////////////////////////////////////
// Push/Pop
///////////////////////////////////////////////////////////

template<typename T, typename Alloc>
inline void vector<T,Alloc>::push_back() { 
	if(full()) { reallocate(); }
	construct(iend());
	++m_size;
}

template<typename T, typename Alloc> 
inline void vector<T,Alloc>::push_back(const_reference val) { emplace_back_internal(val); }

template<typename T, typename Alloc> 
inline void vector<T,Alloc>::push_back(rvalue_reference val)  { emplace_back_internal(std::move(val)); }

template<typename T, typename Alloc> 
inline void vector<T,Alloc>::pop_back() { if(!empty()) { --m_size; destroy(iend()); } }

template<typename T, typename Alloc>
inline void vector<T,Alloc>::xpush_back(const_reference val) {
	if(full()) { reallocate(); }
	construct(iend(), val);
	++m_size;
}

template<typename T, typename Alloc>
inline void vector<T,Alloc>::xpush_back(rvalue_reference val) {
	if(full()) { reallocate(); }
	construct(iend(), std::move(val));
	++m_size;
}


///////////////////////////////////////////////////////////
// Insert
///////////////////////////////////////////////////////////

template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::iterator vector<T,Alloc>::insert(const_iterator pos, const_reference val) {
	pointer const position = iterator_to_pointer(pos);
	if(position == iend()) {
		return pointer_to_iterator(emplace_back_internal(val));
	} else {
		return pointer_to_iterator(emplace_internal(position, val));
	}
}

// Insert with move
template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::iterator vector<T,Alloc>::insert(const_iterator pos, rvalue_reference val) {
	// Turn const iterator into pointer (this'll also be useful if we're using checked iterators)
	pointer const position = iterator_to_pointer(pos);

	return pointer_to_iterator(
		(position == iend()) ? emplace_back_internal(std::move(val)) : emplace_internal(position, std::move(val))
	);
}

template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::iterator vector<T,Alloc>::insert(const_iterator pos, size_type count, const_reference val) {
	pointer const position = iterator_to_pointer(pos);

	return pointer_to_iterator(
		(position == iend()) ? emplace_back_n_internal(count, val) : emplace_n_internal(position, count, val)
	);
}


template<typename T, typename Alloc> 
template<typename InputIterator, typename>
inline typename vector<T,Alloc>::iterator vector<T,Alloc>::insert(const_iterator pos, InputIterator first, InputIterator last) {
	pointer const position = iterator_to_pointer(pos);
	
	if(position == iend()) {
		return pointer_to_iterator(append_range(first, last));
	} else {
		// The standard requires us to return the first-inserted element; what's most convenient 
		// internally is the one-past-the-last inserted.  So, we have to calculate it.
		const size_type oldSize = size();
		pointer const insertedEnd = insert_range(position, first, last);
		const size_type insertedCount = size() - oldSize;
		return pointer_to_iterator(insertedEnd - insertedCount);
	}
}

///////////////////////////////////////////////////////////
// Fast insert
///////////////////////////////////////////////////////////
template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::iterator vector<T,Alloc>::xinsert(const_iterator pos, const_reference val) {
	pointer const position = iterator_to_pointer(pos);
	if(position == iend()) {
		// TODO: Optimise
		xpush_back(val);
		return pointer_to_iterator(iend() - 1);

	} else {
		pointer const newPosition = make_gap_1(position);
		if(value_has_trivial_destroy || newPosition != position) {
			construct(newPosition, val);
		} else {
			*newPosition = val;
		}
		return pointer_to_iterator(newPosition);
	}
}

// Insert with move
template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::iterator vector<T,Alloc>::xinsert(const_iterator pos, rvalue_reference val) {
	pointer const position = iterator_to_pointer(pos);
	if(position == iend()) {
		// TODO: Optimise
		xpush_back(std::move(val));
		return pointer_to_iterator(iend() - 1);

	} else {
		pointer const newPosition = make_gap_1(position);
		if(value_has_trivial_destroy || newPosition != position) {
			construct(newPosition, std::move(val));
		} else {
			*newPosition = std::move(val);
		}
		return pointer_to_iterator(newPosition);
	}
}

template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::iterator vector<T,Alloc>::xinsert(const_iterator pos, size_type count, const_reference val) {
	pointer const position = iterator_to_pointer(pos);

	if(position == iend()) {
		if(many(size() + count)) {
			reallocate(best_capacity(capacity() + count));
		}
		pointer const result = iend();
		iend(construct_n(result, count, val));
		return pointer_to_iterator(result);

	} else {
		// ranges.first is the start of the initialised segment, ranges.second is the start of the 
		// uninitialised segment.  newPosition is the new location of the element at position
		const std::pair<pointer, pointer> ranges = make_gap_n(position);
		pointer const newPosition = ranges.first + count;

		// If value_has_trivial_destory, we can use copy construction for the whole range.
		// Otherwise, we need to split it up.
		// TODO: Check if the compiler would optimise this without needing the test in this method
		if(value_has_trivial_destroy) {
			construct_range(ranges.first, newPosition, val);
		} else {
			// Copy construct the first range, copy assign the second range
			construct_range(ranges.first, ranges.second, val);
			assign_range(ranges.second, newPosition, val);
		}

		return pointer_to_iterator(ranges.first);

	}
}

///////////////////////////////////////////////////////////
// Emplace
///////////////////////////////////////////////////////////

template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::reference vector<T,Alloc>::emplace_back() { 
	if(full()) { reallocate(); }
	// We need to return a reference to the inserted element, so store the pointer
	pointer const result = iend();
	construct(result);
	++m_size;
	return *result;
}

template<typename T, typename Alloc>
inline typename vector<T,Alloc>::iterator vector<T,Alloc>::emplace(const_iterator pos) {
	pointer const position = iterator_to_pointer(pos);
	// Have to do this test, because make_gap_1 is written in a way that'll go wrong if you give 
	// it iend()
	if(position == iend()) {
		emplace_back();
		return pointer_to_iterator(iend() - 1);
	} else {
		pointer const newPosition = make_gap_1(position);
		if(value_has_trivial_destroy || position != newPosition) {
			construct(newPosition);
		} else {
			*newPosition = value_type();
		}
		return pointer_to_iterator(newPosition);
	}
}

template<typename T, typename Alloc> 
template<typename... Args>
inline typename vector<T,Alloc>::reference vector<T,Alloc>::emplace_back(Args&&... args) { return *emplace_back_internal(std::forward<Args>(args)...); }

template<typename T, typename Alloc>
template<typename... Args>
inline typename vector<T,Alloc>::iterator vector<T,Alloc>::emplace(const_iterator pos, Args&&... args) {
	pointer const position = iterator_to_pointer(pos);

	if(position == iend()) {
		return pointer_to_iterator(emplace_back_internal(std::forward<Args>(args)...));
	} else {
		return pointer_to_iterator(emplace_internal(position, std::forward<Args>(args)...));
	}
}

///////////////////////////////////////////////////////////
// Erase
///////////////////////////////////////////////////////////

template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::iterator vector<T,Alloc>::erase(const_iterator pos) {
	pointer const position = iterator_to_pointer(pos);
	// The algorithm is actually the same no matter how many things you're erasing
	// (Except if you're erasing the end, in which case it could potentially be optimised slightly,
	// but it's really not worth the effort.)
	return pointer_to_iterator(erase_internal(position, position + 1));
}

template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::iterator vector<T,Alloc>::erase(const_iterator first0, const_iterator last0) {
	pointer const first = iterator_to_pointer(first0);
	pointer const last = iterator_to_pointer(last0);

	if(first == last) { 
		return pointer_to_iterator(first);
	} else if(last == iend()) {
		return pointer_to_iterator(truncate_internal(first));
	} else {
		return pointer_to_iterator(erase_internal(first, last));
	}
}

///////////////////////////////////////////////////////////
// Swap and clear
///////////////////////////////////////////////////////////
template<typename T, typename Alloc> 
inline void vector<T,Alloc>::swap(vector& vec) 
	noexcept(std::allocator_traits<Alloc>::propagate_on_container_swap::value || 
			 std::allocator_traits<Alloc>::is_always_equal::value) {

	std::swap(m_data, vec.m_data);
	std::swap(m_size, vec.m_size);
	std::swap(m_capacity, vec.m_capacity);
	if(alloc_propagate_swap) {
		std::swap(m_allocator(), vec.m_allocator());
	}
}

template<typename T, typename Alloc> inline void vector<T,Alloc>::clear() noexcept { truncate_internal(m_data); }


///////////////////////////////////////////////////////////////////////////////////////////////////
// Modifiers (Non-Standard Extensions)
///////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////
// Take
///////////////////////////////////////////////////////////

template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::value_type vector<T,Alloc>::take_back() {
	if(empty()) {
		// Could throw an exception for this, I suppose
		// Wait, no, standard says pop_back() shall never throw an exception, so neither should 
		// take_back()
		return value_type();
	} else {
		--m_size;
		value_type result( std::move(*iend()) );
		
		// Forgot to destroy iend() the last time; remember, the source of a moved element is still
		// initialised, and thus must be destroyed.
		destroy(iend());

		// Return temporary, using NRVO
		return result;
	}
}

template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::value_type vector<T,Alloc>::take(const_iterator pos) {
	pointer const position = iterator_to_pointer(pos);
	// Grab temporary
	value_type result( std::move(*position) );
	
	// Shift everything back, and destroy the end
	move_assign_from_range(position, position + 1, iend());
	--m_size;
	destroy(iend());

	// Return temporary, using NRVO
	return result;
}

template<typename T, typename Alloc>
template<typename OutputIterator>
inline typename vector<T,Alloc>::iterator vector<T,Alloc>::take(const_iterator first0, const_iterator last0, OutputIterator out) {
	pointer const first = iterator_to_pointer(first0);
	pointer const last = iterator_to_pointer(last0);
	// Allow taking out as a reference-wrapped iterator, so the user can get their iterator back
	// TODO: See if this works without unwrap_ref
	//auto out = boost::unwrap_ref(out0);

	// First, move the elements out to the output iterator
	for(pointer i = first; i != last; ++i, ++out) { *out = std::move(*i); }
	// Then use the internal function to erase
	return pointer_to_iterator(last == iend() ? truncate_internal(first) : erase_internal(first, last));
}

//////////////////////////////////////////////////////////
// Replace
//////////////////////////////////////////////////////////

// Replace range with N default-constructed elements
template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::iterator vector<T,Alloc>::replace(const_iterator first0, const_iterator last0, size_type count) {
	pointer first = iterator_to_pointer(first0);
	pointer const last = iterator_to_pointer(last0);

	// Get the number of elements we're replacing
	const size_type rangeSize = last - first;

	// Figure out whether we're increasing or decreasing the size
	if(count <= rangeSize) {
		// Decreasing the size, so this'll be an assign + erase.  assign_n() will perform some 
		// trivial-destroy optimisations if they're available.
		pointer const mid = assign_n(first, count);
		// If count == rangeSize, this erase will get skipped immediately.
		return pointer_to_iterator(erase_internal(mid, last));

	} else {
		// Increasing the size.  By enough to require a reallocation?
		const size_type insertSize = count - rangeSize;

		if(!many(insertSize)) {
			// Increasing the size, but not by enough to force a reallocation.  First, move the suffix 
			// forward...
			pointer const ufirst = move_forward_n(last, insertSize);
			pointer const ulast = first + count;
			// Default-assign into the initialised elements
			assign_range(first, ufirst);
			// Then default-construct the remainder, if it exists
			construct_range(ufirst, ulast);
			// This can get a trivial-destroy optimisation, but I'm not going to bother yet.  If I 
			// find this case--the assign_range/construct_range thing--popping up elsewhere, I'll 
			// write a helper function to optimise it.

			return pointer_to_iterator(ulast);

		} else {
			// Increasing the size by loads, so we have to reallocate.  We'll do it manually here, so
			// we can skip moving the elements to be overwritten, since they're to be erased anyway.
			// Allocate the best amount...
			const size_type oldCapacity = capacity();
			const size_type newCapacity = best_capacity(size() + insertSize);
			const pointer newData = allocate(newCapacity);
		
			// Move the prefix over...
			const pointer insertFirst = move_construct_from_range(newData, ibegin(), first);
			// ...default-construct the elements to be inserted...
			const pointer insertLast = construct_n(insertFirst, count);
			// ...then move the suffix.
			const pointer newSize = move_construct_from_range(insertLast, last, iend());

			// Destroy and deallocate the old array.
			destroy(ibegin(), iend());
			deallocate(m_size, oldCapacity);

			// And set the new array.
			reset(newData, newSize, newCapacity);

			return pointer_to_iterator(insertLast);
		}
	}
}
template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::iterator vector<T,Alloc>::replace(const_iterator first0, const_iterator last0, size_type count, const_reference val) {
	pointer first = iterator_to_pointer(first0);
	pointer const last = iterator_to_pointer(last0);

	// Because of insert quirks (namely, val is allowed to be in the vector), insert has some 
	// special considerations.  Given that, it's worth it to reduce to an insert if first == last.
	if(first == last) { 
		// Check for dumb user
		if(count > 0) { 
			return pointer_to_iterator(emplace_n_internal(first, count, val) + count); 
		} else {
			return pointer_to_iterator(first);
		}
	}

	// Get the number of elements we're replacing
	const size_type rangeSize = last - first;

	// Figure out whether we're increasing or decreasing the size
	if(count <= rangeSize) {
		// Decreasing the size, so this'll be an assign + erase.  assign_n() will perform some 
		// trivial-destroy optimisations if they're available.
		pointer const mid = assign_n(first, count, val);
		// If count == rangeSize, this erase will get skipped immediately.
		return pointer_to_iterator(erase_internal(mid, last));

	} else {
		// Increasing the size.  By enough to require a reallocation?
		const size_type insertSize = count - rangeSize;

		if(!many(insertSize)) {
			// Increasing the size, but not by enough to force a reallocation.  Gotta get clever 
			// here, because we have no guarantee that val isn't in the suffix.  We'll assign the 
			// first element of the replaced range, and then use that element as the exemplar for
			// the rest of the function.
			*first = val;

			// Now, move the suffix forward
			pointer const ufirst = move_forward_n(last, insertSize);
			pointer const ulast = first + count;
			// Default-assign into the initialised elements
			assign_range(first + 1, ufirst, *first);
			// Then copy-construct the remainder, if it exists
			construct_range(ufirst, ulast, *first);
			// This can get a trivial-destroy optimisation, but I'm not going to bother yet.  If I 
			// find this case--the assign_range/construct_range thing--popping up elsewhere, I'll 
			// write a helper function to optimise it.

			return pointer_to_iterator(ulast);

		} else {
			// Increasing the size by loads, so we have to reallocate.  We'll do it manually here, so
			// we can skip moving the elements to be overwritten, since they're to be erased anyway.
			// As with above, we need to assign val into the new array first so we can reuse it as
			// the exemplar.
			// Allocate the best amount...
			const size_type oldCapacity = capacity();
			const size_type newCapacity = best_capacity(size() + insertSize);
			const pointer newData = allocate(newCapacity);

			// Assign our exemplar...
			pointer const exemplar = newData + (first - ibegin());
			*exemplar = val;
		
			// Move the prefix over...
			move_construct_from_range(newData, ibegin(), first);
			// ...copy-construct the elements to be inserted...
			const pointer insertLast = construct_n(exemplar + 1, count - 1, *exemplar);
			// ...then move the suffix.
			const pointer newSize = move_construct_from_range(insertLast, last, iend());

			// Destroy and deallocate the old array.
			destroy(ibegin(), iend());
			deallocate(m_size, oldCapacity);

			// And set the new array.
			reset(newData, newSize, newCapacity);

			return pointer_to_iterator(insertLast);
		}
	}
}

template<typename T, typename Alloc> 
template<typename InputIterator, typename>
inline typename vector<T,Alloc>::iterator vector<T,Alloc>::replace(const_iterator first0, const_iterator last0, InputIterator first2, InputIterator last2) {
	pointer first1 = iterator_to_pointer(first0);
	pointer const last1 = iterator_to_pointer(last0);

	// Check that we can't reduce this to an insert or erase
	if(first1 == last1) {
		if(first2 != last2) {
			const size_type oldSize = size();
			pointer const insertFirst = insert_range(first1, first2, last2);
			const size_type newSize = size();
			return pointer_to_iterator(insertFirst - count); 
		} else {
			return pointer_to_iterator(first);
		}
	} else if(first2 == last2) {
		erase_internal(first1, last1);
		return first0;
	}

	using tag = iterator_category<InputIterator>;
	return do_replace_range(first1, last1, first2, last2, tag());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Helper Functions
///////////////////////////////////////////////////////////////////////////////////////////////////

// For these, it's safe to cast to const pointer...  though unpleasant
template<typename T, typename Alloc> inline 
typename vector<T,Alloc>::pointer vector<T,Alloc>::iterator_to_pointer(iterator i) const { return i; }
template<typename T, typename Alloc> inline 
typename vector<T,Alloc>::pointer vector<T,Alloc>::iterator_to_pointer(const_iterator i) const { return const_cast<pointer>(i); }

template<typename T, typename Alloc> inline 
typename vector<T,Alloc>::iterator vector<T,Alloc>::pointer_to_iterator(pointer i) const { return i; }
template<typename T, typename Alloc> inline 
typename vector<T,Alloc>::iterator vector<T,Alloc>::pointer_to_iterator(const_pointer i) const { return const_cast<iterator>(i); }

///////////////////////////////////////////////////////////////////////////////
// Construction Helpers
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////
// Construct with initial size
///////////////////////////////////////
// For these initialSize constructors, the memory is already allocated by a base class constructor, 
// we just need to initialise it and update m_size.
template<typename T, typename Alloc>
inline void vector<T,Alloc>::do_construct(size_type initialSize) {
	// Totally unnecessary comparison, but what if some dickish allocator gives 
	// us a signed size_type?
	if(initialSize > 0) { iend(construct_n(m_data, initialSize)); }
}
template<typename T, typename Alloc>
inline void vector<T,Alloc>::do_construct(size_type initialSize, const_reference val) {
	// Totally unnecessary comparison, but what if some dickish allocator gives 
	// us a signed size_type?
	if(initialSize > 0) { iend(construct_n(m_data, initialSize, val)); }
}

template<typename T, typename Alloc>
template<typename InputIterator, typename> 
inline typename vector<T,Alloc>::size_type vector<T,Alloc>::select_capacity_from_range(InputIterator first, InputIterator last) {
	if(is_exactly_input_iterator<InputIterator>::value) {
		return second_capacity_increment;
	} else {
		return size_type(std::distance(first, last));
	} 
}

///////////////////////////////////////
// Construct from range
///////////////////////////////////////

template<typename T, typename Alloc>
template<typename InputIterator>
void vector<T,Alloc>::do_construct_range(InputIterator first, InputIterator last, const std::input_iterator_tag) {
	// To avoid reallocations, we'll pick a small initial capacity and try inserting up to that
	// before falling back on the horrible input iterator insert routine.  The second capacity 
	// increment (4) is the current choice.  (This is done in the constructor with 
	// select_capacity_from_range().)
	// If the user wants to reclaim the excess memory when inserting a small number of elements, 
	// they can use shrink_to_fit(), or they could just manually set a capacity before assigning
	// content with assign(i,j).
	std::tie(m_size, first) = checked_copy_construct_range(ubegin(), uend(), first, last);
	
	// If we're done, awesome.
	if(first == last) { return; }

	// Otherwise, we still have elements to insert, so we'll have to do horrible things
	const size_type numCap = capacity();
	const horrible_insert_helper horrible(insert_horrible(numCap, numCap, numCap, first, last));
	// Move in the initially-grabbed elements
	move_construct_range_n(horrible.newData, ibegin(), numCap);
	// Destroy and deallocate the temp array
	destroy(ibegin(), iend());
	deallocate(m_data, numCap);
	// Set the final array
	reset(horrible.newData, horrible.newSize, horrible.newCapacity);
}

template<typename T, typename Alloc>
template<typename ForwardIterator>
inline void vector<T,Alloc>::do_construct_range(ForwardIterator first, ForwardIterator last, const std::forward_iterator_tag) {
	// Note that determining the size of [first,last) is already done in the constructor with 
	// select_capacity_from_range().
	std::tie(m_size, first) = copy_construct_range_n(ubegin(), first, available());
	// Should assert first == last here
	_ASSERTE(first == last);
}

template<typename T, typename Alloc>
template<typename InputIterator>
__forceinline void vector<T,Alloc>::do_construct_range(InputIterator first, InputIterator last) {
	using tag = iterator_category<InputIterator>;
	do_construct_range(first, last, tag());
}

///////////////////////////////////////////////////////////
// Assignment Helpers
///////////////////////////////////////////////////////////

template<typename T, typename Alloc>
inline void vector<T,Alloc>::do_move_assign(vector&& rhs) {
	// Two different approaches, based on whether allocators propogate on move
	if(alloc_propagate_move == false) { 
		// We're keeping our allocator.  If our allocator can accept the other vector's memory,
		// we can just do a swap-and-clear.
		if(alloc_is_always_equal || m_allocator() == rhs.m_allocator()) {
			std::swap(m_data, rhs.m_data);
			std::swap(m_size, rhs.m_size);
			std::swap(m_capacity, rhs.m_capacity);

		// Otherwise, we need to do a full move assignment, same as calling assign().
		} else {
			assign(std::make_move_iterator(rhs.ibegin()), std::make_move_iterator(rhs.iend()));
		}

		// Either way, we need to clear the RHS
		rhs.clear();

	} else {
		// We're moving the allocator, so we can move the memory as well, so start by clearing our
		// existing memory.
		clear();

		// If the rhs's allocator can handle our memory, we can potentially do a swap, leaving our 
		// empty memory in the rhs.  If it's an xvalue, this doesn't really matter, because it's going 
		// to die soon.  If it's not an xvalue, then giving it our memory may save a few costly 
		// reallocations down the line if it's later reused.
		// We only need to consider this if we actually have data to swap.
		if(m_data) {
			// When the allocators are always equal, we don't even need to test anything, and since 
			// this is the most common case, we'll optimise for it.  This is a compile-time constant,
			// so the compiler should elide the impossible path.
			if(alloc_is_always_equal) {
				m_allocator() = std::move(rhs.m_allocator());  // Has to be a no-op, surely
				std::swap(m_data, rhs.m_data);
				std::swap(m_size, rhs.m_size);
				std::swap(m_capacity, rhs.m_capacity);

			} else {
				// Alright, gotta test for allocator equality.  Moving an allocator could potentially
				// change the equality comparison, though this is unlikely.
				allocator_type newAlloc(std::move(rhs.m_allocator()));

				// Check if the rhs's (post-move) allocator can handle our old memory.
				if(m_allocator() == rhs.m_allocator()) {
					// Awesome, rhs can handle our memory, so we can just swap pointers and return.
					std::swap(m_data, rhs.m_data);
					std::swap(m_size, rhs.m_size);
					std::swap(m_capacity, rhs.m_capacity);

				} else {
					// rhs can't handle our memory, so we need to deallocate...
					deallocate(m_data, m_capacity);
					// ...take its pointers...
					reset(rhs.m_data, rhs.m_size, rhs.m_capacity);
					// ...and reset it.
					rhs.reset();
				}

				// Either way, we now move our new allocator in.
				m_allocator() = std::move(newAlloc);
			}
		} else {
			// No data to swap, so move the allocator and the pointers
			m_allocator() = std::move(rhs.m_allocator());
			reset(rhs.m_data, rhs.m_size, rhs.m_capacity);
			rhs.reset();
		}
	}
}

///////////////////////////////////////////////////////////
// Insert-At-End helpers
///////////////////////////////////////////////////////////

template<typename T, typename Alloc>
template<typename InputIterator> 
inline typename vector<T,Alloc>::pointer vector<T,Alloc>::append_range(InputIterator first, InputIterator last, const std::input_iterator_tag) {
	// If uninitialised, allocate space for at least 16 elements (chances are, this'll shake out to
	// 64 or 128 bytes, so one or two cache lines).
	if(!m_data) { reset(allocate(16), 16); }

	const size_type oldSize = size();

	// First, try appending into the empty space
	if(!full()) {
		pointer position;
		std::tie(position, first) = checked_copy_construct_range(ubegin(), uend(), first, last);
		iend(position);
	}

	// If not done, do horrible things
	if(first != last) {
		const size_type oldCapacity = capacity();
		const horrible_insert_helper horrible(insert_horrible(oldCapacity, oldCapacity, oldCapacity, first, last));
		
		// Move the old elements into the new array
		move_construct_range_n(horrible.newData, ibegin(), oldCapacity);

		// Destroy old array
		destroy(ibegin(), iend());
		deallocate(m_data, oldCapacity);

		// Set new array
		reset(horrible.newData, horrible.newSize, horrible.newCapacity);
	}

	return ibegin() + oldSize;
}
	
template<typename T, typename Alloc>
template<typename ForwardIterator>
inline typename vector<T,Alloc>::pointer vector<T,Alloc>::append_range(ForwardIterator first, ForwardIterator last, const std::forward_iterator_tag) {
	const size_type count = static_cast<size_type>(std::distance(first, last));
	reserve(size() + count);

	pointer const oldEnd = iend();
	iend( copy_construct_range_n(oldEnd, first, count).first );
	return oldEnd;
}

template<typename T, typename Alloc>
template<typename InputIterator>
__forceinline typename vector<T,Alloc>::pointer vector<T,Alloc>::append_range(InputIterator first, InputIterator last) {
	using tag = iterator_category<InputIterator>;
	return append_range(first, last, tag());
}

// Append N default-constructed elements
template<typename T, typename Alloc>
typename typename vector<T,Alloc>::pointer vector<T,Alloc>::append_defaults(size_type sz) {
	reserve(size() + sz);
	pointer const oldEnd = iend();
	iend(construct_n(oldEnd, sz));
	return oldEnd;
}

///////////////////////////////////////////////////////////
// Erase helpers
///////////////////////////////////////////////////////////

template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::pointer vector<T,Alloc>::erase_internal(pointer first, pointer last) {
	// Move the suffix backward onto the elements to be erased, then erase everything from the end
	// of the suffix to the end of the allocated space.
	// So easy!
	truncate_internal( move_assign_from_range(first, last, iend()) );
	return first;
}
template<typename T, typename Alloc> 
inline typename vector<T,Alloc>::pointer vector<T,Alloc>::truncate_internal(pointer last) {
	destroy(last, iend());
	iend(last);
	return last;
}

///////////////////////////////////////////////////////////
// Insert dispatches
///////////////////////////////////////////////////////////

// TODO: Fix this whole damn function
template<typename T, typename Alloc>
template<typename InputIterator> 
inline typename vector<T,Alloc>::pointer vector<T,Alloc>::insert_range(pointer position, InputIterator first, InputIterator last, const std::input_iterator_tag) {
	if(first == last) { 
		return position; 
	}
	
	// This MAY require a horribleness.  If it does, it'll involve rearranging a bunch of chunks of
	// this vector.  We can simplify that with this array:
	std::pair<pointer, pointer> oldRanges[4];
	// 0 is the prefix, 3 is the suffix, 1 and 2 can be used below for temporary input ranges.
	oldRanges[0] = std::make_pair(ibegin(), position);
	oldRanges[1] = oldRanges[2] = std::make_pair(nullptr, nullptr);
	oldRanges[3] = std::make_pair(position, iend());

	// Store the sizes temporarily
	const size_type suffixSize(iend() - position);
	const size_type remainderSize(remainder());
	const bool suffixSmaller = suffixSize < remainderSize;

	// If this vector's already full, we obviously need to reallocate.  Otherwise, it's more 
	// complicated.
	if(!full()) {		
		// Okay, not an easy insert.  Start by assuming that we won't need to reallocate.  There are
		// really two ways to deal with this: we could use MSVC's append-and-rotate approach, which is
		// decent for small POD types, or we could create some temporary memory and use it as a buffer.
		// We'll go with the latter approach, because it has better time costs when allocations are 
		// relatively cheap (which is usually the case).

		// Given this, let's consider the insert position.  We'll use a temporary buffer the size 
		// of the smaller of the suffix and the available capacity, to minimise the memory 
		// overhead.  If the suffix is smaller, once we've filled the buffer, we can move on to 
		// inserting into the available capacity until that's consumed as well.
		// Additionally, we could allocate the temporary buffer on the stack if it's going to be 
		// small enough, but we'll trust the allocator to optimise small allocations for us.
		// Another possible optimisation is to use another Kane::Vector when the buffer we'd need is
		// sufficiently large, so as to avoid unnecessary large allocations when the input range is 
		// small.  (TODO: Consider this for later.)

		// We do this slightly differently depending on which is smaller, but either way, we start
		// by inserting into the temp buffer.
		// TODO: Not sure why the suffixSmaller thing here
		pointer const tempData = allocate(suffixSmaller ? suffixSize : remainderSize);
		pointer const tempCapacity = tempData + (suffixSmaller ? suffixSize : remainderSize);
		pointer tempSize;
		std::tie(tempSize, first) = checked_copy_construct_range(tempData, tempCapacity, first, last);

		// Check if we're finished.  If so, we can bail early.
		if(first == last) { 
			// Insert the elements from the temp buffer (using random-access iterators) and return
			insert_range(position, std::make_move_iterator(tempData), std::make_move_iterator(tempSize));
			destroy(tempData, tempSize);
			deallocate(tempData, tempCapacity);
			return position;
		}

		// Still elements left in the input range.  We'll probably have to do horrible things.  
		// Add the temp buffer's elements to the oldRanges.
		oldRanges[1] = std::make_pair(tempData, tempSize);

		// If the suffix was smaller, we can also try inserting into some remaining capacity...
		if(suffixSmaller) {
			// tempData currently holds exactly suffixSize elements, so any further elements would 
			// be inserted starting at ubegin()--since the suffix has been pushed forward to that 
			// point, already.  So, we start inserting elements there, and continue until we run 
			// out of space to move the suffix up into position.
			pointer fallbackEnd;
			std::tie(fallbackEnd, first) = checked_copy_construct_range(ubegin(), uend() - suffixSize, first, last);

			if(first == last) {
				// Avoided reallocation at the last minute!
				// Just move the suffix to the correct location and replace it with the temp data.
				iend( move_construct_range_n(fallbackEnd, position, suffixSize).first );
				move_construct_range_n(position, tempData, suffixSize);
				destroy(tempData, tempSize);
				deallocate(tempData, tempCapacity);
				return position;
			}
			
			// Well, that amounted to nothing, set the fallback buffer range
			// Note that we now have suffixSize capacity remaining--the space we reserved at the 
			// end of the array to move the suffix up to.  We could, conceivably, continue using up
			// that capacity before going to the horrible stage.  (TODO: consider this.)
			oldRanges[2] = std::make_pair(ubegin(), fallbackEnd);
		}
	}

	// Alright, definitely need to do horrible things.  Basically, at this point, we have a new 
	// "prefix" consisting of the old prefix (oldRanges[0]) plus the temporarily-inserted elements
	// (oldRanges[1] and oldRanges[2]), and therefore a new insert position.
	// So, figure out the index of the old insert position and the new one.
	const size_type oldIndex = position - ibegin();
	const size_type newIndex = (oldRanges[0].second - oldRanges[0].first) +
	                           (oldRanges[1].second - oldRanges[1].first) +
	                           (oldRanges[2].second - oldRanges[2].first);

	// Do the horrible insert.  Note that we increased the size, so we have to specify that in the 
	// parameters.
	const horrible_insert_helper horrible(insert_horrible(newIndex, newIndex + suffixSize, capacity(), first, last));
	
	// Move the old prefixes into the new array
	pointer recombinePosition = horrible.newData;
	recombinePosition = move_construct_from_range(recombinePosition, oldRanges[0].first, oldRanges[0].second);
	recombinePosition = move_construct_from_range(recombinePosition, oldRanges[1].first, oldRanges[1].second);
	                    move_construct_from_range(recombinePosition, oldRanges[2].first, oldRanges[2].second);
	// Move the old suffix into the new array
	pointer const newNewSize = move_construct_from_range(horrible.newSize, oldRanges[3].first, oldRanges[3].second);

	// Destroy all the old ranges
	destroy(oldRanges[0].first, oldRanges[0].second);
	destroy(oldRanges[1].first, oldRanges[1].second);
	destroy(oldRanges[2].first, oldRanges[2].second);
	destroy(oldRanges[3].first, oldRanges[3].second);

	// Deallocate the temp buffer, which is always oldRanges[1].
	// (THAT MEANS CHANGE THIS LINE IF YOU CHANGE THAT INDEX.)
	deallocate(oldRanges[1].first, oldRanges[1].second);
	// Deallocate the current array
	deallocate(ibegin(), uend());

	// Set the new array and return the adjusted pointer
	reset(horrible.newData, newNewSize, horrible.newCapacity);
	return ibegin() + oldIndex;
}

// Fast implementation of insert for forward iterators.
template<typename T, typename Alloc>
template<typename ForwardIterator> 
inline typename vector<T,Alloc>::pointer vector<T,Alloc>::insert_range(pointer position, ForwardIterator first, ForwardIterator last, const std::forward_iterator_tag) {
	// Get the number of elements to insert
	const size_type count = static_cast<size_type>(std::distance(first, last));
	
	if(!many(count)) {
		// New elements will fit within our existing capacity.  Make space for them.  The space 
		// will be split into an initialised prefix and an uninitialised suffix.  ufirst points to
		// the first uninitialised element
		pointer const ufirst = move_forward_n(position, count);

		if(value_has_trivial_destroy) {
			// If values can be trivially destroyed, don't worry about treating the initialised 
			// and uninitialised elements differently.  "Destroy" the initialised ones (as a no-op)
			// and construct the whole rage.
			copy_construct_range_n(position, first, count);

		} else {
			// The initialised range is [position, ufirst)
			// The uninitialised range is [ufirst, ulast)
			pointer const ulast = position + count;
			first = copy_assign_range(position, ufirst, first);
			        copy_construct_range(ufirst, ulast, first);
		}

		return ufirst;

	} else {
		// Have to reallocate.  Allocate the new array first
		const size_type oldCapacity = capacity();
		const size_type newCapacity = best_capacity(size() + count);
		pointer const newData = allocate(newCapacity);
		
		// Move the prefix from the old array...
		pointer const newPosition = move_construct_from_range(newData, ibegin(), position);
		// Then insert the new elements...
		pointer const insertedEnd = move_construct_from_range(newPosition, first, last);
		// Then move the suffix from the old array.
		pointer const newSize = move_construct_from_range(insertedEnd, position, iend());

		// Finally, destroy and deallocate the old array, and set the new one
		destroy(ibegin(), iend());
		deallocate(m_data, oldCapacity);
		reset(newData, newSize, newCapacity);

		return insertedEnd;
	}
}

template<typename T, typename Alloc>
template<typename InputIterator>
__forceinline typename vector<T,Alloc>::pointer vector<T,Alloc>::insert_range(pointer position, InputIterator first, InputIterator last) {
	using tag = iterator_category<InputIterator>;
	return insert_range(first, last, tag());
}

///////////////////////////////////////
// Assign from single-pass range
///////////////////////////////////////

template<typename T, typename Alloc>
template<typename InputIterator>
inline void vector<T,Alloc>::do_assign(InputIterator first, InputIterator last, const std::input_iterator_tag) {
	// Since we can only make a single pass through the input, we have to just blindly insert.
	// Still, there are a few optimisations we can make.
	if(value_has_trivial_destroy) {
		// If we have a trivial destructor, then we can clear the vector as a no-op, then
		// insert directly into uninitialised space with copy construction.
		clear();
		append_range(first, last);
	} else {
		// Without trivial destructors, we don't want to clear unless absolutely necessary.
		// First, try overwriting our existing content
		pointer position;
		std::tie(position, first) = checked_copy_assign_range(ibegin(), iend(), first, last);
		
		if(first == last) {
			// If we consumed the entire input sequence, then we need to erase the remaining 
			// elements (if there are any)
			truncate_internal(position);
		} else {
			// Otherwise, we still have elements remaining, fall back onto insert_at_end
			append_range(first, last);
		}
	}
}

///////////////////////////////////////
// Assign from multi-pass range
///////////////////////////////////////

template<typename T, typename Alloc>
template<typename ForwardIterator>
inline void vector<T,Alloc>::do_assign(ForwardIterator first, ForwardIterator last, std::forward_iterator_tag) {
	// Assuming iteration is orders of magnitude faster than multiple copy constructions and 
	// reallocations...  (Probably a safe assumption.)
	const size_type newSize = static_cast<size_type>(std::distance(first, last));

	// If we have a trivial destructor, then we can clear the vector as a no-op, then insert directly
	// into uninitialised space with copy construction.  Alternatively, if a reallocation is required, 
	// might as well clear first to save unnecessary copying.
	if(value_has_trivial_destroy || newSize > capacity()) {
		// Clear and reserve the necessary space
		clear();
		reserve(newSize);
		// Insert will happen at the end of the function

	} else {
		// Without trivial destructors, we'll first try overwriting our existing elements.
		pointer position;
		std::tie(position, first) = checked_copy_assign_range(ibegin(), iend(), first, last);
		
		if(first == last) {
			// If we consumed the entire input sequence, then we just need to erase the remaining 
			// elements (if there are any), and then we're finished
			truncate_internal(position);
			return;
		} 
		// Otherwise, we hit the end of the initialised sequence, so continue below...
	}

	// If we get here, all remaining elements can be placed at the end of the initialised sequence
	iend( copy_construct_from_range(iend(), first, last) );
}

template<typename T, typename Alloc>
template<typename InputIterator>
__forceinline void vector<T,Alloc>::do_assign(InputIterator first, InputIterator last) {
	using tag = iterator_category<InputIterator>;
	do_assign(first, last, tag());
}

///////////////////////////////////
// Replace Dispatches
///////////////////////////////////

// Slow implementation of insert for input iterators.
template<typename T, typename Alloc>
template<typename InputIterator> 
inline typename vector<T,Alloc>::iterator vector<T,Alloc>::do_replace_range(pointer first1, pointer last1, InputIterator first2, InputIterator last2, const std::input_iterator_tag) {
	// First, try assigning as many elements as we can
	std::tie(first1, first2) = checked_copy_assign_range(first1, last1, first2, last2);

	// Then, handle the excess...
	if(first2 == last2) {
		// If we have leftover elements in the range, erase them
		return erase(pointer_to_iterator(first), pointer_to_iterator(last));
	} else {
		// Otherwise, if we have elements to insert, insert them.  Internally, this'll use the 
		// horrible insert algorithm.
		const size_type oldSize = size();
		iterator tempResult = insert(pointer_to_iterator(last), first2, last2);
		// TODO: Damnit, these return values.  Maybe make this return the "correct" result?
		return tempResult + (size() - oldSize);
	}
}

// Fast implementation of replace with range for forward iterators
template<typename T, typename Alloc>
template<typename InputIterator> 
inline typename vector<T,Alloc>::iterator vector<T,Alloc>::do_replace_range(pointer first, pointer last, InputIterator first2, InputIterator last2, const std::forward_iterator_tag) {
	// Get the size of the input range
	size_type count = static_cast<size_type>(std::distance(first2, last2));

	// Get the number of elements we're replacing
	const size_type rangeSize = last - first;

	// We already know neither of them are 0

	// Figure out whether we're increasing or decreasing the size
	if(count <= rangeSize) {
		// Decreasing the size, so this'll be a copy_assign + erase.
		pointer const mid = copy_assign_from_range(first, first2, last2);
		// If count == rangeSize, this erase will get skipped immediately.
		return pointer_to_iterator(erase_internal(mid, last));

	} else {
		// Increasing the size.  By enough to require a reallocation?
		const size_type insertSize = count - rangeSize;

		if(!many(insertSize)) {
			// Increasing the size, but not by enough to force a reallocation.

			// Now, move the suffix forward
			pointer const ufirst = move_forward_n(last, insertSize);
			pointer const ulast = first + count;
			// Default-assign into the initialised elements
			first2 = copy_assign_range(first, ufirst, first2);
			// Then copy-construct the remainder, if it exists
			construct_range(ufirst, ulast, first2);

			return pointer_to_iterator(ulast);

		} else {
			// Increasing the size by loads, so we have to reallocate.  We'll do it manually here, so
			// we can skip moving the elements to be overwritten, since they're to be erased anyway.
			// Allocate the best amount...
			const size_type oldCapacity = capacity();
			const size_type newCapacity = best_capacity(size() + insertSize);
			const pointer newData = allocate(newCapacity);

			// Move the prefix over...
			const pointer insertFirst = move_construct_from_range(newData, ibegin(), first);
			// ...copy-construct the elements to be inserted...
			const pointer insertLast = copy_construct_from_range(insertFirst, first2, last2);
			// ...then move the suffix.
			const pointer newSize = move_construct_from_range(insertLast, last, iend());

			// Destroy and deallocate the old array.
			destroy(ibegin(), iend());
			deallocate(m_size, oldCapacity);

			// And set the new array.
			reset(newData, newSize, newCapacity);

			return pointer_to_iterator(insertLast);
		}
	}
}

template<typename T, typename Alloc>
template<typename InputIterator>
inline typename vector<T,Alloc>::iterator vector<T,Alloc>::simple_append(InputIterator first, InputIterator last) {
	const size_type insertIndex = size();
	while(true) {
		pointer newEnd;
		// Copy construct up to the available capacity and update the initialised-end pointer
		std::tie(newEnd, first) = checked_copy_construct_range(ubegin(), uend(), first, last);
		iend(newEnd);
		// If not done, reallocate and continue
		if(first != last) { reallocate(); } else { break; }
	}
	return pointer_to_iterator(ibegin() + insertIndex);
}

///////////////////////////////////////
// Insert Operations
///////////////////////////////////////
template<typename T, typename Alloc>
template<typename... Args>
inline typename vector<T,Alloc>::pointer vector<T,Alloc>::emplace_internal(pointer position, Args&&... args) {
	// If the container isn't full, we need to use the simple algorithm anyway, because the content
	// we're moving in make_gap_1() may contain one of the params.
	if(kane::use_simple_insert<T>::value || !full()) {
		// Create a temporary (to solve the aliasing problem)
		value_type temp(std::forward<Args>(args)...);
		pointer const newPosition = make_gap_1(position);
		// Select whether to use move-assignment or move-construction
		if(value_has_trivial_destroy || newPosition != position) {
			// newPosition is uninitialised (or has no-op destroy)
			construct(newPosition, std::move(temp));
		} else {
			// newPosition is initialised, so need to use assignment
			*newPosition = std::move(temp);
		}
		return newPosition;

	} else {
		// We're using the complex algorithm and the container is full.  Avoid the copy by 
		// doing partial reallocation.
		// Get the new array.
		const size_type newCapacity = next_capacity();
		pointer const newData = allocate(newCapacity);
		// Calculate the new insert position based on the index in the old array
		pointer const newPosition = newData + (position - ibegin());
		// Emplace-construct
		construct(newPosition, std::forward<Args>(args)...);
		// Move the old content in and calculate the new size
		/* newPosition is = */  move_construct_from_range(newData, ibegin(), position);
		pointer const newSize = move_construct_from_range(newPosition + 1, position, iend());
		// Destroy, deallocate, and set the new pointers
		destroy(ibegin(), iend());
		deallocate(m_data, m_capacity);
		reset(newData, newSize, newCapacity);
		return newPosition;
	}
}

template<typename T, typename Alloc>
template<typename... Args>
inline typename vector<T,Alloc>::pointer vector<T,Alloc>::emplace_back_internal(Args&&... args) {
	if(!full()) {
		// Can just insert optimally at end
		pointer const position = iend();
		construct(position, std::forward<Args>(args)...);
		++m_size;
		return position;

	} else if(kane::use_simple_insert<T>()) {
		// Reallocating, use a copy to solve aliasing problem
		value_type temp(std::forward<Args>(args)...);
		reallocate();
		// Can now insert at end
		pointer const position = iend();
		construct(position, std::move(temp));
		++m_size;
		return position;

	} else {
		// Do partial reallocation.  Get the new array.
		const size_type newCapacity = next_capacity();
		pointer const newData = allocate(newCapacity);
		// Calculate the new insert position
		pointer const newPosition = newData + size();
		// Emplace-construct
		construct(newPosition, std::forward<Args>(args)...);
		// Move the old content in
		move_construct_from_range(newData, ibegin(), iend());
		// Destroy, deallocate, and set the new pointers
		destroy(ibegin(), iend());
		deallocate(m_data, m_capacity);
		// (we inserted at end, so the newPosition is the last element)
		reset(newData, newPosition + 1, newCapacity);
		return newPosition;
	}
}

template<typename T, typename Alloc>
template<typename... Args>
inline typename vector<T,Alloc>::pointer vector<T,Alloc>::emplace_n_internal(pointer const position, size_type count, Args&&... args) {
	// If the container has enough capacity, we need to use the simple algorithm anyway, because 
	// the content we're moving in make_gap_n() may contain one of the params.
	if(kane::use_simple_insert<T>::value || few(count)) {
		// Create a temporary (to solve the aliasing problem)
		const value_type temp(std::forward<Args>(args)...);
		// ranges.first is the start of the initialised segment, ranges.second is the start of the 
		// uninitialised segment.  newPosition is the new location of the element at position
		const std::pair<pointer, pointer> ranges = make_gap_n(position);
		pointer const newPosition = ranges.first + count;
		
		// If value_has_trivial_destory, we can use copy construction for the whole range.
		// Otherwise, we need to split it up.
		// TODO: Check if the compiler would optimise this without needing the test in this method
		if(value_has_trivial_destroy) {
			construct_range(ranges.first, newPosition, temp);
		} else {
			// Copy construct the first range, copy assign the second range
			construct_range(ranges.first, ranges.second, temp);
			assign_range(ranges.second, newPosition, temp);
		}

		return ranges.first;

	} else {
		// We're using the complex algorithm and the container is full.  Avoid the copy by 
		// doing partial reallocation.
		// Get the new array.
		const size_type newCapacity = best_capacity(size() + count);
		pointer const newData = allocate(newCapacity);
		// Calculate the new insert position based on the index in the old array
		pointer const newPosition = newData + (position - ibegin());
		// Emplace-construct an exemplar
		construct(newPosition, std::forward<Args>(args)...);
		// Then, copy-construct the remaining elements from the exemplar
		pointer const last = construct_n(newPosition + 1, count - 1, *newPosition);
		// Move the old content in and calculate the new size
		/* newPosition is = */  move_construct_from_range(newData, ibegin(), position);
		pointer const newSize = move_construct_from_range(last, position, iend());
		// Destroy, deallocate, and set the new pointers
		destroy(ibegin(), iend());
		deallocate(m_data, m_capacity);
		reset(newData, newSize, newCapacity);
		
		return newPosition;
	}
}

template<typename T, typename Alloc>
template<typename... Args>
inline typename vector<T,Alloc>::pointer vector<T,Alloc>::emplace_back_n_internal(size_type count, Args&&... args) {
	if(!full()) {
		// Emplace the exemplar element
		emplace_back_internal(std::forward<Args>(args>)...);
		// Reallocate if necessary for remaining elements
		if(many(--count)) { reallocate(best_capacity(size() + count)); }
		// Copy-construct remaining elements and set new end
		pointer const position = iend() - 1;
		iend( construct_n(iend(), count, *position) );
		// Return result
		return position;

	} else if(kane::use_simple_insert<T>()) {
		// Create temporary for exemplar element
		const value_type temp(std::forward<Args>(args)...);
		// Reallocate
		reallocate(best_capacity(size() + count));
		// Copy construct elements from exemplar and set new end
		pointer const position = iend();
		iend( construct_n(position, count, temp) );
		// Return
		return position;
	
	} else {
		// Do partial reallocation.  Get the new array.
		const size_type newCapacity = best_capacity(size() + count);
		pointer const newData = allocate(newCapacity);
		// Calculate the new insert position
		pointer const newPosition = newData + size();
		// Emplace-construct exemplar
		construct(newPosition, std::forward<Args>(args)...);
		// Copy-construct remaining new elements, while determining new size
		pointer const newSize = construct_n(newPosition + 1, count - 1, *newPosition);
		// Move the old content in
		move_construct_from_range(newData, ibegin(), iend());
		// Destroy, deallocate, and set the new pointers
		destroy(ibegin(), iend());
		deallocate(m_data, m_capacity);
		// (we inserted at end, so the newPosition is the last element)
		// TODO: Test if it's faster to say newPosition + count (almost certainly not)
		reset(newData, newSize, newCapacity);
		return newPosition;
	}
}

template<typename T, typename Alloc>
template<typename U, typename OtherAlloc>
inline bool vector<T,Alloc>::operator==(const vector<U,OtherAlloc>& rhs) const {
	return size() == rhs.size() && std::equal(ibegin(), iend(), rhs.ibegin());
}

template<typename T, typename Alloc>
template<typename U, typename OtherAlloc>
inline bool vector<T,Alloc>::operator!=(const vector<U,OtherAlloc>& rhs) const {
	return size() != rhs.size() || !std::equal(ibegin(), iend(), rhs.ibegin());
}

template<typename T, typename Alloc>
template<typename U, typename OtherAlloc>
inline bool vector<T,Alloc>::operator<(const vector<U,OtherAlloc>& rhs) const {
	if(size() < rhs.size()) {
		return true;
	} else if(size() == rhs.size()) {
		return kane::unchecked_lexicographical_compare(ibegin(), iend(), rhs.ibegin());
	} else {
		return false;
	}
}

template<typename T, typename Alloc>
template<typename U, typename OtherAlloc>
inline bool vector<T,Alloc>::operator<=(const vector<U,OtherAlloc>& rhs) const {
	if(size() < rhs.size()) {
		return true;
	} else if(size() == rhs.size()) {
		return !kane::unchecked_lexicographical_compare(rhs.ibegin(), rhs.iend(), ibegin());
	} else {
		return false;
	}
}

template<typename T, typename Alloc>
template<typename U, typename OtherAlloc>
inline bool vector<T,Alloc>::operator>(const vector<U,OtherAlloc>& rhs) const { 
	return !(rhs <= *this); 
}

template<typename T, typename Alloc>
template<typename U, typename OtherAlloc>
inline bool vector<T,Alloc>::operator>=(const vector<U,OtherAlloc>& rhs) const { 
	return !(rhs < *this); 
}

template<typename T, typename Alloc>
std::ostream& operator<<(std::ostream& stream, const vector<T,Alloc>& vec) {
	typedef typename vector<T,Alloc>::const_pointer const_pointer;
	stream << '[';
	
	if(!vec.empty()) {
		const_pointer i = vec.ibegin();
		const const_pointer e = vec.iend();

		stream << *i;
		while(++i != e) { stream << ' ' << *i; }
	}

	return stream << ']';
}

// Construct a back inserter pointing to the "end" of any container
template<typename VectorType>
__forceinline pod_back_insert_iterator<VectorType>::pod_back_insert_iterator() : m_vector(NULL), m_next(NULL) { }
// Construct a back inserter for the specified vector
template<typename VectorType>
__forceinline pod_back_insert_iterator<VectorType>::pod_back_insert_iterator(VectorType& v) : m_vector(&v), m_next(v.full() ? NULL : v.iend()) { }
// Copy the specified back inserter
template<typename VectorType>
__forceinline pod_back_insert_iterator<VectorType>::pod_back_insert_iterator(const pod_back_insert_iterator& other) : m_vector(other.m_vector), m_next(other.m_next) { }
// Copy the specified back inserter
template<typename VectorType>
__forceinline pod_back_insert_iterator<VectorType>& pod_back_insert_iterator<VectorType>::operator=(const pod_back_insert_iterator& rhs) { 
	m_vector = other.m_vector; m_next = other.m_next; return *this; 
}

///////////////////////////////
// Iterator functionality
// Access the next uninitialised element in the container
template<typename VectorType>
__forceinline typename pod_back_insert_iterator<VectorType>::reference pod_back_insert_iterator<VectorType>::operator*() { check_next(); return *m_next; }
template<typename VectorType>
__forceinline typename pod_back_insert_iterator<VectorType>::pointer pod_back_insert_iterator<VectorType>::operator->() { check_next(); return m_next; }

// Insert the next uninitialised element into the container and move 
// forward to the next one.
template<typename VectorType>
__forceinline pod_back_insert_iterator<VectorType>& pod_back_insert_iterator<VectorType>::operator++() {
	// Check if we need to grow vector
	check_next();
	// Advance cached pointer and update the vector
	m_vector->iend(++m_next);
	// Then, if we've hit the end of initialised space, un-cache it until the user needs it.
	if(m_next == m_vector->uend()) { m_next = NULL; }

	return *this;
}

// Slightly different than the typical operator++; this returns a normal iterator to the inserted element
template<typename VectorType>
__forceinline typename VectorType::iterator pod_back_insert_iterator<VectorType>::operator++(int) {
	// Get the inserted element if we don't already have a pointer to it
	check_next();
	pointer const last = m_next;
	// Advance cached pointer and update the vector
	m_vector->iend(++m_next);
	// Then, if we've hit the end of initialised space, un-cache it until the user needs it.
	if(m_next == m_vector->uend()) { m_next = NULL; }
	
	// Convert the old pointer to an iterator and return
	return m_vector->pointer_to_iterator(last);
}


// Test for equality/inequality
template<typename VectorType>
__forceinline bool pod_back_insert_iterator<VectorType>::operator==(const pod_back_insert_iterator& rhs) const { return false; }
template<typename VectorType>
__forceinline bool pod_back_insert_iterator<VectorType>::operator!=(const pod_back_insert_iterator& rhs) const { return true; }

template<typename VectorType>
__forceinline void pod_back_insert_iterator<VectorType>::check_next() { if(!m_next) { grow_vector(); m_next = m_vector->iend(); } }

// For now, let's allow this to be inlined
template<typename VectorType>
inline void pod_back_insert_iterator<VectorType>::grow_vector() { m_vector->reallocate(); }

template<typename T, typename Alloc>
inline pod_back_insert_iterator<vector<T,Alloc>> pod_back_inserter(vector<T,Alloc>& v) { 
	return pod_back_insert_iterator<vector<T,Alloc>>(v);
}

}