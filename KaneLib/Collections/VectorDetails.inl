#pragma once

namespace kane { namespace detail {

///////////////////////////////////////////////////////////////////////////////////////////////////
////////                          //////// vector_base ////////                             ////////
///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////
// Constructors
///////////////////////////////////////////////////////////////////////////////////////////////////

// Construct empty vector with default-constructed allocator
template<typename T, typename Alloc>
inline vector_base<T,Alloc>::vector_base() 
	: members_base(), alloc_base() { }

// Construct empty vector with specified allocator
template<typename T, typename Alloc>
inline vector_base<T,Alloc>::vector_base(const Alloc& a) 
	: members_base(), alloc_base(a) { }

// Construct empty vector with specified initial capacity and default-constructed allocator
template<typename T, typename Alloc>
inline vector_base<T,Alloc>::vector_base(const size_type sz) 
	: members_base(kane::no_default_construct), alloc_base() { 
	if(sz > 0) {
		reset( allocate(sz), sz );
	} else {
		reset();
	}
}

// Construct empty vector with specified initial capacity and allocator
template<typename T, typename Alloc>
inline vector_base<T,Alloc>::vector_base(const size_type sz, const Alloc& a) 
	: members_base(kane::no_default_construct), alloc_base(a) { 
	if(sz > 0) {
		reset( allocate(sz), sz );
	} else {
		reset();
	}
}

// Copy-construct from another vector, using its allocator
template<typename T, typename Alloc>
inline vector_base<T,Alloc>::vector_base(const vector_base& other) 
	: members_base(kane::no_default_construct), alloc_base( other.alloc_occc() ) { 

	const size_type otherSize = other.size();
	if(otherSize) {
		pointer const newData = allocate(otherSize);
		pointer const newSize = copy_construct_from_range(newData, other.ibegin(), other.iend());
		reset(newData, newSize, newSize);
	} else {
		reset();
	}
}

// Copy-construct from another vector, using specified allocator
template<typename T, typename Alloc>
inline vector_base<T,Alloc>::vector_base(const vector_base& other, const Alloc& a) 
	: members_base(kane::no_default_construct), alloc_base(a) { 

	const size_type otherSize = other.size();
	if(otherSize) {
		pointer const newData = allocate(otherSize);
		pointer const newSize = copy_construct_from_range(newData, other.ibegin(), other.iend());
		reset(newData, newSize, newSize);
	} else {
		reset();
	}
}

// Construct by moving vector and allocator
template<typename T, typename Alloc>
inline vector_base<T,Alloc>::vector_base(vector_base&& rv) 
	: members_base(rv), alloc_base(std::move(rv.m_allocator())) {
	// Successful move, zero out the rv
	rv.reset();
}

// Construct with allocator and move content from other vector; allocators definitely equal
template<typename T, typename Alloc>
inline vector_base<T,Alloc>::vector_base(vector_base&& rv, const Alloc& a, const std::true_type) 
	: members_base(rv), alloc_base(a) {
	// Successful move, don't need to do anything else, reset the rv
	rv.reset();
}

// Construct with allocator and move content from other vector; allocators possibly different
template<typename T, typename Alloc>
inline vector_base<T,Alloc>::vector_base(vector_base&& rv, const Alloc& a, const std::false_type) 
	: members_base(kane::no_default_construct), alloc_base(a) {

	// First, if rv is empty, do nothing, just reset our members
	if(rv.empty()) {
		reset();

	// Otherwise, have to check if the allocators are equal...
	} else if(m_allocator() == rv.m_allocator()) {
		// If the allocators are equal, we can just move the content directly in
		m_data = rv.m_data;
		m_size = rv.m_size;
		m_capacity = rv.m_capacity;

		// Leave RV in a consistent state:
		rv.reset();

	} else {
		// They aren't, so we need to allocate our own memory with our allocator and move each 
		// element individually.
		m_data = allocate(rv.size());
		m_capacity = m_size = move_construct_from_range(m_data, rv.ibegin(), rv.iend());
		// Then, reset the rv
		rv.destroy(rv.ibegin(), rv.iend());
		rv.iend(rv.ibegin());
	}
}

// Default-construct allocator, but leave other members uninitialised
template<typename T, typename Alloc>
inline vector_base<T,Alloc>::vector_base(const kane::no_default_construct_t)
	: members_base(kane::no_default_construct), alloc_base() { }

// Use specified allocator, but leave other members uninitialised
template<typename T, typename Alloc>
inline vector_base<T,Alloc>::vector_base(const kane::no_default_construct_t, const Alloc& a)
	: members_base(kane::no_default_construct), alloc_base(a) { }

///////////////////////////////////////////////////////////////////////////////////////////////////
// Members Helpers
///////////////////////////////////////////////////////////////////////////////////////////////////
// Size of initialised storage
template<typename T, typename Alloc>
inline typename vector_base<T,Alloc>::size_type vector_base<T,Alloc>::size() const { return size_type(m_size - m_data); }
// Capacity of allocated storage
template<typename T, typename Alloc>
inline typename vector_base<T,Alloc>::size_type vector_base<T,Alloc>::capacity() const { return size_type(m_capacity - m_data); }
// Number of elements that can be inserted before the container becomes full
template<typename T, typename Alloc>
inline typename vector_base<T,Alloc>::size_type vector_base<T,Alloc>::available() const { return size_type(m_capacity - m_size); }
// True if the container has no allocated memory, or its allocated memory is empty
template<typename T, typename Alloc>
inline bool vector_base<T,Alloc>::empty() const { return m_data == m_size; }
// True if the container's allocated memory is completely full
template<typename T, typename Alloc>
inline bool vector_base<T,Alloc>::full() const { return m_size == m_capacity; }

// Quick pointers to the beginning and end of the uninitialised data.  (We can't necessarily 
// use end() for uend() because we might be using checked iterators.)// Pointer to beginning of initialised storage
template<typename T, typename Alloc> 
inline typename vector_base<T,Alloc>::pointer vector_base<T,Alloc>::ibegin() { return m_data; }
// Pointer to end of initialised storage
template<typename T, typename Alloc> 
inline typename vector_base<T,Alloc>::pointer vector_base<T,Alloc>::iend() { return m_size; }
// Pointer to beginning of uninitialised storage (same as iend())
template<typename T, typename Alloc> 
inline typename vector_base<T,Alloc>::pointer vector_base<T,Alloc>::ubegin() { return iend(); }
// Pointer to end of uninitialised storage, end of allocated storage
template<typename T, typename Alloc>
inline typename vector_base<T,Alloc>::pointer vector_base<T,Alloc>::uend() { return m_capacity; }

// Pointer to beginning of initialised storage
template<typename T, typename Alloc> 
inline typename vector_base<T,Alloc>::const_pointer vector_base<T,Alloc>::ibegin() const { return m_data; }
// Pointer to end of initialised storage
template<typename T, typename Alloc> 
inline typename vector_base<T,Alloc>::const_pointer vector_base<T,Alloc>::iend() const { return m_size; }
// Pointer to beginning of uninitialised storage (same as iend())
template<typename T, typename Alloc> 
inline typename vector_base<T,Alloc>::const_pointer vector_base<T,Alloc>::ubegin() const { return iend(); }
// Pointer to end of uninitialised storage, end of allocated storage
template<typename T, typename Alloc>
inline typename vector_base<T,Alloc>::const_pointer vector_base<T,Alloc>::uend() const { return m_capacity; }

// Set size
template<typename T, typename Alloc> 
inline void vector_base<T,Alloc>::size(const size_type newSize) { m_size = m_data + newSize; }
template<typename T, typename Alloc> 
inline void vector_base<T,Alloc>::iend(pointer const newSize) { m_size = newSize; }
template<typename T, typename Alloc> 
inline void vector_base<T,Alloc>::ubegin(pointer const newSize) { iend(newSize); }

// Reset data members...
// ...to their initial state (NULL, NULL, NULL)
template<typename T, typename Alloc> 
inline void vector_base<T,Alloc>::reset() { m_data = NULL; m_size = NULL; m_capacity = NULL; }
// ...to (ds, ds, c)
template<typename T, typename Alloc> 
inline void vector_base<T,Alloc>::reset(pointer const ds, pointer const c) { m_data = ds; m_size = ds; m_capacity = c; }
// ...to (ds, ds, ds + cap)
template<typename T, typename Alloc> 
inline void vector_base<T,Alloc>::reset(pointer const ds, const size_type cap) { m_data = ds; m_size = ds; m_capacity = ds + cap; }
// ...to (d, s, c)
template<typename T, typename Alloc> 
inline void vector_base<T,Alloc>::reset(pointer const d, pointer const s, pointer const c) { m_data = d; m_size = s; m_capacity = c; }
// ...to (d, s, d + c)
template<typename T, typename Alloc> 
inline void vector_base<T,Alloc>::reset(pointer const d, pointer const s, const size_type c) { m_data = d; m_size = s; m_capacity = d + c; }

// Silly helpers
// Back in the day, I called things like this hogwash.  It's a shame I've so little time for 
// hogwash these days.
template<typename T, typename Alloc>
inline bool vector_base<T,Alloc>::many(const size_type sz) const { return (size() + sz) > capacity(); }
template<typename T, typename Alloc>
inline bool vector_base<T,Alloc>::few(const size_type sz) const { return (size() + sz) <= capacity(); }

///////////////////////////////////////////////////////////////////////////////////////////////////
// Vector Operations
///////////////////////////////////////////////////////////////////////////////////////////////////
// Determines the next capacity (currently, (m_capacity * 2, or 2 on first allocation).
template<typename T, typename Alloc> 
inline typename vector_base<T,Alloc>::size_type vector_base<T,Alloc>::next_capacity() const { 
	return next_capacity(capacity());
}

// Determines the next capacity (currently, (sz * 2, or 2 on first allocation).
template<typename T, typename Alloc>
inline constexpr typename vector_base<T,Alloc>::size_type vector_base<T,Alloc>::next_capacity(const size_type sz) {
	// Huh, compiler implements this as "add eax,eax."  Surprising that add reg,reg is faster than 
	// shl reg/imm...
	return sz ? (sz * 2) : 2;
	// (Roughly) MSVC's resize factor, for performance comparisons
	//return sz / 2 + sz + 1;
}

// Determines the best next capacity large enough to contain the specified size.
// Currently, this is the larger of needed and next_capacity()
template<typename T, typename Alloc> 
inline typename vector_base<T,Alloc>::size_type vector_base<T,Alloc>::best_capacity(const size_type needed) const {
	return std::max(needed, next_capacity());
}

// Reallocate the internal array to the next_capacity()
template<typename T, typename Alloc> 
inline void vector_base<T,Alloc>::reallocate() { really_reallocate(capacity(), next_capacity()); }

// Reallocate the internal array to the specified capacity if greater than the current capacity.
template<typename T, typename Alloc> 
inline void vector_base<T,Alloc>::reallocate(const size_type newCapacity) {
	// This will be needed later, so calculate it here so the compiler (in theory) can reuse it.
	const size_type oldCapacity = capacity();
	// Only reallocate if necessary
	if(newCapacity > oldCapacity) { really_reallocate(oldCapacity, newCapacity); }
}

// Internal function for actually handling the reallocation.  Note that no sanity checks are 
// performed on the input.  This should only be used when we actually know 
// oldCapacity < newCapacity and newCapacity != 0.
template<typename T, typename Alloc> 
inline void vector_base<T,Alloc>::really_reallocate(const size_type oldCapacity, const size_type newCapacity) {
	// Allocate new array...
	pointer const newData = allocate(newCapacity);
	pointer newSize = newData;

	// If the current array is non-empty, move the old elements into the new array and erase 
	// the old elements.  Note that we still have to destroy moved elements.
	// move_construct_from_range() and destroy() both have the same first != last check at the front,
	// so we don't need to check if it's empty first.  But we DO have to test for m_data to figure
	// out if we're deallocating, so might as well do that here.
	// (Note that deallocate() is not the same as delete, and therefore doesn't have to follow the
	// same rule that it does nothing to a NULL pointer.)
	if(m_data) {
		newSize = move_construct_from_range(newSize, ibegin(), iend());
		destroy(ibegin(), iend());
		deallocate(m_data, oldCapacity);
	}

	// Finally, set the new array
	reset(newData, newSize, newCapacity);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Uninitialised Memory Operations
///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////
// Unchecked Moves
///////////////////////////////////////

template<typename T, typename Alloc>
inline void vector_base<T,Alloc>::move_forward_1(pointer const position) {
	// We aren't supposed to get position == iend(), so use an assert in this case
	_ASSERTE(position != iend());
	pointer dest = iend();

	if(value_has_trivial_destroy) {
		// Move everything using move construction if move-construct == move-assign
		while(dest != position) {
			pointer const src = dest - 1;
			construct(dest, std::move(*src));
			dest = src;
		}

	} else {
		// Otherwise, just move the first element using move construction
		// (Putting it in a scope so I can reuse the name below)
		{
			pointer const src = dest - 1;
			construct(dest, std::move(*src));
			dest = src;
		}

		// And move any remaining elements using move assignment
		while(dest != position) {
			pointer const src = dest - 1;
			*dest = std::move(*src);
			dest = src;
		}
	}

	m_size++;
}

// TODO: Check every caller of this goddamn function to make sure it's using the return value 
// properly.
template<typename T, typename Alloc>
inline typename vector_base<T,Alloc>::pointer vector_base<T,Alloc>::move_forward_n(pointer const position, const size_type sz) {
	// We aren't supposed to get position == iend(), so use an assert in this case
	pointer const last = iend();
	_ASSERTE(position != last);
	_ASSERTE(sz > 0);

	pointer src = last;
	pointer dest = src + sz;
	iend(dest);

	if(value_has_trivial_destroy) {
		while(src != position) {
			--src;
			--dest;
			construct(dest, std::move(*src));
		}
		// In this case, we can return either last or dest, since there's no distinction between 
		// uninitialised and initialised space.  For simplicity, I've chosen to return the 
		// "default" value returned from the overlapped case below.
	} else {
		// Figure what to do based on whether it's overlapped or not
		if((position + sz) < last) {
			// Overlapped, so need to do it in two parts
			// First move-construct into uninitialised space
			do {
				--dest;
				--src;
				construct(dest, std::move(*src));
			} while(dest != last);

			// Then move-assign into initialised space
			while(src != position) {
				--dest;
				--src;
				*dest = std::move(*src);
			}
		} else {
			// Not overlapped, so just use construction
			// (This is the same as the trivial destructor case, but I don't want to try combining 
			// them because it'd risk confusing the optimiser.)
			while(src != position) {
				--src;
				--dest;
				construct(dest, std::move(*src));
			}

			// In the non-overlapped case, we have to return last, which is the old iend we stored
			// earlier.
			return last;
		}
	}

	return dest;
}

///////////////////////////////////////
// Checked moves
///////////////////////////////////////

template<typename T, typename Alloc>
inline typename vector_base<T,Alloc>::pointer vector_base<T,Alloc>::make_gap_1(pointer const position) {
	if(full()) {
		const size_type oldCapacity = capacity();
		const size_type newCapacity = next_capacity();
		pointer const newData = allocate(newCapacity);

		pointer const newPosition = move_construct_from_range(newData, ibegin(), position);
		pointer const newSize = move_construct_from_range(newPosition + 1, position, iend());

		destroy(ibegin(), iend());
		deallocate(m_data, oldCapacity);

		reset(newData, newSize, newCapacity);
		return newPosition;

	} else {
		move_forward_1(position);
		return position;
	}
}

template<typename T, typename Alloc>
inline std::pair<typename vector_base<T,Alloc>::pointer, typename vector_base<T,Alloc>::pointer>
vector_base<T,Alloc>::make_gap_n(pointer const position, const size_type sz) {
	const size_type oldCapacity = capacity();
	if((size() + sz) > oldCapacity) {
		const size_type newCapacity = next_capacity(size() + sz);
		pointer const newData = allocate(newCapacity);

		pointer const newPosition = move_construct_from_range(newData, ibegin(), position);
		pointer const newSize = move_construct_from_range(newPosition + sz, position, iend());

		destroy(ibegin(), iend());
		deallocate(m_data, oldCapacity);

		reset(newData, newSize, newCapacity);
		return std::make_pair(newPosition, newPosition);

	} else {
		return std::make_pair(position, move_forward_n(position, sz));
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// InputIterator Helper
///////////////////////////////////////////////////////////////////////////////////////////////////
// Inserting elements provided by an InputIterator is a knotty task, since we don't know the 
// size of the input sequence.  Normally, when inserting at the end, you just blindly insert 
// one element at a time, reallocating naturally as necessary.  This is amortised linear time, 
// but can mean up to 4N copies.  More importantly, it's no better than repeated calls to 
// push_back(), and ideally we'd like the batch-insert algorithms we provide to be better than 
// a naive solution the user could do themselves.  (After all, we have direct access to our 
// memory and pointers.)
//
// It's worse, though, when inserting into the middle.  There are several commonly used ways to 
// handle it:
//  - You could create a temporary new vector and insert at the end of that, then when 
//    finished move the content into this vector in one go.
//  - Dinkumware/MSVC's solution inserts at the end, then uses three reverse() operations to 
//    "rotate" the elements into position.  This is slower in every case I know of, but might
//    use less memory.
//  - GCC uses repeated calls to the single-element insert(p,t), an O(n^2) solution, which is 
//    basically just giving up.
// The first option is satisfactory if you want to keep memory usage down, but I figure that's 
// less commonly the case than wanting to keep the time cost down, particularly since 
// Kane::Vector was designed for my use in performance-critical code, and anyway the user can 
// do that themselves if they like.
//
// This solution is a faster alternative that uses some extra temporary memory.  This function 
// takes the iterator range, an index at which inserting is to begin, and the current size and 
// capacity of the vector.  It then allocates a new array of the next capacity and begins 
// inserting into it as though it were a circular buffer, beginning at the insert index--that 
// is, it inserts beginning at the index where the next element SHOULD appear in the final 
// array.  Once the buffer is full, it updates the index, and then repeats the process with the 
// next capacity increment.  Once it consumes the entire input sequence, it then combines all
// the previous buffers together into the final array, deallocates the previous buffers, and
// returns the final array and its size and capacity.
//
// Because it starts inserting into the temporary buffers at the correct index for elements 
// it's inserting, when combining the buffers together it can potentially use the last-
// allocated buffer as the final array.  If that buffer was never filled past capacity, all 
// the elements currently in it are in the correct places, it just has to move in the previous 
// elements.  Back-of-the-envelope calculations suggest that we shouldn't use more than N 
// additional memory beyond what the alternatives mentioned above use, and that this requires
// at most O(I) copy-constructions and O(N) move-constructions, where N is the final size of 
// the vector and I is the size of the input sequence; a quick experiment suggests that when I is
// much larger than N, it's usually around O(1.75*I) total copies plus moves.
// BUG: There's clearly some kind of uninitialised-pointer error happening near this function!
// Only occurs in certain cases.  Possibly when only one array is needed?
template<typename T, typename Alloc>
template<typename InputIterator>
KNOINLINE typename vector_base<T,Alloc>::horrible_insert_helper 
vector_base<T,Alloc>::insert_horrible(const size_type index, const size_type oldSize, const size_type oldCapacity, 
									 InputIterator first, const InputIterator last) {

	// Local type for defining a temporary buffer.  The first-inserted element is unspecified, 
	// since that can be deduced as we're moving through the reconstruction algorithm.
	struct temp_array { 
		pointer begin;	// Pointer to the array (not the first-inserted element)
		pointer end;	// One past the end of the array
	};
	// Local type for keeping track of the temporary buffer.  Since we're allocating these on the 
	// stack, we want to minimise the number of _alloca() calls we use, so we allocate in blocks 
	// of 7.  That's 14 pointers, leaving room for two more pointers if we want to hit 16*4 == 64 
	// bytes exactly.
	struct temp_array_block {
		temp_array arrays[7];
		temp_array* lastArray;
		temp_array_block* next;
	};

	// Head of the block list
	// Chances are, we'll never actually need to allocate another block, so by putting this in 
	// explicitly we might be able to skip ever calling _alloca()
	temp_array_block firstBlock;
	firstBlock.next = NULL;

	// Initial state
	temp_array_block* currentBlock = &firstBlock;

	size_type currentSize = oldSize;
	size_type currentCapacity = oldCapacity;
	size_type currentIndex = index;
	pointer lastInserted = NULL;

	bool finishedInserting = false;
	do {
		currentBlock->lastArray = currentBlock->arrays;
		temp_array* const arraysEnd = currentBlock->arrays + 7;

		do {
			///////////////////////////////////////////////////
			// Create and prepare the next array for inserts

			// First, calculate the next capacity that can contain the current size
			{
				size_type nextCapacity = currentCapacity;
				do {
					nextCapacity = next_capacity(nextCapacity);
				} while(nextCapacity < currentSize);
				currentCapacity = nextCapacity;
			}
		
			// Then allocate a new array of that size and get the necessary pointers
			pointer const arrayBegin = allocate(currentCapacity);
			pointer const arrayEnd = arrayBegin + currentCapacity;
			pointer const arrayMid = arrayBegin + currentIndex;
			// Store the array and advance the lastArray pointer.
			currentBlock->lastArray->begin = arrayBegin;
			currentBlock->lastArray->end = arrayEnd;
			++(currentBlock->lastArray);

			///////////////////////////////////////////////////
			// Insert elements into temporary buffer
			bool firstLoop = true;
			while(true) {
				pointer const posFirst = (firstLoop) ? arrayMid : arrayBegin;
				pointer const posLast = (firstLoop) ? arrayEnd : arrayMid;
				pointer pos = posFirst;

				while(pos != posLast && first != last) {
					construct(pos, *first);
					++pos;
					++first;
				}

				if(first == last) { finishedInserting = true; }

				lastInserted = pos;
				const size_type insertedCount = pos - posFirst;
				currentSize += insertedCount;
				currentIndex += insertedCount;

				if(!finishedInserting && firstLoop) {
					firstLoop = false;
					continue;
				} else {
					break;
				}
			}
		} while(!finishedInserting && currentBlock->lastArray != arraysEnd);

		// If we broke out because we ran out of space in the block, allocate a new one
		if(!finishedInserting) {
			currentBlock->next = (temp_array_block*)_alloca(sizeof(temp_array_block));
			currentBlock = currentBlock->next;
			currentBlock->next = NULL;
		}
	} while(!finishedInserting);

	///////////////////////////////////////////////////
	// Combine the arrays together

	// First, check the last array.  If it's suitable to be used as the final array, we'll use it 
	// as such.  Otherwise, we'll need to allocate a new array and use some special logic to handle
	// it (since it's only partly full).
	const bool lastArrayBad = currentSize > currentCapacity;
	while(currentSize > currentCapacity) {
		currentCapacity = next_capacity(currentCapacity);
	}

	(currentBlock->lastArray)--;
	pointer const finalArray = (lastArrayBad) ? allocate(currentCapacity) : currentBlock->lastArray->begin;
	pointer const finalArrayEnd = finalArray + currentCapacity;

	// TODO: Update this comment
	// Note that at this point currentArray is either the final array or part-full--either way, 
	// it needs special handling.  So we can safely move elements from and deallocate everything 
	// up (but not including) to currentArray.

	size_type combineIndex = index;
	pointer combinePosition = finalArray + combineIndex;

	// Now, loop over every block and every array in that block, inserting into the new array
	// We'll now repurpose the nextArray pointer for this job
	for(temp_array_block* b = &firstBlock; b != NULL; b = b->next) {
		for(temp_array* a = b->arrays; a != b->lastArray; ++a) {
			// Get some information about the array
			pointer const arrayBegin = a->begin;
			pointer const arrayEnd = a->end;
			pointer const arrayMid = arrayBegin + combineIndex;
			const size_type arrayCapacity = arrayEnd - arrayBegin;

			// Copy in both bits
			combinePosition = move_construct_from_range(combinePosition, arrayMid, arrayEnd);
			combinePosition = move_construct_from_range(combinePosition, arrayBegin, arrayMid);
			// Destroy the old elements
			destroy(arrayBegin, arrayEnd);
			// Deallocate the old array
			deallocate(arrayBegin, arrayCapacity);

			// Update the combineIndex
			combineIndex += arrayCapacity;
		}
	}

	// Now, the special logic for the final array.
	if(lastArrayBad) {
		// Last array contained some content, but may not have been full.  Move the remainder and 
		// deallocate.
		temp_array* const lastArray = currentBlock->lastArray;
		// Get some information about the array
		pointer const arrayBegin = lastArray->begin;
		pointer const arrayEnd = lastArray->end;
		pointer const arrayMid = arrayBegin + combineIndex;
		const size_type arrayCapacity = arrayEnd - arrayBegin;

		// lastInserted is a pointer into this array, remember.
		if(lastInserted <= arrayMid) {
			// Second half is full, so insert all of it
			combinePosition = move_construct_from_range(combinePosition, arrayMid, arrayEnd);
			// First half partly full, so insert as much as there is
			combinePosition = move_construct_from_range(combinePosition, arrayBegin, lastInserted);
			// Destroy the elements
			destroy(arrayMid, arrayEnd);
			destroy(arrayBegin, lastInserted);
		} else {
			// First half is empty, second half is part full, so only one move operation
			combinePosition = move_construct_from_range(combinePosition, arrayMid, lastInserted);
			// Destroy those elements
			destroy(arrayMid, lastInserted);
		}

		deallocate(arrayBegin, arrayCapacity);
	} else {
		// Last array was reused as the final array.  That means we need to jump combinePosition
		// up to the end of the inserted elements
		combinePosition = lastInserted;
	}

	// Finally, create and return the result
	horrible_insert_helper result;
	result.newData = finalArray;
	result.newSize = combinePosition;
	result.newCapacity = finalArrayEnd;
	return result;
}

} }