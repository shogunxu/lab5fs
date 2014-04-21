#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include "lab5fs.h"
#include "lab5fs_super.h"
#include "lab5fs_inode.h"


/*MACRO for accessing the superblock info pointer*/
#define LAB5FS_SB_INFO(sb) ((struct lab5fs_sb_info*)((sb)->s_fs_info))

/*function prototypes for super block operations*/
void lab5fs_read_inode (struct inode *);
void lab5fs_clear_inode (struct inode *);
void lab5fs_put_super (struct super_block *);
void lab5fs_write_super (struct super_block *sb);
void lab5fs_write_inode(struct inode *ino, int sync);
void lab5fs_delete_inode (struct inode *ino);

/*Note: still need to actually implement these functions*/
struct super_operations lab5fs_super_ops ={
	read_inode: lab5fs_read_inode,
	write_inode: lab5fs_write_inode,
	clear_inode: lab5fs_clear_inode,
	delete_inode: lab5fs_delete_inode,
	put_super: lab5fs_put_super,
	write_super: lab5fs_write_super,
};

/* Store custom metadata about filesystem*/
struct lab5fs_sb_info {
	/*lab5fs super block*/
	struct buffer_head *s_sbh;
        struct lab5fs_super_block *s_lab5fs_sb;

	/*lab5fs block bitmap*/
        struct buffer_head *s_block_bitmap_bh;
        struct lab5fs_bitmap *s_lab5fs_block_bitmap;

	/*lab5fs inode bitmap*/
        struct buffer_head *s_inode_bitmap_bh;
        struct lab5fs_bitmap *s_lab5fs_inode_bitmap;

	/*lab5fs inode table*/
	struct buffer_head *s_inode_table_bh;
	struct lab5fs_inode_table *s_lab5fs_inode_table;
};


/*Locate the block number of an inode given its inode number*/
unsigned long lab5fs_find_block_num(struct inode *ino)
{
        unsigned long ino_num = ino->i_ino;
        unsigned long block_num = 0;
        struct lab5fs_inode_table *inode_table;

        if (ino_num < LAB5FS_ROOT_INODE || ino_num > LAB5FS_MAX_INODE_COUNT) {
             printk("inode number '%lu' is out of range\n", ino_num);
             return 0;
        }

        inode_table = LAB5FS_SB_INFO(ino->i_sb)->s_lab5fs_inode_table;
        block_num = le32_to_cpu(inode_table->inodes[ino_num]);
        printk("inode number '%lu' is on block %lu\n", ino_num, block_num);

        return block_num;
}




/*
 * Allocates a free block number.
 * returns 0 if no free numbers are available.
 */
int lab5fs_alloc_block_num(struct super_block *sb)
{
        struct lab5fs_sb_info* sb_info = LAB5FS_SB_INFO(sb);
        struct lab5fs_super_block* lab5fs_sb = sb_info->s_lab5fs_sb;
	struct lab5fs_bitmap *block_bitmap = sb_info->s_lab5fs_block_bitmap;
        struct buffer_head *sbh = sb_info->s_sbh;
        struct buffer_head *bbh = sb_info->s_block_bitmap_bh;
        int block_num = 0;

        printk("allocating block\n");

        lock_super(sb);

        if (lab5fs_sb->s_free_blocks_count == 0) {
                printk("Error: no more free blocks.\n");
                goto ret;
        }
	
		/*go to bitmap for first free block*/
		block_num = find_first_zero_bit((unsigned long*)(block_bitmap->map),LAB5FS_MAX_BLOCK_COUNT);
		if(block_num >= LAB5FS_MAX_BLOCK_COUNT || block_num<=LAB5FS_ROOT_DATA_FIRST_NUM){
			printk("Error: Could not find free block. Block num=%d.\n",block_num);
			block_num=0;
            goto ret;
		}
		set_bit(block_num, (unsigned long*)(block_bitmap->map));
        lab5fs_sb->s_free_blocks_count--;
		mark_buffer_dirty(bbh);
        mark_buffer_dirty(sbh);
        sb->s_dirt = 1;

        printk("Allocated block number %d\n", block_num);

ret:
        unlock_super(sb);
        return block_num;
}


/*
 * Frees a previously allocated block number.
 * returns 0 on success, a negative error code on failure.
 */
