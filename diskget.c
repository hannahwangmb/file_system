#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <string.h>
#include <ctype.h>

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

struct FatInfo {
    int free_blocks;
    int reserved_blocks;
    int allocated_blocks;
};

void printFileContent(const char* output_filename, char* file, int file_size, int block_size, int block_start, int block_count, uint32_t* fatPtr) {
    // Open the file using open system call
    int fd = open(output_filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    uint32_t fatEntry = block_start;
    for (uint32_t j = 0; j < block_count && file_size > 0; j++) {
        // Get a pointer to the current block
        void* block = file + fatEntry * block_size;

        // Determine how much to write from the current block
        int write_size = (file_size < block_size) ? file_size : block_size;

        // Use write system call to write to the file
        if (write(fd, block, write_size) == -1) {
            perror("write");
            close(fd);
            exit(EXIT_FAILURE);
        }

        // Update the remaining file size
        file_size -= write_size;

        // Check the FAT entry for the next block
        fatEntry = ntohl(*(fatPtr + fatEntry));

        // Break if the FAT entry is invalid or indicates the end of the file
        if (fatEntry > 0xFFFFFF00) {
            break;
        }
    }

    // Close the file
    close(fd);
}


void toUpperCase(char* str) {
    for (int i = 0; str[i]; i++) {
        str[i] = toupper(str[i]);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Usage: %s <file_system_image> </subdir1/subdir2/.../source_file> <output_filename>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char* output_filename = argv[3];
    // Open the file system image
    int fd = open(argv[1], O_RDWR);
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


    char* subDir = argv[2];
    char* token = (char*)strtok(subDir, "/");
    int block_start = superBlock.root_dir_starts;
    int block_count = superBlock.root_dir_blocks;
    int file_size = 0;

    while (token != NULL) {
        struct dir_entry_t* dirPtr = (struct dir_entry_t*)(file + block_start * superBlock.block_size);
        int found = 0;
        //toUpperCase(token); // Convert the token to uppercase
        for (int i = 0; i < block_count * superBlock.block_size / sizeof(struct dir_entry_t); i++) {
            struct dir_entry_t dirEntry = *dirPtr; // Create a copy of the data
            dirEntry.size = ntohl(dirEntry.size);
            dirEntry.starting_block = ntohl(dirEntry.starting_block);
            dirEntry.block_count = ntohl(dirEntry.block_count);
            dirEntry.create_time.year = ntohs(dirEntry.create_time.year);
            dirEntry.modify_time.year = ntohs(dirEntry.modify_time.year);
            // Check if the filename matches after converting both to uppercase
            if (dirEntry.status == 0x05 && strcmp((const char*)dirEntry.filename, token) == 0) {
                block_start = dirEntry.starting_block;
                block_count = dirEntry.block_count;
                found = 1;
                break;
            } else if (dirEntry.status == 0x03 && strcmp((const char*)dirEntry.filename, token) == 0) {
                // If it's the last token, print the content of the specified file
                file_size = dirEntry.size;
                block_start = dirEntry.starting_block;
                block_count = dirEntry.block_count;
                if (strtok(NULL, "/") == NULL) {
                    printFileContent(output_filename, file, file_size, superBlock.block_size, block_start, block_count, fatPtr);
                    munmap(file, size);
                    close(fd);
                    return 0;
                } else {
                    printf("File not found.\n");
                    exit(EXIT_FAILURE);
                }
            }
            dirPtr++;
        }
        if (found == 0) {
            printf("File not found.\n");
            exit(EXIT_FAILURE);
        }
        token = strtok(NULL, "/");
    }

    if (file_size == 0) {
        printf("File not found.\n");
        exit(EXIT_FAILURE);
    }

    // Unmap the file
    munmap(file, size);

    // Close the file
    close(fd);

    return 0;
}
