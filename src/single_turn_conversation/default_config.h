#ifndef SINGLE_TURN_CONVERSATION_DEFAULT_CONFIG_H
#define SINGLE_TURN_CONVERSATION_DEFAULT_CONFIG_H

#include <iostream>
#include <string>

struct DefaultConfig {
    std::string pair_file;
    std::string post_file;
    std::string response_file;
    bool check_grad;
    bool one_response;
    bool learn_test;
    bool only_decode;
    int max_sample_count;
    int dev_size;
    int test_size;
    int device_id;
    std::string output_model_file_prefix;
    std::string input_model_file;

    void print() const {
        std::cout << "pair_file:" << pair_file << std::endl
            << "post_file:" << post_file << std::endl
            << "response_file:" << response_file << std::endl
            << "check_grad:" << check_grad << std::endl
            << "one_response:" << one_response << std::endl
            << "learn_test:" << learn_test << std::endl
            << "only_decode:" << only_decode << std::endl
            << "max_sample_count:" << max_sample_count << std::endl
            << "dev_size:" << dev_size << std::endl
            << "test_size:" << test_size << std::endl
            << "device_id:" << device_id << std::endl
            << "output_model_file_prefix" << output_model_file_prefix << std::endl
            << "input_model_file:" << input_model_file << std::endl;
    }
};

DefaultConfig &GetDefaultConfig() {
    static DefaultConfig default_config;
    return default_config;
}

#endif
