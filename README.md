Three C programming Assignments from the CS 416: Operating System Design course at Rutgers University.

Assignment 2:

Assignment 3: User level Memory Management
This project implemented a user level page table that can translate virtual addresses to physical adressess through building a multi level page table. In additiom the project also implemented a translation lookaside buffer to reduce translation cost.
Users can use this library to malloc data as well as put and get data to the memory blocks they malloced, and then free the memory blocks.
The program works with variable page sizes.
The translation lookaside buffers(TLBs) were used to cache recent virtual memory to physical memory translations in order to provide efficiency when accessing the same memory blocks multiple times.

Assignment 4: Tiny File System using FUSE library
This project implemented a file system built on top of the FUSE library.
It emulated a working disk by storing data in a flat file that was accessed like a block device. All user data as well as all indexing blocks, management information, and metadata about files were stored in this flat file.
It provided the file abstraction by fetching segments from this virtual disk.
Low level block I/O operations, bitmaps, and inodes were used in the file system.
The file system has directory functionality, allowing for the creation, opening, and removing of directories as well as the creation, opening, and removing of files.
The file system is mounted in the mount path.
