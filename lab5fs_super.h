#ifndef LAB5FS_SUPER_H
#define LAB5FS_SUPER_H

#include <linux/fs.h>


/*
 * Utilities
 */
int lab5fs_alloc_block_num(struct super_block *); //grabs the first free block number from the block bitmap
int lab5fs_release_block_num(struct super_block *, int); //releases block number
int lab5fs_alloc_inode_num(struct super_block *, int); //grabs the first free inode number
int lab5fs_release_inode_num(struct super_block *, int ); //releases the given inode number
unsigned long lab5fs_find_block_num(struct inode *ino); //finds the block number of a given inode

int lab5fs_fill_super(struct super_block*,void *, int);

#endif /*LAB5_SUPER_H*/
