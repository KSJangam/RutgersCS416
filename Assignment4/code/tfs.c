/*
 *  Copyright (C) 2020 CS416 Rutgers CS
 *	Tiny File System
 *	File:	tfs.c
 *ksj48 Kunal Jangam ls.cs.rutgers.edu
 */

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>

#include "block.h"
#include "tfs.h"

char diskfile_path[PATH_MAX];
struct superblock* super;
int init = 0;

int get_avail_ino() {
  bitmap_t imap=(bitmap_t)malloc(BLOCK_SIZE);
  int num = super->i_bitmap_blk;
  bio_read(num, (void *)imap);
  int i;
  for(i=0; i<MAX_INUM; i++){
    if(get_bitmap(imap, i)==0){
      set_bitmap(imap, i);
      bio_write(num, (void *)imap);
      free(imap);
      return i;
    }
  } 
  free(imap);
  return -1;
}

int get_avail_blkno() {
  bitmap_t bmap=(bitmap_t)malloc(BLOCK_SIZE);
  int num = super->d_bitmap_blk;
  bio_read(num, (void *)bmap);
  int i;
  for(i=0; i<MAX_DNUM; i++){
    if(get_bitmap(bmap, i)==0){
      set_bitmap(bmap, i);
      bio_write(num, (void *)bmap);
      free(bmap);
      return i+super->d_start_blk;
    }
  }
  free(bmap);
  return -1;
}

int readi(uint16_t ino, struct inode *inode) {
  struct inode buf[BLOCK_SIZE/sizeof(struct inode)];  
  bio_read(super->i_start_blk+ino/(BLOCK_SIZE/sizeof(struct inode)),(void*) buf);
  *inode = (buf[ino%(BLOCK_SIZE/sizeof(struct inode))]);
  return 0;
}

int writei(uint16_t ino, struct inode *inode) {
  struct inode buf[BLOCK_SIZE/sizeof(struct inode)];  
  bio_read(super->i_start_blk+ino/(BLOCK_SIZE/sizeof(struct inode)),(void*)buf);
  buf[ino%(BLOCK_SIZE/sizeof(struct inode))]=*inode;
  bio_write(super->i_start_blk+ino/(BLOCK_SIZE/sizeof(struct inode)),(void*)buf);
  return 0;
}


