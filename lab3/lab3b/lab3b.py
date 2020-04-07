#!/usr/bin/env python3

# NAME: Collin Prince, Thilan Tran
# EMAIL: cprince99@g.ucla.edu, thilanoftran@gmail.com
# ID: 505091865, 605140530

import sys
import os
import csv

# global lists to hold each type of entry
superblock = list()
groups = list()
bfree = list()
ifree = list()
inodes = list()
dirent = list()
indirect = list()
inconsistent = 0 # global flag if an inconsistency was found

def main():
    arg_check()
    read_csv()
    consistency()
    exit(inconsistent)


def arg_check():
    if (len(sys.argv) < 2):
        print('Too few arguments given')
        exit(1)
    elif (len(sys.argv) > 2):
        print('Too many arguments given')
        exit(1)


def read_csv():
    try:
        with open(sys.argv[1]) as csvfile:
            reader = csv.reader(csvfile, delimiter=',')
            for row in reader:
                #  print(row)
                if (row[0] == 'INODE'):
                    inodes.append(row)
                elif (row[0] == 'INDIRECT'):
                    indirect.append(row)
                elif (row[0] == 'SUPERBLOCK'):
                    superblock.append(row)
                elif (row[0] == 'DIRENT'):
                    dirent.append(row)
                elif (row[0] == 'GROUP'):
                    groups.append(row)
                elif (row[0] == 'BFREE'):
                    bfree.append(row)
                elif (row[0] == 'IFREE'):
                    ifree.append(row)
                elif (row[0] == 'DIRENT'):
                    dirent.append(row)
    except:
        print('Error reading csv: ' + str(sys.exc_info()[0]), file=sys.stderr)
        exit(1)


def consistency(): # run the actual logic of the program
    block_max = int(superblock[0][1]) # max valid block number
    block_size = int(superblock[0][3])
    num_inodes_in_group = int(groups[0][3])
    inode_size = int(superblock[0][4])
    first_blk_inode_table = int(groups[0][8])
    # # assuming only 1 group from spec
    blocks_for_inode_table = (num_inodes_in_group * inode_size) // block_size
    overflow = (num_inodes_in_group * inode_size) % block_size
    if (overflow > 0):
        blocks_for_inode_table += 1
    block_min = first_blk_inode_table + blocks_for_inode_table
    inode_min = int(superblock[0][7])
    inode_max = int(superblock[0][2])

    block_consistency(block_min, block_max, block_size)
    ref_blocks(block_min, block_max, block_size)
    inode_alloc(inode_min, inode_max)
    inode_links()
    dirent_links(inode_max)


def block_consistency(block_min: int, block_max: int, block_size:int):
    invalid = list()
    bad = list()
    for i in range(12, 27): # iterate through each block pointer
        invalid = []
        for row in inodes: # iterate through all inodes
            if len(row) == 27: # we only want to check files/dirs
                # print(row)
                num = int(row[i])
                if (num != 0 and num < block_min): # invalid/reserved range checks
                    if (row not in invalid):
                        invalid.append(row)
                elif (num != 0 and num >= block_max):
                    if (row not in invalid):
                        invalid.append(row)
                elif (num > 0 and num < block_min):
                    if (row not in invalid):
                        invalid.append(row)

        if (invalid != []): #not empty
            inconsistent = 2
            for row in invalid: #iterate through each invalid entry and print accordingly
                num = int(row[i])
                msg = block_message(num, block_max, int(row[1]), i, block_size)
                if (i < 24): # direct block pointer
                    start = direct_start(num, block_max)
                else: #indirect block pointer
                    start = indirect_start(num, i-23, block_max)
                print(start + ' ' + msg) # print out corresponding message

    for row in indirect: # iterate through indirect
        num = int(row[5])
        if (num != 0 and (num < block_min or num >= block_max)):
            if (row not in bad):
                bad.append(row)
    for row in bad: # print out bad indirect entries
        inconsistent = 2
        block_no = int(row[5])
        start = indirect_start(block_no, int(row[2]), block_max)
        print(start + ' BLOCK ' + str(block_no) + ' IN INODE ' + str(row[1]) + ' AT OFFSET ' + str(row[3]))


# indirect message for the block consistency check
def indirect_start(block_no: int, indir_level: int, block_max: int) -> str:
    start = ''
    indirect = ''
    start = direct_start(block_no, block_max)
    indirect = indirect_level(indir_level)
    return (start + ' ' + indirect)


#indirect level for each print message
def indirect_level(indir_level: int):
    if (indir_level == 1):
        return 'INDIRECT'
    elif (indir_level == 2):
        return 'DOUBLE INDIRECT'
    else:
        return 'TRIPLE INDIRECT'


#return invalid if out of range, reserved if not
def direct_start(block_no:int, block_max:int) -> str:
    return ('INVALID' if (block_no < 0 or block_no >= block_max) else 'RESERVED')


#return appropriate offset for the direct, single, double, and triple indirect pointers in the inode entries
def block_message(block_no:int, block_max:int, inode_no:int, index:int, block_size:int) -> str:
    msg = ''
    if (index < 24):
        msg = ('BLOCK ' + str(block_no) + ' IN INODE ' + str(inode_no) + ' AT OFFSET ' + str((index-12)))
    elif (index == 24):
        msg = ('BLOCK ' + str(block_no) + ' IN INODE ' + str(inode_no) + ' AT OFFSET 12')
    elif (index == 25):
        msg = ('BLOCK ' + str(block_no) + ' IN INODE ' + str(inode_no) + ' AT OFFSET ' + str(int((block_size/4) +12)))
    else:
        msg = ('BLOCK ' + str(block_no) + ' IN INODE ' + str(inode_no) + ' AT OFFSET ' + str(int((block_size/4)*(block_size/4)+(block_size/4)+12)))
    return msg


