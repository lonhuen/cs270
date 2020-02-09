#define FUSE_USE_VERSION 35
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include <algorithm>
#include <functional>

#include "fs/file_system.h"
#include "inode/inode.h"
#include "directory/directory.h"
#include "utils/fs_exception.h"
#include "utils/string_utils.h"


using namespace solid;
FileSystem *fs;


bool existException(std::function<void(void)> f) {
    try {
        f();
    } catch (const fs_exception& e) {
        return true;
    }
    return false;
};


extern "C" {

    //void*s_init(struct fuse_conn_info *conn, struct fuse_config *cfg){
    //    return NULL;
    //}

    int s_getattr(const char* path, struct stat* st, struct fuse_file_info *fi) {
        LOG(INFO) << "getattr " << path;

        uint32_t id;
        if (existException([&](){id = fs->path2iid(path);})) {
            LOG(ERROR) << "file not exists" << path;
            return -ENOENT;
        }

        LOG(INFO) << "getinode " << id;
		INode inode = fs->im->read_inode(id);
        LOG(INFO) << String::of(inode);

		st->st_ino     = id;
		st->st_mode    = inode.mode;
		st->st_nlink   = inode.links;
		st->st_uid     = inode.uid;
		st->st_gid     = inode.gid;
		st->st_size    = inode.size;
		st->st_blocks  = inode.block;
		st->st_atime   = inode.atime;
		st->st_ctime   = inode.ctime;
		st->st_mtime   = inode.mtime;
		st->st_blksize = config::block_size;
		// ingore this
		//fi->st_dev     = inode.dev;
		if (inode.itype != INodeType::DIRECTORY) {
			st->st_mode = st->st_mode | S_IFREG;
		} else if (inode.itype == INodeType::DIRECTORY) {
			st->st_mode = st->st_mode | S_IFDIR;
		} else {
			LOG(ERROR) << "Unkown file type for " << path;
        }
        LOG(INFO) << "getattr return 0 " << path;
		return 0;
	}

    int s_open(const char* path, struct fuse_file_info* fi) {
        LOG(INFO) << "open " << path;
        
        INodeID id;
        if (existException([&](){
            id = fs->path2iid(path);
        })) {
            LOG(ERROR) << "file not exists" << path;
            return -ENOENT;
        }
        fi->fh = id;    // cache inode id
        return 0;
    }

    int s_read(const char *path, char *buf, size_t size, off_t offset,
                struct fuse_file_info *fi) {
        LOG(INFO) << "read " << path;
        
        int res = s_open(path, fi);
        if (res != 0) {
            return res;
        }

        INodeID id = fi->fh;
        return fs->read(id, (uint8_t *)buf, (uint32_t)size,(uint32_t)offset);
    }

    int s_write(const char *path, const char* buf, size_t size, off_t offset,
                struct fuse_file_info *fi) {
        LOG(INFO) << "write " << path;
        int res = s_open(path, fi);
        if (res != 0) {
            return res;
        }

        INodeID id = fi->fh;
        return fs->write(id, (const uint8_t *)buf,
                         (uint32_t) size, (uint32_t) offset);
    }

    int s_truncate(const char *path, off_t offset, struct fuse_file_info *fi) {
        LOG(INFO) << "truncate " << path;
        
        int res = s_open(path, fi);
        if (res != 0) {
            return res;
        }

        INodeID id = fi->fh;
        return fs->truncate(id, (uint32_t) offset)-1; // return 0 if success   
    }

    int s_unlink(const char *path) {
        LOG(INFO) << "unlink " << path;

        std::string p(path);
        std::string dir_name = fs->directory_name(p);
        std::string f_name = fs->file_name(p);

        INodeID dir_id;
        if (existException([&](){
            dir_id = fs->path2iid(dir_name);
        })) {
            LOG(ERROR) << "directory not exists" << dir_name;
            return -ENOENT;
        }

        INodeID f_id;
        Directory dr = fs->read_directory(dir_id);
        if (existException([&](){
            f_id = dr.get_entry(f_name);
        })) {
            LOG(ERROR) << "file not exists" << f_name;
            return -ENOENT;
        }
        dr.remove_entry(f_name);
        fs->write_directory(dir_id,dr);
        return fs->unlink(f_id);
    }
    
    int s_readdir(const char * path, void *buf, fuse_fill_dir_t filler,
                  off_t offset, struct fuse_file_info *fi, 
                  enum fuse_readdir_flags flags) {
        LOG(INFO) << "readdir " << path;
        int res = s_open(path, fi);
        if (res == -1) {
            LOG(ERROR) << "Cannot read file " << path;
            return -1;
        }

        INodeID id = fi->fh;
        
        Directory dir = fs->read_directory(id);
        for (auto entry : dir.entry_m) {
            //struct stat st;
            //memset(&st, 0, sizeof(st));
            //res = filler(buf, entry.first.c_str(), &st, 0, fuse_fill_dir_flags::FUSE_FILL_DIR_PLUS);
            res = filler(buf, entry.first.c_str(), NULL, 0, fuse_fill_dir_flags::FUSE_FILL_DIR_PLUS);
            if (res != 0) {
                return res;
            }
        }
        return 0;
    }

    int s_mknod(const char *path, mode_t mode, dev_t dev) {
        LOG(INFO) << "mknod " << path;
        //if (!S_INSREG(mode)) {
        //    return -ENOTSUP;
        //}
        // parse path
        std::string p(path);
        std::string dir_name = fs->directory_name(p);
        std::string f_name = fs->file_name(p);

        // get dir
        INodeID dir_id;
        if (existException([&](){
            dir_id = fs->path2iid(dir_name);
        })) {
            LOG(ERROR) << "directory not exists" << dir_name;
            return -ENOENT;
        }

        INode dir_inode = fs->im->read_inode(dir_id);

        // allocate a new inode for this file
        INodeID f_id = fs->new_inode(f_name,dir_inode);
        if (f_id == 0) {
            LOG(ERROR) << "Cannot allocate a new inode " << path;
            return -1;
        } 
        // update inode metadata
        INode inode = INode::get_inode(f_id,INodeType::REGULAR);
        inode.mode = mode;
        // dev??
        //inode.dev
        fs->im->write_inode(f_id,inode);
        return 0;
    }

    int s_utimens(const char *path, const struct timespec ts[2],
		       struct fuse_file_info *fi) {
        LOG(INFO) << "utime " << path;
        
        INodeID id;
        if (existException([&](){
            id = fs->path2iid(path);
        })) {
            LOG(ERROR) << "file not exists" << path;
            return -ENOENT;
        }
        
        INode inode = fs->im->read_inode(id);
        inode.atime = ts[0].tv_nsec;
        inode.ctime = ts[1].tv_nsec;
        fs->im->write_inode(id,inode);
        return 0;
    }

    int s_mkdir(const char* path, mode_t mode) {
        LOG(INFO) << "mkdir " << path;
        std::string p(path);
        std::string dir_name = fs->directory_name(p);
        std::string f_name = fs->file_name(p);

        INodeID dir_id;
        if (existException([&](){
            dir_id = fs->path2iid(dir_name);
        })) {
            LOG(ERROR) << "directory not exists" << dir_name;
            return -ENOENT;
        }
        
        INode dir_inode = fs->im->read_inode(dir_id);

        // allocate a new inode for this dir 
        INodeID f_id = fs->new_inode(f_name,dir_inode);
        if (f_id == 0) {
            LOG(ERROR) << "Cannot allocate a new inode " << path;
            return -1;
        } 
        // update inode metadata
        INode inode = INode::get_inode(f_id,INodeType::DIRECTORY);
        inode.mode = mode;
        fs->im->write_inode(f_id,inode);
        //update directory
        Directory dr(f_id,dir_id);
        fs->write_directory(f_id,dr);
        // dev??
        //inode.dev
        return 0;
    }

    int s_rmdir(const char *path) {
        LOG(INFO) << "rmdir " << path;
        INodeID id;
        if (existException([&](){
            id = fs->path2iid(path);
        })) {
            LOG(ERROR) << "directory not exists" << path;
            return -ENOENT;
        }
        // judge whether it's empty or not
        Directory dir = fs->read_directory(id);
        if(dir.entry_m.size() > 2) {
            LOG(ERROR) << "Cannot rmdir a non-empty directory " << path;
            return -ENOTEMPTY;
        }
        return s_unlink(path);
    }

}
  
