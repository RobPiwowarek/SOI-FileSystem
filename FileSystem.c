#include <unistd.h>
#include <malloc.h>
#include <stdlib.h>
#include <memory.h>
#include "FileSystem.h"

/* TODO:
 * 4. testy
 * */

int createVirtualFileSystem(SIZE size){
    FILE* file_ptr;
    SIZE system_size;
    SIZE blocks_number;

    file_ptr = fopen("filesystem", "w+b");

    if (!file_ptr){
        printf("Unable to create filesystem\n");
        return 1;
    }

    blocks_number = getRequiredBlocksNumber(size);

    system_size = (SIZE)sizeof(SUPERBLOCK) + (SIZE)MAX_FILE_COUNT*sizeof(INODE) + blocks_number*sizeof(BLOCK) + (SIZE)sizeof(int)*MAX_FILE_COUNT + (SIZE)sizeof(int)*blocks_number;

    // reduce the file length
    truncate("filesystem", system_size);

    // setup super_block
    super_block = (SUPERBLOCK*)malloc(sizeof(SUPERBLOCK));

    if (!super_block){
        printf("Error. Could not allocate memory for super block\n");
        fclose(file_ptr);
        return 6;
    }

    super_block->first_INode = MAX_FILE_COUNT;
    super_block->size = system_size;
    super_block->user_space = BLOCK_SIZE*blocks_number;
    super_block->user_space_in_use = 0;
    super_block->total_blocks_number = blocks_number;

    if (fwrite(super_block, sizeof(SUPERBLOCK), 1, file_ptr) != 1){
         printf("Error. Could not write superblock data\n");

        fclose(file_ptr);
        return 2;
    }

    printf("Successfully created file system\n");

    fclose(file_ptr);
    return 0;
}

int copyFileFromPhysicalDisk(char *file_name) {
    FILE* file_ptr, *vfs_ptr;
    BLOCK* temp_block;
    INODE* temp_inode;
    size_t length;
    unsigned long file_size;
    int i, j, i2, inode_index;
    SIZE blocks;

    printf("file name %s\n", file_name);

    file_ptr = fopen(file_name, "r");
    vfs_ptr = fopen("filesystem", "r+b");

    if (!file_ptr){
        printf("Error. Failed to open file %s\n", file_name);
        return -1;
    }

    if (!vfs_ptr){
        printf("Error. Failed to open file system \n");
        return -3;
    }

    length = strlen(file_name);

    if (length >= MAX_NAME_LENGTH){
        printf("Error. File name too long\n");
        fclose(file_ptr);
        fclose(vfs_ptr);
        return -1;
    }

    if (isFileOnVirtualDisk(file_name, vfs_ptr)==1){
        printf("Error. File of that name is already in file system\n");
        fclose(file_ptr);
        fclose(vfs_ptr);
        return -5;
    }

    file_size = getFileSize(file_ptr);

    if (!isEnoughSpaceLeft(file_size)){
        printf("Not enough space left\n");
        fclose(file_ptr);
        fclose(vfs_ptr);
        return -2;
    }

    // first block

    i = getIndexOfFirstFreeBlock();
    blocks_bitmap[i] = '1';

    // create, update and write inode

    inode_index = getIndexOfFirstFreeInode();
    temp_inode = (INODE*)malloc(sizeof(INODE));
    temp_inode->index_of_first_block = (SIZE) i;

    strcpy(temp_inode->name, file_name);
    temp_inode->name[length] = '\0';

    temp_inode->size = (SIZE)file_size;

    inode_bitmap[inode_index] = '1';

    if (super_block->first_INode > inode_index){
        super_block->first_INode = (SIZE) inode_index;
    }

    fseek(vfs_ptr, getOffsetToInode(inode_index), SEEK_SET);

    if (fwrite(temp_inode, sizeof(INODE), 1, vfs_ptr) != 1){
        printf("Error. Failed to write inode to file system\n");
        free(temp_inode);
        freeAllSystemPointers();
        fclose(file_ptr);
        fclose(vfs_ptr);
        return 62;
    }

    free(temp_inode);

    //

    temp_block = (BLOCK*)calloc(1, sizeof(BLOCK));

    for (j = 0, blocks = getRequiredBlocksNumber(file_size); j < blocks; ++j){

        if (fread(temp_block->data, BLOCK_SIZE, 1,file_ptr) != 1 && !feof(file_ptr)) {
            printf("Error. Failed to read data from file\n");

            free(temp_block);
            freeAllSystemPointers();
            fclose(file_ptr);
            fclose(vfs_ptr);
            return 69;
        }

        i2 = getIndexOfFirstFreeBlock();

        if (j+1 != blocks){
            temp_block->next_block = (SIZE) i2;
        } else {
            temp_block->next_block = super_block->total_blocks_number;
        }

        fseek(vfs_ptr, getOffsetToBlock(i), SEEK_SET);
        if (fwrite(temp_block, sizeof(BLOCK), 1, vfs_ptr) != 1){
            printf("Error. Failed to write block to file system\n");
            freeAllSystemPointers();
            free(temp_block);
            fclose(vfs_ptr);
            fclose(file_ptr);
            return 63;
        }

        if (j+1 != blocks) {
            i = i2;
            blocks_bitmap[i] = '1';
        }
    }

    free(temp_block);

    super_block->user_space_in_use += blocks*BLOCK_SIZE;

    rewind(vfs_ptr);
    saveFileSystem(vfs_ptr);

    freeAllSystemPointers();

    fclose(vfs_ptr);
    fclose(file_ptr);

    printf("Successfully copied file %s from physical disk\n", file_name);

    return 0;
}

