#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <exception>
#include <unordered_map>

/* fixed size page buffer implementation
 * assuming one block header
 */

class BufferedFile
{
	
private:
	struct BufferFrame {
		bool is_valid;
		bool is_dirty;
		long block_number;        
		void* data;
		
		BufferFrame *next, *prev;
		
		BufferFrame() : is_valid(false), is_dirty(false), block_number(-1), next(nullptr), prev(nullptr) { data = malloc(block_size); }
		~BufferFrame() { free(data); }
		const void* readData() { return data; }
		void updateData(const void* new_block_data) { is_dirty = true; memcpy(data, new_block_data, block_size); }
	};
	
	int fd;
	const size_t block_size;
	const int buffer_pool_size;
    
	long last_block_alloted;
	
	BufferFrame* frame_pool;
	BufferFrame* free_list_head;
	BufferFrame* header;
	
	std::unordered_map< long, BufferFrame* > block_hash;
	
	off_t getblockoffset(long blknbr) { return (off_t) (blknbr * block_number); }
    
public:
	
	// default numbers are arbitrary. change to best value.
	// reserved_memory is the size of buffer pool in main memory to be reserved for the application.
	BufferedFile(const char* filepath, size_t blksize = 4096, size_t reserved_memory = 1048576);
	~BufferedFile();
	const void* readBlock(long block_number);
	void writeBlock(long block_number, const void* modified_page);
	const void* readHeader(); 
	void writeHeader(const void* header_buffer);
	long allotBlock();
	void deleteBlock(long block_number);
};

BufferedFile::BufferedFile(const char* filepath, size_t blksize = 4096, size_t reserved_memory = 1048576) :
						block_size(blksize), buffer_pool_size(reserved_memory/blksize), last_block_alloted(0)
{
	fd = open(filepath, O_RDWR|O_CREAT|O_APPEND, 0755);
	
	if(flock(fd, LOCK_EX | LOCK_NB)==-1)
	{
		close(fd);
		throw std::runtime_error{"Unable to lock file"};
	}
	
	frame_pool = new BufferFrame[buffer_pool_size];
	free_list_head = new BufferFrame();
	header = new BufferFrame();
	
	header->is_valid = true;
	header->block_number = 0;
	pread(fd, header->data, block_size, getblockoffset(0));
	
	free_list_head.next = frame_pool;
	free_list_head.prev = (frame_pool + buffer_pool_size - 1);
	
	for(auto i=1; i< buffer_pool_size-1; i++)
	{
		frame_pool[i].next = (frame_pool + 1);
		frame_pool[i].prev = (frame_pool - 1);
	}
}

BufferedFile::~BufferedFile()
{
	if(header->is_dirty)
		pwrite(fd, header->readData(), block_size, getblockoffset(0));
	
	for(auto i=0; i < buffer_pool_size; i++)
	{
		if(frame_pool[i].is_dirty)
		{
			pwrite(fd, frame_pool[i].readData(), block_size, getblockoffset(frame_pool[i].block_number));
		}
	}
    
	flock(fd, LOCK_UN | LOCK_NB);
	close(fd);
	delete header;
	delete [] frame_pool;
	delete free_list_head;
}

const void* BufferedFile::readHeader()
{
	return header->readData();
}

void BufferedReader::writeHeader(const void* header_buffer)
{
	header->updateData(header_buffer);
}

long BufferedReader::allotBlock()
{
	last_block_alloted++;
	return last_block_alloted;
}

const void* BufferedReader::readBlock(long block_number)
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
        
		free_list_head->next = alloted->next;
		alloted->next = free_list_head;
		alloted->prev = free_list_head->prev;
		free_list_head->prev->next = alloted;
		
		if(alloted->is_valid && alloted->is_dirty)
		{
			pwrite(fd, alloted->data, block_size, getblockoffset(alloted->block_number));
		}
		alloted->is_valid = true;
		alloted->is_dirty = false;
		alloted->block_number = block_number;
		pread(fd, alloted->data, block_size, getblockoffset(block_number));
		
		block_hash.insert(block_number, alloted);
		
		return alloted->readData();
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
		
		return got->second->readData();
	}
}

void BufferedFile::writeBlock(long block_number, void* modified_page)
{
	readBlock(block_number);
	
	std::unordered_map<long, BufferFrame*>::iterator got = block_hash.find(block_number);
	got->second->updateData(modified_page);
}