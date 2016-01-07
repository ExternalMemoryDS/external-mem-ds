#include <sys/file.h>
#include <unistd.h>
#include <unordered_map>
#include <cstring>
#include "constant_defs.h"

/* fixed size page buffer implementation
 * assuming one block header
 */

class FramePool;
class BufferFrame;

class BufferedFile
{
	friend class BufferFrame;
	friend class FramePool;
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
	BufferedFile(const char* filepath, int file_type, size_t blksize = DEFAULT_BLKSIZE, size_t reserved_memory = DEFAILT_RESERVEDMEM);
	~BufferedFile();
	BufferFrame* readBlock(long block_number);
	void writeBlock(long block_number);
	BufferFrame* readHeader(); 
	void writeHeader();
	long allotBlock();
	void deleteBlock(long block_number);
};

class BufferFrame {
	friend class BufferedFile;
	friend class FramePool;
	friend class NonPriorityFramePool;
	friend class PriorityFramePool;
private:
	bool is_valid;
	bool is_dirty;
	bool is_pinned;
	size_t priority_num;
	size_t max_chance;
	long block_number;
	void* data;
	const BufferedFile* file_ref;
	
	BufferFrame *next, *prev;
public:
	BufferFrame(const BufferedFile* file) : is_valid(false), is_dirty(false), is_pinned(false), priority_num(0), max_chance(0), block_number(-1), next(nullptr), prev(nullptr), file_ref(file) { data = malloc(file->block_size); }
	BufferFrame() : is_valid(false), is_dirty(false), is_pinned(false), priority_num(0), max_chance(0), block_number(-1), next(nullptr), prev(nullptr), file_ref(nullptr), data(nullptr) { }
	~BufferFrame() { free(data); }
	void setBufferedFile(const BufferedFile* file) { file_ref = file; data = malloc(file_ref->block_size); }
	void pin() { is_pinned = true; }
	void unpin() { is_pinned = false; }
	void setPriority(size_t p) { max_chance = p; priority_num = max_chance; }
	void memcpy(const void* src, size_t offset, size_t size);
	void memset(char ch, size_t offset, size_t size);
	void memmove(const void* src, size_t offset, size_t size);
	template <typename T> T write(size_t offset, const T& a);
	template <typename T> T read(size_t offset);
	template <typename T> T* readPtr(size_t offset);
	const void* readRawData(size_t offset);
};

void BufferFrame::memcpy(const void* src, size_t offset, size_t size)
{
	is_dirty = true;
	std::memcpy(((char*)data + offset), src, size);
}

void BufferFrame::memset(char ch, size_t offset, size_t size)
{
	is_dirty = true;
	std::memset(((char*)data + offset), ch, size);
}

void BufferFrame::memmove(const void* src, size_t offset, size_t size)
{
	is_dirty = true;
	std::memmove(((char*)data + offset), src, size);
}

template <typename T>
T BufferFrame::write(size_t offset, const T& a)
{
	memcpy(&a, offset, sizeof(a));
}

template <typename T>
T BufferFrame::read(size_t offset)
{
	return *((T*)((char*)data + offset));
}

template <typename T>
T* BufferFrame::readPtr(size_t offset)
{
	is_dirty = true;
	return ((T*)((char*)data + offset));
}

const void* BufferFrame::readRawData(size_t offset)
{
	return (void*)((char*)data + offset);
}

class FramePool
{
	friend class BufferedFile;
protected:
	const int pool_size;
	BufferFrame* dllist;
	BufferFrame* head;
public:
	FramePool(const BufferedFile* file, int buffer_pool_size);
	virtual ~FramePool()
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
	BufferFrame* getHead();
	virtual BufferFrame* getNewFrame(){}
	virtual void doAccessUpdate(BufferFrame* ptr){}
	virtual void removeFrame(BufferFrame* ptr){}
};

FramePool::FramePool(const BufferedFile* file, int buffer_pool_size) : pool_size(buffer_pool_size)
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

BufferFrame* FramePool::getHead()
{
	return head;
}

