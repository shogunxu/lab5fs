#ifndef _LAB5FS_H
#define _LAB5FS_H

#define LAB5FS_SUPER_MAGIC 0xBADC0DE

#define LAB5FS_SUPER_BLOCK_NUM 0
#define LAB5FS_BLOCK_BITMAP_NUM 1
#define LAB5FS_INODE_BITMAP_NUM 2
#define LAB5FS_INODE_TABLE_NUM 3
#define LAB5FS_ROOT_INODE_NUM 4
#define LAB5FS_ROOT_DATA_INDEX_NUM 5
#define LAB5FS_ROOT_DATA_FIRST_NUM 6
//set root inode number to 1. Reserve inode number 0 for null
#define LAB5FS_ROOT_INODE 1

#define LAB5FS_BLOCK_SIZE 1024
#define LAB5FS_BITS	10
#define LAB5FS_MAX_SIZE LAB5FS_BLOCK_SIZE*LAB5FS_BLOCK_SIZE/4
#define LAB5FS_MAX_INODE_COUNT 1024*8
#define LAB5FS_MAX_BLOCK_COUNT 1024*8
#define LAB5FS_MAX_FNAME 16
#define LAB5FS_MAX_BLOCK_INDEX 256

#include <linux/types.h>
struct lab5fs_super_block {
    uint32_t s_magic; /* sb magic number*/
    uint32_t s_inode_count;  /*total number of inodes in fs*/
    uint32_t s_blocks_count; /*total number of blocks in fs*/
    uint32_t s_free_blocks_count; /*number of available blocks*/
    uint32_t s_free_inodes_count; /*number of available inodes*/
    uint32_t s_block_size; /*size of each block*/
    char s_volume_name[16]; //Volume name
};

struct lab5fs_inode {
    uint16_t i_mode; //inode type/file access rights
    uint16_t i_uid; //owner id
    uint16_t i_gid; //group id
    uint32_t i_size; //file length in bytes
    uint32_t i_atime; //time of last access
    uint32_t i_mtime; //time of last file change
    uint32_t i_ctime; //time of last inode change
    uint16_t i_link_count; //number of hard links
    uint32_t i_num_blocks; //number of blocks of data used by file
    uint32_t i_block_num; //block number of this inode
    uint32_t i_data_index_block_num; //block number of corresponding data index
};

struct lab5fs_dir {
    uint32_t dir_inode;
    uint8_t dir_name_len;
    uint8_t dir_file_type;
    char dir_name[LAB5FS_MAX_FNAME];
};

struct lab5fs_bitmap {
    uint8_t map[1024];
};

struct lab5fs_inode_table {
    uint32_t inodes[256];
};

struct lab5fs_inode_data_index { /*Data index block. Basically just an array of block numbers*/
    uint32_t blocks[LAB5FS_MAX_BLOCK_INDEX];
};


#endif /* _LAB5FS_H */
