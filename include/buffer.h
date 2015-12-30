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

class BufferedFile
{
	
public:
	class BufferFrame {
		friend class BufferedWriter;
		friend class BufferedFile;
		friend class BufferedReader;
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
		
		static const void* readRawData(BufferFrame* frame, size_t offset)
		{
			return (void*)((char*)frame->data + offset);
		}
	};

	class FramePool
	{
		const int pool_size;
		BufferFrame* dllist;
		BufferFrame* head;

	public:
		FramePool(const BufferedFile* file, int buffer_pool_size) : pool_size(buffer_pool_size)
		{
			dllist = new BufferFrame[pool_size]();
			for(int i=0; i<pool_size; i++)
				dllist[i].setBufferedFile(file);
			head = new BufferFrame(file);

			head->next = dllist;
			head->prev = (dllist + pool_size - 1);
			dllist[0].next = (dllist+1);
			dllist[0].prev = head;
			dllist[pool_size-1].next = head;
			dllist[pool_size-1].prev = dllist+pool_size-2;

			for(auto i=1; i< pool_size-1; i++)
			{
				dllist[i].next = (dllist + i + 1);
				dllist[i].prev = (dllist + i - 1);
			}
		}
		~FramePool()
		{
			for(auto i=0; i < pool_size; i++)
			{
				if(dllist[i].is_dirty)
				{
					pwrite(dllist[i].file_ref->fd, dllist[i].data, dllist[i].file_ref->block_size, dllist[i].file_ref->getblockoffset(dllist[i].block_number));
				}
			}
			delete [] dllist;
			delete head;
		}
		BufferFrame* getHead()
		{
			return head;
		}
		BufferFrame* getNewFrame()
		{
			return head->next;
		}
		void doAccessUpdate(BufferFrame* ptr)
		{
			ptr->next->prev = ptr->prev;
			ptr->prev->next = ptr->next;
			ptr->next = head;
			ptr->prev = head->prev;
			head->prev->next = ptr;
			head->prev = ptr;
		}
		void removeFrame(BufferFrame* ptr)
		{
			ptr->next->prev = ptr->prev;
			ptr->prev->next = ptr->next;
			ptr->next = head->next;
			ptr->prev = head;
			head->next->prev = ptr;
			head->next = ptr;

			ptr->is_valid = false;
			ptr->is_dirty = false;
			ptr->block_number = -1;
		}
	};
	
private:
	int fd;
	const size_t block_size;
	const int buffer_pool_size;
    
	long last_block_alloted;

	FramePool* frame_pool;
	BufferFrame* header;
	
	std::unordered_map< long, BufferFrame* > block_hash;

	off_t getblockoffset(long blknbr) const { return (off_t) (blknbr * block_size); }

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

class BufferedFile::BufferFrame {
	friend class BufferedWriter;
	friend class BufferedFile;
	friend class BufferedReader;
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

class BufferedFile::BufferedFrameWriter
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

class BufferedFile::BufferedFrameReader
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
	
	frame_pool = new FramePool(this,buffer_pool_size);
	header = new BufferFrame(this);
	
	header->is_valid = true;
	header->block_number = 0;
	pread(fd, header->data, block_size, getblockoffset(0));
	
	last_block_alloted = BufferedFrameReader::read<long>(header, 0);
}

BufferedFile::~BufferedFile()
{
	long* last_block_header = (long*) header->data;
	*last_block_header = last_block_alloted;
	
	pwrite(fd, header->data, block_size, getblockoffset(0));

	delete frame_pool;

	ftruncate(fd, (last_block_alloted+1)*block_size);

	fsync(fd);
    
	flock(fd, LOCK_UN | LOCK_NB);
	close(fd);
	delete header;
}

BufferedFile::BufferFrame* BufferedFile::readHeader()
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

//incomplete modularization
BufferedFile::BufferFrame* BufferedFile::readBlock(long block_number)
{
	std::unordered_map<long, BufferFrame*>::iterator got = block_hash.find(block_number);
	if(got == block_hash.end())
	{
		BufferFrame *alloted;
		alloted = frame_pool->getNewFrame();
		
		if(alloted->is_valid && alloted->is_dirty)
		{
			writeBlock(alloted->block_number);
		}
		
		if(alloted->is_valid)
		{
			block_hash.erase(alloted->block_number);
		}

		frame_pool->doAccessUpdate(alloted);
		
		//to be modularized yet
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
		frame_pool->doAccessUpdate(got->second);
		return got->second;
	}
}

//inclomplete modularization updates
void BufferedFile::writeBlock(long block_number)
{
	if(block_number > last_block_alloted)
		return;
	
	std::unordered_map<long, BufferFrame*>::iterator got = block_hash.find(block_number);
	if(got!=block_hash.end() && got->second->is_valid)
	{
		pwrite(fd, got->second->data, block_size, getblockoffset(block_number));
		got->second->is_dirty = false;
	}
}

//verify removeFrame operation
void BufferedFile::deleteBlock(long block_number) {
	if(block_number == 0)
		return;
	
	std::unordered_map<long, BufferFrame*>::iterator got = block_hash.find(block_number);
	if(got!=block_hash.end())
		frame_pool->removeFrame(got->second);

	last_block_alloted = block_number - 1;
	
	return;
}

typedef BufferedFile::BufferFrame BufferFrame;
typedef BufferedFile::BufferedFrameWriter BufferedFrameWriter;
typedef BufferedFile::BufferedFrameReader BufferedFrameReader;