int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {
  int i =0;
  int j = 0;
  int ok = 1;
  int k = 0;
  struct inode inode;
  readi(ino, &inode);
  time(&(inode.vstat.st_atime));
  writei(ino, &inode);
  readi(ino, &inode);
  struct dirent* dir=(struct dirent*)malloc(BLOCK_SIZE);
  memset(dir,0,BLOCK_SIZE);
  for(k=0; k<inode.vstat.st_blocks; k++){
    bio_read(inode.direct_ptr[k], (void*)dir);
    for(j=0;j<BLOCK_SIZE/sizeof(struct dirent); j++){
      while(dir[j].valid && dir[j].len==name_len && i<name_len){
	if(dir[j].len>i && dir[j].name[i]!=fname[i]){
	  i=name_len+1;//break out of while
	  ok = 0;
	}
	else i++;
      }
      if(dir[j].valid&&i==dir[j].len && ok){
	dirent->ino =dir[j].ino;
	free(dir);
	return 1;
      }
      else{
	i=0;
	ok = 1;
      }
    }
  }
  free(dir);
  return 0;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {
  int k=0;
  int i=0;
  int j=0;
  int ok=1;
  struct dirent* dir=(struct dirent*)malloc(BLOCK_SIZE);
  memset(dir, 0, BLOCK_SIZE);
  for(k=0; k<dir_inode.vstat.st_blocks; k++){
    bio_read(dir_inode.direct_ptr[k], (void*)dir);
    for(j=0;j<BLOCK_SIZE/sizeof(struct dirent); j++){
      while(dir[j].valid && dir[j].len==name_len && i<name_len){
	if(dir[j].len>i && dir[j].name[i]!=fname[i]){
	  i=name_len+1;//break out of while
	  ok = 0;
	}
	else i++;
      }
      if(dir[j].valid&&i==dir[j].len && ok){
	free(dir);
	return 0;
      }
      else{
	i=0; 
	ok = 1;
      }
    }
  }
  struct dirent toadd;
  toadd.valid=1;
  for(j=0; j<name_len; j++){
    toadd.name[j]=fname[j];
  }
  toadd.name[j]='\0';
  toadd.len=name_len;
  toadd.ino=f_ino;
  for(k=0; k<dir_inode.vstat.st_blocks; k++){
    bio_read(dir_inode.direct_ptr[k], (void*)dir);
    for(j=0;j<BLOCK_SIZE/sizeof(struct dirent); j++){
      if(dir[j].valid==0){
	dir[j]=toadd;
	dir_inode.link=dir_inode.link+1;
	dir_inode.size=dir_inode.size+sizeof(struct dirent);
	bio_write(dir_inode.direct_ptr[k], (void*)dir);
	time(&dir_inode.vstat.st_mtime);
	writei(dir_inode.ino, &dir_inode);
	free(dir);
	return 1;

      }
    }
  }
  if(k<16){
    dir_inode.size=dir_inode.size+sizeof(struct dirent);
    dir_inode.vstat.st_blocks=dir_inode.vstat.st_blocks+1;
    dir_inode.link=dir_inode.link+1;
    j=get_avail_blkno();
    dir_inode.direct_ptr[k]=j;
    memset(dir,0,BLOCK_SIZE);
    dir[0]=toadd;
    bio_write(j, (void*)dir);
    time(&dir_inode.vstat.st_mtime);
    writei(dir_inode.ino, &dir_inode);
    free(dir);
    return 1;
  }
  return -1;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {
  int k=0;
  int i=0;
  int j=0;
  int ok=1;
  struct dirent* dir=(struct dirent*)malloc(BLOCK_SIZE);
  memset(dir,0,BLOCK_SIZE);
  for(k=0; k<dir_inode.vstat.st_blocks; k++){
    bio_read(dir_inode.direct_ptr[k], (void*)dir);
    for(j=0;j<BLOCK_SIZE/sizeof(struct dirent); j++){
      while(dir[j].valid && dir[j].len==name_len&&i<name_len){
	if(dir[j].len>i && dir[j].name[i]!=fname[i]){
	  i=name_len+1;//break out of while
	  ok = 0;
	}
	else i++;
      }
      if(dir[j].valid&&i==dir[j].len && ok){
	dir[j].valid=0;
	dir_inode.link=dir_inode.link-1;
	dir_inode.size=dir_inode.size-sizeof(struct dirent);
	bio_write(dir_inode.direct_ptr[k], (void*)dir);
	time(&dir_inode.vstat.st_mtime);
	writei(dir_inode.ino, &dir_inode);
	free(dir);
	return 1;
      }
      else{
	i=0; 
	ok = 1;
      }
    }
  }
  free(dir);
  return 0;
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
  struct dirent* dir=(struct dirent*)malloc(sizeof(struct dirent));
  int i=1;
  int ok;
  int j = ino;
  int start = 1;
   if(path[i]=='\0'&&path[0]=='/'){
     readi(0, inode);
     return 1;
  }
  
  while(path[i]!='\0'){
    if(path[i]=='/'){
      ok = dir_find(j, &(path[start]), i-start,  dir);
      if(ok==0)return 0;
      j=dir->ino;
      start=i+1;
    }
    i++;
  }
  ok = dir_find(j, &(path[start]), i-start,  dir);
  if(ok==0)return 0;
  readi(dir->ino, inode);
  free(dir);
		
  return 1;
}

/* 
 * Make file system
 */
int tfs_mkfs() {
  super = (struct superblock *)malloc(sizeof(struct superblock));
  dev_init(diskfile_path);
  dev_open(diskfile_path);
  init=1;
  super->magic_num=MAGIC_NUM;
  super->max_inum=MAX_INUM;
  super->max_dnum=MAX_DNUM;
  super->i_bitmap_blk=MAGIC_NUM+1;
  super->d_bitmap_blk=MAGIC_NUM+2;
  super->i_start_blk=MAGIC_NUM+3;
  super->d_start_blk=super->i_start_blk+MAX_INUM/(BLOCK_SIZE/sizeof(struct inode));
  bio_write(MAGIC_NUM, (void*)super);
  bitmap_t imap = (bitmap_t)malloc(BLOCK_SIZE);
  bitmap_t dmap = (bitmap_t)malloc(BLOCK_SIZE);
  memset(imap,0,BLOCK_SIZE);
  memset(imap,0,BLOCK_SIZE);
  struct inode root;
  root.ino=0;
  root.valid=1;
  root.type=1;
  root.link=0;
  root.size=0;
  root.vstat.st_blocks=0;
  root.vstat.st_mode=S_IFDIR | 0755;
  time(&root.vstat.st_atime);
  time(&root.vstat.st_mtime);
  set_bitmap(imap, 0);
  bio_write(super->i_bitmap_blk, (void*)imap);
  bio_write(super->d_bitmap_blk, (void*)dmap);
  struct inode* ib = (struct inode*)malloc(BLOCK_SIZE);
  memset(ib, 0, BLOCK_SIZE);
  int p;
  for(p=0; p<(MAX_INUM/BLOCK_SIZE);p++){
    bio_write(super->i_start_blk+p, ib);
  }
  writei(0, &root);
  free(imap);
  free(dmap);
  return 0;
}


/* 
 * FUSE file operations
 */
static void *tfs_init(struct fuse_conn_info *conn) {
  if(init==0)
    tfs_mkfs();
  else{
    dev_open(diskfile_path);
    bio_read(MAGIC_NUM, (void*)super);
  }

  return (void*)super;
}

static void tfs_destroy(void *userdata) {
  init=0;
  dev_close();

}

static int tfs_getattr(const char *path, struct stat *stbuf) {
  struct inode ino;
  int i = get_node_by_path(path, 0, &ino);
  if(i==0)return -ENOENT;
  stbuf->st_ino=ino.ino;
  stbuf->st_uid=getuid();
  stbuf->st_gid=getgid();
  if(ino.type){
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink=ino.link;
  }
  else{
    stbuf->st_mode = S_IFREG | 0666;
    stbuf->st_nlink=1;
    stbuf->st_size=ino.size;
  }
  time(&stbuf->st_mtime);
  stbuf->st_blksize=BLOCK_SIZE;
  stbuf->st_blocks=ino.vstat.st_blocks;
  time(&stbuf->st_atime);
  return 0;
}

static int tfs_opendir(const char *path, struct fuse_file_info *fi) {
  struct inode ino;
  if(path[0]=='/'&&path[1]=='\0')return 0;
  int i = get_node_by_path(path, 0, &ino);
  if(i==0)return  -ENOENT;
  return 0;
}

static int tfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
  struct inode ino;
  int i = get_node_by_path(path, 0, &ino);
  if(i==0)return -ENOENT;
  int k;
  struct dirent* dir=(struct dirent*)malloc(BLOCK_SIZE);
  for(k=0; k<ino.vstat.st_blocks; k++){
    bio_read(ino.direct_ptr[k], (void*)dir);    
    for(i=0; i<BLOCK_SIZE/sizeof(struct dirent);i++){
      if(dir[i].valid){
	filler(buffer, dir[i].name, NULL, 0);
      }
    }
  }
  free(dir);
  return 0;
}


static int tfs_mkdir(const char *path, mode_t mode) {
  char *dirc, *basec, *bname, *dname;
  dirc=strdup(path);
  basec=strdup(path);
  dname=dirname(dirc);
  bname=basename(basec);
  struct inode ino;
  int i = get_node_by_path(dname, 0, &ino);
  if(i==0) return  -ENOENT;
  uint16_t k = get_avail_ino();
  if(k==-1)return  -ENOENT;
  int s=0;
  while(bname[s]!='\0')s++;
  i = dir_add(ino, k, bname, s);
  if(i==0)return  ENOENT;
  struct inode target;
  target.ino=k;
  target.valid=1;
  target.size=0;
  target.type=1;
  target.vstat.st_blocks=0;
  target.link=2;
  time(&target.vstat.st_atime);
  time(&target.vstat.st_mtime);
  target.vstat.st_mode=mode;
  writei(k, &target);
  return 0;
}

static int tfs_rmdir(const char *path) {
  char *dirc, *basec, *bname, *dname;
  dirc=strdup(path);
  basec=strdup(path);
  dname=dirname(dirc);
  bname=basename(basec);
  struct inode bino;
  struct inode dino;
  int i = get_node_by_path(path, 0, &bino);
  if(i==0) return  -ENOENT;
  int j = get_node_by_path(dname, 0, &dino);
  if(j==0) return  -ENOENT;
  if(bino.size>0)//if directory is not empty, error
    return -1;
  bitmap_t bmap=(bitmap_t)malloc(BLOCK_SIZE);
  bio_read(super->d_bitmap_blk, (void*)bmap);
  int k;
  for(k=0; k<bino.vstat.st_blocks; k++){
    unset_bitmap(bmap, bino.direct_ptr[k] - super->d_start_blk);
  }
  bio_write(super->d_bitmap_blk, (void*)bmap);
  bio_read(super->i_bitmap_blk, (void*)bmap);
  unset_bitmap(bmap, bino.ino);
  bio_write(super->i_bitmap_blk, (void*)bmap);
  int s=0;
  while(bname[s]!='\0')s++;
  dir_remove(dino, bname, s);
  free(bmap);
	return 0;
}

static int tfs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
  char *dirc, *basec, *bname, *dname;
  dirc=strdup(path);
  basec=strdup(path);
  dname=dirname(dirc);
  bname=basename(basec);
  struct inode ino;
  int i = get_node_by_path(dname, 0, &ino);
  if(i==0) return  -ENOENT;
  int j = get_avail_ino();
  if(j==-1)return  -1;
  int s=0;
  while(bname[s]!='\0')s++;
  int k  = dir_add(ino, j, bname, s);
  if(k==0)return ENOENT;
  struct inode nino;
  nino.ino=j;
  nino.valid=1;
  nino.size=0;
  nino.type=0;
  nino.link=1;
  time(&nino.vstat.st_atime);
  time(&nino.vstat.st_mtime);
  nino.vstat.st_mode=mode;
  nino.vstat.st_blocks=0;
  writei(j, &nino);
  return 0;
}

