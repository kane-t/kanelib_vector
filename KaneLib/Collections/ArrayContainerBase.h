///////////////////////////////////////////////////////////////////////////////////////////////////
////////                      //////// array_container_base ////////                       ////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// Utility base class for classes that use an allocator to manage contiguous arrays of homogeneous
// types, like vectors and circular buffers.  array_container_base stores an allocator (correctly 
// rebound for type T) and provides access to it to derived classes, but also provides a 
// comprehensive set of functions for handling allocation, deallocation, construction, assignment, 
// and destruction, with appropriate optimisations for trivial construction or destruction.  The 
// allocator is also stored using the empty base class optimisation, if possible.  Convenience 
// functions are also provided for managing the allocator itself.
//
// array_container_base does *not* manage allocated memory itself--it's the responsibility of the 
// deriving class to manage its memory.  This allows array_container_base to be used by classes to
// manage statically-allocated arrays (like child node arrays in tree data structures).  In this 
// case, the std::allocator should be used, and allocate() and deallocate() can be ignored.
//
// Because array_container_base isn't aware of the memory being managed, it's the responsibility 
// of deriving classes to pass valid parameters.  For example, assign_n(p,n,v) should only be 
// called when the range [p,p+n) contains only initialised elements (or value_type has both a
// trivial destructor and a trivial default constructor).  construct_range(i,j,v) should only be 
// called when the range [i,j) contains only uninitialised elements (or when value_type has a 
// trivial destructor).
#pragma once

#include <KaneLib/Collections/ContainerFwd.h>

namespace kane { namespace detail { 

	template<typename T, typename Alloc>
	class array_container_base : private alloc_container<typename std::allocator_traits<Alloc>::template rebind_alloc<T>> {
	private:
		typedef alloc_container<typename std::allocator_traits<Alloc>::template rebind_alloc<T>> my_base;
		typedef array_container_base<T, Alloc> my_type;

	public:
		///////////////////////////////////////////////////////////////////////////////////////////
		// Standard Typedefs
		///////////////////////////////////////////////////////////////////////////////////////////
		// Typedefs copied from the allocator, provided for convenience to deriving classes.
		typedef typename my_base::allocator_type      allocator_type;
		typedef std::allocator_traits<allocator_type> allocator_type_traits;

		typedef typename allocator_type_traits::value_type	    value_type;
		// TODO: Consider switching these to the allocator_traits typedefs?
		typedef value_type&							            reference;
		typedef value_type&&						            rvalue_reference;
		typedef const value_type&					            const_reference;
		typedef typename allocator_type_traits::pointer		    pointer;
		typedef typename allocator_type_traits::const_pointer   const_pointer;
		typedef typename allocator_type_traits::size_type	    size_type;
		typedef typename allocator_type_traits::difference_type difference_type;

		///////////////////////////////////////////////////////////////////////////////////////////
		// Type Traits
		///////////////////////////////////////////////////////////////////////////////////////////
		// value_type traits
		static constexpr bool value_has_trivial_construct = std::is_trivially_constructible_v<value_type>;
		static constexpr bool value_has_trivial_destroy = std::is_trivially_destructible_v<value_type>;
		// allocator_type traits
		static constexpr bool alloc_propagate_copy = typename allocator_type_traits::propagate_on_container_copy_assignment();
		static constexpr bool alloc_propagate_move = typename allocator_type_traits::propagate_on_container_move_assignment();
		static constexpr bool alloc_propagate_swap = typename allocator_type_traits::propagate_on_container_swap();
		static constexpr bool alloc_is_always_equal = typename allocator_type_traits::is_always_equal();
		
		///////////////////////////////////////////////////////////////////////////////////////////
		// Constructors
		///////////////////////////////////////////////////////////////////////////////////////////
		array_container_base()				 : my_base() { }
		array_container_base(const Alloc& a) : my_base(a) { }
		array_container_base(Alloc&& a)		 : my_base(std::move(a)) { }
		// Copy construction (automatically selects correct allocator based on allocator traits)
		array_container_base(const array_container_base& other) : my_base(other.select_occc()) { }
		// Doesn't explicitly initialise the allocator.  Experimental!
		array_container_base(kane::no_default_construct_t) : my_base(kane::no_default_construct) { }
		
		///////////////////////////////////////////////////////////////////////////////////////////
		// Allocator Utilities
		///////////////////////////////////////////////////////////////////////////////////////////

		///////////////////////////////////
		// Retrieve Allocator
		      allocator_type& m_allocator()       { return my_base::m_allocator(); }
		const allocator_type& m_allocator() const { return my_base::m_allocator(); }

