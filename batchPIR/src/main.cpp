#include <iostream>
#include <cstdlib>
#include <cassert>
#include <chrono>
#include <functional>
#include "server.h"
#include "pirparams.h"
#include "client.h"
#include "batchpirparams.h"
#include "batchpirserver.h"
#include "batchpirclient.h"

using namespace std;
using namespace chrono;

void print_usage()
{
    std::cout << "Usage: vectorized_batch_pir -n <db_entries> -s <entry_size>\n";
}

// 校验命令行参数，确保输入的数据库条目数和条目大小有效
bool validate_arguments(int argc, char *argv[], size_t &db_entries, size_t &entry_size)
{
    // 如果命令行参数为 -h，打印帮助信息
    if (argc == 2 && string(argv[1]) == "-h")
    {
        print_usage();
        return false;
    }
    // 检查是否输入了有效的参数
    if (argc != 5 || string(argv[1]) != "-n" || string(argv[3]) != "-s")
    {
        std::cerr << "Error: Invalid arguments.\n";
        print_usage();
        return false;
    }
    // 将输入的字符串转换为数字
    db_entries = stoull(argv[2]);
    entry_size = stoull(argv[4]);
    return true;
}

// 启动向量化的 PIR 处理
int vectorized_pir_main(int argc, char *argv[])
{
    size_t db_entries = 0;
    size_t entry_size = 0;
    const int client_id = 0;

    // 校验命令行参数
    if (!validate_arguments(argc, argv, db_entries, entry_size))
    {
        // 如果参数无效，则返回错误码
        return 1;
    }

    uint64_t num_databases = 128;  // 假设数据库数量为128
    uint64_t first_dim = 64;       // 假设数据的第一维大小为64

    // 创建加密参数
    auto encryption_params = utils::create_encryption_parameters();

    // 根据数据库条目数、条目大小和第一维大小创建 PirParams 对象
    PirParams params(db_entries, entry_size, num_databases, encryption_params, first_dim);
    params.print_values();  // 打印参数

    // 创建服务器和客户端对象
    Server server(params);
    Client client(params);

    // 加载原始数据库到服务器
    server.load_raw_dbs();

    // 将原始数据库转换为 PIR 数据库
    server.convert_merge_pir_dbs();

    // 对数据库进行 NTT 预处理
    server.ntt_preprocess_db();

    // 设置客户端的密钥
    server.set_client_keys(client_id, client.get_public_keys());

    // 生成条目索引
    vector<uint64_t> entry_indices;
    for (int i = 0; i < num_databases; i++)
    {
        entry_indices.push_back(0);  // 这里默认使用 0 索引，实际可以根据需求生成随机值
    }

    // 客户端生成查询
    auto query = client.gen_query(entry_indices);

    // 记录响应生成的时间
    auto start_time = std::chrono::high_resolution_clock::now();
    PIRResponseList response = server.generate_response(client_id, query);
    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // 输出生成响应的耗时
    std::cout << "generate_response time: " << elapsed_time.count() << " ms" << std::endl;

    // 客户端解码响应
    auto entries = client.single_pir_decode_responses(response);

    // 服务器检查解码后的条目是否匹配
    server.check_decoded_entries(entries, entry_indices);

    std::cout << "Main: decoded entries matched" << std::endl;
    return 0;
}

// 哈希测试函数，验证批量 PIR 的哈希过程
int hashing_test_main(int argc, char *argv[])
{

    // 检查命令行参数是否正确
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " batch_size num_entries entry_size" << std::endl;
        return 1;
    }

    // 解析批次大小、条目数和条目大小
    int batch_size = std::stoi(argv[1]);
    size_t num_entries = std::stoull(argv[2]);
    size_t entry_size = std::stoull(argv[3]);

    // 创建加密参数
    auto encryption_params = utils::create_encryption_parameters();

    // 创建 BatchPirParams 和客户端对象
    BatchPirParams params(batch_size, num_entries, entry_size, encryption_params);
    BatchPIRClient client(params);

    vector<uint64_t> myvec(batch_size);

    int trials = std::pow(2, 30);  // 设定试验次数为2的30次方
    for (int j = 0; j < trials; j++)
    {
        std::cout << "Trial " << j << "/" << trials << ": ";
        for (int i = 0; i < batch_size; i++)
        {
            myvec[i] = rand() % num_entries;  // 随机生成条目索引
        }

        // 进行哈希测试
        if (client.cuckoo_hash_witout_checks(myvec))
        {
            std::cout << "success" << std::endl;
        }
        else
        {
            std::cout << "failure" << std::endl;
            throw std::invalid_argument("Attempt failed");
        }
    }
    return 0;
}

