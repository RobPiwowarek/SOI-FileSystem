#include <unistd.h>
#include <malloc.h>
#include <stdlib.h>
#include <memory.h>
#include "FileSystem.h"

/* TODO:
 *
 * 1. user size = 10kb tzn ze mozna wsadzic 10kb plik
 * 2. usuniecie zbednych rewindow
 * 3. usuniecie wielokrotnych mallocow
 * 4. testy
 * 5. zapis bitmap na dysku
 * */


int createVirtualFileSystem(SIZE size){
    FILE* file_ptr;
    SIZE system_size;
    SIZE blocks_number;

    file_ptr = fopen("filesystem", "wb");

    if (!file_ptr){
        printf("Unable to create filesystem\n");
        return 1;
    }

    blocks_number = size/BLOCK_SIZE*sizeof(BLOCK);

    system_size = (SIZE)sizeof(SUPERBLOCK) + (SIZE)MAX_FILE_COUNT*sizeof(INODE) + blocks_number;

    // reduce the file length
    truncate("filesystem", system_size);

    // setup super_block
    super_block = (SUPERBLOCK*)malloc(sizeof(SUPERBLOCK));

    super_block->first_INode = 0;
    super_block->size = system_size;
    super_block->user_space = size;
    super_block->user_space_in_use = 0;

    //

    setValuesOfInodeBitmapToZero();

    // setup block bitmap
    blocks_bitmap = (int*)calloc(blocks_number, sizeof(int));

    if (fwrite(super_block, sizeof(SUPERBLOCK), 1, file_ptr) != 1){
         printf("Error. Could not write superblock data\n");

        if (blocks_bitmap)
            free(blocks_bitmap);

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
    vfs_ptr = fopen("filesystem", "w+b");

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

    i = getIndexOfFirstFreeBlock();
    blocks_bitmap[i] = 1;

    //create, update and write inode

    inode_index = getIndexOfFirstFreeInode();
    temp_inode = (INODE*)malloc(sizeof(INODE));
    temp_inode->index_of_first_block = (SIZE) i;

    strcpy(temp_inode->name, file_name);
    temp_inode->name[length] = '\0';

    temp_inode->size = (SIZE)file_size;

    inode_bitmap[inode_index] = 1;
    fseek(vfs_ptr, getOffsetToInode(inode_index), SEEK_SET);
    fwrite(temp_inode, sizeof(INODE), 1, vfs_ptr);

    free(temp_inode);
    rewind(vfs_ptr);

    //

    for (j = 0, blocks = getRequiredBlocksNumber(file_size); j < blocks; ++j){
        temp_block = (BLOCK*)calloc(1, sizeof(BLOCK));
        fread(temp_block->data, BLOCK_SIZE-2*sizeof(SIZE), 1, file_ptr);

        i2 = getIndexOfFirstFreeBlock();

        if (j+1 != blocks){
            temp_block->next_block = (SIZE) i2;
            temp_block->is_last = 0;
        } else {
            temp_block->is_last = 1;
        }

        fseek(vfs_ptr, getOffsetToBlock(i), SEEK_SET);
        fwrite(temp_block, sizeof(BLOCK), 1, vfs_ptr);

        if (j+1 != blocks) {
            i = i2;
            blocks_bitmap[i] = 1;
        }


        rewind(vfs_ptr);
        free(temp_block);
    }

    super_block->user_space_in_use += blocks*sizeof(BLOCK);
    fwrite(super_block, sizeof(SUPERBLOCK), 1, vfs_ptr);

    fclose(vfs_ptr);
    fclose(file_ptr);
    return 0;
}

int copyFileFromVirtualDisk(char * file_name) {
    SIZE index;
    INODE* temp_inode;
    FILE* vfs_ptr, *file_ptr;
    int *i, j, equal;

    equal = 0;
    j = 0;

    index = super_block->first_INode;

    vfs_ptr = fopen("filesystem", "rb");

    if (!vfs_ptr){
        printf("Error. Could not open file system\n");
        return -1;
    }

    do{
        temp_inode = (INODE*)malloc(sizeof(INODE));

        fseek(vfs_ptr, getOffsetToInode(index), SEEK_SET);

        fread(temp_inode, sizeof(INODE), 1, vfs_ptr);

        if (strcmp(temp_inode->name, file_name) == 0){
            equal = 1;
            j = temp_inode->index_of_first_block;
        }

        ++index;

        rewind(vfs_ptr);

        free(temp_inode);
    } while(equal == 0 && index != MAX_FILE_COUNT);

    if (index == MAX_FILE_COUNT && equal == 0){
        printf("File named %s not found on disk\n", file_name);
        fclose(vfs_ptr);
        return -2;
    }

    file_ptr = fopen(file_name, "w+b");

    if (!file_ptr){
        printf("Error. Could not create %s\n", file_name);
        fclose(vfs_ptr);
        return -2;
    }

    i = (int*)calloc(1, sizeof(int));

    do {
        fseek(vfs_ptr, getOffsetToBlock(j), SEEK_SET);

        fwrite(file_ptr, sizeof(BLOCK)-2*sizeof(SIZE), 1, vfs_ptr);

        // index of next block

        fwrite(i, sizeof(SIZE), 1, vfs_ptr);

        j = *i;

        //

        fwrite(i, sizeof(SIZE), 1, vfs_ptr); // is last?

        rewind(vfs_ptr);

    } while (*i == 0);

    fclose(vfs_ptr);
    fclose(file_ptr);

    return 0;
}

int deleteFileFromVirtualDisk(char * file_name) {
    FILE *vfs_ptr;
    BLOCK* temp_block;
    INODE* temp_inode;
    int i, j;

    vfs_ptr = fopen("filesystem", "rb");

    if (!vfs_ptr){
        printf("Failed to open file system\n");
        return -1;
    }

    for (i = 0; i < MAX_FILE_COUNT; ++i){
        if (inode_bitmap[i] == 0)
            continue;

        fseek(vfs_ptr, getOffsetToInode(i), SEEK_SET);

        temp_inode = (INODE*)malloc(sizeof(INODE));

        if (strcmp(temp_inode->name, file_name) == 0){
            temp_block = (BLOCK*)malloc(sizeof(BLOCK));

            j = temp_inode->index_of_first_block;

            do {
                fseek(vfs_ptr, getOffsetToBlock(j), SEEK_SET);

                fread(temp_block, sizeof(BLOCK), 1, vfs_ptr);

                blocks_bitmap[j] = 0;

                j = temp_block->next_block;

            } while (temp_block->is_last != 1);

            fclose(vfs_ptr);
            free(temp_inode);
            free(temp_block);
            return 0;
        }

        fread(temp_inode, sizeof(INODE), 1, vfs_ptr);

        free(temp_inode);
    }

    return -1;
}

int deleteVirtualDisk() {
    if (unlink("filesystem") == -1){
        printf("Error. Failed to delete file system\n");
        return -1;
    }

    return 0;
}

void displayCatalogue(){
    int i;
    int temp_index;
    FILE* vfs_ptr;
    INODE* temp_inode;

    vfs_ptr = fopen("filesystem", "rb");

    if (!vfs_ptr){
        printf("Error. Failed to open file system\n");
        return;
    }

    for (i = 0; i < MAX_FILE_COUNT; ++i){
        if (inode_bitmap[i] == 1){
            temp_inode = (INODE*)malloc(sizeof(INODE));

            fseek(vfs_ptr, getOffsetToInode(i), SEEK_SET);

            fread(temp_inode, sizeof(INODE), 1, vfs_ptr);

            printf("File: %s Size: %d First Bloc k: %d\n", temp_inode->name, temp_inode->size, temp_inode->index_of_first_block);

            rewind(vfs_ptr);

            free(temp_inode);
        }

    }

    fclose(vfs_ptr);
}

void displayFileSystemInformation(){
    FILE* file_ptr;
    file_ptr = fopen("filesystem", "rb");

    super_block = (SUPERBLOCK*)malloc(sizeof(SUPERBLOCK));

    if (!file_ptr){
        printf("Error. Failed to open file system\n");

        if (super_block)
            free(super_block);

        return;
    }

    //fseek(file_ptr, 0, SEEK_SET);

    if (fread(super_block, sizeof(SUPERBLOCK), 1, file_ptr) != 1){
        printf("Error. Failed to read super block from file system\n");
        return;
    }

    printf("SUPERBLOCK READ\n");
    printf("TOTAL FILESYSTEM SIZE: %d\n", super_block->size);
    printf("TOTAL USER SPACE SIZE: %d\n", super_block->user_space);
    printf("USER SPACE IN USE:     %d\n", super_block->user_space_in_use);
    printf("FREE SPACE:            %d\n", super_block->user_space-super_block->user_space_in_use);

    fclose(file_ptr);
}

void setValuesOfInodeBitmapToZero(){
    int i;

    for (i = 0; i < MAX_FILE_COUNT; ++i){
        inode_bitmap[i]=0;
    }
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
            copyFileFromPhysicalDisk(argv[2]);
            displayCatalogue();

        case 'm' :
            if(argc != 3) {
                return -1;
            }
            return copyFileFromVirtualDisk(argv[2]);
        case 'l' :
            displayCatalogue();
            return 0;
        case 's' :
            displayFileSystemInformation();
            return 0;
        case 'd' :
            return deleteFileFromVirtualDisk(argv[2]);
        case 'r' :
            if(argc != 3) {
                return -1;
            }

            return deleteVirtualDisk();
        default :
            return -1;
    }

}