		///////////////////////////////////
		// Allocator Information
		// Maximum possible allocation size
		size_type max_size() const { return allocator_type_traits::max_size(m_allocator()); }

		// Obtain the allocator to use for a copy of this container
		allocator_type select_allocator_on_copy_construction() const { 
			return select_on_container_copy_construction(*this); 
		}

		// Obtain the allocator to use for a copy of the specified container
		static allocator_type select_allocator_on_copy_construction(const array_container_base& other) { 
			return allocator_type_traits::select_on_container_copy_construction(other.m_allocator());
		}

		// Shortcuts for select_allocator_on_copy_construction
		allocator_type alloc_occc() const { return select_allocator_on_copy_construction(); }
		static allocator_type select_occc(const array_container_base& other) { return select_allocator_on_copy_construction(other); }

		///////////////////////////////////
		// Allocation, Construction, and Destruction
		// Return a new array of the specified capacity
		pointer allocate(const size_type sz) { return allocator_type_traits::allocate(m_allocator(), sz); }
		pointer allocate(const size_type sz, pointer const p) { return allocator_type_traits::allocate(m_allocator(), sz, p); }
		// Deallocates the specified array allocated by our allocator
		void deallocate(pointer const p, const size_type sz) { allocator_type_traits::deallocate(m_allocator(), p, sz); }
		// Deallocate, getting the capacity from beginning and end pointers
		void deallocate(pointer const p, pointer const c) { deallocate(p, c - p); }
		// Construct single element
		template<typename... Args> 
		void construct(pointer const p, Args&&... args) { allocator_type_traits::construct(m_allocator(), p, std::forward<Args>(args)...); }
		// Destroy an element
		void destroy(pointer const p) { allocator_type_traits::destroy(m_allocator(), p); }

		///////////////////////////////////////////////////////////////////////////////////////////
		// Range Helpers
		///////////////////////////////////////////////////////////////////////////////////////////
		// Easier, optimised operations on ranges of elements.  These are generally faster than 
		// directly calling their single-element equivalents in a loop, because they use value_type
		// optimisations when available.

		///////////////////////////////////////////////////////
		// Range Destruction
		///////////////////////////////////////////////////////
		// Destroys the elements in range [first,last).  Returns last.
		pointer destroy(pointer first, pointer const last) {
			if(!value_has_trivial_destroy) {
				while(first != last) { destroy(first); ++first; }
			}
			return last;
		}

		// Destroys n elements.  Returns pointer to one past the last element destroyed.
		pointer destroy_n(pointer first, size_type n) {
			return destroy(first, first + n);
		}

		///////////////////////////////////////////////////////
		// Range Construction
		///////////////////////////////////////////////////////

		// Fill Range
		// Default constructs range.  Returns last.
		pointer construct_range(pointer first, pointer const last) {
			if(!value_has_trivial_construct) {
				while(first != last) { construct(first); ++first; }
			}
			return last;
		}

		// Copy-constructs range to an exemplar element.  Returns last.
		pointer construct_range(pointer first, pointer const last, const_reference val) {
			while(first != last) { construct(first, val); ++first; }
			return last;
		}

		// Default-constructs n elements.  Returns pointer to one past the last constructed.
		pointer construct_n(pointer first, size_type n) {
			if(!value_has_trivial_construct) {
				while(n != 0) { construct(first); ++first; --n; }
				return first;
			} else {
				return first + n;
			}
		}

		// Copy-constructs n elements to an exemplar element.  Returns pointer to one past the last
		// constructed.
		pointer construct_n(pointer first, size_type n, const_reference val) {
			while(n != 0) { construct(first, val); ++first; --n; }
			return first;
		}

		///////////////////////////////////
		// Unchecked Range Copy/Move Construct
		
		// Copy-construct a range of elements [first2, last2) into the memory beginning at first1.
		// The destination range is unchecked.  Returns pointer to the end of the destination 
		// range.
		template<typename InputIterator>
		pointer copy_construct_from_range(pointer first1, InputIterator first2, InputIterator const last2) {
			while(first2 != last2) { construct(first1, *first2); ++first1; ++first2; }
			return first1;
		}

		// Move-construct  a range of elements [first2, last2) into the memory beginning at first1.
		// The destination range is unchecked.  Returns pointer to the end of the destination 
		// range.
		template<typename InputIterator>
		pointer move_construct_from_range(pointer first1, InputIterator first2, InputIterator const last2) {
			while(first2 != last2) { construct(first1, std::move(*first2)); ++first1; ++first2; }
			return first1;
		}

