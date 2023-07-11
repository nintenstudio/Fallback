#pragma once

#include <memory>

// Taken from GCC libstdc++ 13.1.0
namespace std {
#if (!FALLBACK_SHARED_PTR_ATOMIC_SUPPORTED)
    template<typename _Tp>
    class _Sp_atomic
    {
      using value_type = _Tp;

      friend class atomic<_Tp>;

      // An atomic version of __shared_count<> and __weak_count<>.
      // Stores a _Sp_counted_base<>* but uses the LSB as a lock.
      struct _Atomic_count
      {
	// Either __shared_count<> or __weak_count<>
	using __count_type = decltype(_Tp::_M_refcount);

	// _Sp_counted_base<>*
	using pointer = decltype(__count_type::_M_pi);

	// Ensure we can use the LSB as the lock bit.
	static_assert(alignof(remove_pointer_t<pointer>) > 1);

	constexpr _Atomic_count() noexcept = default;

	explicit
	_Atomic_count(__count_type&& __c) noexcept
	: _M_val(reinterpret_cast<uintptr_t>(__c._M_pi))
	{
	  __c._M_pi = nullptr;
	}

	~_Atomic_count()
	{
	  auto __val = _M_val.load(memory_order_relaxed);
	  _GLIBCXX_TSAN_MUTEX_DESTROY(&_M_val);
	  __glibcxx_assert(!(__val & _S_lock_bit));
	  if (auto __pi = reinterpret_cast<pointer>(__val))
	    {
	      if constexpr (__is_shared_ptr<_Tp>)
		__pi->_M_release();
	      else
		__pi->_M_weak_release();
	    }
	}

	_Atomic_count(const _Atomic_count&) = delete;
	_Atomic_count& operator=(const _Atomic_count&) = delete;

	// Precondition: Caller does not hold lock!
	// Returns the raw pointer value without the lock bit set.
	pointer
	lock(memory_order __o) const noexcept
	{
	  // To acquire the lock we flip the LSB from 0 to 1.

	  auto __current = _M_val.load(memory_order_relaxed);
	  while (__current & _S_lock_bit)
	    {
#if __cpp_lib_atomic_wait
	      __detail::__thread_relax();
#endif
	      __current = _M_val.load(memory_order_relaxed);
	    }

	  _GLIBCXX_TSAN_MUTEX_TRY_LOCK(&_M_val);

	  while (!_M_val.compare_exchange_strong(__current,
						 __current | _S_lock_bit,
						 __o,
						 memory_order_relaxed))
	    {
	      _GLIBCXX_TSAN_MUTEX_TRY_LOCK_FAILED(&_M_val);
#if __cpp_lib_atomic_wait
	      __detail::__thread_relax();
#endif
	      __current = __current & ~_S_lock_bit;
	      _GLIBCXX_TSAN_MUTEX_TRY_LOCK(&_M_val);
	    }
	  _GLIBCXX_TSAN_MUTEX_LOCKED(&_M_val);
	  return reinterpret_cast<pointer>(__current);
	}

	// Precondition: caller holds lock!
	void
	unlock(memory_order __o) const noexcept
	{
	  _GLIBCXX_TSAN_MUTEX_PRE_UNLOCK(&_M_val);
	  _M_val.fetch_sub(1, __o);
	  _GLIBCXX_TSAN_MUTEX_POST_UNLOCK(&_M_val);
	}

	// Swaps the values of *this and __c, and unlocks *this.
	// Precondition: caller holds lock!
	void
	_M_swap_unlock(__count_type& __c, memory_order __o) noexcept
	{
	  if (__o != memory_order_seq_cst)
	    __o = memory_order_release;
	  auto __x = reinterpret_cast<uintptr_t>(__c._M_pi);
	  _GLIBCXX_TSAN_MUTEX_PRE_UNLOCK(&_M_val);
	  __x = _M_val.exchange(__x, __o);
	  _GLIBCXX_TSAN_MUTEX_POST_UNLOCK(&_M_val);
	  __c._M_pi = reinterpret_cast<pointer>(__x & ~_S_lock_bit);
	}

#if __cpp_lib_atomic_wait
	// Precondition: caller holds lock!
	void
	_M_wait_unlock(memory_order __o) const noexcept
	{
	  _GLIBCXX_TSAN_MUTEX_PRE_UNLOCK(&_M_val);
	  auto __v = _M_val.fetch_sub(1, memory_order_relaxed);
	  _GLIBCXX_TSAN_MUTEX_POST_UNLOCK(&_M_val);
	  _M_val.wait(__v & ~_S_lock_bit, __o);
	}

