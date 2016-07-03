#ifndef _MYRANDOM_H_
#define _MYRANDOM_H_

#ifdef _MSC_VER
	#pragma warning(disable: 4819)
#endif

#include <vector>
#include <boost/cast.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/numeric/interval.hpp>

#if !defined(__INTEL_COMPILER) && !defined(__GXX_EXPERIMENTAL_CXX0X__)
	#include <boost/noncopyable.hpp>
#endif

#if (_MSC_VER >= 1600) || defined(__GXX_EXPERIMENTAL_CXX0X__)
	#include <memory>
	#include <random>
	#include <cstdint>
#else
	#include <boost/random.hpp>
	#include <boost/cstdint.hpp>
	#include <boost/checked_delete.hpp>
	#include <boost/interprocess/smart_ptr/unique_ptr.hpp>
#endif

#if (_MSC_VER < 1600) || defined(__GXX_EXPERIMENTAL_CXX0X__)
	#include <ctime>
#endif

namespace MyRandom {
#if (_MSC_VER >= 1600) || defined(__GXX_EXPERIMENTAL_CXX0X__)
	using std::uint32_t;
	using std::uint_least32_t;
	using std::mt19937;
	using std::unique_ptr;
#else
	using boost::uint32_t;
	using boost::uint_least32_t;
	using boost::mt19937;
	using boost::shared_ptr;
	using boost::interprocess::unique_ptr;
#endif

	template <typename T>
	struct unique_pointer {
#if (_MSC_VER >= 1600)
		typedef std::unique_ptr<T> type;
#else
		typedef boost::interprocess::unique_ptr<T, boost::checked_deleter<T> > type;
#endif
	};

	class MyRand
#if !defined(__INTEL_COMPILER) && !defined(__GXX_EXPERIMENTAL_CXX0X__)
		: private boost::noncopyable
#endif
    {
#if defined(__INTEL_COMPILER) || defined(__GXX_EXPERIMENTAL_CXX0X__)
		MyRand(const MyRand &) = delete;
		MyRand & operator=(const MyRand &) = delete;
		MyRand() = delete;
#endif

#if (_MSC_VER < 1600) || defined(__GXX_EXPERIMENTAL_CXX0X__)
		const unique_pointer<mt19937>::type prandEngine;
#else
		unique_pointer<mt19937>::type prandEngine;
#endif

#if (_MSC_VER >= 1600)
		static const std::vector<uint_least32_t>::size_type SIZE = 64;
		const std::uniform_int_distribution<int> distribution;
#elif defined(__GXX_EXPERIMENTAL_CXX0X__)
		std::uniform_int_distribution<int> distribution;
#else
		boost::variate_generator<mt19937, boost::uniform_int<> > rand;
#endif
	public:
		explicit MyRand(const boost::numeric::interval<int> & range);
#if (_MSC_VER >= 1600) || defined(__GXX_EXPERIMENTAL_CXX0X__)
		int myrand()
		{ return distribution(*prandEngine); }
#else
		int myrand()
		{ return rand(); }
#endif
	};
}

namespace MyRandom {
#if (_MSC_VER >= 1600)
	inline MyRand::MyRand(const boost::numeric::interval<int> & range)
	 :	distribution(range.lower(), range.upper())
	{
		// ランダムデバイス
		std::random_device rnd;

		// 初期化用ベクタ
		std::vector<uint_least32_t> v(SIZE);

		// ベクタの初期化
		boost::generate(v, std::ref(rnd));

		const std::seed_seq seq = std::seed_seq(v.begin(), v.end());
		// 乱数エンジン
		prandEngine.reset(new mt19937(seq));
	}
#elif defined(__GXX_EXPERIMENTAL_CXX0X__)
	inline MyRand::MyRand(const boost::numeric::interval<int> & range)
	 :  prandEngine(new mt19937(boost::numeric_cast<uint32_t>(std::time(NULL)))),
		distribution(range.lower(), range.upper())
	{
	}
#else
	inline MyRand::MyRand(const boost::numeric::interval<int> & range)
	 :	prandEngine(new mt19937(boost::numeric_cast<uint32_t>(std::time(NULL)))),
		rand(*prandEngine, boost::uniform_int<>(range.lower(), range.upper()))
	{
	}
#endif
}

#endif	// _MYRANDOM_H_