int batchpir_main(int argc, char* argv[])
{
    const int client_id = 0;
    // 定义输入选择，包含批量大小、条目数量和条目大小
    std::vector<std::array<size_t, 3>> input_choices;
    input_choices.push_back({32, 1048576, 32});
    input_choices.push_back({64, 1048576, 32});
    input_choices.push_back({256, 1048576, 32});
    
    // 用于记录不同阶段的时间
    std::vector<std::chrono::milliseconds> init_times;
    std::vector<std::chrono::milliseconds> query_gen_times;
    std::vector<std::chrono::milliseconds> resp_gen_times;
    std::vector<size_t> communication_list;

    // 遍历每种输入选择
    for (size_t iteration = 0; iteration < input_choices.size(); ++iteration)
    {
        std::cout << "***************************************************" << std::endl;
        std::cout << "             Starting example " << (iteration + 1) << "               " << std::endl;
        std::cout << "***************************************************" << std::endl;

        const auto& choice = input_choices[iteration];

        // 将输入选择转换为字符串
        string selection = std::to_string(choice[0]) + "," + std::to_string(choice[1]) + "," + std::to_string(choice[2]);

        // 创建加密参数，并初始化 BatchPirParams
        auto encryption_params = utils::create_encryption_parameters(selection);
        BatchPirParams params(choice[0], choice[1], choice[2], encryption_params);
        params.print_params();

        // 记录初始化时间
        auto start = chrono::high_resolution_clock::now();
        BatchPIRServer batch_server(params);
        auto end = chrono::high_resolution_clock::now();
        auto duration_init = chrono::duration_cast<chrono::milliseconds>(end - start);
        init_times.push_back(duration_init);

        BatchPIRClient batch_client(params);

        // 获取服务器的哈希映射并设置给客户端
        auto map = batch_server.get_hash_map();
        batch_client.set_map(map);

        // 设置客户端密钥
        batch_server.set_client_keys(client_id, batch_client.get_public_keys());

        // 随机生成条目索引
        vector<uint64_t> entry_indices;
        for (int i = 0; i < choice[0]; i++)
        {
            entry_indices.push_back(rand() % choice[2]);
        }

        // 生成查询并记录时间
        cout << "Main: Starting query generation for example " << (iteration + 1) << "..." << endl;
        start = chrono::high_resolution_clock::now();
        auto queries = batch_client.create_queries(entry_indices);
        end = chrono::high_resolution_clock::now();
        auto duration_querygen = chrono::duration_cast<chrono::milliseconds>(end - start);
        query_gen_times.push_back(duration_querygen);
        cout << "Main: Query generation complete for example " << (iteration + 1) << "." << endl;

        // 生成响应并记录时间
        cout << "Main: Starting response generation for example " << (iteration + 1) << "..." << endl;
        start = chrono::high_resolution_clock::now();
        PIRResponseList responses = batch_server.generate_response(client_id, queries);
        end = chrono::high_resolution_clock::now();
        auto duration_respgen = chrono::duration_cast<chrono::milliseconds>(end - start);
        resp_gen_times.push_back(duration_respgen);
        cout << "Main: Response generation complete for example " << (iteration + 1) << "." << std::endl;

        // 检查解码后的条目是否匹配
        cout << "Main: Checking decoded entries for example " << (iteration + 1) << "..." << endl;
        auto decode_responses = batch_client.decode_responses_chunks(responses);

        // 记录通信数据大小
        communication_list.push_back(batch_client.get_serialized_commm_size());

        // 获取并检查 Cuckoo 哈希表
        auto cuckoo_table = batch_client.get_cuckoo_table();
        if (batch_server.check_decoded_entries(decode_responses, cuckoo_table))
        {
            cout << "Main: All the entries matched for example " << (iteration + 1) << "!!" << std::endl;
        }

        cout << std::endl;
    }

    // 输出各个阶段的性能报告
    cout << "***********************" << std::endl;
    cout << "     Timings Report    " << std::endl;
    cout << "***********************" << std::endl;
    for (size_t i = 0; i < input_choices.size(); ++i)
    {
        cout << "Input Parameters: ";
        cout << "Batch Size: " << input_choices[i][0] << ", ";
        cout << "Number of Entries: " << input_choices[i][1] << ", ";
        cout << "Entry Size: " << input_choices[i][2] << std::endl;

        cout << "Initialization time: " << init_times[i].count() << " milliseconds" << std::endl;
        cout << "Query generation time: " << query_gen_times[i].count() << " milliseconds" << std::endl;
        cout << "Response generation time: " << resp_gen_times[i].count() << " milliseconds" << std::endl;
        cout << "Total communication: " << communication_list[i] << " KB" << std::endl;
        cout << std::endl;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    // 调用 batchpir_main 函数
    batchpir_main(argc, argv);
    return 0;
}
