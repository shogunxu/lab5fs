#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/statfs.h>
#include "lab5fs.h"
#include "lab5fs_super.h"
#include "lab5fs_inode.h"

/* inode operations go here*/
struct inode_operations lab5fs_inode_ops = {
	lookup: lab5fs_lookup,
};

/* file operations go here*/
struct file_operations lab5fs_file_ops = {
	llseek:generic_file_llseek,
	read:  generic_file_read,
	write: generic_file_write,
	mmap:  generic_file_mmap,
	open:  generic_file_open,
};

/* dir operations go her */
struct file_operations lab5fs_dir_ops = {
	readdir: lab5fs_readdir,
};

/* address operations go here*/
struct address_space_operations lab5fs_address_ops = {
};

/*Read inode data from a block on disk and fill out a VFS inode*/
int lab5fs_inode_read_ino(struct inode *ino, unsigned long block_num){

        int err = -ENOMEM;
        struct super_block *sb = ino->i_sb;
        struct buffer_head *ibh = NULL;
        struct lab5fs_inode *lab5fs_ino = NULL;
        unsigned long bi_block_num = 0;
        struct lab5fs_inode_info *inode_meta = NULL;


        printk("Reading inode %ld\n", ino->i_ino);

        /* read the inode's block from disk. */
        if (!(ibh = sb_bread(sb,block_num))) {
                printk("Unable to read inode block %lu.\n", block_num);
                goto ret_err;
        }
        lab5fs_ino = (struct lab5fs_inode *)((char *)(ibh->b_data));

        bi_block_num = le32_to_cpu(lab5fs_ino->i_data_index_block_num);

        printk("Inode %ld, block_index_num=%lu\n",
                             ino->i_ino, bi_block_num);

        /* initialize the inode's meta data. */
        inode_meta = kmalloc(sizeof(struct lab5fs_inode_info),GFP_KERNEL);
        if (inode_meta==NULL) {
                printk("Not enough memory to allocate inode meta struct.\n");
                goto ret_err;
        }
        inode_meta->i_block_num = block_num;
        inode_meta->i_bi_block_num = bi_block_num;

	/* fill out VFS inode*/
        ino->i_mode = le16_to_cpu(lab5fs_ino->i_mode);
        ino->i_nlink = le16_to_cpu(lab5fs_ino->i_link_count);
        ino->i_size = le32_to_cpu(lab5fs_ino->i_size);
        ino->i_blksize = LAB5FS_BLOCK_SIZE;
        ino->i_blkbits = LAB5FS_BITS;
        ino->i_blocks = le32_to_cpu(lab5fs_ino->i_num_blocks);
        ino->i_uid = le32_to_cpu(lab5fs_ino->i_uid);
        ino->i_gid = le32_to_cpu(lab5fs_ino->i_gid);
        ino->i_atime.tv_sec = le32_to_cpu(lab5fs_ino->i_atime);
        ino->i_mtime.tv_sec = le32_to_cpu(lab5fs_ino->i_mtime);
        ino->i_ctime.tv_sec = le32_to_cpu(lab5fs_ino->i_ctime);
        ino->u.generic_ip = inode_meta;

        /* set the inode operations structs  */
        ino->i_op = &lab5fs_inode_ops;
        /* TODO: switch based on type */
        /* ino->i_fop = &lab5fs_file_ops; */
        ino->i_fop = &lab5fs_dir_ops;
        ino->i_mapping->a_ops = &lab5fs_address_ops;

        printk(    "Inode %ld: i_mode=%o, i_nlink=%d, "
                   "i_uid=%d, i_gid=%d\n",
                   ino->i_ino, ino->i_mode, ino->i_nlink,
                   ino->i_uid, ino->i_gid);
	return 0;

ret_err:
	if (ibh)
                brelse(ibh);
        return err;
}

/*Free memory used by VFS inode object*/
void lab5fs_inode_clear(struct inode *ino){
	struct lab5fs_inode_info *inode_info = LAB5FS_INODE_INFO(ino);
	kfree(inode_info);
	ino->u.generic_ip = NULL;
}

int lab5fs_getblock(struct inode *dir, int *blocknum) {
	int err = 0;
	struct super_block *sb = dir->i_sb;
	struct buffer_head *bh = NULL;
	struct lab5fs_inode_info *info = LAB5FS_INODE_INFO(dir);
	struct lab5fs_inode_data_index *data;

	bh = __bread(sb->s_bdev, info->i_bi_block_num, LAB5FS_BLOCK_SIZE);
	data = (struct lab5fs_inode_data_index *)bh->b_data;
	*blocknum = (int)(data->blocks[0]);
out:
	if(bh)
		brelse(bh);
	return err;
}

int lab5fs_getfile(struct inode *dir, const char *name, int len, ino_t *ino) {
	int err = 0;
	struct super_block *sb = dir->i_sb;
	struct buffer_head *bh = NULL;
	struct lab5fs_dir *drec = NULL;
	int blocknum;

	printk("lab5fs_getfile:: name: %s, len: %d\n", name, len);
	err = lab5fs_getblock(dir, &blocknum);
	if(!err) {
		bh = __bread(sb->s_bdev, blocknum, LAB5FS_BLOCK_SIZE);
		drec = (struct lab5fs_dir*)bh->b_data;
		*ino = drec->dir_inode;
	}

out:
	if(bh)
		brelse(bh);
	return err;
}

/* Needed for ls */
struct dentry* lab5fs_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *data) {
	int err = 0;
	struct inode *inode = NULL;
	ino_t ino;

	printk("lab5fs_lookup:: name: %s, len: %d\n", dentry->d_name.name, dentry->d_name.len);
	err = lab5fs_getfile(dir, dentry->d_name.name, dentry->d_name.len, &ino);
	if(!err && ino>0) {
		inode = iget(dir->i_sb, ino);
		d_add(dentry, inode);
	}
	return dentry;
}

/* List a directory's files */
int lab5fs_readdir(struct file *filep, void *dirent, filldir_t filldir) {
	int err = 0;
	struct dentry *dentry = filep->f_dentry;
	struct inode *inode = dentry->d_inode;
	struct super_block *sb = inode->i_sb;
	struct buffer_head *bh = NULL;

	if(filep->f_pos == 0) {
		filldir(dirent, ".", 1, filep->f_pos, inode->i_ino, DT_DIR);
		filep->f_pos++;
	}

	if(filep->f_pos == 1) {
		filldir(dirent, "..", 2, filep->f_pos, dentry->d_parent->d_inode->i_ino, DT_DIR);
		filep->f_pos++;
	}

out:
	if(bh)
		brelse(bh);
	return err;
}
