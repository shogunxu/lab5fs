#ifndef LAB5FS_SUPER_H
#define LAB5FS_SUPER_H

#include <linux/fs.h>


/*
 * Allocates a free block number.
 * returns 0 if no free numbers are available.
 */
int lab5fs_alloc_block_num(struct super_block *);
int lab5fs_release_block_num(struct super_block *, int);
int lab5fs_alloc_inode_num(struct super_block *, int);
int lab5fs_release_inode_num(struct super_block *, int );


int lab5fs_fill_super(struct super_block*,void *, int);

#endif /*LAB5_SUPER_H*/