int copyFileFromVirtualDisk(char * file_name) {
    SIZE index;
    INODE* temp_inode;
    BLOCK* temp_block;
    SIZE temp_size;
    FILE* vfs_ptr, *file_ptr;
    int *i, j, equal, diff_blocks;

    diff_blocks = 0;
    equal = 0;
    j = 0;

    index = super_block->first_INode;

    vfs_ptr = fopen("filesystem", "rb");

    if (!vfs_ptr){
        printf("Error. Could not open file system\n");
        freeAllSystemPointers();
        return -1;
    }

    temp_inode = (INODE*)malloc(sizeof(INODE));

    do{

        fseek(vfs_ptr, getOffsetToInode(index), SEEK_SET);

        if (fread(temp_inode, sizeof(INODE), 1, vfs_ptr) != 1){
            printf("Error. Failed to read inode\n");
            freeAllSystemPointers();
            fclose(vfs_ptr);
            return 64;
        }

        if (strcmp(temp_inode->name, file_name) == 0){
            equal = 1;
            j = temp_inode->index_of_first_block;
        }

        ++index;

    } while(equal == 0 && index != MAX_FILE_COUNT);

    temp_size = temp_inode->size;

    if (index == MAX_FILE_COUNT && equal == 0){
        printf("File named %s not found on disk\n", file_name);
        freeAllSystemPointers();
        fclose(vfs_ptr);
        return -2;
    }

    file_ptr = fopen(file_name, "w+b");

    truncate(file_name, temp_size);

    if (!file_ptr){
        printf("Error. Could not create %s\n", file_name);
        freeAllSystemPointers();
        fclose(vfs_ptr);
        return -2;
    }

    i = (int*)calloc(1, sizeof(int));

    temp_block = (BLOCK*) malloc(sizeof(BLOCK));

    if (!temp_block){
        printf("Error. Failed to allocate memory for temp_block\n");

        freeAllSystemPointers();
        fclose(file_ptr);
        fclose(vfs_ptr);
        free(i);
    }

    do {
        fseek(vfs_ptr, getOffsetToBlock(j), SEEK_SET);

        fread(temp_block, sizeof(BLOCK), 1, vfs_ptr);

        if (getRequiredBlocksNumber(temp_inode->size)-diff_blocks > 1) {
            if (fwrite(temp_block, BLOCK_SIZE, 1, file_ptr) != 1) {
                printf("Error. Failed to write data to file_ptr\n");
                freeAllSystemPointers();
                fclose(file_ptr);
                fclose(vfs_ptr);
                free(i);
                return 66;
            }
        }
        else{
            if (fwrite(temp_block, temp_inode->size - BLOCK_SIZE*diff_blocks, 1, file_ptr) != 1) {
                printf("Error. Failed to write data to file_ptr\n");
                freeAllSystemPointers();
                fclose(file_ptr);
                fclose(vfs_ptr);
                free(i);
                return 66;
            }

        }

        // index of next block

        j = *i = temp_block->next_block;
        ++diff_blocks;

    } while (*i == 0);

    freeAllSystemPointers();

    fclose(vfs_ptr);
    fclose(file_ptr);

    free(i);
    free(temp_block);
    free(temp_inode);

    printf("Successfully copied file from virtual file system\n");

    return 0;
}