static int tfs_open(const char *path, struct fuse_file_info *fi) {
  struct inode ino;
  int i = get_node_by_path(path, 0, &ino);
  if(i==0)return  -ENOENT;
  return 0;
}

static int tfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
  struct inode ino;
  int i = get_node_by_path(path, 0, &ino);
  if(i==0) return  -ENOENT;
  int num=0;
  char* buf=(char*)malloc(BLOCK_SIZE);
  memset(buf, 0, BLOCK_SIZE);
  bio_read(ino.direct_ptr[offset/BLOCK_SIZE], (void*)buf);
  int j=BLOCK_SIZE;
  if(BLOCK_SIZE-offset%BLOCK_SIZE>size)j=size;
  for(i=offset%BLOCK_SIZE; i<j; i++){
    buffer[num]=buf[i];
    num++;
  }
  if(num==size){
    free(buf);
    return num;
  }
  int k = offset/BLOCK_SIZE + 1;
  if(k>ino.vstat.st_blocks){
    free(buf);
    return num;
  }
  bio_read(ino.direct_ptr[k], (void*)buf);
  i=0;
  while(num<size){
    if(i==BLOCK_SIZE){
      i=0;
      k++;
      if(k>ino.vstat.st_blocks){
	free(buf);
	return num;
      }
      bio_read(ino.direct_ptr[k], (void*)buf);
    }
    else{
      buffer[num]=buf[i];
      i++;
      num++;
    }
  }
  free(buf);
  return num;
}