	void
	notify_one() noexcept
	{
	  _GLIBCXX_TSAN_MUTEX_PRE_SIGNAL(&_M_val);
	  _M_val.notify_one();
	  _GLIBCXX_TSAN_MUTEX_POST_SIGNAL(&_M_val);
	}

	void
	notify_all() noexcept
	{
	  _GLIBCXX_TSAN_MUTEX_PRE_SIGNAL(&_M_val);
	  _M_val.notify_all();
	  _GLIBCXX_TSAN_MUTEX_POST_SIGNAL(&_M_val);
	}
#endif

      private:
	mutable __atomic_base<uintptr_t> _M_val{0};
	static constexpr uintptr_t _S_lock_bit{1};
      };

      typename _Tp::element_type* _M_ptr = nullptr;
      _Atomic_count _M_refcount;

      static typename _Atomic_count::pointer
      _S_add_ref(typename _Atomic_count::pointer __p)
      {
	if (__p)
	  {
	    if constexpr (__is_shared_ptr<_Tp>)
	      __p->_M_add_ref_copy();
	    else
	      __p->_M_weak_add_ref();
	  }
	return __p;
      }

      constexpr _Sp_atomic() noexcept = default;

      explicit
      _Sp_atomic(value_type __r) noexcept
      : _M_ptr(__r._M_ptr), _M_refcount(std::move(__r._M_refcount))
      { }

      ~_Sp_atomic() = default;

      _Sp_atomic(const _Sp_atomic&) = delete;
      void operator=(const _Sp_atomic&) = delete;

      value_type
      load(memory_order __o) const noexcept
      {
	__glibcxx_assert(__o != memory_order_release
			   && __o != memory_order_acq_rel);
	// Ensure that the correct value of _M_ptr is visible after locking.,
	// by upgrading relaxed or consume to acquire.
	if (__o != memory_order_seq_cst)
	  __o = memory_order_acquire;

	value_type __ret;
	auto __pi = _M_refcount.lock(__o);
	__ret._M_ptr = _M_ptr;
	__ret._M_refcount._M_pi = _S_add_ref(__pi);
	_M_refcount.unlock(memory_order_relaxed);
	return __ret;
      }

      void
      swap(value_type& __r, memory_order __o) noexcept
      {
	_M_refcount.lock(memory_order_acquire);
	std::swap(_M_ptr, __r._M_ptr);
	_M_refcount._M_swap_unlock(__r._M_refcount, __o);
      }

      bool
      compare_exchange_strong(value_type& __expected, value_type __desired,
			      memory_order __o, memory_order __o2) noexcept
      {
	bool __result = true;
	auto __pi = _M_refcount.lock(memory_order_acquire);
	if (_M_ptr == __expected._M_ptr
	      && __pi == __expected._M_refcount._M_pi)
	  {
	    _M_ptr = __desired._M_ptr;
	    _M_refcount._M_swap_unlock(__desired._M_refcount, __o);
	  }
	else
	  {
	    _Tp __sink = std::move(__expected);
	    __expected._M_ptr = _M_ptr;
	    __expected._M_refcount._M_pi = _S_add_ref(__pi);
	    _M_refcount.unlock(__o2);
	    __result = false;
	  }
	return __result;
      }

#if __cpp_lib_atomic_wait
      void
      wait(value_type __old, memory_order __o) const noexcept
      {
	auto __pi = _M_refcount.lock(memory_order_acquire);
	if (_M_ptr == __old._M_ptr && __pi == __old._M_refcount._M_pi)
	  _M_refcount._M_wait_unlock(__o);
	else
	  _M_refcount.unlock(memory_order_relaxed);
      }

