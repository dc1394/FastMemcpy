#include "MyRandom.h"
#include <stdexcept>
#include <cstring>
#include <intrin.h>
#include <windows.h>
#include <boost/assert.hpp>
#include <boost/optional.hpp>
#include <boost/type_traits.hpp>
#include <boost/static_assert.hpp>
#include <boost/utility/enable_if.hpp>

#ifdef _DEBUG
	#include <ostream>
	#include <stdexcept>
#endif

#if (_MSC_VER >= 1700)
	#include <system_error>
#else
	#include <boost/system/error_code.hpp>
	#include <boost/system/system_error.hpp>
#endif

#if (_MSC_VER >= 1600)
	#include <tuple>
	#include <cstdint>
#else
	#include <boost/cstdint.hpp>
	#include <boost/shared_ptr.hpp>
	#include <boost/tuple/tuple.hpp>
	#include <boost/noncopyable.hpp>
#endif

#if defined(_OPENMP)
	#include <omp.h>
	#define cilk_for for
#elif defined(__cilk)
	#include <cilk/cilk.h>
#else
	BOOST_STATIC_ASSERT(false)
#endif

namespace FastMemcpy {
#if (_MSC_VER >= 1600)
	using std::uint32_t;
	using std::get;
	using std::tie;
	using std::tuple;
	using std::make_tuple;
	using std::system_error;

#ifdef _WIN64
	typedef std::uint64_t uinttype;
#else
	typedef std::uint32_t uinttype;
#endif

#else
	using boost::uint32_t;
	using boost::get;
	using boost::tie;
	using boost::tuple;
	using boost::make_tuple;
	using boost::system::system_error;

#ifdef _WIN64
	typedef boost::uint64_t uinttype;
#else
	typedef boost::uint32_t uinttype;
#endif

#endif

	typedef tuple<const char * const, char * const, const std::size_t> mytuple;

#if defined(__INTEL_COMPILER) || defined(__GXX_EXPERIMENTAL_CXX0X__)
	static constexpr std::size_t PREFETCHSIZE = 4096;
	static constexpr uinttype ZeroFh = 0x0Fu;

#ifdef _WIN64
	static constexpr uinttype MASK = 0xFFFFFFFFFFFFFFF0ull;
#else
	static constexpr uinttype MASK = 0xFFFFFFF0ull;
#endif

#else
	static const std::size_t PREFETCHSIZE = 4096;
	static const uinttype ZeroFh = 0x0Fu;

#ifdef _WIN64
	static const uinttype MASK = 0xFFFFFFFFFFFFFFF0ull;
#else
	static const uinttype MASK = 0xFFFFFFF0ull;
#endif

#endif

#if (_MSC_VER < 1600)
	const // const オブジェクトであって、
	class nullptr_t
		: private boost::noncopyable
	{
		void operator&() const;	// アドレスを取得することができない

	public:
		template <typename T>
		operator T*() const		// 任意の非メンバ型のヌルポインタや、
		{ return 0; }
 
		template <typename C, typename T>
		operator T C::*() const	// 任意のメンバ型のヌルポインタに変換可能である、
		{ return 0; }
	} nullptr = {};
#endif

	template <typename T>
	class MyDeleter {
		T * const p_;
	public:
		MyDeleter(T * const p) : p_(p) {}
		void operator()(const T * const) const {
			::VirtualFree(p_, 0, MEM_RELEASE);
		}
	};

	template <typename T>
	struct smart_pointer {
#if (_MSC_VER >= 1600)
		typedef std::unique_ptr<T, const MyDeleter<T>> type;
#else
		typedef boost::shared_ptr<T> type;
#endif
	};

	template <typename T>
#if (_MSC_VER < 1600)
	const
#endif
	typename smart_pointer<T>::type allocate_init(std::size_t num, bool clear = false);

	template <typename T>
#if (_MSC_VER < 1600)
	const
#endif
	typename smart_pointer<T>::type aligned_allocate_init(std::size_t num, bool clear = false);