int lab5fs_release_block_num(struct super_block *sb, int block_num)
{
        struct lab5fs_sb_info* sb_info = LAB5FS_SB_INFO(sb);
        struct lab5fs_super_block* lab5fs_sb = sb_info->s_lab5fs_sb;
		struct lab5fs_bitmap* block_bitmap = sb_info->s_lab5fs_block_bitmap;
        struct buffer_head *sbh = sb_info->s_sbh;
        struct buffer_head *bbh = sb_info->s_block_bitmap_bh;

        printk("freeing block %d\n",block_num);

        /* Prevent freeing any of the low number blocks. */
        if (block_num <= LAB5FS_ROOT_DATA_FIRST_NUM) {
                printk("trying to free at or below "
                       "mandatory block %d\n",
                       LAB5FS_ROOT_DATA_FIRST_NUM);
				return -1;
        }
		
		/*check block number is less than max block number*/
		if(block_num >= LAB5FS_MAX_BLOCK_COUNT){
			printk("trying to free a block with block number" 
					"greater than maximum block number %d\n",
					LAB5FS_MAX_BLOCK_COUNT);
				return -1;
		}

        lock_super(sb);

		/*clear bitmap*/
		clear_bit(block_num, (unsigned long*)(block_bitmap->map));
		
		mark_buffer_dirty(bbh);
        lab5fs_sb->s_free_blocks_count++;
        mark_buffer_dirty(sbh);
        sb->s_dirt = 1;

        unlock_super(sb);

        printk("block %d freed\n", block_num);

        return 0;
}


/*
 * Allocates a free inode number and creates an entry for it in the inode table.
 * The block_num parameter indicates where the inode number should be mapped to.
 * returns 0 if no free numbers are available.
 */
int lab5fs_alloc_inode_num(struct super_block *sb, int block_num)
{
		struct lab5fs_sb_info* sb_info = LAB5FS_SB_INFO(sb);
        struct lab5fs_super_block* lab5fs_sb = sb_info->s_lab5fs_sb;
		struct lab5fs_bitmap* inode_bitmap = sb_info->s_lab5fs_inode_bitmap;
		struct lab5fs_inode_table* inode_table = sb_info->s_lab5fs_inode_table;
        struct buffer_head *sbh = sb_info->s_sbh;
        struct buffer_head *ibh = sb_info->s_inode_bitmap_bh;
        struct buffer_head *ith = sb_info->s_inode_table_bh;
        int inode_num = 0;

        printk("allocating inode to block %d\n",block_num);

        lock_super(sb);

        if (lab5fs_sb->s_free_inodes_count == 0) {
                printk("Error: no more free inodes.\n");
                goto ret;
        }
	
		/*go to bitmap for first free inode*/
		inode_num = find_first_zero_bit((unsigned long*)(inode_bitmap->map),LAB5FS_MAX_INODE_COUNT);
		if(inode_num >= LAB5FS_MAX_INODE_COUNT || inode_num <= LAB5FS_ROOT_INODE){
			printk("Error: Could not find free inode. Inode num=%d.\n",inode_num);
			inode_num=0;
            goto ret;
		}
		set_bit(inode_num, (unsigned long*)(inode_bitmap->map));
		inode_table->inodes[inode_num]=block_num;
		
        lab5fs_sb->s_free_inodes_count--;
		mark_buffer_dirty(ith);
		mark_buffer_dirty(ibh);
        mark_buffer_dirty(sbh);
        sb->s_dirt = 1;

        printk("Allocated inode number %d\n", inode_num);

ret:
        unlock_super(sb);
        return inode_num;
}

/*
 * Frees a previously allocated inode number.
 * returns 0 on success, a negative error code on failure.
 */
int lab5fs_release_inode_num(struct super_block *sb, int inode_num)
{
        struct lab5fs_sb_info* sb_info = LAB5FS_SB_INFO(sb);
        struct lab5fs_super_block* lab5fs_sb = sb_info->s_lab5fs_sb;
	struct lab5fs_bitmap* inode_bitmap = sb_info->s_lab5fs_inode_bitmap;
	struct lab5fs_inode_table* inode_table = sb_info->s_lab5fs_inode_table;
        struct buffer_head *sbh = sb_info->s_sbh;
        struct buffer_head *ibh = sb_info->s_inode_bitmap_bh;
        struct buffer_head *ith = sb_info->s_inode_table_bh;


        printk("freeing inode %d\n",inode_num);

        /* Prevent freeing root inode. */
        if (inode_num == 0) {
                printk("trying to free root inode %d\n",
                       LAB5FS_ROOT_DATA_FIRST_NUM);
				return -1;
        }
		
		/*check block number is less than max block number*/
		if(inode_num >= LAB5FS_MAX_INODE_COUNT){
			printk("trying to free a inode with inode number" 
					"greater than max inode number %d\n",
					LAB5FS_MAX_INODE_COUNT);
				return -1;
		}

        lock_super(sb);

		/*clear bitmap*/
		clear_bit(inode_num, (unsigned long*)(inode_bitmap->map));
		/*for cleanliness set inode table entry to 0*/
		inode_table->inodes[inode_num]=0;
		
		mark_buffer_dirty(ith);
		mark_buffer_dirty(ibh);
        lab5fs_sb->s_free_blocks_count++;
        mark_buffer_dirty(sbh);
        sb->s_dirt = 1;

        unlock_super(sb);

        printk("inode num %d freed\n", inode_num);

        return 0;
}



