#include <vector>

#include "cryptoTools/Common/Timer.h"
#include "cryptoTools/Common/Defines.h"
#include "cryptoTools/Common/block.h"
#include "cryptoTools/Common/Matrix.h"

#include "batchPIR/header/batchpirparams.h"
#include "batchPIR/header/batchpirserver.h"
#include "batchPIR/header/batchpirclient.h"

#include "volePSI/PxUtil.h"
#include "volePSI/Paxos.h"

namespace deadline
{
    using namespace volePSI;
    using namespace oc;

    template <typename IdxType>
    class OkvrSender
    {
    public:
        Paxos<IdxType> mPaxos;
        BatchPIRServer mServer;

        block paxosKey;

        // Paxos Data
        std::vector<block> mKeys;
        std::vector<block> mValues;
        std::vector<block> mEncoding;

        OkvrSender() {};

        void init(u64 numItems, block seed)
        {
            /*
            初始化 Paoxs
            */
            // 默认 w=3,denseType为GF128,统计参数为40
            PaxosParam pp(numItems);

            mKeys.resize(numItems);
            mValues.resize(numItems);
            mEncoding.resize(pp.size());

            // 检查n是否小于索引类型的最大值
            u64 maxN = std::numeric_limits<IdxType>::max() - 1;
            if (maxN < pp.size())
            {
                std::cout << "n must be smaller than the index type max value. " LOCATION << std::endl; // 输出错误信息
                throw RTE_LOC;                                                                          // 抛出异常
            }

            mPaxos.init(numItems, pp, seed);

            /*
            初始化BatchPIR
            */
            u64 batchSize = 128, entrySize = 16;
            // 将输入选择转换为字符串
            string selection = std::to_string(batchSize) + "," + std::to_string(mPaxos.size()) + "," + std::to_string(entrySize);
            // 创建加密参数，并初始化 BatchPirParams
            auto encryption_params = utils::create_encryption_parameters(selection);
            BatchPirParams params(batchSize, mPaxos.size(), entrySize, encryption_params);
            params.print_params();

            mServer = BatchPIRServer(params);
        };

        void setKeysAndValues()
        {
            PRNG prng(ZeroBlock); // 初始化随机数生成器
            prng.get<block>(mKeys);
            prng.get<block>(mValues);
        };

        void paxosEncoding()
        {
            mPaxos.template solve<block>(mKeys, oc::span<block>(mValues), oc::span<block>(mEncoding)); // 执行求解
            mServer.setEntries((uint8_t *)mEncoding.data());
        };

        std::unordered_map<std::string, u64> getServerHash()
        {
            return mServer.get_hash_map();
        };

        void setClientKeys(u32 client_id, std::pair<seal::GaloisKeys, seal::RelinKeys> public_Key)
        {
            mServer.set_client_keys(client_id, public_Key);
        }

        PIRResponseList genResponse(u32 client_id, vector<PIRQuery> queries)
        {
            return mServer.generate_response(client_id, queries);
        };
    };

    template <typename IdxType>
    class OkvrRecv
    {
    public:
        Paxos<IdxType> mPaxos;
        BatchPIRClient mClient;

        block paxosKey;

        std::vector<block> mKeys;
        std::vector<block> mValues;
        std::vector<block> mEncoding;

        OkvrRecv() {};

        void init(u64 numItems, block seed)
        {
            // 默认 w=3,denseType为GF128,统计参数为40
            PaxosParam pp(numItems);
            mKeys.resize(numItems);
            mValues.resize(numItems);
            mEncoding.resize(pp.size());

            // 检查n是否小于索引类型的最大值
            u64 maxN = std::numeric_limits<IdxType>::max() - 1;
            if (maxN < pp.size())
            {
                std::cout << "n must be smaller than the index type max value. " LOCATION << std::endl; // 输出错误信息
                throw RTE_LOC;                                                                          // 抛出异常
            }

            mPaxos.init(numItems, pp, seed);

            /*
            初始化BatchPIR
            */
            u64 batchSize = 128, entrySize = 16;
            // 将输入选择转换为字符串
            string selection = std::to_string(batchSize) + "," + std::to_string(mPaxos.size()) + "," + std::to_string(entrySize);
            // 创建加密参数，并初始化 BatchPirParams
            auto encryption_params = utils::create_encryption_parameters(selection);
            BatchPirParams params(batchSize, mPaxos.size(), entrySize, encryption_params);
            mClient = BatchPIRClient(params);
        };

        void setKeysAndValues()
        {
            PRNG prng(ZeroBlock); // 初始化随机数生成器
            prng.get<block>(mKeys);
            prng.get<block>(mValues);
        };

        void setKeys() {
            //
        };

        void paxosDecoding()
        {
            mPaxos.template decode<block>(mKeys, oc::span<block>(mValues), oc::span<block>(mEncoding)); // 执行解码
        };

        void setServerHashMap(std::unordered_map<std::string, u64> map)
        {
            mClient.set_map(map);
        };

        std::pair<seal::GaloisKeys, seal::RelinKeys> getPublicKeys()
        {
            return mClient.get_public_keys();
        }

        vector<uint64_t> computeIndeies()
        {
            auto paxosSize = mPaxos.size();
            vector<IdxType> results(paxosSize, 0);

            oc::Matrix<IdxType> rows(32, mPaxos.mWeight);
            vector<block> dense(32);

            auto inIter = mKeys.data();
            auto main = mKeys.size() / 32 * 32;

            for (u64 i = 0; i < main; i += 32, inIter += 32)
            {
                mPaxos.mHasher.hashBuildRow32(inIter, rows.data(), dense.data());
                for (u64 j = 0; j < 32; i++)
                {
                    results[rows(j, 0) - 1] = 1;
                    results[rows(j, 1) - 1] = 1;
                    results[rows(j, 2) - 1] = 1;
                }
            }

            for (u64 i = main; i < mKeys.size(); ++i, ++inIter)
            {
                mPaxos.mHasher.hashBuildRow1(inIter, rows.data(), dense.data());
                results[rows(i, 0) - 1] = 1;
                results[rows(i, 1) - 1] = 1;
                results[rows(i, 2) - 1] = 1;
            }

            vector<uint64_t> result2;

            for (u64 i = 0; i < paxosSize; i++)
            {
                if (results[i] == 1)
                {
                    result2.push_back(i);
                }
            }
        }

        vector<PIRQuery> genQueies(vector<uint64_t> indeies)
        {
            return mClient.create_queries(indeies);
        };

        vector<RawDB> answer(PIRResponseList list)
        {
            return mClient.decode_responses_chunks(list);
        }
    };
}