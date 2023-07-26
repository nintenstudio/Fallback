#pragma once

#include <memory>
#include <shared_mutex>

// Taken from GCC libstdc++ 13.1.0 and heavily modified to not depend on it.
// Currently uses a shared mutex to lock on read and writes.
namespace std {
#if (!FALLBACK_SHARED_PTR_ATOMIC_SUPPORTED)
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
      : _M_ptr(std::move(__r))
      { }

      atomic(const atomic&) = delete;
      void operator=(const atomic&) = delete;

      shared_ptr<_Tp>
      load(memory_order __o = memory_order_seq_cst) const noexcept
      { shared_lock<shared_mutex> lock(_M_mtx); return _M_ptr; }

      operator shared_ptr<_Tp>() const noexcept
      { shared_lock<shared_mutex> lock(_M_mtx); return _M_ptr; }

      void
      store(shared_ptr<_Tp> __desired,
	    memory_order __o = memory_order_seq_cst) noexcept
      { unique_lock<shared_mutex> lock(_M_mtx); _M_ptr = __desired; }

      void
      operator=(shared_ptr<_Tp> __desired) noexcept
      { unique_lock<shared_mutex> lock(_M_mtx); _M_ptr = __desired; }

      // _GLIBCXX_RESOLVE_LIB_DEFECTS
      // 3893. LWG 3661 broke atomic<shared_ptr<T>> a; a = nullptr;
      void
      operator=(nullptr_t) noexcept
      { store(nullptr); }

      shared_ptr<_Tp>
      exchange(shared_ptr<_Tp> __desired,
	       memory_order __o = memory_order_seq_cst) noexcept
      {
		unique_lock<shared_mutex> lock(_M_mtx);
		shared_ptr<_Tp> prev = _M_ptr;
		_M_ptr = __desired;
		return prev;
      }

      bool
      compare_exchange_strong(shared_ptr<_Tp>& __expected,
			      shared_ptr<_Tp> __desired,
			      memory_order __o, memory_order __o2) noexcept
      {
		if (*this == __expected) {
			*this = __desired;
			return true;
		}
		else {
			__expected = *this;
			return false;
		}
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
      shared_ptr<_Tp> _M_ptr;
	mutable shared_mutex _M_mtx;
    };
#endif
}