#check which blocks have been referenced (second part of block consistency)
def ref_blocks(block_min:int, block_max:int, block_size:int):
    results = list()
    for i in range(block_min, block_max): # go through each possible block
        results = []
        for row in bfree: # get all matching free blocks
            if int(row[1]) == i:
                results.append(row)

        for row in inodes: # get all matching inodes
            if len(row) == 27: # only valid dirs/files
                for j in range(12, 27): # iterate through each block pointer
                    if int(row[j]) == i and row not in results: # add to list if a match
                        results.append(row)

        for row in indirect: # get all matching indirect entries
            if int(row[5]) == i:
                results.append(row)
        count = len(results)
        if (count == 0): # our block was never referenced
            inconsistent = 2
            print('UNREFERENCED BLOCK ' + str(i))
        if (count > 1): # referenced too many times
            inconsistent = 2
            for row in results:
                if (row[0] == 'BFREE'): # if marked on free list, print message
                    print('ALLOCATED BLOCK ' + str(i) + ' ON FREELIST')
                    break
                elif (row[0] == 'INODE'):
                    for x in range(12, 27): # check for multiple refs to same block in a single line
                        if int(row[x]) == i:
                            if (x < 24):
                                start = 'DUPLICATE'
                            else:
                                start = 'DUPLICATE ' + indirect_level(x-23)
                            msg = block_message(i, block_max, int(row[1]), x, block_size)
                            print(start + ' ' + msg)
                            continue
                elif (row[0] == 'INDIRECT'): # check for duplicate ref in indirect entries
                    block_no = int(row[5])
                    start = 'DUPLICATE ' + indirect_level(int(row[2]))
                    print(start + ' BLOCK ' + str(block_no) + ' IN INODE ' + str(row[1]) + ' AT OFFSET ' + str(row[3]))


# see if inode is on free list and allocated o
def inode_alloc(inode_min, inode_max):
    results = list()
    # print(inode_min)
    for i in range(inode_min, inode_max): # go through each unreserved inode
        results = []
        for row in ifree: # see if in free list
            if (int(row[1]) == i):
                results.append(row)

        for row in inodes: # see if in inodes
            if (int(row[1]) == i):
                results.append(row)

        count = len(results)
        if (count == 0): # not on free list
            inconsistent = 2
            print('UNALLOCATED INODE ' + str(i) + ' NOT ON FREELIST')
        # elif (count > 1): # allocated node on free list if count > 1
            # print('ALLOCATED INODE ' + str(i) + ' ON FREELIST')

    for row_x in inodes: #check for double matches
            for row_y in ifree:
                results = []
                if (int(row_x[1]) == int(row_y[1])):
                    results.append(row_x)
                if (len(results) > 0): # if referenced in inodes and freelist
                    inconsistent = 2
                    print('ALLOCATED INODE ' + str(row_x[1]) + ' ON FREELIST')


# see if inode has been linked a different number of times than inode entry indicates
def inode_links():
    for row in inodes:
        results = 0
        inum = int(row[1])
        for entry in dirent: # get total number of direntries that point to this inode
            if (int(entry[3]) == inum):
                results += 1
        if (results != int(row[6])): #if number of directory entries does not equal inode link count
            inconsistent = 2
            print('INODE ' + str(inum) + ' HAS ' + str(results) + ' LINKS BUT LINKCOUNT IS ' + str(row[6]))


# check dirents
def dirent_links(inode_max:int):
    for row in dirent: # iterate through each dirent
        # get parent inode, inode number, name of dirent
        parent, inum, name = int(row[1]), int(row[3]), str(row[6])

        results = 0
        if (inum < 1 or inum > inode_max): # if inode invalid, print
            inconsistent = 2
            print('DIRECTORY INODE ' + str(parent) + ' NAME ' + name + ' INVALID INODE ' + str(inum))
        else:
            for ref in inodes:
                if (int(ref[1]) == inum):
                    results += 1
            if (results == 0): # if inode was on freelist but referenced
                inconsistent=2
                print('DIRECTORY INODE ' + str(parent) + ' NAME ' + name + ' UNALLOCATED INODE ' + str(inum))

        if (name == "'.'"):
            # should link to itself
            if (inum != parent):
                inconsistent=2
                print(f"DIRECTORY INODE {str(parent)} NAME {name} " \
                      f'LINK TO INODE {str(inum)} SHOULD BE {str(parent)}')

        if (name == "'..'"):
            # should link to correct parent directory inode
            # get all other dirents that reference the current directory,
            # except for the current directory dirent '.'
            parents = list()
            for entry in dirent:
                if (int(entry[1]) != parent and int(entry[3]) == parent):
                    parents.append(entry)

            noref = True
            for entry in parents:
                if (str(entry[6]) != "'..'"):
                    noref = False
                    if (inum != int(entry[1])):
                        inconsistent=2
                        print(f"DIRECTORY INODE {str(parent)} NAME {name} " \
                              f'LINK TO INODE {str(inum)} SHOULD BE {str(entry[1])}')
            if (noref and inum != parent):
                # if never referenced, '..' dirent should point to itself
                inconsistent=2
                print(f"DIRECTORY INODE {str(parent)} NAME {name} " \
                    f'LINK TO INODE {str(inum)} SHOULD BE {str(parent)}')


main()