int deleteFileFromVirtualDisk(char * file_name) {
    FILE *vfs_ptr;
    BLOCK* temp_block;
    INODE* temp_inode;
    int i, j;

    if (super_block->first_INode == MAX_FILE_COUNT){
        printf("Error. No files in file system.\n");
        freeAllSystemPointers();
        return -2;
    }

    vfs_ptr = fopen("filesystem", "r+b");

    if (!vfs_ptr){
        printf("Failed to open file system\n");
        freeAllSystemPointers();
        return -1;
    }

    // find index of inode with file

    temp_inode = (INODE*)malloc(sizeof(INODE));

    if (!temp_inode){
        printf("Error. Failed to allocate memory for temp_inode\n");
        freeAllSystemPointers();
        fclose((vfs_ptr));
        return -3;
    }

    printf("filesystem first node: %d\n", super_block->first_INode);

    for (i = super_block->first_INode; i < MAX_FILE_COUNT; ++i){
        fseek(vfs_ptr, getOffsetToInode(i), SEEK_SET);

        if (fread(temp_inode, sizeof(INODE), 1, vfs_ptr) != 1){
            printf("Error. Failed to read to temp_inode\n");
            freeAllSystemPointers();
            fclose(vfs_ptr);
            free(temp_inode);
            return -2;
        }

        if (strcmp(temp_inode->name, file_name)==0) break;

    }

    if (i == MAX_FILE_COUNT){
        printf("No such file in file system\n");
        fclose(vfs_ptr);
        freeAllSystemPointers();
        free(temp_inode);
        return -3;
    }

    inode_bitmap[i] = '\0';

    // find blocks to remove

    temp_block = (BLOCK*)malloc(sizeof(BLOCK));

    for (j = temp_inode->index_of_first_block; j < super_block->total_blocks_number; j = temp_block->next_block) {
        fseek(vfs_ptr, getOffsetToBlock(j), SEEK_SET);

        if (fread(temp_block, sizeof(BLOCK), 1, vfs_ptr) != 1) {
            printf("Error. Could not read to temp_block\n");
            freeAllSystemPointers();
            free(temp_block);
            free(temp_inode);
            fclose(vfs_ptr);
            return -4;
        }

        super_block->user_space_in_use -= sizeof(BLOCK);

        blocks_bitmap[j] = '\0';
    }

    while (i < MAX_FILE_COUNT){
        if (inode_bitmap[i] == '1'){
            super_block->first_INode = (SIZE) i;
            break;
        }

        ++i;
    }

    if (i == MAX_FILE_COUNT){
        super_block->first_INode = MAX_FILE_COUNT;
    }

    rewind(vfs_ptr);
    saveFileSystem(vfs_ptr);
    freeAllSystemPointers();
    free(temp_block);
    free(temp_inode);
    fclose(vfs_ptr);

    printf("File successfully deleted from file system\n");

    return -1;
}

int deleteVirtualDisk() {
    if (unlink("filesystem") == -1){
        printf("Error. Failed to delete file system\n");
        return -1;
    }

    printf("VFS deleted\n");

    return 0;
}

void displayVirtualDiskStatus(){
    int i, free_blocks, free_inodes;

    for (i = 0, free_blocks = 0; i < super_block->total_blocks_number; ++i){
        if (blocks_bitmap[i] == '\0'){
            ++free_blocks;
        }
    }

    for (i = 0, free_inodes = 0; i < MAX_FILE_COUNT; ++i){
        if (inode_bitmap[i] == '\0'){
            ++free_inodes;
        }
    }

    printf("OFFSET    |    SIZE    |    COUNT    |    STATUS    |    TYPE    |\n");
    printf("%10i|%12zu|%13d|          full|  superblock\n", SUPER_BLOCK_OFFSET, sizeof(SUPERBLOCK), 1);
    printf("%10zu|%12zu|%13d|          ----|inode bitmap\n", sizeof(SUPERBLOCK), sizeof(char)*MAX_FILE_COUNT, MAX_FILE_COUNT);
    printf("%10zu|%12zu|%13d|          ----|block bitmap\n", sizeof(SUPERBLOCK)+sizeof(char)*MAX_FILE_COUNT, sizeof(char)*super_block->total_blocks_number, super_block->total_blocks_number);
    printf("%10i|%12zu|%13d|          free|      inodes\n", getOffsetToInode(getIndexOfFirstFreeInode()), free_inodes*sizeof(INODE), free_inodes);
    printf("%10i|%12zu|%13d|          full|      inodes\n", getOffsetToInode(super_block->first_INode), (MAX_FILE_COUNT-free_inodes)*sizeof(INODE), MAX_FILE_COUNT-free_inodes);
    printf("%10i|%12zu|%13d|          free| data blocks\n", getOffsetToBlock(0), sizeof(BLOCK)*free_blocks, free_blocks);
    printf("      ----|%12zu|%13d|          full| data blocks\n\n", sizeof(BLOCK)*(super_block->total_blocks_number-free_blocks), super_block->total_blocks_number-free_blocks);

    freeAllSystemPointers();
}

