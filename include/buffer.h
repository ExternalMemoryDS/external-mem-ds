#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <stddef.h>
#include <stdint.h>
#include <exception>
#include <unordered_map>
#include <cstring>

/* fixed size page buffer implementation
 * assuming one block header
 */

class BufferFrame;

class BufferedFile
{
	friend class BufferFrame;
private:	
	int fd;
	const size_t block_size;
	const int buffer_pool_size;
    
	long last_block_alloted;
	
	BufferFrame* frame_pool;
	BufferFrame* free_list_head;
	BufferFrame* header;
	
	std::unordered_map< long, BufferFrame* > block_hash;
	
	off_t getblockoffset(long blknbr) { return (off_t) (blknbr * block_size); }
    
public:
	
	// default numbers are arbitrary. change to best value.
	// reserved_memory is the size of buffer pool in main memory to be reserved for the application.
	BufferedFile(const char* filepath, size_t blksize = 4096, size_t reserved_memory = 1048576);
	~BufferedFile();
	BufferFrame* readBlock(long block_number);
	void writeBlock(long block_number);
	BufferFrame* readHeader(); 
	void writeHeader();
	long allotBlock();
	void deleteBlock(long block_number);
};

class BufferFrame {
	friend class BufferedFrameWriter;
	friend class BufferedFile;
	friend class BufferedFrameReader;
private:
	bool is_valid;
	bool is_dirty;
	long block_number;
	void* data;
	const BufferedFile* file_ref;
	
	BufferFrame *next, *prev;
public:
	BufferFrame(const BufferedFile* file) : is_valid(false), is_dirty(false), block_number(-1), next(nullptr), prev(nullptr), file_ref(file) { data = malloc(file->block_size); }
	BufferFrame() : is_valid(false), is_dirty(false), block_number(-1), next(nullptr), prev(nullptr), file_ref(nullptr), data(nullptr) { }
	~BufferFrame() { free(data); }
	void setBufferedFile(const BufferedFile* file) { file_ref = file; data = malloc(file_ref->block_size); }
};

class BufferedFrameWriter
{
public:
	static void memcpy(BufferFrame* frame, const void* src, size_t offset, size_t size)
	{
		frame->is_dirty = true;
		std::memcpy(((char*)frame->data + offset), src, size);
	}
	
	static void memset(BufferFrame* frame, char ch, size_t offset, size_t size)
	{
		frame->is_dirty = true;
		std::memset(((char*)frame->data + offset), ch, size);
	}
	
	static void memmove(BufferFrame* frame, const void* src, size_t offset, size_t size)
	{
		frame->is_dirty = true;
		std::memmove(((char*)frame->data + offset), src, size);
	}
	
	template <typename T>
	static T write(BufferFrame* frame, size_t offset, const T& a)
	{
		memcpy(frame, &a, offset, sizeof(a));
	}
};

class BufferedFrameReader
{
public:
	template <typename T>
	static T read(BufferFrame* frame, size_t offset)
	{
		return *((T*)((char*)frame->data + offset));
	}
	
	template <typename T>
	static T* readPtr(BufferFrame* frame, size_t offset)
	{
		frame->is_dirty = true;
		return ((T*)((char*)frame->data + offset));
	}
};



BufferedFile::BufferedFile(const char* filepath, size_t blksize, size_t reserved_memory) :
						block_size(blksize), buffer_pool_size(reserved_memory/blksize), last_block_alloted(0)
{
	fd = open(filepath, O_RDWR|O_CREAT, 0755);
	
	if(flock(fd, LOCK_EX | LOCK_NB)==-1)
	{
		close(fd);
		throw std::runtime_error{"Unable to lock file"};
	}
	
	frame_pool = new BufferFrame[buffer_pool_size]();
	for(int i=0; i<buffer_pool_size; i++)
		frame_pool[i].setBufferedFile(this);
	free_list_head = new BufferFrame(this);
	header = new BufferFrame(this);
	
	header->is_valid = true;
	header->block_number = 0;
	pread(fd, header->data, block_size, getblockoffset(0));
	
	last_block_alloted = BufferedFrameReader::read<long>(header, 0);
	
	free_list_head->next = frame_pool;
	free_list_head->prev = (frame_pool + buffer_pool_size - 1);
	frame_pool[0].next = (frame_pool+1);
	frame_pool[0].prev = free_list_head;
	frame_pool[buffer_pool_size-1].next = free_list_head;
	frame_pool[buffer_pool_size-1].prev = frame_pool+buffer_pool_size-2;
	
	for(auto i=1; i< buffer_pool_size-1; i++)
	{
		frame_pool[i].next = (frame_pool + i + 1);
		frame_pool[i].prev = (frame_pool + i - 1);
	}
}

BufferedFile::~BufferedFile()
{
	long* last_block_header = (long*) header->data;
	*last_block_header = last_block_alloted;
	
	pwrite(fd, header->data, block_size, getblockoffset(0));
	
	for(auto i=0; i < buffer_pool_size; i++)
	{
		if(frame_pool[i].is_dirty)
		{
			pwrite(fd, frame_pool[i].data, block_size, getblockoffset(frame_pool[i].block_number));
		}
	}
    
    fsync(fd);
    
	flock(fd, LOCK_UN | LOCK_NB);
	close(fd);
	delete header;
	delete [] frame_pool;
	delete free_list_head;
}

BufferFrame* BufferedFile::readHeader()
{
	return header;
}

void BufferedFile::writeHeader()
{	
	pwrite(fd, header->data, block_size, getblockoffset(0));
	header->is_dirty = false;
}

long BufferedFile::allotBlock()
{
	last_block_alloted++;
	return last_block_alloted;
}

BufferFrame* BufferedFile::readBlock(long block_number)
{
	std::unordered_map<long, BufferFrame*>::iterator got = block_hash.find(block_number);
	if(got == block_hash.end())
	{
		BufferFrame *alloted;
		alloted = free_list_head->next;
		
		if(alloted->is_valid)
		{
			block_hash.erase(alloted->block_number);
		}
		
		if(alloted->is_valid && alloted->is_dirty)
		{
			pwrite(fd, alloted->data, block_size, getblockoffset(alloted->block_number));
		}
		
		alloted->next->prev = free_list_head;
		free_list_head->next = alloted->next;
		alloted->next = free_list_head;
		alloted->prev = free_list_head->prev;
		free_list_head->prev->next = alloted;
				
		alloted->is_valid = true;		
		alloted->block_number = block_number;
		BufferedFrameWriter::memset(alloted, 0, 0, block_size);
		alloted->is_dirty = false;
		pread(fd, alloted->data, block_size, getblockoffset(block_number));
		
		block_hash.insert({block_number, alloted});
		
		return alloted;
	}
	else
	{
		BufferFrame *accessed, *accessed_prev, *accessed_next;
		accessed = got->second;
		accessed_next = accessed->next;
		accessed_prev = accessed->prev;
		
		accessed_prev->next = accessed_next;
		accessed_next->prev = accessed_prev;
		
		accessed->prev = free_list_head->prev;
		accessed->next = free_list_head;
		free_list_head->prev->next = accessed;
		free_list_head->prev = accessed;
		
		return got->second;
	}
}

void BufferedFile::writeBlock(long block_number)
{
	//readBlock(block_number);	
	std::unordered_map<long, BufferFrame*>::iterator got = block_hash.find(block_number);
	if(got!=block_hash.end() && got->second->is_valid)
	{
		pwrite(fd, got->second->data, block_size, getblockoffset(block_number));
		got->second->is_dirty = false;
	}
}

void BufferedFile::deleteBlock(long block_number) {
	return;
}