/* Fill in vfs superblock from lab5fs image*/
int lab5fs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct buffer_head *bh,*bb_bh,*ib_bh,*it_bh;
	struct lab5fs_super_block *disk_sb;
	struct lab5fs_bitmap *disk_block_bitmap;
	struct lab5fs_bitmap *disk_inode_bitmap;
	struct lab5fs_inode_table *disk_inode_table;
	struct inode *inode;
	struct lab5fs_sb_info *metadata;
	printk("Mounting lab5fs\n");

	/*init buffer heads and read data from disk*/
	sb_set_blocksize(sb, LAB5FS_BLOCK_SIZE);
	bh = sb_bread(sb, 0);
	disk_sb = (struct lab5fs_super_block*)bh->b_data;
	printk("magic: %0x, free: %0x\n", disk_sb->s_magic, disk_sb->s_free_blocks_count);

	if(!(bb_bh = sb_bread(sb, LAB5FS_BLOCK_BITMAP_NUM))){
		printk("Unable to read block bitmap\n");
	}
	disk_block_bitmap = (struct lab5fs_bitmap*)bb_bh->b_data;

	if(!(ib_bh = sb_bread(sb, LAB5FS_INODE_BITMAP_NUM))){
		printk("Unable to read inode bitmap");
	}
	disk_inode_bitmap = (struct lab5fs_bitmap*) ib_bh->b_data;

	if(!(it_bh = sb_bread(sb, LAB5FS_INODE_TABLE_NUM))){
		printk("Unable to read inode table");
	}
	disk_inode_table = (struct lab5fs_inode_table*) it_bh->b_data;

	/* set up superblock meta data*/
	metadata = kmalloc(sizeof(struct lab5fs_sb_info), GFP_KERNEL);
	if(metadata == NULL)
	{
		printk("Not enough memory to allocate super block struct.\n");
	}
	metadata->s_sbh = bh;
	metadata->s_lab5fs_sb = disk_sb;
	metadata->s_block_bitmap_bh = bb_bh;
	metadata->s_lab5fs_block_bitmap = disk_block_bitmap;
	metadata->s_inode_bitmap_bh = ib_bh;
	metadata->s_lab5fs_inode_bitmap = disk_inode_bitmap;
	metadata->s_inode_table_bh = it_bh;
	metadata->s_lab5fs_inode_table = disk_inode_table;

	/*fill vfs super block*/
	sb->s_maxbytes = LAB5FS_MAX_SIZE;
	sb->s_blocksize = LAB5FS_BLOCK_SIZE;
	sb->s_blocksize_bits = LAB5FS_BITS;
	sb->s_magic = LAB5FS_SUPER_MAGIC;
	sb->s_op = &lab5fs_super_ops;
	sb->s_fs_info = metadata;

	/*load root inode*/
	inode = iget(sb,LAB5FS_ROOT_INODE);
	sb->s_root = d_alloc_root(inode);

	return 0;
}

void lab5fs_read_inode (struct inode *ino)
{
        unsigned long block_num = 0;

        /* find the inode's block number. */
        block_num = lab5fs_find_block_num(ino);
        if (block_num == 0)
		printk("Error reading inode\n");
        lab5fs_inode_read_ino(ino, block_num); /*function defined in lab5fs_inode.c*/
}

/*Free bufferheads and release memory*/
void lab5fs_put_super(struct super_block *sb){
	struct lab5fs_sb_info *sb_info = LAB5FS_SB_INFO(sb);
	printk("Releasing VFS super block\n");
	brelse(sb_info->s_sbh);
	brelse(sb_info->s_block_bitmap_bh);
	brelse(sb_info->s_inode_bitmap_bh);
	brelse(sb_info->s_inode_table_bh);
	kfree(sb_info);
	sb->s_fs_info = NULL;
}

/*Write Inode to on-disk*/
void lab5fs_write_inode(struct inode *ino,int sync)
{
        printk("writing inode %ld to disk\n", ino->i_ino);
        lab5fs_inode_write_ino (ino);
}

/*Delete inode from VFS and disk*/
void lab5fs_delete_inode (struct inode *ino)
{
        printk("deleting inode %ld\n", ino->i_ino);

        /* delete the inode from the file-system - free its blocks,
         * then mark it as free. */

        /* free data blocks of this inode. */
        ino->i_size = 0;
        if (ino->i_blocks) { /*file contains data inside*/
                printk("clearing data blocks, #blocks = %ld\n", ino->i_blocks);
                lab5fs_inode_clear_blocks(ino);
        }

        /* free the block index and the inode's block numbers. */
        lab5fs_inode_free_inode(ino);

  
        clear_inode(ino);
}

/*Release an inode and clear memory used by inode*/
void lab5fs_clear_inode (struct inode * ino){
	printk("Releasing inode #%ld from VFS\n",ino->i_ino);
	lab5fs_inode_clear(ino); /*function defined in lab5fs_inode.c*/
}


/* nothing to do - the super-block is stored in buffers, which already get written to disk*/
void lab5fs_write_super (struct super_block *sb)
{
        printk("writing superblock to disk\n");
        sb->s_dirt = 0;
}