		// Copy-construct a range of elements [first1, last1) from the range beginning with first2.  
		// The source range is unchecked.  Returns iterator to the end of the source range.
		template<typename InputIterator>
		InputIterator copy_construct_range(pointer first1, pointer const last1, InputIterator first2) {
			while(first1 != last1) { construct(first1, *first2); ++first1; ++first2; }
			return first2;
		}

		// Move-construct a range of elements [first1, last1) from the range beginning with first2.  
		// The source range is unchecked.  Returns iterator to the end of the source range.
		template<typename InputIterator>
		InputIterator move_construct_range(pointer first1, pointer const last1, InputIterator first2) {
			while(first1 != last1) { construct(first1, std::move(*first2)); ++first1; ++first2; }
			return first2;
		}
		
		// Copy the first N elements of the source range to the destination range.  Neither the
		// source or destination ranges are checked.
		// Returns a tuple containing the ends of both ranges (dest+count, src+count).
		template<typename InputIterator>
		std::pair<pointer, InputIterator> copy_construct_range_n(pointer dest, InputIterator src, size_type count) {
			// This is probably not as good as a pointer range in this case, because it's an extra 
			// arithmetic operation, but it depends on compiler and CPU environment.  The CPU could easily 
			// do the count decrement "on the side" using its branch-prediction machinery, since the count
			// is exclusively a loop counter and not needed anywhere else, making it basically free.
			while(count != 0) { construct(dest, *src); ++dest; ++src; --count; }
			return std::make_pair(dest, src);
		}

		// Move the first N elements of the source range to the destination range.  Neither the
		// source or destination ranges are checked.
		// Returns a tuple containing the ends of both ranges (dest+count, src+count).		
		template<typename InputIterator>
		std::pair<pointer, InputIterator> move_construct_range_n(pointer dest, InputIterator src, size_type count) {
			while(count != 0) { construct(dest, std::move(*src)); ++dest; ++src; --count; }
			return std::make_pair(dest, src);
		}

		///////////////////////////////////
		// Checked Range Copy/Move Construction
		// These routines return when either the destination or source ranges are consumed.  The 
		// return value is a pair containing the final values of first1 and first2.
		
		// Copy-constructs a range of elements from an input range
		template<typename InputIterator>
		std::pair<pointer, InputIterator> checked_copy_construct_range(pointer first1, pointer const last1, InputIterator first2, InputIterator const last2) {
			// TODO: Optimise this for InputIterators for which std::distance() is O(1)?
			// TODO: Check if this works
			if(std::is_same_v<std::random_access_iterator_tag, typename std::iterator_traits<InputIterator>::iterator_category>) {
				// If range 2 is smaller than range 1, adjust range1 to be the same size
				const auto dist1 = std::distance(first1, last1);
				const auto dist2 = std::distance(first2, last2);
				if(dist2 < dist1) { last1 = first1 + dist2; }
				while(first1 != last1) {
					construct(first1, *first2);
					++first1;
					++first2;
				}

			} else {
				while(first1 != last1 && first2 != last2) {
					construct(first1, *first2);
					++first1;
					++first2;
				}
			}

			return std::make_pair(first1, first2);
		}

		// Move-constructs a range of elements from an input range
		template<typename InputIterator>
		std::pair<pointer, InputIterator> checked_move_construct_range(pointer first1, pointer const last1, InputIterator first2, InputIterator const last2) {
			// TODO: Optimise this for InputIterators for which std::distance() is O(1)?
			// TODO: Check if this works
			if(std::is_same_v<std::random_access_iterator_tag, typename std::iterator_traits<InputIterator>::iterator_category>) {
				// If range 2 is smaller than range 1, adjust range1 to be the same size
				const auto dist1 = std::distance(first1, last1);
				const auto dist2 = std::distance(first2, last2);
				if(dist2 < dist1) { last1 = first1 + dist2; }
				while(first1 != last1) {
					construct(first1, std::move(*first2));
					++first1;
					++first2;
				}

			} else {
				while(first1 != last1 && first2 != last2) {
					construct(first1, std::move(*first2));
					++first1;
					++first2;
				}
			}

			return std::make_pair(first1, first2);
		}

		///////////////////////////////////////////////////////
		// Range Assignment
		// Set initialised range to value_type().  Returns last.
		pointer assign_range(pointer first, pointer const last) {
			// We can potentially optimise this if value_type has a trivial destructor.  If destroy() is a 
			// no-op, we can just treat this as a default-construct operation, which will be at least as 
			// fast as copy-assign, and probably itself a no-op.
			if(value_has_trivial_destroy) {
				construct_range(first, last); 
				return last;
			} else {
				// TODO: Test optimisation
				assign_range(first, last, value_type());
				return last;
				//const value_type val;
				//while(first != last) { *first = val; ++first; }
				//return first;
			}
		}

