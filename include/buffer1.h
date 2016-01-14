#include <cstdlib>
#include <type_traits>
#include <cstring>

typedef long block_t;
typedef std::size_t blocksize_t;

template <typename T>
class FrameContainer {
private:
	FrameContainer *next, *prev;
public:
	T frame;
};

template <blocksize_t blocksize>
class BufferFrameIOHelper;

template <blocksize_t blocksize, int poolsize>
class FramePool;

template <blocksize_t blocksize, int poolsize, typename T>
class BufferedFile;

template <blocksize_t blocksize>
class BufferFrame
{
friend class BufferFrameIOHelper<blocksize>;

protected:
	void* data;
	bool is_dirty;
	bool is_valid;
	FramePool* pool;
	FrameContainer<BufferFrame<blocksize>*> cont;

private:
	void access() { pool->accessUpdate(cont) };

public:
	block_t block_number;
	BufferFrame() : is_valid(false), is_dirty(false), block_number(-1), pool(nullptr) { data = std::malloc(blocksize); }
	void setFrameProperties(FramePool* pool, FrameContainer* cont) { this->pool = pool; this->cont = cont; }
	~BufferFrame() { std::free(data); }

	bool isValid() { return is_valid; }
	bool isDirty() { return is_dirty; }
	void markValid() { is_valid = true; }
	void markInvalid() { is_valid = false; }
	void markDirty() { is_dirty = true; }

	// io primitives.
	static int memcpy(BufferFrame* dest_frame,const void* src, blocksize_t offset, blocksize_t size) {
		dest_frame->is_dirty = true;
		std::memcpy((void*) ((char*)dest_frame->data + offset) , src, size);
		dest_frame->access();
		return 0;
	}

	static int memmove(BufferFrame* dest_frame, const void* src, blocksize_t offset, blocksize_t size) {
		dest_frame->is_dirty = true;
		std::memmove((void*) ((char*)dest_frame->data + offset) , src, size);
		dest_frame->access();
		return 0;
	}

	static int memset(BufferFrame* dest_frame, char ch, blocksize_t offset, blocksize_t size) {
		dest_frame->is_dirty = true;
		std::memset((void*) ((char*)dest_frame->data + offset) , ch, size);
		dest_frame->access();
		return 0;
	}

	static const void* readRawData(const BufferFrame* src_frame, blocksize_t offset) {
		src_frame->access();
		return (void*)((char*)src_frame->data + offset);
	}

	BufferFrameIOHelper<blocksize> operator[](blocksize_t offset);
};

template <blocksize_t blocksize>
class BufferFrameIOHelper {
private:
	blocksize_t offset;
	BufferFrame<blocksize>* frame;
	BufferFrameIOHelper() : offset(-1), frame(nullptr) { };
	BufferFrameIOHelper(blocksize_t off, BufferFrame<blocksize>* io_frame) : offset(off), frame(io_frame) {};

	template <typename T>
	BufferFrameIOHelper& operator= (const T& rhs)
	{
		BufferFrame<blocksize>::memcpy(frame, &rhs, offset, sizeof(rhs));
	}

	template <typename T>
	T convert_function(true_type) const{
		return ((T)((char*)frame->data + offset));
	}

	template <typename T>
	T convert_function(false_type) const{
		return *((T*)((char*)frame->data + offset));
	}

	template <typename T>
	operator T() {
		if(is_pointer<T>::value) {
			frame->setDirty();
		}
		return convert_function<T> (typename is_pointer<T>::type());
	}
};

template <blocksize_t blocksize>
BufferFrameIOHelper<blocksize> BufferFrame<blocksize>::operator[] (int offset) {
	return BufferFrameIOHelper<blocksize>(offset, this);
}

template <blocksize_t blocksize, int poolsize>
class FramePool {
private:
	FrameContainer<BufferFrame<blocksize>*> free_list_head;
protected:
	FrameContainer<BufferFrame<blocksize>*>* frames;
public:
	FramePool();
	~FramePool();
	virtual BufferFrame<blocksize>* getNextReplacementFrame();
	virtual void accessUpdate(BufferFrame<blocksize>*);
	virtual void pushToBack(FrameContainer<BufferFrame<block_size>*>* cont);
	virtual void pushToBack(FrameContainer<BufferFrame<block_size>*>* cont);
};

