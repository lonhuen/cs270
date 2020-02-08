#pragma once

#include <string>
#include <vector>
#include "common.h"
#include "utils/log_utils.h"
#include "utils/fs_exception.h"
#include "inode/inode_manager.h"
#include "block/block_manager.h"
#include "directory/directory.h"

namespace solid {
    //TODO(lonhh) when should we update the inode?
    class FileSystem {
    
    public:
        INodeManager* im;
        BlockManager* bm;
        Storage* storage;

    public:
        // just used for DEBUG
        FileSystem() {};
        FileSystem(BlockID nr_blocks,BlockID nr_iblock_blocks);
        void mkfs();

        int read(INodeID id,uint8_t* dst,uint32_t size,uint32_t offset);
        int write(INodeID id,const uint8_t* src,uint32_t size,uint32_t offset);
        int truncate(INodeID id, uint32_t size);
        int unlink(INodeID id);

        INodeID path2iid(const std::string& path);
        Directory read_directory(INodeID id);
        void write_directory(INodeID id,Directory& dr);

        std::vector<std::string> parse_path(const std::string& path);

    //private:
        // allocate a new datablock for inode, but shall we see the index of datablocks? yes we can
        // most of the time we should write the file immediately after allocating a new block for it
        BlockID new_dblock(INode& inode);

        // we don't modify the file size
        int delete_dblock(INode& inode);
        // notice we only allocate a new inode, but we need to write it/init it
        INodeID new_inode(const std::string& file_name,INode& inode);

        Directory read_directory(INode& inode);
        void write_directory(INode& inode,Directory& dr);

        std::vector<BlockID> read_dblock_index(INode& inode,uint32_t begin,uint32_t end);
        uint32_t block_lookup_per_region(INode& inode,uint32_t begin,uint32_t end,std::vector<BlockID>& vec,int depth);

        std::string simplifyPath(std::string path);
        std::string directory_name(std::string path);
        std::string file_name(std::string path);
    };
};