	template <typename T>
#if (_MSC_VER < 1600)
	const
#endif
	boost::optional<const mytuple> check(const typename smart_pointer<T>::type & _Src,
										 typename smart_pointer<T>::type & _Dst,
										 std::size_t num);

	template <typename T>
	void copy(const typename smart_pointer<T>::type & _Src,
			  typename smart_pointer<T>::type & _Dst,
			  std::size_t num);

	template <typename T, bool U>
	void copy(const typename smart_pointer<T>::type & _Src,
			  typename smart_pointer<T>::type & _Dst,
			  std::size_t num);

	template <typename T, bool U>
	void aligned_copy(const typename smart_pointer<T>::type & _Src,
					  typename smart_pointer<T>::type & _Dst,
					  std::size_t num);

	template <bool U>
	void memcpy(char * _Dst, const char * _Src, std::size_t _Size,
				typename boost::disable_if_c<U>::type* = nullptr);

	template <bool U>
	void memcpy(char * _Dst, const char * _Src, std::size_t _Size,
				typename boost::enable_if_c<U>::type* = nullptr);

	template <int N, bool U>
	void memcpy(char * _Dst, const char * _Src, std::size_t _Size,
				typename boost::disable_if_c<U>::type* = nullptr);

	template <int N, bool U>
	void memcpy(char * _Dst, const char * _Src, std::size_t _Size,
				typename boost::enable_if_c<U>::type* = nullptr);

#ifdef _DEBUG
	template <typename T>
	void memcheck(const typename smart_pointer<T>::type & ptr,
				  const typename smart_pointer<T>::type & ptr2,
				  std::size_t size);
#endif
}

namespace FastMemcpy {
	static MyRandom::MyRand mr(boost::numeric::interval<int>(1, ZeroFh));

