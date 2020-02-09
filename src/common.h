#pragma once

#include <stdint.h>
#include <system_error>

namespace solid {
    using BlockID = uint64_t;
    using INodeID = uint32_t;
    class config {
    public:
        const static uint32_t data_ptr_cnt = 13;
        const static uint32_t inode_size = 256;
        const static uint32_t block_size = 4096;
        inline static uint64_t idiv_block_size(uint64_t x) {
            return x >> 12;
        }
        inline static uint64_t mod_block_size(uint64_t x) {
            return x & 0xfff;
        }
        inline static uint64_t conv_file_handler(INodeID x) {
            return (uint64_t)(x + 2);
        }
        inline static uint64_t rest_file_handler(uint64_t x) {
            return (BlockID)(x - 2);
        }
    };
};