#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

size_t getDeviceBlockSize(char* devicename)
{
	using namespace std;
	const char infofilepath[] = "/queue/physical_block_size";
	const char diskpath[] = "/sys/block/";
	char *diskblkfile = (char*)malloc(sizeof(char)*(strlen(infofilepath)+strlen(devicename)+strlen(diskpath)));
	strcat(diskblkfile,diskpath);
	strcat(diskblkfile,devicename);
	strcat(diskblkfile,infofilepath);
	FILE* fptr = fopen(diskblkfile,"r");
	size_t blksize;
	fscanf(fptr,"%lu",&blksize);
	return blksize;
}

class BufferedFile {
private:
	int fd;
	long lastblk;
	const size_t BlkSize;
	long getblockoffset(long blknbr);

public:
	BufferedFile(size_t blksize, const char* filepath);
	~BufferedFile();
	void readBlock(long blknbr, void* buffer);
	void writeBlock(long blknbr, void* buffer);
	long allotBlock();
	void readHeader(void* buffer);
	void writeHeader(void* buffer);
	void deleteBlock(long blknbr);	
};

long BufferedFile::getblockoffset(long blknbr) {
	return blknbr*((long) BlkSize);
}

long BufferedFile::allotBlock() {
	lastblk++;
	return lastblk-1;
}

BufferedFile::BufferedFile(size_t blksize, const char* filepath) : BlkSize(blksize) {	
	fd = open(filepath, O_RDWR|O_CREAT|O_APPEND, 0755);
	lastblk = lseek(fd, 0, SEEK_END)/BlkSize;
	lseek(fd, 0, SEEK_SET);
}

void BufferedFile::readHeader(void* buffer) {
	pread(fd, buffer, (off_t) BlkSize, getblockoffset(0));
}

void BufferedFile::writeHeader(void* buffer) {	
	pwrite(fd, buffer, (off_t) BlkSize, getblockoffset(0));
}

void BufferedFile::readBlock(long blknbr, void* buffer) {	
	pread(fd, buffer, (off_t) BlkSize, getblockoffset(blknbr));
}

void BufferedFile::writeBlock(long blknbr, void* buffer) {	
	pwrite(fd, buffer, (off_t) BlkSize, getblockoffset(blknbr));
}

void BufferedFile::deleteBlock(long blknbr) {
	return;
}

BufferedFile::~BufferedFile() {
	close(fd);
}
