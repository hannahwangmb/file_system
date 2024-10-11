#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <string.h>

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

void printList(char* file, int block_size, int block_start, int block_count, int arg_count, int argc) {
    struct dir_entry_t* dirPtr = (struct dir_entry_t*)(file + block_start * block_size);
    for (int i = 0; i < block_count * block_size / sizeof(struct dir_entry_t); i++) {
        struct dir_entry_t dirEntry = *dirPtr; // Create a copy of the data

        dirEntry.size = ntohl(dirEntry.size);
        dirEntry.starting_block = ntohl(dirEntry.starting_block);
        dirEntry.block_count = ntohl(dirEntry.block_count);
        dirEntry.create_time.year = ntohs(dirEntry.create_time.year);
        dirEntry.modify_time.year = ntohs(dirEntry.modify_time.year);
        if (dirEntry.status == 0x03 && arg_count==argc) {
            printf("F %10d %30s %04d/%02d/%02d %02d:%02d:%02d\n", dirEntry.size, dirEntry.filename, dirEntry.create_time.year, dirEntry.create_time.month, dirEntry.create_time.day, dirEntry.create_time.hour, dirEntry.create_time.minute, dirEntry.create_time.second);
        } else if (dirEntry.status == 0x05) {
            printf("D %10d %30s %04d/%02d/%02d %02d:%02d:%02d\n", dirEntry.size, dirEntry.filename, dirEntry.create_time.year, dirEntry.create_time.month, dirEntry.create_time.day, dirEntry.create_time.hour, dirEntry.create_time.minute, dirEntry.create_time.second);
        }
        dirPtr++;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        printf("Usage: %s <file_system_image> </subdir1/subdir2/...(optional)>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

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
    char *file = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (file == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    // Read super block information
    struct SuperBlock superBlock;

    superBlock.block_size = ntohs(*((uint16_t *)(file + 8)));
    superBlock.block_count = ntohl(*((uint32_t *)(file + 10)));
    superBlock.fat_starts = ntohl(*((uint32_t *)(file + 14)));
    superBlock.fat_blocks = ntohl(*((uint32_t *)(file + 18)));
    superBlock.root_dir_starts = ntohl(*((uint32_t *)(file + 22)));
    superBlock.root_dir_blocks = ntohl(*((uint32_t *)(file + 26)));

    int arg_count = 0;
    if (argc == 2) {
        arg_count = 2;
        printList(file, superBlock.block_size, superBlock.root_dir_starts, superBlock.root_dir_blocks, arg_count, argc);
    }
    else{
        arg_count = 3;
        char* subDir = argv[2];
        char* token = (char*)strtok(subDir, "/");
        int block_start = superBlock.root_dir_starts;
        int block_count = superBlock.root_dir_blocks;

        while (token != NULL) {
            struct dir_entry_t* dirPtr = (struct dir_entry_t*)(file + block_start * superBlock.block_size);
            int found = 0;
            for (int i = 0; i < block_count * superBlock.block_size / sizeof(struct dir_entry_t); i++) {
                struct dir_entry_t dirEntry = *dirPtr; // Create a copy of the data
                dirEntry.size = ntohl(dirEntry.size);
                dirEntry.starting_block = ntohl(dirEntry.starting_block);
                dirEntry.block_count = ntohl(dirEntry.block_count);
                dirEntry.create_time.year = ntohs(dirEntry.create_time.year);
                dirEntry.modify_time.year = ntohs(dirEntry.modify_time.year);
                if (dirEntry.status == 0x05 && strcmp((const char*)dirEntry.filename, token) == 0) {
                    block_start = dirEntry.starting_block;
                    block_count = dirEntry.block_count;
                    found = 1;
                    break;
                }
                dirPtr++;
            }
            if (found == 0) {
                printf("File not found.\n");
                exit(EXIT_FAILURE);
            }
            token = strtok(NULL, "/");
        }
        printList(file, superBlock.block_size, block_start, block_count, arg_count, argc);
    }
    // Unmap the file
    munmap(file, size);
    
    // Close the file
    close(fd);

    return 0;
}