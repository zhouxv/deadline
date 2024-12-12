#include "perf.h"
#include "cryptoTools/Network/IOService.h"
#include "cryptoTools/Common/Timer.h"

#include "volePSI/Defines.h"
#include "volePSI/config.h"
#include "volePSI/SimpleIndex.h"
#include "volePSI/PxUtil.h"
#include "volePSI/Paxos.h"

#include "ourImp/Okvr.h"

#include "libdivide.h"
using namespace oc;
using namespace volePSI;
using namespace deadline;

/**
 * @brief 执行对一组无符号64位整数的模运算。
 *
 * 此函数接受一个命令行解析器对象，检索要处理的元素数量，并初始化一个随机值的向量。
 * 然后使用不同的方法（基线、AVX和libdivide）应用模运算，并检查结果的正确性。
 *
 * @param cmd 一个对oc::CLP对象的引用，包含命令行参数。
 *            参数“n”指定要处理的元素数量，参数“nn”可用于调整“n”的大小。
 *
 * 函数包括以下例程：
 * - `cRoutine`：基线例程，应用模运算。
 * - `libDivRoutine`：使用libdivide库进行模运算的例程。
 *
 * @throw RTE_LOC 如果结果不正确，将抛出异常。
 */
void perfMod(oc::CLP &cmd)
{
	auto n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10));

	std::vector<u64> vals(n);
	PRNG prng_(oc::ZeroBlock);
	u64 mod = prng_.get<u32>();

	auto rand = [&](oc::span<u64> v)
	{
		PRNG prng(oc::ZeroBlock);
		prng.get<u64>(v);
	};
	auto check = [&](std::string name)
	{
		std::vector<u64> v2(n);
		rand(v2);

		for (u64 i = 0; i < n; ++i)
		{
			if (vals[i] != (v2[i] % mod))
			{
				std::cout << name << std::endl;
				throw RTE_LOC;
			}
		}
	};

	auto cRoutine = [&]
	{
		for (u64 i = 0; i < n; ++i)
			vals[i] = vals[i] % mod;
	};

	// auto avxRoutine = [&] {
	//	auto n32 = n / 32;
	//	auto mod256 = ::_mm256_set1_epi64x(mod);

	//	//row256[0] = _mm256_loadu_si256(llPtr);
	//	//row256[1] = _mm256_loadu_si256(llPtr + 1);
	//	//row256[2] = _mm256_loadu_si256(llPtr + 2);
	//	//row256[3] = _mm256_loadu_si256(llPtr + 3);
	//	//row256[4] = _mm256_loadu_si256(llPtr + 4);
	//	//row256[5] = _mm256_loadu_si256(llPtr + 5);
	//	//row256[6] = _mm256_loadu_si256(llPtr + 6);
	//	//row256[7] = _mm256_loadu_si256(llPtr + 7);

	//	for (u64 i = 0; i < n; i += 32)
	//	{
	//		__m256i* row256 = (__m256i*)&vals[i];
	//		mod256x8(row256, mod256);
	//	}
	//};

	auto libDivRoutine = [&]
	{
		PaxosHash<u32> hasher;
		hasher.mMods.emplace_back(libdivide::libdivide_u64_gen(mod));
		hasher.mModVals.emplace_back(mod);
		// libdivide::libdivide_u64_t mod2 = libdivide::libdivide_u64_gen(mod);
		//__m256i temp;
		for (u64 i = 0; i < n; i += 32)
		{
			hasher.mod32(&vals[i], 0);
			//__m256i* row256 = (__m256i*) & vals[i];
			// temp = libdivide::libdivide_u64_do_vec256(*row256, &mod2);
			// auto temp64 = (u64*)&temp;
			// vals[i + 0] -= temp64[0] * mod;
			// vals[i + 1] -= temp64[1] * mod;
			// vals[i + 2] -= temp64[2] * mod;
			// vals[i + 3] -= temp64[3] * mod;
		}
	};
	oc::Timer timer;
	rand(vals);
	timer.setTimePoint("start");
	cRoutine();
	timer.setTimePoint("c");

	check("baseline");
	rand(vals);
	timer.setTimePoint("rand");

	// avxRoutine();
	// timer.setTimePoint("avx");

	// check("avx");
	// rand(vals);
	// timer.setTimePoint("rand");

	libDivRoutine();
	timer.setTimePoint("ibdivide");
	check("libDiv");

	std::cout << timer << std::endl;
}