class NonPriorityFramePool : public FramePool
{
	friend class BufferedFile;
public:
	NonPriorityFramePool(const BufferedFile* file, int buffer_pool_size) : FramePool(file, buffer_pool_size) {}
	~NonPriorityFramePool() {}
	BufferFrame* getNewFrame();
	void doAccessUpdate(BufferFrame* ptr);
	void removeFrame(BufferFrame* ptr);
};

BufferFrame* NonPriorityFramePool::getNewFrame()
{
	BufferFrame* trav = head->next;
	while(trav->is_pinned == true && trav != head)
		trav = trav -> next;

	return trav;
}
void NonPriorityFramePool::doAccessUpdate(BufferFrame* ptr)
{
	ptr->next->prev = ptr->prev;
	ptr->prev->next = ptr->next;
	ptr->next = head;
	ptr->prev = head->prev;
	head->prev->next = ptr;
	head->prev = ptr;
}
void NonPriorityFramePool::removeFrame(BufferFrame* ptr)
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

class PriorityFramePool : public FramePool
{
	friend class BufferedFile;
private:
	size_t pages_pinned;
public:
	PriorityFramePool(const BufferedFile* file, int buffer_pool_size) : FramePool(file, buffer_pool_size), pages_pinned(0) {}
	~PriorityFramePool() {}
	BufferFrame* getNewFrame();
	void doAccessUpdate(BufferFrame* ptr);
	void removeFrame(BufferFrame* ptr);
};

BufferFrame* PriorityFramePool::getNewFrame()
{
	BufferFrame* trav = head->next;
	if(pages_pinned!=pool_size)
	{
		while(trav == head || trav->is_pinned==true || trav->priority_num!=0)
		{
			if(trav !=head && trav->priority_num!=0) trav->priority_num--;
			trav = trav->next;
		}
	}
	else
	{
		while(trav == head || trav->priority_num!=0)
		{
			if(trav !=head) trav->priority_num--;
			trav = trav->next;
		}
	}
	return trav;
}
void PriorityFramePool::doAccessUpdate(BufferFrame* ptr)
{
	ptr->next->prev = ptr->prev;
	ptr->prev->next = ptr->next;
	ptr->next = head;
	ptr->prev = head->prev;
	head->prev->next = ptr;
	head->prev = ptr;

	if(ptr->max_chance!=0 && ptr->priority_num < ptr->max_chance) ptr->priority_num++;
}
void PriorityFramePool::removeFrame(BufferFrame* ptr)
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
	ptr->priority_num = 0;
	ptr->max_chance = 0;
	ptr->is_pinned = false;	
}

BufferedFile::BufferedFile(const char* filepath, int file_type, size_t blksize, size_t reserved_memory) :
						block_size(blksize), buffer_pool_size(reserved_memory/blksize), last_block_alloted(0)
{
	fd = open(filepath, O_RDWR|O_CREAT, 0755);
	
	if(flock(fd, LOCK_EX | LOCK_NB)==-1)
	{
		close(fd);
		throw std::runtime_error{"Unable to lock file"};
	}
	
	switch (file_type)
	{
	case VECTOR_TYPE:
		frame_pool = new NonPriorityFramePool(this,buffer_pool_size);
		break;
	case BTREE_TYPE:
		frame_pool = new PriorityFramePool(this,buffer_pool_size);
		break;
	default:
		throw "invalid data structure selected!";
	}

	header = new BufferFrame(this);
	
	header->is_valid = true;
	header->block_number = 0;
	pread(fd, header->data, block_size, getblockoffset(0));
	
	last_block_alloted = header->read<long>(0);
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
		
		alloted->is_valid = true;		
		alloted->block_number = block_number;
		alloted->memset(0, 0, block_size);
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

void BufferedFile::deleteBlock(long block_number) {
	if(block_number == 0)
		return;
	
	std::unordered_map<long, BufferFrame*>::iterator got = block_hash.find(block_number);
	if(got!=block_hash.end())
		frame_pool->removeFrame(got->second);

	last_block_alloted = block_number - 1;
	
	return;
}