# solidfs

## Get Started

0. Test Environment

   * Instance type: m1.large
   * Image distribute: ubuntu-15.10
   * Image ID: emi-67a061da
   * Architecture: x86_64

1. Installation

     Download and unzip the file.

2. Run Solid File System (in cs270/build directory)

    ``` shell
    # create build directory
    cd cs270
    mkdir build
    # creat the mount point in build directory
    cd build
    mkdir <mount_pt>
    # make file system
    cmake ..
    make -j
    # run file system
    sudo ./solidFS -m <mount_pt>
    ```
    
    An example command to run the file system is under cs270/build
    
    ```shell
    mkdir temp
    cmake ..
    make -j
    sudo ./solidFS -m temp
    ```
    
    For other options, checkout usage message of the file system
    
    ```shell
    ./solidFS -h
    solid file system
    Usage:
      sudo ./solidFS [OPTION...]

      -b, --block arg    number of blocks (default: 2097253)
      -i, --inode arg    number of inode (default: 100)
      -s, --storage arg  storage in GB
      -e, --entry arg    number of files
      -f, --file arg     storage file (default: /dev/vdb)
      -m, --mount arg    mount point
      -h, --help         Print usage
    ```

4. Run Tests (optional - all in cs270/build directory)

   open another terminal
   ``` shell
   cd cs270/build
   ```

   * test_syscall. The corresponding files are in `cs270/test/fuse/`

     ``` shell
     # syscall Tests
     ./syscallTest `realpath <mount_pt>`
     ```
      
   * Simple read and write test. The corresponding file is `cs270/test/fuse/file_operation_test.cpp`

     ``` shell
     cd build
     ./FuseTests `realpath <mount_pt>`
     ```
5. re-init the fs on device

  ``` shell
  sudo dd if=/dev/zero of=/dev/vdb bs=4096 count=1 conv=notrunc
  ```

## Acknowledgements

1. A private stream processing repo named `nova`
2. [libfuse](https://github.com/libfuse/libfuse)
3. [pjdfstest](https://github.com/pjd/pjdfstest)
4. [some excellent code on leetcode](https://leetcode.com/problems/simplify-path/discuss/25687/C%2B%2B-using-stack
)
