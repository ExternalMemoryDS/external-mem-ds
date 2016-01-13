#include <cstdlib>
#include <type_traits>
#include <cstring>

typedef long block_t;
typedef std::size_t blocksize_t;

template <blocksize_t blocksize>
class BufferFrameIOHelper;

template <blocksize_t blocksize, int poolsize>
class FramePool;

template <blocksize_t blocksize>
class BufferFrame
{
protected:
	block_t block_number;
	void* data;
	bool is_valid;
	bool is_dirty;
	FramePool* pool;

private:
	void access() { pool->accessUpdate(this) };

public:
	BufferFrame() : is_valid(false), is_dirty(false), block_number(-1), pool(nullptr) { data = std::malloc(blocksize); }
	//add appropriate constructors
	~BufferFrame() { std::free(data); }

	bool isValid() { return is_valid; }
	bool isDirty() { return is_dirty; }
	void setValid() { is_valid = true; }
	void setDirty() { is_dirty = true; }

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
	//check if offset >= blocksize
	BufferFrameIOHelper(blocksize_t off, BufferFrame<blocksize>* io_frame) : offset(off), frame(io_frame) {};

	template <typename T>
	BufferFrameIOHelper& operator= (const T& rhs)
	{
		//check if offset + sizeof(rhs) >= blocksize
		BufferFrame<blocksize>::memcpy(frame, &rhs, offset, sizeof(rhs));
	}

	template <typename T>
	T convert_function(true_type) const{
		return ((T)(BufferFrame<blocksize>::readRawData(frame, offset)));
	}

	template <typename T>
	T convert_function(false_type) const{
		return *((T*)(BufferFrame<blocksize>::readRawData(frame, offset)));
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
protected:
	BufferFrame<blocksize>* frames;
public:
	FramePool() : { frames = new BufferFrame<blocksize>[poolsize](); }
	~FramePool() { delete [] frames; }
	virtual BufferFrame<blocksize>* getNextReplacementFrame();
	virtual void accessUpdate(BufferFrame<blocksize>*);
};

template <blocksize_t blocksize, int poolsize, typename T>
class BufferedFile {
private:
	last_block_alloted = 1;
	std::unordered_map< block_t, BufferFrame<blocksize>* > block_hash;
protected:
	int fd;
	BufferFrame<blocksize>* header;
	FramePool<blocksize, poolsize>* page_cache;
	blocksize_t getblockoffset(block_t blknbr) { }
public:
	BufferedFile(const char* filepath) {}
	~BufferedFile() {}
	virtual block_t allotNewBlock();
	virtual T& getHeader();
	virtual void setHeader(const T& newHeader);
	virtual void deleteBlock(block_t block_number);
	virtual BufferFrame<blocksize>* readBlock(block_t block_number);
	virtual void flushBlock(block_t block_number);
};
