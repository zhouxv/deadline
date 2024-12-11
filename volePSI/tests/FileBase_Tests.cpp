#include "FileBase_Tests.h"

#include "volePSI/fileBased.h"
#include "cryptoTools/Crypto/RandomOracle.h"
using namespace oc;
using namespace volePSI;

template<typename T>
std::vector<u64> setItersect(std::vector<T>& v, std::vector<T>& sub)
{
	std::unordered_set<T> ss(sub.begin(), sub.end());

	std::vector<u64> r;
	for (u64 i = 0; i < v.size(); ++i)
	{
		if (ss.find(v[i]) != ss.end())
			r.push_back(i);
	}

	return r;
}

std::vector<block> writeFile(std::string path, u64 step, u64 size, FileType ft)
{
	std::ofstream o;
	std::vector<block> r; r.reserve(size);
	if (ft == FileType::Bin)
	{
		o.open(path, std::ios::trunc | std::ios::binary);

		if (o.is_open() == false)
			throw RTE_LOC;

		for (u64 i = 0; i < size; ++i)
		{
			auto v = i * step;
			block b(v, v);
			r.push_back(b);
			o.write((char*)&b, 16);
		}
	}
	else if(ft == FileType::Csv)
	{
		o.open(path, std::ios::trunc);

		if (o.is_open() == false)
			throw RTE_LOC;

		for (u64 i = 0; i < size; ++i)
		{
			auto v = i * step;
			block b(v, v);
			r.push_back(b);
			o << b << "\n";
		}
	}
	else
	{
		o.open(path, std::ios::trunc);

		if (o.is_open() == false)
			throw RTE_LOC;

		for (u64 i = 0; i < size; ++i)
		{
			auto v = "prefix_" + std::to_string(i * step) + "\n";

			oc::RandomOracle ro(16);
			ro.Update(v.data(), v.size());
			block b;
			ro.Final(b);
			r.push_back(b);

			o << v;
		}
	}

	return r;
}

bool checkFile(std::string path,std::vector<u64>& exp, FileType ft)
{

	if (ft == FileType::Bin)
	{
		std::ifstream o;
		o.open(path, std::ios::in | std::ios::binary);
		if (o.is_open() == false)
			throw std::runtime_error("failed to open file: " + path);

		auto size = static_cast<size_t>(filesize(o));
		if (size % sizeof(u64))
			throw RTE_LOC;

		auto s = size / sizeof(u64);
		if (s != exp.size())
			return false;

		std::vector<u64> vals(s);

		o.read((char*)vals.data(), size);

		std::unordered_set<u64> ss(vals.begin(), vals.end());

		if (ss.size() != s)
			throw RTE_LOC;

		for (u64 i = 0; i < exp.size(); ++i)
		{
			if (ss.find(exp[i]) == ss.end())
				return false;
		}
	}
	else 
	{
		std::ifstream file(path, std::ios::in);
		if (file.is_open() == false)
			throw std::runtime_error("failed to open file: " + path);

		std::unordered_set<u64> ss;

		while (file.eof() == false)
		{
			u64 i = -1;
			file >> i;

			if (ss.find(i) != ss.end())
				throw RTE_LOC;
			ss.insert(i);
		}

		for (u64 i = 0; i < exp.size(); ++i)
		{
			if (ss.find(exp[i]) == ss.end())
				return false;
		}
	}

	return true;
}

void filebase_readSet_Test()
{
	u64 ns = 34234;
	auto ft = FileType::Bin;
	std::string sFile = "./sFile_deleteMe";
	auto s = writeFile(sFile, 1, ns, ft);

	auto s2 = readSet(sFile, ft, true);

	if (s != s2)
		throw RTE_LOC;
}