static int tfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
  struct inode ino;
  int i = get_node_by_path(path, 0, &ino);
  if(i==0) return  -ENOENT;
  int num=0;
  char* buf=(char*)malloc(BLOCK_SIZE);
  memset(buf,0,BLOCK_SIZE);
  while((offset+size)/BLOCK_SIZE>=ino.vstat.st_blocks){
    if(ino.vstat.st_blocks>16){
      free(buf);
      writei(ino.ino,&ino);
      return -1;
    }
    i = get_avail_blkno();
    if(i==-1){
      free(buf);

      writei(ino.ino,&ino);
      return -1;
    }
    ino.direct_ptr[ino.vstat.st_blocks]=i;
    bio_write(i, (void*)buf);
    ino.vstat.st_blocks=ino.vstat.st_blocks+1;
  }
  bio_read(ino.direct_ptr[offset/BLOCK_SIZE], (void*)buf);
  int j=BLOCK_SIZE;
  if(BLOCK_SIZE-(offset%BLOCK_SIZE)>size)j=size;
  for(i=offset%BLOCK_SIZE; i<j; i++){
    buf[i]=buffer[num];
    ino.size=ino.size+1;
    num++;
  }
   bio_write(ino.direct_ptr[offset/BLOCK_SIZE], (void*)buf);
  if(num==size){
    free(buf);

    writei(ino.ino,&ino);
    return num;
  }
  int k = offset/BLOCK_SIZE + 1;
  if(k>16){
    free(buf);

    writei(ino.ino,&ino);
    return num;
  }
  bio_read(ino.direct_ptr[k], (void*)buf);
  i=0;
  while(num<size){
    if(i==BLOCK_SIZE){
      i=0;
      bio_write(ino.direct_ptr[k], (void*)buf);
      k++;
      if(k>16){
	free(buf);

	writei(ino.ino,&ino);
	return num;
      }
      bio_read(ino.direct_ptr[k], (void*)buf);
    }
    else{
      buf[i]=buffer[num];
      ino.size=ino.size+1;
      i++;
      num++;
    }
  }
  bio_write(ino.direct_ptr[k], (void*)buf);
  free(buf);

  writei(ino.ino,&ino);
  return num;
}

