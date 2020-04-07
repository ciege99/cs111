/*
  NAME: Thilan Tran, Collin Prince
  EMAIL: thilanoftran@gmail.com, cprince99@g.ucla.edu
  ID: 605140530, 505091865
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include "ext2_fs.h"

#define SUPER_BLCK_SIZE 1024
#define SUPER_BLCK_OFFSET 1024
#define GROUP_DESC_SIZE 32

int fs; //global variable for file system fd
int fs_flag = 0; //global flag for whether fs is opened
int block_size = 0; //global variabel for blocksize

//forward declaration of functions
void input_handler(int argc, char** arv);
void exit_func(int status, char* msg);
void superblock();
void groups();

void bitmap(char *type, int bitmap_id, int num_elem, int offset_elem);
char file_type(__u16 i_mode);
void convert_time(char* buf, __u32 time_s);
void inodes_in_group(__u32 num_inodes, int table_id, int offset_elem);
void inode_datablocks(int inode_no, struct ext2_inode *inode);
void dir_entries(int inode_no, int block_no, int offset_bytes);
void indirect_blocks(int inode_no, int block_no, int offset, int level, int is_dir);

int main(int argc, char** argv) {
  input_handler(argc, argv); //handle input, create filesystem fd
  superblock(); //print super block info
  groups();
  exit_func(0, "");
}

//function to handle argc, argv and check for input
void input_handler(int argc, char** argv) {
  if (argc <= 1)
    exit_func(1, "Too few arguments provided\n");
  if (argc > 2)  //too many args
    exit_func(1, "Too many arguments provided\n");

  fs = open(argv[1], O_RDONLY);
  if (fs < 0) //check for failed open
    exit_func(1, "Could not open file system\n");
  fs_flag = 1; //set flag
}

//exit handler
void exit_func(int status, char* msg) {
  if (status == 1) //put out error message if failed
    fprintf(stderr, "%s", msg);
  if (fs_flag) //if fs has been opened, close it
    close(fs);
  exit(status);
}

void superblock() {
  struct ext2_super_block super; //defined in given header
  int ret = pread(fs, &super, SUPER_BLCK_SIZE, SUPER_BLCK_OFFSET); //read superblock at 1024 offset
  if (ret == -1)
    exit_func(2, "Couldn't read from superblock\n");
  block_size = 	 (EXT2_MIN_BLOCK_SIZE << super.s_log_block_size);
  printf("SUPERBLOCK,%d,%d,%d,%hu,%d,%d,%d\n",
  	 super.s_blocks_count, super.s_inodes_count,
	 block_size,
	 super.s_inode_size, super.s_blocks_per_group,
  	 super.s_inodes_per_group, super.s_first_ino);
}

void groups() {
  int rc;
  unsigned int i, num_groups, num_blocks, overflow;
  struct ext2_super_block super;
  struct ext2_group_desc* groups;

  //read in from superblock for block group info
  rc = pread(fs, &super, SUPER_BLCK_SIZE, 1024);
  if (rc == -1)
    exit_func(2, "Couldn't read from superblock\n");

  num_groups = super.s_blocks_count / super.s_blocks_per_group;
  overflow = super.s_blocks_count % super.s_blocks_per_group;

  if (overflow > 0) {
    num_groups += 1; // round up (last block group has a smaller block count)
  }

  groups = malloc(sizeof(struct ext2_group_desc) * num_groups);
  rc = pread(fs, groups, num_groups * GROUP_DESC_SIZE, SUPER_BLCK_OFFSET + SUPER_BLCK_SIZE);
  if (rc == -1)
    exit_func(2, "Couldn't read from block group descriptor table\n");

  for (i = 0; i < num_groups; i++) {
    num_blocks = i == num_groups-1 ? overflow : super.s_blocks_per_group; // last block group residue
    printf("GROUP,%d,%d,%d,%d,%d,%d,%d,%d\n",
           i,
           num_blocks, super.s_inodes_per_group,
           groups[i].bg_free_blocks_count, groups[i].bg_free_inodes_count,
           groups[i].bg_block_bitmap, groups[i].bg_inode_bitmap, groups[i].bg_inode_table);
    bitmap("BFREE", groups[i].bg_block_bitmap, num_blocks, i*super.s_blocks_per_group);
    bitmap("IFREE", groups[i].bg_inode_bitmap, super.s_inodes_per_group, i*super.s_inodes_per_group);
    inodes_in_group(super.s_inodes_per_group, groups[i].bg_inode_table, i*super.s_blocks_per_group);
  }

  free(groups);
}

void bitmap(char *type, int bitmap_id, int num_elem, int offset_elem) {
  int i, j, rc,  bit, elem_no;
  struct ext2_super_block super;
  char *buf;

  rc = pread(fs, &super, SUPER_BLCK_SIZE, SUPER_BLCK_OFFSET);
  if (rc == -1)
    exit_func(2, "Couldn't read from superblock\n");

  buf = malloc(sizeof(char) * block_size);
  rc = pread(fs, buf, block_size, bitmap_id * block_size);
  if (rc == -1)
    exit_func(2, "Couldn't read from free inode bitmap\n");

  for (i = 0; i < num_elem;) {
    for (j = 0; i < block_size && j < 8; i++, j++) {
      bit = buf[i/8] & (1 << j); // little endian
      elem_no = offset_elem + i + 1;
      if (!bit) {
        printf("%s,%d\n", type, elem_no);
      }
    }
  }

  free(buf);
}

void inodes_in_group(__u32 num_inodes, int table_id, int offset_elem) {
  unsigned int i;
  int rc;
  struct ext2_inode inode; //use for getting each inode
  int offset = table_id * block_size;
  for (i = 0; i < num_inodes; i++) {
    rc = pread(fs, &inode, sizeof(struct ext2_inode),
	       offset+(i*sizeof(struct ext2_inode))); //read in inode
    if (rc == -1)
      exit_func(2, "Could not read inode\n");
    if (inode.i_mode == 0 || inode.i_links_count == 0) //skip empty inodes
      continue;
    __u32 inum = offset_elem + i + 1; //inode number
    char c_time[20], m_time[20], a_time[20];
    convert_time(c_time, inode.i_ctime); //not sure about these times
    convert_time(m_time, inode.i_mtime); //modtime and accesstime are fine
    convert_time(a_time, inode.i_atime); //but does change time==create time?
    printf("INODE,%u,%c,%o,%hi,%hi,%d,%s,%s,%s,%d,%d",
	   inum, file_type(inode.i_mode), inode.i_mode & 0xfff, // lower 12 bits for mode
	   inode.i_uid, inode.i_gid, inode.i_links_count,
	   c_time, m_time, a_time,
	   (inode.i_size), inode.i_blocks);

    if (file_type(inode.i_mode) == 's' && inode.i_size < 60) {
      printf("\n");
    }
    else {
      inode_datablocks(inum, &inode);
      //assuming file size == blocks*512
    }
  }
}

//returns file type based on inode i_mode
char file_type(__u16 i_mode) {
  if ((i_mode & 0xa000) == 0xa000)
    return 's';
  else if ((i_mode & 0x8000) == 0x8000)
    return 'f';
  else if ((i_mode & 0x4000) == 0x4000)
    return 'd';
  else
    return '?';
}

void convert_time(char* buf, __u32 time_s) {
  time_t raw_time = time_s;
  struct tm* time_info;
  time_info = gmtime(&raw_time);
  strftime(buf,20, "%m/%d/%y %H:%M:%S", time_info);
}

void inode_datablocks(int inode_no, struct ext2_inode *inode) {
  int i;
  char type;
  __u32* block_num = inode->i_block;
  if ((inode->i_mode & 0x8000) || (inode->i_mode & 0x4000) || (inode->i_size >= 60)) {
    for (i = 0; i < EXT2_N_BLOCKS; i++) {
      printf(",%u", block_num[i]);
    }
  }
  printf("\n");

  type = file_type(inode->i_mode);
  if (type == 'd') {
    // may have to also check indirect data blocks as well
    for (i = 0; i < EXT2_NDIR_BLOCKS; i++) {
      dir_entries(inode_no, inode->i_block[i], i*block_size);
    }
  }

  int ind_offset = EXT2_NDIR_BLOCKS;
  int dind_offset = ind_offset + block_size/4;
  int tind_offset = dind_offset + (block_size/4) * (block_size/4);
  if (type =='d' || type == 'f') {
    indirect_blocks(inode_no, block_num[EXT2_IND_BLOCK],
                    ind_offset, 1, type == 'd');
    indirect_blocks(inode_no, block_num[EXT2_DIND_BLOCK],
                    dind_offset, 2, type == 'd');
    indirect_blocks(inode_no, block_num[EXT2_TIND_BLOCK],
                    tind_offset, 3, type == 'd');
  }
}

void dir_entries(int inode_no, int block_no, int offset_bytes) {
  int rc, logical_byte_offset;
  struct ext2_dir_entry dir;

  if (block_no == 0)
    return;

  logical_byte_offset = 0;
  // read through data block, each directory entry is separated by dir.rec_len bytes
  while (logical_byte_offset < block_size) {
    rc = pread(fs, &dir, sizeof(struct ext2_dir_entry),
               block_size*block_no + logical_byte_offset);
    if (rc == -1)
      exit_func(2, "Could not read directory entry\n");
    if (dir.inode == 0) {
      logical_byte_offset += dir.rec_len;
      continue;
    }
    printf("DIRENT,%d,%d,%d,%d,%d,'%s'\n",
           inode_no, logical_byte_offset + offset_bytes,
           dir.inode, dir.rec_len,
           dir.name_len, strndup(dir.name, dir.name_len));
    logical_byte_offset += dir.rec_len;
  }
}

// recursively explore indirect blocks, return the first valid data block offset
void indirect_blocks(int inode_no, int ind_block_no, int offset, int level, int is_dir) {
  if (level == 0 || ind_block_no == 0) //base case
    return;

  int i, rc; //iterator and return value int
  __u32 block_no;
  // traverse every 4-byte u32 block number in the indirect block
  for (i = 0; i < block_size / 4; i++) {
    rc = pread(fs, &block_no, sizeof(__u32), block_size*ind_block_no + sizeof(__u32)*i);
    if (rc == -1)
      exit_func(2, "Could not read indirect block\n");
    if (block_no != 0) {
      indirect_blocks(inode_no, block_no, offset, level-1, is_dir);
      printf("INDIRECT,%d,%d,%d,%d,%d\n",
             inode_no, level,
             // for some reason, this makes the sanity check fail,
             // even though spec says to print the logical offset of *first, direct* data block:
             // level == 1 ? offset : orig_offset,
             offset,
             ind_block_no, block_no);

      if (is_dir && level == 1) //run dirent
        dir_entries(inode_no, block_no, offset*block_size);
    }

    // increment the offset depending on the level
    switch (level) {
      case 1:
        offset++;
        break;
      case 2:
        // number of 4-byte u32 block numbers in a double-indirect block
        offset += block_size / 4;
        break;
      case 3:
        // number of 4-byte u32 block numbers in a triple-indirect block
        offset += (block_size/4) * (block_size/4);
        break;
    }
  }

}
