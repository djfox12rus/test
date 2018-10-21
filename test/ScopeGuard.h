#pragma once
#include <exception>

namespace detail {

	class ScopeGuardImplBase {
	public:
		void dismiss() noexcept {
			dismissed_ = true;
		}

	protected:
		ScopeGuardImplBase() noexcept : dismissed_(false) {}

		//static void warnAboutToCrash() noexcept;
		static ScopeGuardImplBase makeEmptyScopeGuard() noexcept {
			return ScopeGuardImplBase{};
		}

		template <typename T>
		static const T& asConst(const T& t) noexcept {
			return t;
		}

		bool dismissed_;
	};

	template <typename FunctionType, bool InvokeNoexcept>
	class ScopeGuardImpl : public ScopeGuardImplBase {
	public:
		explicit ScopeGuardImpl(FunctionType& fn) noexcept(
			std::is_nothrow_copy_constructible<FunctionType>::value)
			: ScopeGuardImpl(
				asConst(fn),
				makeFailsafe(
					std::is_nothrow_copy_constructible<FunctionType>{},
					&fn)) {}

		explicit ScopeGuardImpl(const FunctionType& fn) noexcept(
			std::is_nothrow_copy_constructible<FunctionType>::value)
			: ScopeGuardImpl(
				fn,
				makeFailsafe(
					std::is_nothrow_copy_constructible<FunctionType>{},
					&fn)) {}

		explicit ScopeGuardImpl(FunctionType&& fn) noexcept(
			std::is_nothrow_move_constructible<FunctionType>::value)
			: ScopeGuardImpl(
				std::move_if_noexcept(fn),
				makeFailsafe(
					std::is_nothrow_move_constructible<FunctionType>{},
					&fn)) {}

		ScopeGuardImpl(ScopeGuardImpl&& other) noexcept(
			std::is_nothrow_move_constructible<FunctionType>::value)
			: function_(std::move_if_noexcept(other.function_)) {
			// If the above line attempts a copy and the copy throws, other is
			// left owning the cleanup action and will execute it (or not) depending
			// on the value of other.dismissed_. The following lines only execute
			// if the move/copy succeeded, in which case *this assumes ownership of
			// the cleanup action and dismisses other.
			dismissed_ = exchange(other.dismissed_, true);
		}

		~ScopeGuardImpl() noexcept(InvokeNoexcept) {
			if (!dismissed_) {
				execute();
			}
		}

	private:
		static ScopeGuardImplBase makeFailsafe(std::true_type, const void*) noexcept {
			return makeEmptyScopeGuard();
		}

		template <typename Fn>
		static auto makeFailsafe(std::false_type, Fn* fn) noexcept
			-> ScopeGuardImpl<decltype(std::ref(*fn)), InvokeNoexcept> {
			return ScopeGuardImpl<decltype(std::ref(*fn)), InvokeNoexcept>{
				std::ref(*fn)};
		}

		template <typename Fn>
		explicit ScopeGuardImpl(Fn&& fn, ScopeGuardImplBase&& failsafe)
			: ScopeGuardImplBase{}, function_(std::forward<Fn>(fn)) {
			failsafe.dismiss();
		}

		void* operator new(std::size_t) = delete;

		void execute() noexcept(InvokeNoexcept) {
			if (InvokeNoexcept) {
				try {
					function_();
				}
				catch (...) {
					//warnAboutToCrash();
					std::terminate();
				}
			}
			else {
				function_();
			}
		}

		FunctionType function_;
	};

	template <typename F, bool INE>
	using ScopeGuardImplDecay = ScopeGuardImpl<typename std::decay<F>::type, INE>;

} // namespace detail


template <typename F>
detail::ScopeGuardImplDecay<F, true> makeGuard(F&& f) noexcept(
	noexcept(detail::ScopeGuardImplDecay<F, true>(static_cast<F&&>(f)))) {
	return detail::ScopeGuardImplDecay<F, true>(static_cast<F&&>(f));
}

namespace detail {



	/**
	 * ScopeGuard used for executing a function when leaving the current scope
	 * depending on the presence of a new uncaught exception.
	 *
	 * If the executeOnException template parameter is true, the function is
	 * executed if a new uncaught exception is present at the end of the scope.
	 * If the parameter is false, then the function is executed if no new uncaught
	 * exceptions are present at the end of the scope.
	 *
	 * Used to implement SCOPE_FAIL and SCOPE_SUCCESS below.
	 */
	template <typename FunctionType, bool ExecuteOnException>
	class ScopeGuardForNewException {
	public:
		explicit ScopeGuardForNewException(const FunctionType& fn) : guard_(fn) {}

		explicit ScopeGuardForNewException(FunctionType&& fn)
			: guard_(std::move(fn)) {}

		ScopeGuardForNewException(ScopeGuardForNewException&& other) = default;

		~ScopeGuardForNewException() noexcept(ExecuteOnException) {
			if (ExecuteOnException != (exceptionCounter_ < std::uncaught_exceptions())) {
				guard_.dismiss();
			}
		}

	private:
		void* operator new(std::size_t) = delete;
		void operator delete(void*) = delete;

		ScopeGuardImpl<FunctionType, ExecuteOnException> guard_;
		int exceptionCounter_{ std::uncaught_exceptions() };
	};

	/**
	 * Internal use for the macro SCOPE_FAIL below
	 */
	enum class ScopeGuardOnFail {};

	template <typename FunctionType>
	ScopeGuardForNewException<typename std::decay<FunctionType>::type, true>
		operator+(detail::ScopeGuardOnFail, FunctionType&& fn) {
		return ScopeGuardForNewException<
			typename std::decay<FunctionType>::type,
			true>(std::forward<FunctionType>(fn));
	}

	/**
	 * Internal use for the macro SCOPE_SUCCESS below
	 */
	enum class ScopeGuardOnSuccess {};

	template <typename FunctionType>
	ScopeGuardForNewException<typename std::decay<FunctionType>::type, false>
		operator+(ScopeGuardOnSuccess, FunctionType&& fn) {
		return ScopeGuardForNewException<
			typename std::decay<FunctionType>::type,
			false>(std::forward<FunctionType>(fn));
	}

	/**
	 * Internal use for the macro SCOPE_EXIT below
	 */
	enum class ScopeGuardOnExit {};

	template <typename FunctionType>
	ScopeGuardImpl<typename std::decay<FunctionType>::type, true> operator+(
		detail::ScopeGuardOnExit,
		FunctionType&& fn) {
		return ScopeGuardImpl<typename std::decay<FunctionType>::type, true>(
			std::forward<FunctionType>(fn));
	}
} // namespace detail


#define CONCATENATE_IMPL(s1, s2) s1##s2
#define CONCATENATE(s1, s2) CONCATENATE_IMPL(s1, s2)

#ifdef __COUNTER__
#define ANONYMOUS_VARIABLE(str)\
		CONCATENATE (str, __COUNTER__)
#else
#define ANONYMOUS_VARIABLE(str)\
		CONCATENATE (str, __LINE__)
#endif // __COUNTER__



#define SCOPE_EXIT                               \
  auto ANONYMOUS_VARIABLE(SCOPE_EXIT_STATE) = \
      ::detail::ScopeGuardOnExit() + [&]() noexcept


#define SCOPE_FAIL                               \
  auto ANONYMOUS_VARIABLE(SCOPE_FAIL_STATE) = \
      ::detail::ScopeGuardOnFail() + [&]() noexcept

#define SCOPE_SUCCESS                               \
  auto ANONYMOUS_VARIABLE(SCOPE_SUCCESS_STATE) = \
      ::detail::ScopeGuardOnSuccess() + [&]()