int main(int argc, char *argv[]) {
    // TODO: change this later to customize size based on argv
    fs = new FileSystem(10 + 512 + 512 * 512, 9);
    fs->mkfs();

    fuse_operations s_oper;
    memset(&s_oper, 0, sizeof(s_oper));

    // s_oper.init = s_init;
    s_oper.getattr = s_getattr;
    s_oper.open = s_open;
    s_oper.read = s_read;
    s_oper.write = s_write;
    s_oper.truncate = s_truncate;    
    s_oper.unlink = s_unlink;
    s_oper.readdir= s_readdir;
    s_oper.utimens= s_utimens;
    s_oper.mkdir = s_mkdir;
    s_oper.mknod = s_mknod;
    s_oper.rmdir = s_rmdir;

    // call s_init here?
    int argcount = 0;
    char *argument[12];

    char s[] = "-s"; // Use a single thread.
    char d[] = "-d"; // Print debuging output.
    char f[] = "-f"; // Run in the foreground.
    char o[] = "-o"; // Other options
    char p[] = "default_permissions"; // Defer permissions checks to kernel
    char r[] = "allow_other"; // Allow all users to access files

    char mount_point[] = "temp/";

    argument[argcount++] = argv[0];
    argument[argcount++] = f;   
    argument[argcount++] = mount_point;
    argument[argcount++] = o;
    argument[argcount++] = p;
    argument[argcount++] = o;
    argument[argcount++] = r;

    return fuse_main(argcount, argument, &s_oper,0);
}
 
