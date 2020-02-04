#include <iostream>
#include <algorithm>
#include <gtest/gtest.h>
#include "utils/log_utils.h"
#include "storage/memory_storage.h"
#include "block/freelist_blockmanager.h"
#include "fs/file_system.h"
#include "inode/inode.h"
#include "block/block.h"

class FileSystemTest : public testing::Test {

protected:
  static FileSystem* fs;

};

FileSystem* FileSystemTest::fs = new FileSystem(1300,9);

TEST_F(FileSystemTest,InitTest) {
  super_block sb;
  fs->storage->read_block(0,sb.data);
  EXPECT_EQ(sb.nr_block, 1300);
  EXPECT_EQ(sb.nr_dblock, 1290);
  EXPECT_EQ(sb.s_dblock, 10);
  EXPECT_EQ(sb.s_iblock, 1);
  EXPECT_EQ(sb.nr_iblock, 9);
}

TEST_F(FileSystemTest,ReadBlockIndexTest) {
    // first let's write to the root inode
    fs->mkfs();
    INode inode;
    fs->im->read_inode(0,inode.data);
    EXPECT_EQ(inode.itype, inode_type::DIRECTORY);

    // let's write block entries
    for(int i=0;i<10;i++)
        inode.p_block[i] = i;
    
    // let's set the 1-level indirect lookup
    inode.p_block[10] = fs->bm->allocate_dblock();
    EXPECT_EQ(inode.p_block[10], 11);
    Block bl;
    
    for(int i=0;i<512;i++)
        bl.bl_entry[i] = i+10;
    fs->bm->write_dblock(11,bl.data);

    // let's set the 2-level indirect lookup
    inode.p_block[11] = fs->bm->allocate_dblock();
    EXPECT_EQ(inode.p_block[11], 12);

    for(int i=0;i<512;i++) {
        bid_t bid = fs->bm->allocate_dblock();
        bl.bl_entry[i] = bid;
        Block b;
    
        for(int j=0;j<512;j++) {
            b.bl_entry[j] = 10 + 512 + i * 512 + j;
        }
        fs->bm->write_dblock(bid,b.data);
    }
    fs->bm->write_dblock(inode.p_block[11],bl.data);

    // TODO(lonhh): this might need to verifed 
    // let's set the 3-level indirect lookup
    // but with only 1-1-1
    inode.p_block[12] = fs->bm->allocate_dblock();
    
    bid_t bid = fs->bm->allocate_dblock();
    bl.bl_entry[0] = bid;

    Block b2;
    bid_t bid2 = fs->bm->allocate_dblock();
    b2.bl_entry[0] = bid2;
    
    Block b3;
    for(int i=0;i<512;i++){
        b3.bl_entry[i] = 10 + 512 + 512 * 512 + i;
    }
    fs->bm->write_dblock(bid2,b3.data);
    fs->bm->write_dblock(bid,b2.data);
    fs->bm->write_dblock(inode.p_block[12],bl.data);

    fs->im->write_inode(0,inode.data);

    std::vector<bid_t> v = fs->read_dblock_index(inode,0,10+512+512*512+512);
    /* this test costs much time
    for(auto i=0;i<10+512+512*512+512;i++) {
        EXPECT_EQ(v[i],i);
    }
    */

    for(auto i=0;i<10+512+512*2;i++) {
        EXPECT_EQ(v[i],i);
    }
    
    v = fs->read_dblock_index(inode,1,10+512);
    for(auto i=1;i<10+512;i++) {
        EXPECT_EQ(v[i-1],i);
    }
    
    v = fs->read_dblock_index(inode,20,10+512);
    for(auto i=20;i<10+512;i++) {
        EXPECT_EQ(v[i-20],i);
    }
}
TEST_F(FileSystemTest,ReadTest) {
    // let's write the first 20 blocks
    fs->mkfs();
    INode inode;
    fs->im->read_inode(0,inode.data);
    EXPECT_EQ(inode.itype, inode_type::DIRECTORY);

    // let's write block entries
    for(int i=0;i<10;i++)
        inode.p_block[i] = fs->bm->allocate_dblock();
    
    // let's set the 1-level indirect lookup
    inode.p_block[10] = fs->bm->allocate_dblock();
    Block bl;
    
    for(int i=0;i<10;i++)
        bl.bl_entry[i] = fs->bm->allocate_dblock();
    fs->bm->write_dblock(inode.p_block[10],bl.data);

    inode.size = 20 * BLOCK_SIZE;
    fs->im->write_inode(0,inode.data);

    std::vector<bid_t> v = fs->read_dblock_index(inode,0,20);
    /*
    std::for_each(v.begin(),v.end(),[](auto p){
        std::cout << p << std::endl;
        Block tmp;
        // suppose we are writing in units of uint64_t
        for(uint32_t i=0;i<512;i++) {
            tmp.bl_entry[i] = p * 512 + i;
        }
        fs->bm->write_dblock(p,tmp.data);
    });
    */
   for(auto i=0;i<20;i++) {
        Block tmp;
        // suppose we are writing in units of uint64_t
        for(uint32_t j=0;j<512;j++) {
            tmp.bl_entry[j] = i * 512 + j;
        }
        fs->bm->write_dblock(v[i],tmp.data);
   }
    uint8_t buffer[20 * BLOCK_SIZE];

    // full range
    fs->read(0,buffer,20*BLOCK_SIZE,0);
    for(auto i=0;i<20;i++) {
        for(auto j=0;j<512;j++) {
            auto ptr = ((bid_t*)buffer) + i * 512 + j;
            EXPECT_EQ(*ptr, i * 512 + j);
        }
    }
    
    // test offset assign to block
    fs->read(0,buffer,20*BLOCK_SIZE,10 * BLOCK_SIZE);
    for(auto i=0;i<10;i++) {
        for(auto j=0;j<512;j++) {
            auto ptr = ((bid_t*)buffer) + i * 512 + j;
            EXPECT_EQ(*ptr, (i+10) * 512 + j);
        }
    }
    // test offset inside a block
    fs->read(0,buffer,20*BLOCK_SIZE,10 * BLOCK_SIZE + BLOCK_SIZE/2);
    for(auto j=0;j<256;j++) {
        auto ptr = ((bid_t*)buffer) + j;
        EXPECT_EQ(*ptr, 10*512 + j + 256);
    }
    for(auto i=1;i<10;i++) {
        for(auto j=0;j<512;j++) {
            auto ptr = ((bid_t*)buffer) + i * 512 + j - 256;
            EXPECT_EQ(*ptr, (i+10) * 512 + j);
        }
    }
}