void perfBaxos(oc::CLP &cmd)
{
	auto n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10));
	auto t = cmd.getOr("t", 1ull);
	// auto rand = cmd.isSet("rand");
	auto v = cmd.getOr("v", cmd.isSet("v") ? 1 : 0);
	auto w = cmd.getOr("w", 3);
	auto ssp = cmd.getOr("ssp", 40);
	auto dt = cmd.isSet("binary") ? PaxosParam::Binary : PaxosParam::GF128;
	auto nt = cmd.getOr("nt", 0);

	// PaxosParam pp(n, w, ssp, dt);
	auto binSize = 1 << cmd.getOr("lbs", 15);
	u64 baxosSize;
	{
		Baxos paxos;
		paxos.init(n, binSize, w, ssp, dt, oc::ZeroBlock);
		baxosSize = paxos.size();
	}
	std::vector<block> key(n), val(n), pax(baxosSize);
	PRNG prng(ZeroBlock);
	prng.get<block>(key);
	prng.get<block>(val);

	Timer timer;
	auto start = timer.setTimePoint("start");
	auto end = start;
	for (u64 i = 0; i < t; ++i)
	{
		Baxos paxos;
		paxos.init(n, binSize, w, ssp, dt, block(i, i));

		// if (v > 1)
		//	paxos.setTimer(timer);

		paxos.solve<block>(key, val, pax, nullptr, nt);
		timer.setTimePoint("s" + std::to_string(i));

		paxos.decode<block>(key, val, pax, nt);

		end = timer.setTimePoint("d" + std::to_string(i));
	}

	if (v)
		std::cout << timer << std::endl;

	auto tt = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / double(1000);
	std::cout << "total " << tt << "ms, e=" << double(baxosSize) / n << std::endl;
}

template <typename T>
void perfBuildRowImpl(oc::CLP &cmd)
{
	auto n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10)); // 获取要处理的元素数量
	u64 maxN = std::numeric_limits<T>::max() - 1;		  // 获取类型T的最大值
	auto t = cmd.getOr("t", 1ull);						  // 获取执行次数
	// auto rand = cmd.isSet("rand");
	auto v = cmd.isSet("v");												// 获取详细输出标志
	auto w = cmd.getOr("w", 3);												// 获取写入数量
	auto ssp = cmd.getOr("ssp", 40);										// 获取状态存储参数
	auto dt = cmd.isSet("binary") ? PaxosParam::Binary : PaxosParam::GF128; // 获取算法类型

	PaxosParam pp(n, w, ssp, dt); // 初始化Paxos参数
	// std::cout << "e=" << pp.size() / double(n) << std::endl;
	if (maxN < pp.size()) // 检查n是否小于索引类型的最大值
	{
		std::cout << "n must be smaller than the index type max value. " LOCATION << std::endl;
		throw RTE_LOC; // 抛出异常
	}
	std::vector<block> key(n); // 创建密钥向量
	PRNG prng(ZeroBlock);	   // 初始化随机数生成器
	prng.get<block>(key);	   // 获取随机密钥

	Timer timer;								// 初始化计时器
	auto start32 = timer.setTimePoint("start"); // 设置开始时间点
	auto end32 = start32;						// 结束时间点初始化
	oc::Matrix<T> rows(32, w);					// 创建行矩阵
	std::vector<block> hash(32);				// 创建哈希向量
	for (u64 i = 0; i < t; ++i)					// 执行t次
	{
		Paxos<T> paxos;					// 创建Paxos对象
		paxos.init(n, pp, block(i, i)); // 初始化Paxos

		auto k = key.data();			   // 获取密钥数据
		auto main = n / 32 * 32;		   // 计算主处理数量
		for (u64 j = 0; j < main; j += 32) // 每32个元素处理一次
		{
			paxos.mHasher.hashBuildRow32(k + j, rows.data(), hash.data()); // 执行哈希构建
		}
		end32 = timer.setTimePoint("32." + std::to_string(i)); // 设置结束时间点
	}

	auto tt32 = std::chrono::duration_cast<std::chrono::microseconds>(end32 - start32).count() / double(1000); // 计算总时间
	std::cout << "total32 " << tt32 << "ms" << std::endl;													   // 输出总时间

	if (cmd.isSet("single")) // 如果设置了单次处理
	{
		auto start1 = timer.setTimePoint("start"); // 设置开始时间点
		auto end1 = start1;						   // 结束时间点初始化
		for (u64 i = 0; i < t; ++i)				   // 执行t次
		{
			Paxos<T> paxos;					// 创建Paxos对象
			paxos.init(n, pp, block(i, i)); // 初始化Paxos

			auto k = key.data();		// 获取密钥数据
			for (u64 j = 0; j < n; ++j) // 遍历所有元素
			{
				paxos.mHasher.hashBuildRow1(k + j, rows.data(), hash.data()); // 执行单个哈希构建
			}
			end1 = timer.setTimePoint("1." + std::to_string(i)); // 设置结束时间点
		}
		auto tt1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1).count() / double(1000); // 计算总时间
		std::cout << "total1  " << tt1 << "ms" << std::endl;													// 输出总时间
	}
	if (v)								 // 如果需要详细输出
		std::cout << timer << std::endl; // 输出计时器信息
}

