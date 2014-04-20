#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "lab5fs.h"

#define HIGHEST_USED_BLOCK_NUM LAB5FS_ROOT_DATA_FIRST_NUM

/* write the given data to the given logical block number.
 * returns 1 on success, 0 on failure.
 */
int write_block(const char* dev_path,
			   int fd, const char* block_name,
			   int block_num, char* data, int data_len)
{
	int rc;
	off_t seek_pos = block_num * LAB5FS_BLOCK_SIZE;

	/* we need to write into block #1. */
	rc = lseek(fd, seek_pos, SEEK_SET);
	if (rc == -1) {
			printf( "%failed seeking into position %lu "
					"of file '%s'\n",
					seek_pos,dev_path);
			return 0;
	}
	if (rc != seek_pos) {
			printf("failed seeking into position %lu of file '%s' - "
					"lseek returned %d.\n",
					seek_pos,dev_path, rc);
			return 0;
	}

	rc = write(fd, data, data_len);
	if (rc == -1) {
			printf("failed writing '%s' block into '%s' \n",
					block_name,dev_path);
			return 0;
	}
	if (rc != data_len) {
			printf("got only partial write when writing '%s' block "
					" into position %lu of file '%s'.\n",
					block_name,seek_pos,dev_path);
			return 0;
	}

    return 1;
}

/*Write the Lab5 Super Block*/
int write_super_block(const char* dev_path,int fd, int num_blocks, int num_free_blocks)
{
	struct lab5fs_super_block lab5_sb;
	int rc;

	memset(&lab5_sb, 0, sizeof(lab5_sb));
	lab5_sb.s_magic = LAB5FS_SUPER_MAGIC;
	lab5_sb.s_inode_count = LAB5FS_MAX_INODE_COUNT;
	lab5_sb.s_blocks_count = num_blocks;
	lab5_sb.s_free_inodes_count = LAB5FS_MAX_INODE_COUNT - 1;
	lab5_sb.s_free_blocks_count = num_free_blocks;
	lab5_sb.s_block_size=LAB5FS_BLOCK_SIZE; 
	

	/*write to super block (block 0)*/
	rc = write_block(dev_path, fd, "super block",
					LAB5FS_SUPER_BLOCK_NUM,
					(char*)&lab5_sb, sizeof(lab5_sb));
	return rc;
}


/* write the block bitmap. */
int write_block_bitmap(const char* dev_path, int fd)
{
	struct lab5fs_bitmap block_bitmap;
	int rc;

	/* everything should be zero, except for the first 7 bits*/
	memset(&block_bitmap, 0, sizeof(block_bitmap));
	block_bitmap.map[0] = 0x7F; /*set the first 7 bits to 1*/

	/* write to inode block bitmap (block 1). */
	rc = write_block(dev_path, fd, "block bitmap",
					 LAB5FS_BLOCK_BITMAP_NUM,
					 (char*)&block_bitmap, sizeof(block_bitmap));
	return rc;
}

/* write the block bitmap. */
int write_inode_bitmap(const char* dev_path, int fd)
{
	struct lab5fs_bitmap inode_bitmap;
	int rc;

	/* everything should be zero, except for the first inode (maps null)
	 * and second inode (maps to root)
	*/
	memset(&inode_bitmap, 0, sizeof(inode_bitmap));
	inode_bitmap.map[0] = 0x3; /*set the first and second inode bit to 1*/

	/* write to inode bitmap block (block 2). */
	rc = write_block(dev_path, fd, "inode bitmap",
					 LAB5FS_INODE_BITMAP_NUM,
					 (char*)&inode_bitmap, sizeof(inode_bitmap));
	return rc;
}

/* write the inode table block. */
int write_inode_table(const char* dev_path,int fd)
{
	struct lab5fs_inode_table table;
	int rc;

	/*clear out table*/
	memset(&table, 0, sizeof(table));

	/*point inode #0 to the root inode block*/
	table.inodes[LAB5FS_ROOT_INODE]=LAB5FS_ROOT_INODE_NUM;
	/*write to inode table block (block 3)*/
	rc = write_block(dev_path, fd, "inode table",
			LAB5FS_INODE_TABLE_NUM,
			(char*)&table, sizeof(table));
	return rc;
}