static int tfs_unlink(const char *path) {
  char *dirc, *basec, *bname, *dname;

  dirc=strdup(path);
  basec=strdup(path);
  dname=dirname(dirc);
  bname=basename(basec);
  struct inode bino;
  struct inode dino;
  int i = get_node_by_path(path, 0, &bino);
  if(i==0) return  -ENOENT;
  int j = get_node_by_path(dname, 0, &dino);
  if(j==0) return  -ENOENT;
  char* content=(char*)malloc(BLOCK_SIZE);
  int k;
  for(k=0; k<bino.vstat.st_blocks; k++){
    bio_read(bino.direct_ptr[k], (void*)content);
    memset(content,0, BLOCK_SIZE);
    bio_write(bino.direct_ptr[k], (void*)content);
  }
  bitmap_t bmap=(bitmap_t)malloc(BLOCK_SIZE);
  bio_read(super->d_bitmap_blk, (void*)bmap);
  for(k=0; k<bino.vstat.st_blocks; k++){
    unset_bitmap(bmap, bino.direct_ptr[k]-super->d_start_blk);
  }
  bio_write(super->d_bitmap_blk, (void*)bmap);
  bio_read(super->i_bitmap_blk, (void*)bmap);
  unset_bitmap(bmap, bino.ino);
  bio_write(super->i_bitmap_blk, (void*)bmap);
  int s=0;
  while(bname[s]!='\0')s++;
  dir_remove(dino, bname, s);
  free(content);
  free(bmap);
  return 0;
}

static int tfs_truncate(const char *path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_release(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int tfs_flush(const char * path, struct fuse_file_info * fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_utimens(const char *path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}


static struct fuse_operations tfs_ope = {
	.init		= tfs_init,
	.destroy	= tfs_destroy,

	.getattr	= tfs_getattr,
	.readdir	= tfs_readdir,
	.opendir	= tfs_opendir,
	.releasedir	= tfs_releasedir,
	.mkdir		= tfs_mkdir,
	.rmdir		= tfs_rmdir,

	.create		= tfs_create,
	.open		= tfs_open,
	.read 		= tfs_read,
	.write		= tfs_write,
	.unlink		= tfs_unlink,

	.truncate   = tfs_truncate,
	.flush      = tfs_flush,
	.utimens    = tfs_utimens,
	.release	= tfs_release
};


int main(int argc, char *argv[]) {
	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &tfs_ope, NULL);

	return fuse_stat;
}