/**
 * @brief 构建性能行的函数，根据给定的位数选择相应的实现。
 *
 * @param cmd 一个 oc::CLP 类型的命令行参数对象，用于获取位数设置。
 *
 * @throws RTE_LOC 如果位数不是 8、16、32 或 64，将抛出异常并输出错误信息。
 *
 * @details
 * 此函数首先通过命令行参数获取位数设置（默认为 16）。然后根据获取的位数调用相应的模板实现函数：
 * - 8 位：调用 perfBuildRowImpl<u8>(cmd)
 * - 16 位：调用 perfBuildRowImpl<u16>(cmd)
 * - 32 位：调用 perfBuildRowImpl<u32>(cmd)
 * - 64 位：调用 perfBuildRowImpl<u64>(cmd)
 * 如果位数不在上述范围内，将输出错误信息并抛出异常。
 */
void perfBuildRow(oc::CLP &cmd)
{
	auto bits = cmd.getOr("b", 16);
	switch (bits)
	{
	case 8:
		perfBuildRowImpl<u8>(cmd);
		break;
	case 16:
		perfBuildRowImpl<u16>(cmd);
		break;
	case 32:
		perfBuildRowImpl<u32>(cmd);
		break;
	case 64:
		perfBuildRowImpl<u64>(cmd);
		break;
	default:
		std::cout << "b must be 8,16,32 or 64. " LOCATION << std::endl;
		throw RTE_LOC;
	}
}

/**
 * @brief 执行Paxos算法的性能测试实现。
 *
 * @tparam T 数据类型模板参数。
 * @param cmd 命令行参数，包含算法配置选项。
 *
 * 该函数根据提供的命令行参数初始化Paxos算法，并执行指定次数的算法运行。
 * 在每次运行中，算法会根据输入的密钥和数据进行编码和解码，并记录时间。
 * 最后，输出总的执行时间。
 *
 * @note 该函数会抛出异常，如果n的值超过了类型T的最大值。
 */
