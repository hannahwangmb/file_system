#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>


struct __attribute__((__packed__)) dir_entry_timedate_t {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

struct __attribute__((__packed__)) dir_entry_t {
    uint8_t status;
    uint32_t starting_block;
    uint32_t block_count;
    uint32_t size;
    struct dir_entry_timedate_t create_time;
    struct dir_entry_timedate_t modify_time;
    uint8_t filename[31];
    uint8_t unused[6];
};

struct __attribute__((__packed__)) SuperBlock {
    uint16_t block_size;
    uint32_t block_count;
    uint32_t fat_starts;
    uint32_t fat_blocks;
    uint32_t root_dir_starts;
    uint32_t root_dir_blocks;
};

void updateFileContent(char* file, int block_size, int block_start, int block_count, uint32_t* fatPtr, char* content, int content_size) {
    uint32_t fatEntry = block_start;
    int content_index = 0;
    int file_size = content_size;

    for (uint32_t j = 0; j < block_count && content_index < file_size; j++) {
        // Get a pointer to the current block
        void* block = file + fatEntry * block_size;
        // Determine how much to write to the current block
        int write_size = block_size;
        if (file_size - content_index < block_size){
            write_size = file_size - content_index;
        }
            
        // Copy the content to the block
        memcpy(block, content + content_index, write_size);

        // Update the FAT entry for the next block
        if (j < block_count - 1) {
            *(fatPtr + fatEntry) = htonl(fatEntry + 1); // Point to the next block
        } else {
            *(fatPtr + fatEntry) = htonl(0xFFFFFFFF); // Last block of the file
        }

        // Update the remaining content size and index
        content_size -= write_size;
        content_index += write_size;

        // Check the FAT entry for the next block
        fatEntry = ntohl(*(fatPtr + fatEntry));

        // Break if the FAT entry is invalid or indicates the end of the file
        if (ntohl(fatEntry) > 0xFFFFFF00) {
            break;
        }
    }
}


void createNewFile(const char* fileToCopy, const char* filename, char* file, int block_size, int block_start, int block_count, uint32_t* fatPtr, int newFileSize, int fat_starts, int fat_blocks) {
    // Find an empty entry in the directory
    int emptyEntryIndex = -1;
    struct dir_entry_t* dirPtr = (struct dir_entry_t*)(file + block_start * block_size);
    struct dir_entry_timedate_t original_create_time;
    int file_exists = 0;
    
    // Loop through the directory entries
    for (int i = 0; i < block_count*block_size/sizeof(struct dir_entry_t); i++) {
        // check if the file already exists
        if (strcasecmp((const char*)dirPtr[i].filename, filename) == 0) {
        emptyEntryIndex = i;
        file_exists = 1;
        original_create_time = dirPtr[emptyEntryIndex].create_time;
        break;
        }
        // check if the entry is empty
        if (dirPtr[i].status == 0x00) {
            emptyEntryIndex = i;
            break;
        }
    }

    if (emptyEntryIndex == -1) {
        printf("Error: No empty entry in the directory.\n");
        exit(EXIT_FAILURE);
    }

    // Find unused sectors in the FAT
    int unusedBlocks = 0;
    int currentBlock = -1;
    if (file_exists == 1){
        currentBlock = ntohl(dirPtr[emptyEntryIndex].starting_block);
        // free the old blocks
        while (currentBlock != 0xFFFFFFFF) {
            uint32_t nextBlock = ntohl(*(fatPtr + currentBlock));
            *(fatPtr + currentBlock) = htonl(0x00000000);
            currentBlock = nextBlock;
        }
        *(fatPtr + currentBlock) = htonl(0x00000000);// free the last block

    }
    for (int i = 0; i < fat_blocks * block_size / sizeof(uint32_t); i++) {
        if (fatPtr[i] == 0x00000000) {
            if (currentBlock == -1) {
                currentBlock = i;
            }
            unusedBlocks++;
        }
    }
    
    if (unusedBlocks < newFileSize / block_size + 1) {
        printf("Error: Not enough space on disk for the file.\n");
        exit(EXIT_FAILURE);
    }

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    // Create a new entry for the file
    struct dir_entry_t newFileEntry;
    memset(&newFileEntry, 0, sizeof(struct dir_entry_t));
    newFileEntry.status = 0x03; // Assume it's a file 
    newFileEntry.starting_block = 0; // To be updated later
    newFileEntry.block_count = 0;    // To be updated later
    newFileEntry.size = htonl(newFileSize);

    newFileEntry.modify_time.year = htons(tm.tm_year + 1900);
    newFileEntry.modify_time.month = tm.tm_mon + 1;
    newFileEntry.modify_time.day = tm.tm_mday;
    newFileEntry.modify_time.hour = tm.tm_hour;
    newFileEntry.modify_time.minute = tm.tm_min;
    newFileEntry.modify_time.second = tm.tm_sec;
    if (file_exists == 0){
        newFileEntry.create_time = newFileEntry.modify_time;
    }
    else{
        newFileEntry.create_time = original_create_time;
    }

    strncpy((char*)newFileEntry.filename, filename, 31);
    
    // Write the new entry back to the directory
    dirPtr[emptyEntryIndex] = newFileEntry;

    // Update the starting block and block count in the directory entry
    dirPtr[emptyEntryIndex].starting_block = htonl(currentBlock);
    dirPtr[emptyEntryIndex].block_count = htonl(newFileSize / block_size + 1);

    // Get the content of the file
    FILE* linuxFileForContent = fopen(fileToCopy, "r");
    if (!linuxFileForContent) {
        printf("File not found.\n");
        exit(EXIT_FAILURE);
    }

    // Allocate memory for the content
    char* content = (char*)malloc(newFileSize);
    if (!content) {
        perror("malloc");
        fclose(linuxFileForContent);
        exit(EXIT_FAILURE);
    }

    // Read the content from the file
    fread(content, 1, newFileSize, linuxFileForContent);

    // Close the file
    fclose(linuxFileForContent);

    // Write the content of the file to the FAT blocks
    updateFileContent(file, block_size, currentBlock, newFileSize / block_size + 1, fatPtr, content, newFileSize);

    // Free the allocated memory
    free(content);

}

int createDirectories(char* file, int block_size, int block_start, int block_count, uint32_t* fatPtr, const char* dirName, int fat_starts, int fat_blocks) {
    // Find an empty entry in the directory
    int emptyEntryIndex = -1;
    struct dir_entry_t* dirPtr = (struct dir_entry_t*)(file + block_start * block_size);

    // Loop through the directory entries
    for (int i = 0; i < block_count*block_size/sizeof(struct dir_entry_t); i++) {
        if (dirPtr[i].status == 0x00) {
            emptyEntryIndex = i;
            break;
        }
    }

    if (emptyEntryIndex == -1) {
        printf("Error: No empty entry in the directory.\n");
        exit(EXIT_FAILURE);
    }

    // Find an unused block in the FAT for the new directory
    int newDirBlock = -1;
    for (int i = 0; i < fat_blocks * block_size / sizeof(uint32_t); i++) {
        if (fatPtr[i] == 0x00000000) {
            newDirBlock = i;
            break;
        }
    }

    if (newDirBlock == -1) {
        printf("Error: No unused blocks in the FAT.\n");
        exit(EXIT_FAILURE);
    }

    // time
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    // Create a new entry for the directory
    struct dir_entry_t newDirEntry;
    memset(&newDirEntry, 0, sizeof(struct dir_entry_t));
    newDirEntry.status = 0x05; // Directory
    newDirEntry.starting_block = htonl(newDirBlock); // New directory starts at newDirBlock
    newDirEntry.block_count = htonl(1); // New directory has 1 block
    newDirEntry.size = htonl(block_size*block_count);

    newDirEntry.modify_time.year = htons(tm.tm_year + 1900);
    newDirEntry.modify_time.month = tm.tm_mon + 1;
    newDirEntry.modify_time.day = tm.tm_mday;
    newDirEntry.modify_time.hour = tm.tm_hour;
    newDirEntry.modify_time.minute = tm.tm_min;
    newDirEntry.modify_time.second = tm.tm_sec;
    newDirEntry.create_time = newDirEntry.modify_time;

    strncpy((char*)newDirEntry.filename, dirName, 31);

    // Write the new entry back to the directory
    dirPtr[emptyEntryIndex] = newDirEntry;

    // Update the FAT entry for the new directory
    fatPtr[newDirBlock] = htonl(0xFFFFFFFF);

    return ntohl(newDirEntry.starting_block);
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Usage: %s <file_system_image> <source_file> <dest_path(optional)/filename>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* fileSystemImage = argv[1];
    char* fileToCopy = argv[2];
    char* destinationPath = argv[3];

    // Open the file system image
    int fd = open(fileSystemImage, O_RDWR);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    struct stat buffer;
    fstat(fd, &buffer);
    int size = buffer.st_size;

    // Map the file system image into memory
    char* file = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (file == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // Read super block information
    struct SuperBlock superBlock;

    superBlock.block_size = ntohs(*((uint16_t*)(file + 8)));
    superBlock.block_count = ntohl(*((uint32_t*)(file + 10)));
    superBlock.fat_starts = ntohl(*((uint32_t*)(file + 14)));
    superBlock.fat_blocks = ntohl(*((uint32_t*)(file + 18)));
    superBlock.root_dir_starts = ntohl(*((uint32_t*)(file + 22)));
    superBlock.root_dir_blocks = ntohl(*((uint32_t*)(file + 26)));

    // Read FAT information
    uint32_t* fatPtr = (uint32_t*)(file + superBlock.fat_starts * superBlock.block_size);

    // Check if the specified file exists in the current Linux directory
    FILE* linuxFile = fopen(fileToCopy, "r");
    if (!linuxFile) {
        printf("File not found.\n");
        exit(EXIT_FAILURE);
    }
    // Get the size of the file if it exists
    fseek(linuxFile, 0, SEEK_END);
    int newFileSize = ftell(linuxFile);
    fclose(linuxFile);

    // Check if the specified destination path exists in the FAT image
    char* token = (char*)strtok(destinationPath, "/");
    int block_start = superBlock.root_dir_starts;
    int block_count = superBlock.root_dir_blocks;
    char* filename = NULL;

    while (token != NULL) {
        char* nextToken = strtok(NULL, "/");
        if (nextToken == NULL) {
            // Assume the last token is the filename
            // file name should be less than or equal 30 characters and end with a null terminator
            if (strlen(token) > 30) {
                printf("Error: File name should be less than or equal 30 characters.\n");
                exit(EXIT_FAILURE);
            }
            // Valid characters are upper and lower case letters (a-z, A-Z), digits (0-9) and the underscore character (_).
            // except for the extension, which can have a period (.) as well.
            char* extension = strrchr(token, '.');
            int length;
            // check if the file name is valid
            if (extension != NULL){
                length = extension - token;
            }
            else{
                length = strlen(token);
            }
            for (int i = 0; i < length; i++) {
                if (!((token[i] >= 'a' && token[i] <= 'z') || (token[i] >= 'A' && token[i] <= 'Z') || (token[i] >= '0' && token[i] <= '9') || token[i] == '_')) {
                    printf("Error: File name should only contain upper and lower case letters (a-z, A-Z), digits (0-9) and the underscore character (_).\n");
                    exit(EXIT_FAILURE);
                }
            }
            filename = token;
            break;
        }
        struct dir_entry_t* dirPtr = (struct dir_entry_t*)(file + block_start * superBlock.block_size);
        int found = 0;

        for (int i = 0; i < block_count * superBlock.block_size / sizeof(struct dir_entry_t); i++) {
            struct dir_entry_t dirEntry = dirPtr[i];
            dirEntry.size = ntohl(dirEntry.size);
            dirEntry.starting_block = ntohl(dirEntry.starting_block);
            dirEntry.block_count = ntohl(dirEntry.block_count);
            dirEntry.create_time.year = ntohs(dirEntry.create_time.year);
            dirEntry.modify_time.year = ntohs(dirEntry.modify_time.year);
            // Check if the entry is a existing directory
            if (dirEntry.status == 0x05 && strcasecmp((const char*)dirEntry.filename, token) == 0) {
                block_start = dirEntry.starting_block;
                block_count = dirEntry.block_count;
                found = 1;
                break;
            } 
        }
        if (found == 0) {
            // Create a new directory entry in the given path
            block_start=createDirectories(file, superBlock.block_size, block_start, block_count, fatPtr, token, superBlock.fat_starts, superBlock.fat_blocks);
            block_count = 1;

        }

        token = nextToken;
    }

    // Create a new file entry in the given path
    createNewFile(fileToCopy, filename, file, superBlock.block_size, block_start, block_count, fatPtr, newFileSize, superBlock.fat_starts, superBlock.fat_blocks);

    // Unmap the file
    msync(file, size, MS_SYNC);

    munmap(file, size);

    // Close the file
    close(fd);

    return 0;
}