      void
      notify_one() noexcept
      {
	_M_refcount.notify_one();
      }

      void
      notify_all() noexcept
      {
	_M_refcount.notify_all();
      }
#endif
    };


    template<typename _Tp>
    class atomic<shared_ptr<_Tp>>
    {
    public:
      using value_type = shared_ptr<_Tp>;

      static constexpr bool is_always_lock_free = false;

      bool
      is_lock_free() const noexcept
      { return false; }

      constexpr atomic() noexcept = default;

      // _GLIBCXX_RESOLVE_LIB_DEFECTS
      // 3661. constinit atomic<shared_ptr<T>> a(nullptr); should work
      constexpr atomic(nullptr_t) noexcept : atomic() { }

      atomic(shared_ptr<_Tp> __r) noexcept
      : _M_impl(std::move(__r))
      { }

      atomic(const atomic&) = delete;
      void operator=(const atomic&) = delete;

      shared_ptr<_Tp>
      load(memory_order __o = memory_order_seq_cst) const noexcept
      { return _M_impl.load(__o); }

      operator shared_ptr<_Tp>() const noexcept
      { return _M_impl.load(memory_order_seq_cst); }

      void
      store(shared_ptr<_Tp> __desired,
	    memory_order __o = memory_order_seq_cst) noexcept
      { _M_impl.swap(__desired, __o); }

      void
      operator=(shared_ptr<_Tp> __desired) noexcept
      { _M_impl.swap(__desired, memory_order_seq_cst); }

      // _GLIBCXX_RESOLVE_LIB_DEFECTS
      // 3893. LWG 3661 broke atomic<shared_ptr<T>> a; a = nullptr;
      void
      operator=(nullptr_t) noexcept
      { store(nullptr); }

      shared_ptr<_Tp>
      exchange(shared_ptr<_Tp> __desired,
	       memory_order __o = memory_order_seq_cst) noexcept
      {
	_M_impl.swap(__desired, __o);
	return __desired;
      }

      bool
      compare_exchange_strong(shared_ptr<_Tp>& __expected,
			      shared_ptr<_Tp> __desired,
			      memory_order __o, memory_order __o2) noexcept
      {
	return _M_impl.compare_exchange_strong(__expected, __desired, __o, __o2);
      }

      bool
      compare_exchange_strong(value_type& __expected, value_type __desired,
			      memory_order __o = memory_order_seq_cst) noexcept
      {
	memory_order __o2;
	switch (__o)
	{
	case memory_order_acq_rel:
	  __o2 = memory_order_acquire;
	  break;
	case memory_order_release:
	  __o2 = memory_order_relaxed;
	  break;
	default:
	  __o2 = __o;
	}
	return compare_exchange_strong(__expected, std::move(__desired),
				       __o, __o2);
      }

      bool
      compare_exchange_weak(value_type& __expected, value_type __desired,
			    memory_order __o, memory_order __o2) noexcept
      {
	return compare_exchange_strong(__expected, std::move(__desired),
				       __o, __o2);
      }

      bool
      compare_exchange_weak(value_type& __expected, value_type __desired,
			    memory_order __o = memory_order_seq_cst) noexcept
      {
	return compare_exchange_strong(__expected, std::move(__desired), __o);
      }

#if __cpp_lib_atomic_wait
      void
      wait(value_type __old,
	   memory_order __o = memory_order_seq_cst) const noexcept
      {
	_M_impl.wait(std::move(__old), __o);
      }

      void
      notify_one() noexcept
      {
	_M_impl.notify_one();
      }

      void
      notify_all() noexcept
      {
	_M_impl.notify_all();
      }
#endif

    private:
      _Sp_atomic<shared_ptr<_Tp>> _M_impl;
    };
#endif
}