		// Set initialised range to exemplar element.  Returns last.
		pointer assign_range(pointer first, pointer const last, const_reference val) {
			if(value_has_trivial_destroy) {
				construct_range(first, last, val);
				return last;

			} else {
				while(first != last) { *first = val; ++first; }
				return last;
			}
		}

		// Set N initialised elements to exemplar elements.  Returns pointer to one past the last 
		// assigned.
		pointer assign_n(pointer first, size_type count) {
			// TODO: Test optimisation
			return value_has_trivial_destroy ? construct_n(first, count) : assign_n(first, count, value_type());
		}

		// Set N initialised elements to exemplar elements.  Returns pointer to one past the last
		// assigned.
		pointer assign_n(pointer first, size_type count, const_reference val) {
			if(value_has_trivial_destroy) {
				return construct_n(first, count, val);

			} else {
				while(count) { *first = val; ++first; --count; }
				return first;
			}
		}

		///////////////////////////////////
		// Unchecked Range Copy/Move Assign

		// Copy-assign a range of elements [first2, last2) into the memory beginning at first1.
		// The destination range is unchecked.  Returns pointer to the end of the destination 
		// range.
		template<typename InputIterator>
		pointer copy_assign_from_range(pointer first1, InputIterator first2, InputIterator const last2) {
			if(value_has_trivial_destroy) {
				return copy_construct_from_range(first1, first2, last2);

			} else {
				while(first2 != last2) { *first1 = *first2; ++first1; ++first2; }
				return first1;
			}
		}

		// Move-assign a range of elements [first2, last2) into the memory beginning at first1.
		// The destination range is unchecked.  Returns pointer to the end of the destination 
		// range.
		template<typename InputIterator>
		pointer move_assign_from_range(pointer first1, InputIterator first2, InputIterator const last2) {
			if(value_has_trivial_destroy) {
				return move_construct_from_range(first1, first2, last2);

			} else {
				while(first2 != last2) { *first1 = std::move(*first2); ++first1; ++first2; }
				return first1;
			}
		}

		// Copy-assign a range of elements [first1, last1) from the range beginning with first2.  
		// The source range is unchecked.  Returns iterator to the end of the source range.
		template<typename InputIterator>
		InputIterator copy_assign_range(pointer first1, pointer const last1, InputIterator first2) {
			if(value_has_trivial_destroy) {
				return copy_construct_range(first1, last1, first2);

			} else {
				while(first1 != last1) {
					*first1 = *first2; 
					++first1; ++first2;
				}
				return first2;
			}
		}

		// Move-assign a range of elements [first1, last1) from the range beginning with first2.  
		// The source range is unchecked.  Returns iterator to the end of the source range.
		template<typename InputIterator>
		InputIterator move_assign_range(pointer first1, pointer const last1, InputIterator first2) {
			if(value_has_trivial_destroy) {
				return move_construct_range(first1, last1, first2);

			} else {
				while(first1 != last1) { 
					*first1 = std::move(*first2); 
					++first1; ++first2; 
				}
				return first2;
			}
		}

		///////////////////////////////////
		// Checked Range Copy/Move Assign
		// These routines return when either the destination or source ranges are consumed.  The 
		// return value is a pair containing the final values of first1 and first2.

		// Copy-assigns a range of elements from an input range
		template<typename InputIterator>
		std::pair<pointer, InputIterator> checked_copy_assign_range(pointer first1, pointer last1, InputIterator first2, InputIterator const last2) {
			if(value_has_trivial_destroy) {
				return checked_copy_construct_range(first1, last1, first2, last2);

			} else {
				while(first1 != last1 && first2 != last2) {
					*first1 = *first2;
					++first1;
					++first2;
				}
			}

			return std::make_pair(first1, first2);
		}

		// Move-assigns a range of elements from an input range
		template<typename InputIterator>
		std::pair<pointer, InputIterator> checked_move_assign_range(pointer first1, pointer last1, InputIterator first2, InputIterator const last2) {
			if(value_has_trivial_destroy) {
				return checked_move_construct_range(first1, last1, first2, last2);

			} else {
				while(first1 != last1 && first2 != last2) {
					*first1 = std::move(*first2);
					++first1;
					++first2;
				}
			}

			return std::make_pair(first1, first2);
		}
	};
} }