/* write the root inode. */
int write_root_inode(const char* dev_path, int fd)
{
	struct lab5fs_inode root_inode;
	int rc;

	memset((char*)&root_inode, 0, sizeof(root_inode));
	/* permissions - 0x40755 */
	root_inode.i_mode = S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
	root_inode.i_uid = 0;
	root_inode.i_gid = 0;
	root_inode.i_size = 0;
	root_inode.i_atime = 0;
	root_inode.i_mtime = 0;
	root_inode.i_ctime = 0;
	root_inode.i_num_blocks = 1;
	root_inode.i_link_count = 1;
	root_inode.i_block_num = LAB5FS_ROOT_INODE_NUM;
	root_inode.i_data_index_block_num=LAB5FS_ROOT_DATA_INDEX_NUM;
	

	/* write into root inode block (block #4)*/
	rc = write_block(dev_path, fd, "root inode",
			LAB5FS_ROOT_INODE_NUM,
			(char*)&root_inode,
			sizeof(root_inode));
	return rc;
}

/* write the block index of the root inode. */
int write_root_data_index(const char* dev_path, int fd)
{
        struct lab5fs_inode_data_index root_block_index;
        int rc;

        memset((char*)&root_block_index, 0, sizeof(root_block_index));
        root_block_index.blocks[0] = LAB5FS_ROOT_DATA_FIRST_NUM;

        /* write into the root inode's data index block (block 5)*/
        rc = write_block(dev_path, fd,
                         "root inode block index",
                          LAB5FS_ROOT_DATA_INDEX_NUM,
                         (char*)&root_block_index, sizeof(root_block_index));
        return rc;
}

/* write the first (and only) data block of the root directory (i.e. the root
 * inode).
 */
int write_root_data(const char* dev_path, int fd)
{
        struct lab5fs_dir root_dir;
        int rc;

        memset((char*)&root_dir, 0, sizeof(root_dir));
        root_dir.dir_inode = 0;

        /* write data to first root data block (block 6) */
        rc = write_block(dev_path, fd,
                                "root inode first data block",
                                LAB5FS_ROOT_DATA_FIRST_NUM,
                                (char*)&root_dir, sizeof(root_dir));
        return rc;
}

/*Check to make sure file passed is accessable, has write permissions, and is a block device*/
int check_dev(const char* dev_path, int* num_blocks){
	struct stat st;
	/* check path exists. */
	if (stat(dev_path, &st) == -1) {
			printf("cannot find file '%s' \n",
					dev_path);
			return 0;
	}

	/* make sure we have write permission for this path. */
	if (access(dev_path, W_OK) == -1) {

			printf("no write access to '%s'.\n",
					dev_path);
			return 0;
	}

	/* calculate the number of blocks in this file/device. */
	(*num_blocks) = st.st_size / LAB5FS_BLOCK_SIZE;

	/* check that file is a block device*/
/*	if (!S_ISBLK(st.st_mode)) {
			printf("path '%s' is not a block device. "
					"cannot format.\n",
					dev_path);
			return 0;
	}
*/
	return 1;
}

/* create the Lab5 file-system structure. */
int mklab5fs(const char* dev_path, int num_blocks, int num_free_blocks)
{
	int fd = open(dev_path, O_WRONLY | O_EXCL);

	if (fd == -1) {
		printf("%failed opening file '%s' for writing /n",
			dev_path);
		return 0;
	}

	if (!write_super_block(dev_path, fd, num_blocks,
							  num_free_blocks)) {
		close(fd);
		return 0;
	}
	
	if (!write_block_bitmap(dev_path, fd)) {
		close(fd);
		return 0;
	}

	if (!write_inode_bitmap(dev_path, fd)) {
		close(fd);
		return 0;
	}

	if (!write_inode_table(dev_path, fd)) {
		close(fd);
		return 0;
	}

	if (!write_root_inode(dev_path, fd)) {
		close(fd);
		return 0;
	}
	
	if (!write_root_data_index(dev_path, fd)) {
		close(fd);
		return 0;
	}
	if (!write_root_data(dev_path, fd)) {
		close(fd);
		return 0;
	}

	if (close(fd) == -1) {
		printf("error while closing file '%s'",
			   dev_path);
		return 0;
        }

    return 1;
}

int main(int argc, char *argv[]){

	const char *dev_path = NULL;
	int num_blocks = 0;
	int free_blocks = 0;
	const char* progname = argv[0];

	if (argc < 2) {
		printf("Usage: lab5mkfs <image file>\n");
		exit(1);
	}

	dev_path = argv[1];

	/* make basic checks - the path exists and points to a device file*/
	if (!check_dev(dev_path, &num_blocks))
			exit(1);
	free_blocks = num_blocks - (HIGHEST_USED_BLOCK_NUM + 1);

	/* create the file system. */
	if (!mklab5fs(dev_path, num_blocks, free_blocks))
			exit(1);

	return 0;
}