void displayCatalogue(){
    int i;
    int temp_index;
    FILE* vfs_ptr;
    INODE* temp_inode;

    if (super_block->first_INode == MAX_FILE_COUNT){
        printf("No files in file system.\n");
        freeAllSystemPointers();
        return;
    }

    vfs_ptr = fopen("filesystem", "rb");

    if (!vfs_ptr){
        printf("Error. Failed to open file system\n");
        freeAllSystemPointers();
        return;
    }

    temp_inode = (INODE*)malloc(sizeof(INODE));

    for (i = super_block->first_INode; i < MAX_FILE_COUNT; ++i){
        if (inode_bitmap[i] == '1'){

            fseek(vfs_ptr, getOffsetToInode(i), SEEK_SET);

            fread(temp_inode, sizeof(INODE), 1, vfs_ptr);

            printf("File: %s Size: %d First Block: %d\n", temp_inode->name, temp_inode->size, temp_inode->index_of_first_block);
        }
    }

    free(temp_inode);

    fclose(vfs_ptr);
}

void displayFileSystemInformation(){
    FILE* file_ptr;
    file_ptr = fopen("filesystem", "rb");

    if (!file_ptr){
        printf("Error. Failed to open file system\n");

        freeAllSystemPointers();
        return;
    }

    if (fread(super_block, sizeof(SUPERBLOCK), 1, file_ptr) != 1){
        printf("Error. Failed to read super block from file system\n");

        freeAllSystemPointers();
        fclose(file_ptr);
        return;
    }

    printf("SUPERBLOCK READ\n");
    printf("TOTAL FILESYSTEM SIZE: %d\n", super_block->size);
    printf("TOTAL USER SPACE SIZE: %d\n", super_block->user_space);
    printf("TOTAL BLOCKS:          %d\n", super_block->total_blocks_number);
    printf("BLOCK SIZE:            %d\n", BLOCK_SIZE);
    printf("USER SPACE IN USE:     %d\n", super_block->user_space_in_use);
    printf("FREE SPACE:            %d\n", super_block->user_space-super_block->user_space_in_use);

    freeAllSystemPointers();

    fclose(file_ptr);
}

int main(int argc, char* argv[]) {
    if(argc<2) {
        printf("usage:\t name -(cvmlsrd) [file name]\n");
        return -1;
    }

    switch(argv[1][1]) {
        case 'c' :
            if(argc != 3 || atoi(argv[2]) == 0) {
                return -1;
            }
            return createVirtualFileSystem((SIZE)atoi(argv[2]));
        case 'v' :
            if(argc != 3) {
                return -1;
            }
            loadFileSystem();

            return copyFileFromPhysicalDisk(argv[2]);

        case 'm' :
            if(argc != 3) {
                return -1;
            }
            loadFileSystem();

            return copyFileFromVirtualDisk(argv[2]);
        case 'l' :
            loadFileSystem();

            displayCatalogue();
            return 0;
        case 's' :
            loadFileSystem();

            displayFileSystemInformation();
            return 0;
        case 'i':
            loadFileSystem();

            displayVirtualDiskStatus();
            return 0;
        case 'd' :
            loadFileSystem();

            return deleteFileFromVirtualDisk(argv[2]);
        case 'r' :
            if(argc != 3) {
                return -1;
            }

            return deleteVirtualDisk();
        default :
            freeAllSystemPointers();
            return -1;
    }

}

SIZE getRequiredBlocksNumber(unsigned long file_size){

    if (file_size % BLOCK_SIZE == 0) return (SIZE) (file_size / BLOCK_SIZE);
    else{
        return (SIZE) (file_size / (BLOCK_SIZE)) + 1;
    }

}

int isEnoughSpaceLeft(unsigned long size){
    int blocks;
    int i;

    for (i = 0, blocks = 0; i < super_block->total_blocks_number; ++i){
        if (blocks_bitmap[i] == '\0'){
            ++blocks;
        }
    }

    if (blocks >= getRequiredBlocksNumber(size))
        return 1;

    return 0;
}

int isFreeInodeLeft(){
    int i;

    for (i = 0; i < MAX_FILE_COUNT; ++i){
        if (inode_bitmap[i] == '\0'){
            return 1;
        }
    }

    return 0;
}

SIZE getOffsetToInode(int index){
    return index*sizeof(INODE) + sizeof(SUPERBLOCK) + sizeof(char)*MAX_FILE_COUNT + sizeof(char)*super_block->total_blocks_number;
}

