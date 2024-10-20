# A Simple File System (SFS)

A file system similar to Microsoft’s FAT file system in C. Included functions for displaying file system information, viewing root/sub-directory contents, and copying files to/from the Linux directory

Author: hannahwangmb@uvic.ca

Date: Nov.19, 2023

## Usage:

    $ make
    
    $ ./diskinfo <test.img>
    
    $ ./disklist <test.img> </subdir1/subdir2/...>
    
    $ ./diskget <test.img> </subdir1/subdir2/source_filename> <output_filename>
    
    $ ./diskput <test.img> <source_filename> </subdir1/subdir2/dest_filename>

## Design:
    
    diskinfo.c: Print out the superblock and FAT info

    disklist.c: Print out the specified directory file list

    diskget.c: Copy file from specified file system path to the current directory

    diskput.c: Copy file from the current directory to specified file system path
