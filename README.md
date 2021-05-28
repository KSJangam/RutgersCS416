Three C programming Assignments from the CS 416: Operating System Design course at Rutgers University.

Assignment 2:

Assignment 3:

Assignment 4: Tiny File System using FUSE library
This project implemented a file system built on top of the FUSE library.
It emulated a working disk by storing data in a flat file that was accessed like a block device. All user data as well as all indexing blocks, management information, and metadata about files were stored in this flat file.
It provided the file abstraction by fetching segments from this virtual disk.
Low level block I/O operations, bitmaps, and inodes were used in the file system.
The file system has directory functionality, allowing for the creation, opening, and removing of directories as well as the creation, opening, and removing of files.
The file system is mounted in the mount path.
