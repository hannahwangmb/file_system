#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <arpa/inet.h>


// Define structures for the super block and FAT information
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


// Function to display super block information
void displaySuperBlockInfo(struct SuperBlock superBlock) {
    printf("Super block information\n");
    printf("Block size: %d\n", superBlock.block_size);
    printf("Block count: %d\n", superBlock.block_count);
    printf("FAT starts: %d\n", superBlock.fat_starts);
    printf("FAT blocks: %d\n", superBlock.fat_blocks);
    printf("Root directory starts: %d\n", superBlock.root_dir_starts);
    printf("Root directory blocks: %d\n", superBlock.root_dir_blocks);
}

// Function to display FAT information
void displayFatInfo(struct FatInfo fatInfo) {
    printf("\nFAT information\n");
    printf("Free blocks: %d\n", fatInfo.free_blocks);
    printf("Reserved blocks: %d\n", fatInfo.reserved_blocks);
    printf("Allocated blocks: %d\n", fatInfo.allocated_blocks);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <file_system_image>\n", argv[0]);
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

    // Read FAT information
    uint32_t* fatPtr = (uint32_t*)(file + superBlock.fat_starts * superBlock.block_size);
    struct FatInfo fatInfo;
    fatInfo.free_blocks = 0;
    fatInfo.reserved_blocks = 0;
    fatInfo.allocated_blocks = 0;
    uint32_t fatEntry;
    for (int i = 0; i < superBlock.fat_blocks * superBlock.block_size / 4; i++) {
        fatEntry = ntohl(*(fatPtr));
        fatPtr++;
        if (fatEntry == 0) {
            fatInfo.free_blocks++;
        } else if (fatEntry == 1) {
            fatInfo.reserved_blocks++;
        } else {
            fatInfo.allocated_blocks++;
        }
    }

    // Display information
    displaySuperBlockInfo(superBlock);
    displayFatInfo(fatInfo);

    // Unmap the file
    munmap(file, size);
    
    // Close the file
    close(fd);

    return 0;
}