template <blocksize_t blocksize, int poolsize>
FramePool<blocksize, poolsize>::FramePool() {
	frames = new FrameContainer<BufferFrame<blocksize>*>[poolsize] ();
	for(auto i = 0; i < poolsize; i++) {
		frames[i].frame = new BufferFrame<blocksize>();
		frames[i].frame->setFrameProperties(this, *frames[i]);
	}

	free_list_head.frame = nullptr;
	free_list_head.next = &frames[0];
	free_list_head.prev = &frames[poolsize-1];

	for(auto i=0; i< poolsize-1; i++) {
		frames[i].next = &frames[i+1];
	}
	frames[poolsize-1].next = &free_list_head;

	frames[0].prev = &free_list_head;
	for(auto i=1; i< poolsize; i++) {
		frames[i].prev = &frames[i-1];
	}
}

template <blocksize_t blocksize, int poolsize>
FramePool<blocksize, poolsize>::~FramePool() {
	for(auto i = 0; i < poolsize; i++)
		delete frames[i].frame;

	delete [] frames;
}

template <blocksize_t blocksize, int poolsize>
BufferFrame<blocksize>* FramePool<blocksize, poolsize>::getNextReplacementFrame() {

}

template <blocksize_t blocksize, int poolsize, typename T>
class BufferedFile {
private:
	last_block_alloted;
	std::unordered_map< block_t, FrameContainer<BufferFrame<blocksize>*> *> block_hash;
protected:
	int fd;
	BufferFrame<blocksize>* header;
	FramePool<blocksize, poolsize>* page_cache;
	blocksize_t getblockoffset(block_t blknbr) { return (blocksize_t) (blknbr * blocksize); }
public:
	BufferedFile(const char* filepath);
	~BufferedFile();
	virtual block_t allotNewBlock();
	virtual T& getHeader();
	virtual void setHeader(const T& newHeader);
	virtual void deleteBlock(block_t block_number);
	virtual BufferFrame<blocksize>* readBlock(block_t block_number);
	virtual void flushBlock(block_t block_number);
};

template <blocksize_t blocksize, int poolsize, typename T>
BufferedFile<blocksize, poolsize, T>::BufferedFile(const char* filepath) {
	fd = open(filepath, O_RDWR|O_CREAT, 0755);

	if(flock(fd, LOCK_EX | LOCK_NB)==-1)
	{
		close(fd);
		throw std::runtime_error{"Unable to lock file"};
	}

	page_cache = new FramePool<blocksize, poolsize>();

	header = new BufferFrame<blocksize>();
	header->markValid();
	header->block_number = 0;

	pread(fd, header[0], blocksize, getblockoffset(0));

	last_block_alloted = ((block_t)header[sizeof(T)]);
}

template <blocksize_t blocksize, int poolsize, typename T>
BufferedFile<blocksize, poolsize, T>::~BufferedFile() {
	header[sizeof(T)] = last_block_alloted;
	pwrite(fd, header[0], blocksize, getblockoffset(0));

	delete page_cache;

	ftruncate(fd, (last_block_alloted+1)*block_size);

	fsync(fd);

	flock(fd, LOCK_UN | LOCK_NB);
	close(fd);

	delete header;
}

template <blocksize_t blocksize, int poolsize, typename T>
block_t BufferedFile<blocksize, poolsize, T>::allotNewBlock() {
	last_block_alloted++;
	return last_block_alloted;
}


template <blocksize_t blocksize, int poolsize, typename T>
T& BufferedFile<blocksize, poolsize, T>::getHeader() {
	return ((T)header[0]);
}

template <blocksize_t blocksize, int poolsize, typename T>
void BufferedFile<blocksize, poolsize, T>::setHeader(const T& newHeader) {
	header[0] = newHeader;
}

template <blocksize_t blocksize, int poolsize, typename T>
void BufferedFile<blocksize, poolsize, T>::deleteBlock(block_t block_number) {
	if(block_number == 0)
		return;

	std::unordered_map< block_t, FrameContainer<BufferFrame<blocksize>*>* >::iterator got = block_hash.find(block_number);
	if(got!=block_hash.end())
		got->second->frame->markInvalid();

	if(block_number == last_block_alloted) {
		last_block_alloted--;
	}
}

template <blocksize_t blocksize, int poolsize, typename T>
BufferFrame<blocksize>* BufferedFile<blocksize, poolsize, T>::readBlock(block_t block_number) {
	std::unordered_map< block_t, FrameContainer<BufferFrame<blocksize>*>* >::iterator got = block_hash.find(block_number);
	if(got!=block_hash.end()) {
		if(got->second->frame->isValid())
			return got->second->frame;
	}


}