SIZE getOffsetToBlock(int index){
    return index*sizeof(BLOCK) + MAX_FILE_COUNT*sizeof(INODE) + sizeof(SUPERBLOCK) + sizeof(char)*MAX_FILE_COUNT + sizeof(char)*super_block->total_blocks_number;
}

int getIndexOfFirstFreeInode(){
    int i;

    for (i = 0; i < MAX_FILE_COUNT; ++i){
        if (inode_bitmap[i] == '\0'){
            return i;
        }
    }
}

int getIndexOfFirstFreeBlock(){
    int i;

    for (i = 0; i < super_block->total_blocks_number; ++i){
        if (blocks_bitmap[i] == '\0'){
            return i;
        }
    }

    return 0;
}

unsigned long getFileSize(FILE* file){
    unsigned long size;
    fseek(file, 0, SEEK_END);
    size = (unsigned long) ftell(file);
    rewind(file);

    return size;
}

int isFileOnVirtualDisk(char* filename, FILE* vfs_ptr){
    int i;
    INODE * temp_inode;

    if (super_block->first_INode == MAX_FILE_COUNT){
        return 0;
    }

    temp_inode = (INODE*)malloc(sizeof(INODE));

    for (i = super_block->first_INode; i < MAX_FILE_COUNT; ++i){

        if (inode_bitmap[i] == '\0'){
            continue;
        }

        fseek(vfs_ptr, getOffsetToInode(i), SEEK_SET);

        fread(temp_inode, sizeof(INODE), 1, vfs_ptr);

        if (strcmp(temp_inode->name, filename) == 0){
            rewind(vfs_ptr);
            free(temp_inode);
            return 1;
        }

    }

    free(temp_inode);

    return 0;
}

void loadFileSystem(){
    FILE* vfs_ptr;

    vfs_ptr = fopen("filesystem", "r+b");

    if (!vfs_ptr){
        printf("Error. Could not find filesystem file\n");
        exit(EXIT_SUCCESS);
    }

    super_block = (SUPERBLOCK*)malloc(sizeof(SUPERBLOCK));

    if (!super_block){
        printf("Error. Failed to allocate memory for superblock\n");
        fclose(vfs_ptr);
        exit(EXIT_SUCCESS);
    }

    fseek(vfs_ptr, SUPER_BLOCK_OFFSET, SEEK_SET);

    if (fread(super_block, sizeof(SUPERBLOCK), 1, vfs_ptr) != 1){
        printf("Error. Failed to read super_block\n");
        fclose(vfs_ptr);
        exit(EXIT_SUCCESS);
    }

    blocks_bitmap = (char*)malloc(sizeof(char)*super_block->total_blocks_number);

    if (!blocks_bitmap){
        printf("Error. Failed to allocate memory for block bitmap\n");
        free(super_block);
        fclose(vfs_ptr);
        exit(EXIT_SUCCESS);
    }

    if (fread(inode_bitmap, sizeof(char), MAX_FILE_COUNT, vfs_ptr) != MAX_FILE_COUNT){
        printf("Error. Failed to read inode bitmap\n");
        free(super_block);
        free(blocks_bitmap);
        fclose(vfs_ptr);
        exit(EXIT_SUCCESS);
    }

    if (fread(blocks_bitmap, sizeof(char), super_block->total_blocks_number, vfs_ptr) != super_block->total_blocks_number){
        printf("Error. Failed to read block bitmap\n");
        free(super_block);
        free(blocks_bitmap);
        fclose(vfs_ptr);
        exit(EXIT_SUCCESS);
    }

    fclose(vfs_ptr);
}

void saveFileSystem(FILE *vfs_ptr){

    if (fwrite(super_block, sizeof(SUPERBLOCK), 1, vfs_ptr) != 1){
        printf("Error. Could not write super_block\n");
        fclose(vfs_ptr);
        exit(EXIT_SUCCESS);
    }

    if (fwrite(inode_bitmap, MAX_FILE_COUNT*sizeof(char), 1, vfs_ptr) != 1){
        printf("Error. Could not write inode bitmap\n");
        fclose(vfs_ptr);
        exit(EXIT_SUCCESS);
    }

    if (fwrite(blocks_bitmap, super_block->total_blocks_number*sizeof(char), 1, vfs_ptr) != 1){
        printf("Error. Could not write block bitmap\n");
        fclose(vfs_ptr);
        exit(EXIT_SUCCESS);

    }

}

void freeAllSystemPointers(){
    free(super_block);
    free(blocks_bitmap);
}