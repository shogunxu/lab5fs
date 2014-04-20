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

/*grabs the block number of the first data block from a give data block index*/
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

/*Find the inode number of the file specified by char *name */
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

/* Needed for ls. Fill out a VFS inode corresponding to the filename give by the dentry*/
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


/*
 * Initialize a data index block for specified inode at specified block number
 *
 */
int lab5fs_inode_init_block_index(struct inode *ino, int bi_block_num)
{
        int err = 0;
        struct super_block *sb = ino->i_sb;
        struct buffer_head *bibh = NULL;
        struct lab5fs_inode_data_index *lab5fs_data_index = NULL;

        /* read the inode's block index. */
        if (!(bibh = sb_bread(sb, bi_block_num))) {
                printk("unable to read inode block index, block %d.\n",
                       bi_block_num);
                err = -ENOMEM;
                goto ret;
        }
        lab5fs_data_index = (struct lab5fs_inode_data_index *)(bibh->b_data);
        memset(lab5fs_data_index->blocks, 0, sizeof(*lab5fs_data_index));
        mark_buffer_dirty(bibh);

  ret:
        if (bibh)
                brelse(bibh);
        return err;
}

/*
 * Allocate a new inode, to be used when creating a new file or directory.
 */
struct inode *lab5fs_inode_new_inode(struct super_block *sb, int mode)
{
        struct inode *child_ino = NULL;
        ino_t ino_num = 0;
        int inode_block_num = 0;
        int bi_block_num = 0;
        int err = 0;
        struct lab5fs_inode_info *inode_info = NULL;

        /* allocate a disk block to contain this inode's data. */
        inode_block_num = lab5fs_alloc_block_num(sb);
        if (inode_block_num == 0) {
                err = -ENOSPC;
                goto ret_err;
        }

        /* allocate a disk block to contain the block index of this inode. */
        bi_block_num = lab5fs_alloc_block_num(sb);
        if (bi_block_num == 0) {
                err = -ENOSPC;
                goto ret_err;
        }

        /* allocate a free inode number. */
        ino_num = lab5fs_alloc_inode_num(sb, inode_block_num);
        if (ino_num == 0) {
                err = -ENOSPC;
                goto ret_err;
        }

        /* allocate a VFS inode object. */
        child_ino = new_inode(sb);
        if (!child_ino) {
                printk("printk: new_inode() failed... inode %lu\n",ino_num);
                err = -ENOMEM;
                goto ret_err;
        }

        /* initialize the inode's block index. */
        err = lab5fs_inode_init_block_index(child_ino, bi_block_num);
        if (err)
                goto ret_err;

        /* init the inode's data. */
        child_ino->i_ino = ino_num;
        child_ino->i_mode = mode;
        child_ino->i_nlink = 1;  /* this inode will be stored in a directory,
                                  * so there's at least one link to this inode,
                                  * from that directory. */
        child_ino->i_size = 0;
        child_ino->i_blksize = LAB5FS_BLOCK_SIZE;
        child_ino->i_blkbits = 10;
        child_ino->i_blocks = 0;
        child_ino->i_uid = current->fsuid;
        child_ino->i_gid = current->fsgid;
        child_ino->i_atime = child_ino->i_mtime = child_ino->i_ctime = CURRENT_TIME;


        /* init the inode's lab5fs metadata. */
        inode_info = kmalloc(sizeof(struct lab5fs_inode_info),
                                    GFP_KERNEL);
        if (!inode_info) {
                printk("not enough memory to allocate inode meta data.\n");
                goto ret_err;
        }
        inode_info->i_block_num = inode_block_num;
        inode_info->i_bi_block_num = bi_block_num;

        child_ino->u.generic_ip = inode_info;

        /* set the inode operations structs. */
		child_ino->i_op = &lab5fs_inode_ops;
		child_ino->i_fop = &lab5fs_file_ops;
		child_ino->i_mapping->a_ops = &lab5fs_address_ops;
        
		//we are not creating directories
		//child_ino->i_op = &lab5fs_dir_iops;
		//child_ino->i_fop = &slab5s_dir_fops;
		//child_ino->i_mapping->a_ops = &lab5fs_aops;
        

        insert_inode_hash(child_ino);
        /* make sure the inode gets written to disk by the inodes cache. */
        mark_inode_dirty(child_ino);

        /* no errors*/
        err = 0;
        goto ret;

  ret_err:
        if (child_ino)
                iput(child_ino); /* child_ino will be deleted here. */
        if (ino_num > 0)
                lab5fs_release_inode_num(sb, ino_num);
        if (inode_block_num > 0)
                lab5fs_release_block_num(sb, inode_block_num);
        if (bi_block_num > 0)
                lab5fs_release_block_num(sb, bi_block_num);
  ret:
        return (err == 0 ? child_ino : NULL);
}

// /*
 // * Create the file with name specified by VFS dentry, inside the given
 // * directory, with the given access permissions.
 // */
// int lab5fs_inode_create(struct inode *dir, struct dentry *dentry, int mode)
// {
        // struct inode *ino = NULL;
        // int err = 0;

        // printk("Creating inode at %ld, path=%s, mode=%o\n",
                             // dir->i_ino, dentry->d_name.name, mode);

        // /* allocate an inode for the child, and add it to the directory. */
        // ino = lab5fs_inode_new_inode (dir->i_sb, mode);
        // if (ino!=NULL) {
                // err = lab5fs_add_file(dir, ino, dentry);
        // }

        // return err;
// }


// /*
 // * Unlink the inode given by the dentry pointer from the given directory  
 // */
// int lab5fs_inode_unlink(struct inode *dir, struct dentry *dentry)
// {
        // int err = 0;
        // struct inode *child = dentry->d_inode;
        // const char* child_name = dentry->d_name.name;
        // int child_name_len = dentry->d_name.len;

        // printk("unlink inode %ld, path=%s\n",
                             // dir->i_ino, dentry->d_name.name);

        // err = lab5fs_dir_del_link(dir, child, child_name, child_name_len);
        // if (err != 0)
                // return err;

        // /* decrease the number of reference counts*/
        // child->i_ctime = dir->i_ctime;
        // child->i_nlink--;
        // mark_inode_dirty(child);
        
        // printk("parent_i_nlink=%d, child_i_nlink=%d\n",
                             // dir->i_nlink, child->i_nlink);

        // err = 0;
        // return err;
// }