SIZE getRequiredBlocksNumber(unsigned long file_size){
    return (SIZE) (file_size / (BLOCK_SIZE - 4));
}

int isEnoughSpaceLeft(unsigned long size){
    if ((size/(BLOCK_SIZE-2*sizeof(SIZE))) < ((super_block->user_space-super_block->user_space_in_use)/(BLOCK_SIZE-2*sizeof(SIZE))))
        return 1;

    return 0;
}

int isFreeInodeLeft(){
    int i;

    for (i = 0; i < MAX_FILE_COUNT; ++i){
        if (inode_bitmap[i] == 0){
            return 1;
        }
    }

    return 0;
}

SIZE getOffsetToInode(int index){
    return index*sizeof(INODE) + sizeof(SUPERBLOCK);
}

SIZE getOffsetToBlock(int index){
    return index*sizeof(BLOCK) + MAX_FILE_COUNT*sizeof(INODE) + sizeof(SUPERBLOCK);
}

int getIndexOfFirstFreeInode(){
    int i;

    for (i = 0; i < MAX_FILE_COUNT; ++i){
        if (inode_bitmap[i] == 0){
            return i*sizeof(INODE) + sizeof(SUPERBLOCK);
        }
    }
}

int getIndexOfFirstFreeBlock(){
    int i;

    for (i = 0; i < super_block->size/BLOCK_SIZE * sizeof(BLOCK); ++i){
        if (blocks_bitmap[i] == 0){
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

    temp_inode = (INODE*)malloc(sizeof(INODE));

    for (i = super_block->first_INode; i < MAX_FILE_COUNT; ++i){

        if (inode_bitmap[i] == 0){
            continue;
        }

        fseek(vfs_ptr, getOffsetToInode(i), SEEK_SET);

        fread(temp_inode, sizeof(INODE), 1, vfs_ptr);

        if (strcmp(temp_inode->name, filename) == 0){
            rewind(vfs_ptr);
            free(temp_inode);
            return 1;
        }

        rewind(vfs_ptr);
    }

    free(temp_inode);

    return 0;
}