/*
TEST_F(FileSystemTest,path2iid) {
/*
TEST_F(FileSystemTest,path2iid) {
    
  // FileSystem fs;
  // fs.path2iid("/home/lonhuen/////../github/");
  // EXPECT_EQ(fs.path2iid("/"),0);

  const std::string path = "/";
  std::string::size_type p1,p2;
  p2 = path.find('/');
  p1 = 0;
  while(std::string::npos != p2) {
    if((p1 != p2)) {
      std::cout << path.substr(p1,p2-p1) << std::endl;
    }
    p1 = p2 + 1;
    p2 = path.find('/',p1);
  }
}

TEST_F(FileSystemTest, Path2iidTest {
    // TODO
    int iid = fs.path2iid("some path");
    INODE dst;
    im->read_inode(id, inode.data);
    EXPECT_EQ(im.inode_number, id);
}

TEST_F(FileSystemTest, ReadPathTest) {
  // create a test file
  ofstream testfile;
  testfile.open("test_file.txt");
  testfile << "This is a test file.";
  testfile.close();

  // read 2 bytes
  char buf1[3];
  int res;
  res = fs.read("test_file.txt", buf1, 2, 0);
  EXPECT_EQ(res, 2);
  EXPECT_EQ(buf1, "Th");

  // read 5 bytes from offset
  char buf2[6];
  res = fs.read("test_file.txt", buf2, 5, 3);
  EXPECT_EQ(res, 5);
  EXPECT_EQ(buf2, "s is ");

  // read out of range
  char buf3[21];
  res = fs.read("test_file.txt", buf3, 40, 0);
  EXPECT_EQ(res, 20);
  EXPECT_EQ(buf3, "This is a test file.");

  std::remove("test_file.txt");
}
*/