template <typename T>
void perfPaxosImpl(oc::CLP &cmd)
{
	auto n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10)); // 获取要处理的元素数量, 2^n
	u64 maxN = std::numeric_limits<T>::max() - 1;		  // 获取类型T的最大值
	auto t = cmd.getOr("t", 1ull);						  // 获取 the number of trials
	// auto rand = cmd.isSet("rand");
	auto v = cmd.getOr("v", cmd.isSet("v") ? 1 : 0);						// 获取详细输出标志
	auto w = cmd.getOr("w", 3);												// 获取 The okvs weight
	auto ssp = cmd.getOr("ssp", 40);										// 获取 statistical security parameter
	auto dt = cmd.isSet("binary") ? PaxosParam::Binary : PaxosParam::GF128; // 获取dense type类型 Binary or GF128
	auto cols = cmd.getOr("cols", 0);										// 获取列数

	PaxosParam pp(n, w, ssp, dt); // 初始化Paxos参数
	// std::cout << "e=" << pp.size() / double(n) << std::endl; // 输出每个元素的大小
	if (maxN < pp.size()) // 检查n是否小于索引类型的最大值
	{
		std::cout << "n must be smaller than the index type max value. " LOCATION << std::endl; // 输出错误信息
		throw RTE_LOC;																			// 抛出异常
	}

	auto m = cols ? cols : 1;  // 如果 cols ，则使用，否则默认为1
	std::vector<block> key(n); // 创建密钥向量
	oc::Matrix<block> val(n, m), pax(pp.size(), m);
	PRNG prng(ZeroBlock); // 初始化随机数生成器
	prng.get<block>(key);
	prng.get<block>(val);

	Timer timer;							  // 初始化计时器
	auto start = timer.setTimePoint("start"); // 设置开始时间点
	auto end = start;						  // 结束时间点初始化
	for (u64 i = 0; i < t; ++i)				  // 执行t次
	{
		Paxos<T> paxos;					// 创建Paxos对象
		paxos.init(n, pp, block(i, i)); // 初始化Paxos

		if (v > 1)				   // 如果需要详细输出
			paxos.setTimer(timer); // 设置计时器

		if (cols) // 如果设置了列数
		{
			paxos.setInput(key);						 // 设置输入密钥
			paxos.template encode<block>(val, pax);		 // 执行编码
			timer.setTimePoint("s" + std::to_string(i)); // 设置时间点
			paxos.template decode<block>(key, val, pax); // 执行解码
		}
		else // 如果没有设置列数
		{
			paxos.template solve<block>(key, oc::span<block>(val), oc::span<block>(pax));  // 执行求解
			timer.setTimePoint("s" + std::to_string(i));								   // 设置时间点
			paxos.template decode<block>(key, oc::span<block>(val), oc::span<block>(pax)); // 执行解码
		}

		end = timer.setTimePoint("d" + std::to_string(i)); // 设置结束时间点
	}

	if (v)								 // 如果需要详细输出
		std::cout << timer << std::endl; // 输出计时器信息

	auto tt = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / double(1000); // 计算总时间
	std::cout << "total " << tt << "ms" << std::endl;													 // 输出总时间
}

void perfPaxos(oc::CLP &cmd)
{
	auto bits = cmd.getOr("b", 16);
	switch (bits)
	{
	case 8:
		perfPaxosImpl<u8>(cmd);
		break;
	case 16:
		perfPaxosImpl<u16>(cmd);
		break;
	case 32:
		perfPaxosImpl<u32>(cmd);
		break;
	case 64:
		perfPaxosImpl<u64>(cmd);
		break;
	default:
		std::cout << "b must be 8,16,32 or 64. " LOCATION << std::endl;
		throw RTE_LOC;
	}
}

void perfOkvr(oc::CLP &cmd)
{
	auto n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10)); // 获取要处理的元素数量, 2^n
	OkvrSender<u32> okvrS;
	okvrS.init(n, block(1, 1));
	okvrS.setKeysAndValues();
	okvrS.paxosEncoding();

	OkvrRecv<u32> okvrR;
	okvrR.init(n, block(1, 1));
	okvrR.setKeysAndValues();

	auto serverHash = okvrS.getServerHash();
	okvrR.setServerHashMap(serverHash);

	auto pks = okvrR.getPublicKeys();
	okvrS.setClientKeys(0, pks);

	auto indexes = okvrR.computeIndeies();
	auto queies = okvrR.genQueies(indexes);
	auto responses = okvrS.genResponse(0, queies);

	auto answers = okvrR.answer(responses);

	okvrR.paxosDecoding();
}

void perf(oc::CLP &cmd)
{
	if (cmd.isSet("okvr"))
		perfOkvr(cmd);
	if (cmd.isSet("paxos"))
		perfPaxos(cmd);
	if (cmd.isSet("baxos"))
		perfBaxos(cmd);
}

void overflow(CLP &cmd)
{
	auto statSecParam = 40;
	std::vector<std::vector<u64>> sizes;
	for (u64 numBins = 1; numBins <= (1ull << 32); numBins *= 2)
	{
		sizes.emplace_back();
		try
		{
			for (u64 numBalls = 1; numBalls <= (1ull << 32); numBalls *= 2)
			{
				auto s0 = SimpleIndex::get_bin_size(numBins, numBalls, statSecParam, true);
				sizes.back().push_back(s0);
				std::cout << numBins << " " << numBalls << " " << s0 << std::endl;
			}
		}
		catch (...)
		{
		}
	}

	for (u64 i = 0; i < sizes.size(); ++i)
	{
		std::cout << "/*" << i << "*/ {{ ";
		for (u64 j = 0; j < sizes[i].size(); ++j)
		{
			if (j)
				std::cout << ", ";
			std::cout << std::log2(sizes[i][j]);
		}
		std::cout << " }}," << std::endl;
	}
}