	template <typename T>
#if (_MSC_VER < 1600)
	const
#endif
	typename smart_pointer<T>::type allocate_init(std::size_t num, bool clear)
	{
		const std::size_t size = num * sizeof(T);
		const uint32_t shift = mr.myrand();
		void * const p = reinterpret_cast<T * const>(::VirtualAlloc(nullptr, size + shift,
																	MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
		if (!p) {
#if (_MSC_VER >= 1700)
			throw system_error(std::error_code(::GetLastError(), std::system_category()));
#else
			throw system_error(boost::system::error_code(
								::GetLastError(),
								boost::system::get_system_category()));
#endif
		}

		T * const ptr = reinterpret_cast<T * const>(reinterpret_cast<uinttype>(p) + shift);
		if (clear)
			::ZeroMemory(ptr, size);
		else
			for (std::size_t i = 0; i < num; i++)
				ptr[i] = static_cast<int>(i);

		return smart_pointer<T>::type(ptr, MyDeleter<T>(reinterpret_cast<T * const>(p)));
	}

	template <typename T>
#if (_MSC_VER < 1600)
	const
#endif
	typename smart_pointer<T>::type aligned_allocate_init(std::size_t num, bool clear)
	{
		const std::size_t size = num * sizeof(T);
		void * const p = ::VirtualAlloc(nullptr, size + ZeroFh,
										MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		if (!p) {
#if (_MSC_VER >= 1700)
			throw std::system_error(std::error_code(::GetLastError(), std::system_category()));
#else
			throw boost::system::system_error(boost::system::error_code(
											  ::GetLastError(),
											  boost::system::get_system_category()));
#endif
		}

		T * const ptr = reinterpret_cast<T * const>(
			(reinterpret_cast<uinttype>(p) + ZeroFh) & MASK);

		if (clear)
			::ZeroMemory(ptr, size);
		else
			for (std::size_t i = 0; i < num; i++)
				ptr[i] = static_cast<int>(i);
		
		return smart_pointer<T>::type(ptr, MyDeleter<T>(reinterpret_cast<T * const>(p)));
	}

	template <typename T>
#if (_MSC_VER < 1600)
	const
#endif
	boost::optional<const mytuple> check(const typename smart_pointer<T>::type & _Src,
			  typename smart_pointer<T>::type & _Dst,
			  std::size_t num)
	{
		BOOST_STATIC_ASSERT(boost::is_pod<T>::value);
		BOOST_STATIC_ASSERT(sizeof(char) == 1);

		const char * const src = reinterpret_cast<const char * const>(_Src.get());
		char * const dst = reinterpret_cast<char * const>(_Dst.get());
		const std::size_t size = num * sizeof(T);
		if (size < 64) {
			std::memcpy(dst, src, size);
			return boost::none;
		}

		return boost::optional<const mytuple>(make_tuple(src, dst, size));
	}

	template <typename T>
	void copy(const typename smart_pointer<T>::type & _Src,
			  typename smart_pointer<T>::type & _Dst,
			  std::size_t num)
	{
		BOOST_STATIC_ASSERT(boost::is_pod<T>::value);
		std::memcpy(_Dst.get(), _Src.get(), num * sizeof(T));
	}

	template <typename T, bool U>
	void copy(const typename smart_pointer<T>::type & _Src,
			  typename smart_pointer<T>::type & _Dst,
			  std::size_t num)
	{
		const char * src;
		char * dst;
		std::size_t size;

		const boost::optional<const mytuple> pret(check<T>(_Src, _Dst, num));
		if (pret) {
			const mytuple ret(*pret);
			tie(src, dst, size) = ret;
		} else {
			return;
		}

		const uinttype tmp1 = (0x10u - (reinterpret_cast<uinttype>(src) & ZeroFh));
		const uinttype shift_src = (tmp1 != 16u) ? tmp1 : 0;
		
		if (shift_src) {
			std::memcpy(dst, src, shift_src);
			src = reinterpret_cast<const char *>(reinterpret_cast<uinttype>(src) + shift_src);
			dst = reinterpret_cast<char *>(reinterpret_cast<uinttype>(dst) + shift_src);
			size -= shift_src;
		}

		const uinttype tmp2 = 0x10u - (reinterpret_cast<uinttype>(dst) & ZeroFh);
		const uinttype shift_dst = (tmp2 != 16u) ? tmp2 : 0;

		switch (shift_dst) {
		case 0:
			memcpy<U>(dst, src, size);
			break;

		case 1:
			memcpy<1, U>(dst, src, size);
			break;

		case 2:
			memcpy<2, U>(dst, src, size);
			break;

		case 3:
			memcpy<3, U>(dst, src, size);
			break;

		case 4:
			memcpy<4, U>(dst, src, size);
			break;

		case 5:
			memcpy<5, U>(dst, src, size);
			break;

		case 6:
			memcpy<6, U>(dst, src, size);
			break;

		case 7:
			memcpy<7, U>(dst, src, size);
			break;

		case 8:
			memcpy<8, U>(dst, src, size);
			break;

		case 9:
			memcpy<9, U>(dst, src, size);
			break;

		case 10:
			memcpy<10, U>(dst, src, size);
			break;

		case 11:
			memcpy<11, U>(dst, src, size);
			break;

		case 12:
			memcpy<12, U>(dst, src, size);
			break;

		case 13:
			memcpy<13, U>(dst, src, size);
			break;

		case 14:
			memcpy<14, U>(dst, src, size);
			break;

		case 15:
			memcpy<15, U>(dst, src, size);
			break;

		default:
			BOOST_ASSERT(!"何らかのエラー！");
			break;
		}
	}

	template <typename T, bool U>
	void aligned_copy(const typename smart_pointer<T>::type & _Src,
			  typename smart_pointer<T>::type & _Dst,
			  std::size_t num)
	{
		const boost::optional<const mytuple> pret(check<T>(_Src, _Dst, num));
		if (pret) {
			const mytuple ret(*pret);
			if ((reinterpret_cast<std::size_t>(get<0>(ret)) & ZeroFh) ||
				(reinterpret_cast<std::size_t>(get<1>(ret)) & ZeroFh))
			throw std::runtime_error("メモリアライメントが異常です");
			memcpy<U>(get<1>(ret), get<0>(ret), get<2>(ret));
		} else {
			return;
		}
	}

	template <bool U>
	void memcpy(char * _Dst, const char * _Src, std::size_t _Size, typename boost::disable_if_c<U>::type*)
	{
		const int32_t nloop = boost::numeric_cast<const int32_t>(_Size >> 6);
		for (int32_t i = 0; i < nloop; i++) {
			const char * const s = _Src + (i << 6);
			char * const d = _Dst + (i << 6);

			_mm_prefetch(s + PREFETCHSIZE, _MM_HINT_NTA);

			const __m128i xmm0 = _mm_load_si128(reinterpret_cast<const __m128i * const>(s));
			const __m128i xmm1 = _mm_load_si128(reinterpret_cast<const __m128i * const>(s + 16));
			const __m128i xmm2 = _mm_load_si128(reinterpret_cast<const __m128i * const>(s + 32));
			const __m128i xmm3 = _mm_load_si128(reinterpret_cast<const __m128i * const>(s + 48));

			_mm_stream_si128(reinterpret_cast<__m128i * const>(d), xmm0);
			_mm_stream_si128(reinterpret_cast<__m128i * const>(d + 16), xmm1);
			_mm_stream_si128(reinterpret_cast<__m128i * const>(d + 32), xmm2);
			_mm_stream_si128(reinterpret_cast<__m128i * const>(d + 48), xmm3);
		}

		const char * const s = _Src + (nloop << 6);
		char * const d = _Dst + (nloop << 6);
		std::memcpy(d, s, _Size & 0x0000003F);
	}

	template <bool U>
	void memcpy(char * _Dst, const char * _Src, std::size_t _Size, typename boost::enable_if_c<U>::type*)
	{
		const int32_t nloop = boost::numeric_cast<const int32_t>(_Size >> 6);
#ifdef _OPENMP 
		#pragma omp parallel for schedule(guided)
#endif
		cilk_for (int32_t i = 0; i < nloop; i++) {
			const char * const s = _Src + (i << 6);
			char * const d = _Dst + (i << 6);

			_mm_prefetch(s + PREFETCHSIZE, _MM_HINT_NTA);

			const __m128i xmm0 = _mm_load_si128(reinterpret_cast<const __m128i * const>(s));
			const __m128i xmm1 = _mm_load_si128(reinterpret_cast<const __m128i * const>(s + 16));
			const __m128i xmm2 = _mm_load_si128(reinterpret_cast<const __m128i * const>(s + 32));
			const __m128i xmm3 = _mm_load_si128(reinterpret_cast<const __m128i * const>(s + 48));

			_mm_stream_si128(reinterpret_cast<__m128i * const>(d), xmm0);
			_mm_stream_si128(reinterpret_cast<__m128i * const>(d + 16), xmm1);
			_mm_stream_si128(reinterpret_cast<__m128i * const>(d + 32), xmm2);
			_mm_stream_si128(reinterpret_cast<__m128i * const>(d + 48), xmm3);
		}

		const char * const s = _Src + (nloop << 6);
		char * const d = _Dst + (nloop << 6);
		std::memcpy(d, s, _Size & 0x0000003F);
	}

	template <int N, bool U>
	void memcpy(char * _Dst, const char * _Src, std::size_t _Size, typename boost::disable_if_c<U>::type*)
	{
		// 最初だけ16バイトコピー
		__m128i xmm0 = _mm_load_si128(reinterpret_cast<const __m128i * const>(_Src));
		_mm_storeu_si128(reinterpret_cast<__m128i * const>(_Dst), xmm0);
		
		// Nバイトシフト
		xmm0 = _mm_srli_si128(xmm0, N);

		const std::size_t nloop = _Size >> 6;
		for (std::size_t i = 0; i < nloop; i++) {
			// コピーのアドレス
			const char * const s = _Src + (i << 6);
			char * const d = _Dst + (i << 6);

			// フェッチ
			_mm_prefetch(s + PREFETCHSIZE, _MM_HINT_NTA);

			// メモリ読み込み
			__m128i xmm1 = _mm_load_si128(reinterpret_cast<const __m128i * const>(s + 16));
			__m128i xmm3 = _mm_load_si128(reinterpret_cast<const __m128i * const>(s + 32));
			__m128i xmm2 = _mm_load_si128(&xmm1);
			__m128i xmm4 = _mm_load_si128(&xmm3);

			// メモリ合成
			xmm1 = _mm_slli_si128(xmm1, 16 - N);
			xmm2 = _mm_srli_si128(xmm2, N);
			xmm3 = _mm_slli_si128(xmm3, 16 - N);
			xmm4 = _mm_srli_si128(xmm4, N);
			xmm1 = _mm_or_si128(xmm1, xmm0);
			xmm3 = _mm_or_si128(xmm3, xmm2);

			// メモリ書き込み
			_mm_stream_si128(reinterpret_cast<__m128i * const>(d + N), xmm1);
			_mm_stream_si128(reinterpret_cast<__m128i * const>(d + N + 16), xmm3);
			
			// メモリ読み込み
			xmm1 = _mm_load_si128(reinterpret_cast<const __m128i * const>(s + 48));
			xmm3 = _mm_load_si128(reinterpret_cast<const __m128i * const>(s + 64));
			xmm2 = _mm_load_si128(&xmm1);
			xmm0 = _mm_load_si128(&xmm3);
			
			// メモリ合成
			xmm1 = _mm_slli_si128(xmm1, 16 - N);
			xmm2 = _mm_srli_si128(xmm2, N);
			xmm3 = _mm_slli_si128(xmm3, 16 - N);
			xmm0 = _mm_srli_si128(xmm0, N);
			xmm1 = _mm_or_si128(xmm1, xmm4);
			xmm3 = _mm_or_si128(xmm3, xmm2);
			
			// メモリ書き込み
			_mm_stream_si128(reinterpret_cast<__m128i * const>(d + N + 32), xmm1);
			_mm_stream_si128(reinterpret_cast<__m128i * const>(d + N + 48), xmm3);
		}

		const char * const s = _Src + (nloop << 6);
		char * const d = _Dst + (nloop << 6);
		std::memcpy(d, s, _Size & 0x0000003F);
	}

	template <int N, bool U>
	void memcpy(char * _Dst, const char * _Src, std::size_t _Size, typename boost::enable_if_c<U>::type*)
	{
		const int32_t nloop = boost::numeric_cast<const int32_t>(_Size >> 6);
#ifdef _OPENMP 
		#pragma omp parallel for schedule(guided)
#endif
		cilk_for (int32_t i = 0; i < nloop; i++) {
			const char * const s = _Src + (i << 6);
			char * const d = _Dst + (i << 6);

			_mm_prefetch(s + PREFETCHSIZE, _MM_HINT_NTA);

			const __m128i xmm0 = _mm_loadu_si128(reinterpret_cast<const __m128i * const>(s));
			const __m128i xmm1 = _mm_loadu_si128(reinterpret_cast<const __m128i * const>(s + 16));
			const __m128i xmm2 = _mm_loadu_si128(reinterpret_cast<const __m128i * const>(s + 32));
			const __m128i xmm3 = _mm_loadu_si128(reinterpret_cast<const __m128i * const>(s + 48));

			_mm_storeu_si128(reinterpret_cast<__m128i * const>(d), xmm0);
			_mm_storeu_si128(reinterpret_cast<__m128i * const>(d + 16), xmm1);
			_mm_storeu_si128(reinterpret_cast<__m128i * const>(d + 32), xmm2);
			_mm_storeu_si128(reinterpret_cast<__m128i * const>(d + 48), xmm3);
		}

		const char * const s = _Src + (nloop << 6);
		char * const d = _Dst + (nloop << 6);
		std::memcpy(d, s, _Size & 0x0000003F);
	}

#ifdef _DEBUG
	template <typename T>
	void memcheck(const typename smart_pointer<T>::type & ptr,
				  const typename smart_pointer<T>::type & ptr2,
				  std::size_t size)
	{
		for (std::size_t i = 0; i < size; i++) {
			if ((ptr.get())[i] != (ptr2.get())[i]) {
				std::ostringstream oss;
				oss << i << "番目が違います！メモリコピーに失敗しています！\n";
				throw std::runtime_error(oss.str());
			}
		}
	}
#endif
}
