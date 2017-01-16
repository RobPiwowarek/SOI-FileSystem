#ifndef FILESYSTEM_FILESYSTEM_H
#define FILESYSTEM_FILESYSTEM_H

#include <glob.h>
#include <stdio.h>
#include <stdint.h>

#define MAX_FILE_COUNT 100
#define MAX_NAME_LENGTH 30
#define BLOCK_SIZE 4096
#define SUPER_BLOCK_OFFSET 0
#define INODES_OFFSET sizeof(SUPERBLOCK);
#define BLOCKS_OFFSET 2048*sizeof(INODE);

typedef uint32_t SIZE; // 32 bit == 4 bajt

// BLOCKSIZE + 4 + 4 = 5004
typedef struct block{
    char data[BLOCK_SIZE];
    SIZE next_block;
} BLOCK;

// MAX_NAME_LENGTH + 4 + 4 = 38
typedef struct INode{
    char name[MAX_NAME_LENGTH];
    SIZE size; // total file size
    SIZE index_of_first_block;
} INODE;

// 4 + 4 + 4 + 4 + 4 = 20
typedef struct SuperBlock {
    SIZE size; // system size
    SIZE user_space_in_use;
    SIZE user_space;
    SIZE first_INode; // first inode index
    SIZE total_blocks_number;
} SUPERBLOCK;

/*
 * FILE SYSTEM VARIABLES
 */

SUPERBLOCK* super_block;

char inode_bitmap[MAX_FILE_COUNT];
char *blocks_bitmap;

/*
 * FILE SYSTEM FUNCTIONALITY/API
 */

int createVirtualFileSystem(SIZE size); // size - number of bytes

int copyFileFromPhysicalDisk(char *file_name);

int copyFileFromVirtualDisk(char *file_name);

void displayCatalogue();

void displayFileSystemInformation();

void displayVirtualDiskStatus();

int deleteFileFromVirtualDisk(char * file_name);

int deleteVirtualDisk();

/*
 * HELPERS
 */

int isFileOnVirtualDisk(char *filename, FILE* opened_vfs_ptr);

void setValuesOfInodeBitmapToZero();

unsigned long getFileSize(FILE* file);

SIZE getRequiredBlocksNumber(unsigned long file_size);

int isFreeInodeLeft();

int isEnoughSpaceLeft(unsigned long size);

SIZE getOffsetToInode(int index);

SIZE getOffsetToBlock(int index);

int getIndexOfFirstFreeInode();

int getIndexOfFirstFreeBlock();

void loadFileSystem();

void saveFileSystem(FILE *vfs_ptr);

void freeAllSystemPointers();

#endif //FILESYSTEM_FILESYSTEM_H
