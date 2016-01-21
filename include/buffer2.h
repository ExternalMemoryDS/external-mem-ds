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
#include <type_traits>
#include <cstddef>
#include <cstdlib>

typedef long block_t;
typedef std::size_t blocksize_t;

template <blocksize_t blksize, int poolsize>
class FramePool {
public:
	class FrameContainer;
private:
protected:
	FrameContainer* dllist;
	FrameContainer* head;
public:
	class Frame {
	public:
		class FrameData;
	private:
	protected:
		bool is_valid;
		bool is_dirty;
		block_t block_number;
		FrameData* frame_data;
	public:
		/*FrameData is exposed to layer 2*/
		class FrameData {
		private:
		protected:
			void* data;
			Frame* parent_frame;
		public:
			// io primitives
			static const void* readRawData(const FrameData* src_frame, blocksize_t offset) {
				src_frame->parent_frame->access();
				return (void*)((char*)src_frame->data + offset);
			}

			class FrameDataIOHelper {
			private:

			protected:
				Frame* parent_frame;
				blocksize_t offset;
			public:
				FrameDataIOHelper() : offset(-1), parent_frame(nullptr) { };
				FrameDataIOHelper(blocksize_t off, Frame* io_frame) : offset(off), parent_frame(io_frame) {};

				template <typename T>
				FrameDataIOHelper& operator= (const T& rhs)
				{
					parent_frame->is_dirty = true;
					std::memcpy((void*) ((char*)parent_frame->frame_data->data + offset) , (void*) &rhs, sizeof(rhs));
					parent_frame->access();
				}

				template <typename T>
				T convert_function(std::true_type) const{
					return ((T)((char*)parent_frame->frame_data->data + offset));
				}

				template <typename T>
				T convert_function(std::false_type) const{
					return *((T*)((char*)parent_frame->frame_data->data + offset));
				}

				template <typename T>
				operator T() {
					if(std::is_pointer<T>::value) {
						parent_frame->is_dirty = true;
					}
					return convert_function<T> (typename std::is_pointer<T>::type());
				}
			};

			FrameData(Frame* _parent) : parent_frame(_parent) { data = std::malloc(blksize); }
			~FrameData() { std::free(data); }
			void setFrameData(int fd, block_t blk_number) { pread(fd, data, blksize, blksize*blk_number); parent_frame->is_dirty = false; }
			void flushFrameData(int fd) { if(parent_frame->is_dirty()) pwrite(fd, data, blksize, (parent_frame->block_number * blksize)); parent_frame->is_dirty = false; }
			FrameDataIOHelper operator[](blocksize_t offset) {
				//heap or stack???
				return FrameDataIOHelper(offset, this);
			}
		};
		Frame() : is_valid(false), is_dirty(false), block_number(-1) { frame_data = new FrameData(this); }
		~Frame() { delete frame_data; }
		bool isValid() { return is_valid; }
		bool isDirty() { return is_dirty; }
		FrameData* getFrameData() { return frame_data; }
		FrameData* setFrameData(int fd, block_t blk_number)
		{
			is_valid = true;
			is_dirty = false;
			block_number = blk_number;
			frame_data->setFrameData(fd, blk_number);
			return frame_data;
		}
		void markValid() { is_valid = true; }
		void markInvalid() { is_valid = false; }
		void markDirty() { is_dirty = true; }
	};

	/* generic container for frames.
	 * in this case, it will behave like a doubly linked list
	 */
	class FrameContainer {
	private:
		FrameContainer *next, *prev;
	protected:
		Frame* _frame;
	public:
		FrameContainer() : next(nullptr), prev(nullptr) { _frame = new Frame(); }
		~FrameContainer() { delete _frame; }
		bool isValid() { return _frame->isValid(); }
		bool isDirty() { return _frame->isDirty(); }
		//const typename Frame::FrameData* getFrameDataReadOnly() { return const _frame->getFrameData(); }
		typename Frame::FrameData* getFrameData() { return _frame->getFrameData(); }
		typename Frame::FrameData* setFrameData(int fd, block_t block_number) { return _frame->setFrameData(fd, block_number); }
		void setPrevContainer(FrameContainer* _prev) { prev = _prev; }
		void setNextContainer(FrameContainer* _next) { next = _next; }
		void getPrevContainer(FrameContainer* curr) { return prev; }
		void getNextContainer(FrameContainer* curr) { return next; }
	};

	/*public methods exposed to BufferedFile */
	FramePool() 
	{
		dllist = new FrameContainer[poolsize]();
		head = dllist[0];
		dllist[0].setNextContainer(dllist + 1);
		dllist[0].setPrevContainer(dllist + poolsize - 1);
		dllist[poolsize-1].setNextContainer(dllist);
		dllist[poolsize-1].setPrevContainer(dllist + poolsize - 2);
		for(auto i=1; i< poolsize-1; i++)
		{
			dllist[i].setNextContainer(dllist + i + 1);
			dllist[i].setPrevContainer(dllist + i - 1);
		}
	}

	//incomplete
	~FramePool()
	{
		delete [] dllist;
	}

	virtual FrameContainer* getNewFrame() { head = head->getNextContainer(head); return getPrevContainer(head); }
	virtual void setAsNextReplacement(FrameContainer* _frame)
	{
		_frame->getPrevContainer()->setNextContainer(_frame->getNextContainer());
		_frame->getNextContainer()->setPrevContainer(_frame->getPrevContainer());
		head->getPrevContainer()->setNextContainer(_frame);
		head->setPrevContainer(_frame);
		_frame->setNextContainer(head);
		_frame->setPrevContainer(head->getPrevContainer());
		head = _frame;
	}
	virtual void setAsLastReplacement(FrameContainer* _frame)
	{
		_frame->getPrevContainer()->setNextContainer(_frame->getNextContainer());
		_frame->getNextContainer()->setPrevContainer(_frame->getPrevContainer());
		head->getPrevContainer()->setNextContainer(_frame);
		head->setPrevContainer(_frame);
		_frame->setNextContainer(head);
		_frame->setPrevContainer(head->getPrevContainer());
	}
};

template <blocksize_t blksize>
using Frame = typename FramePool<blksize, 0>::Frame;
template <blocksize_t blksize>
using FrameData = typename FramePool<blksize, 0>::Frame::FrameData;
template <blocksize_t blksize>
using FrameContainer = typename FramePool<blksize, 0>::FrameContainer;
//typedef FramePool<blocksize_t, int>::Frame Frame<blocksize_t>;
//typedef FramePool<blocksize_t, int>::FrameContainer FrameContainer;


/* Single block header
 * Writing block automatic
 */
template < typename T, blocksize_t blksize = 4096, blocksize_t reserved_memory = 4096*25 >
class BufferedFile {
private:
	FramePool<blksize, (reserved_memory/blksize)>* page_cache;
	FrameData<blksize>* header;

	std::unordered_map< block_t, FrameContainer<blksize>* > block_hash;
	
protected:
	int fd;
	block_t last_block_alloted;

	blocksize_t getblockoffset(block_t blknbr) { return (blocksize_t) (blknbr*blksize); }
public:
	BufferedFile(const char* _filepath)
	{
		fd = open(_filepath, O_RDWR|O_CREAT, 0755);
		
		if(flock(fd, LOCK_EX | LOCK_NB)==-1)
		{
			close(fd);
			throw std::runtime_error{"Unable to lock file"};
		}

		page_cache = new FramePool<blksize, (reserved_memory/blksize)>();
		Frame<blksize>* temp = new Frame<blksize>();
		header = temp->setFrameData(fd, 0);

		last_block_alloted = (block_t)header[sizeof(T)];
	}
	~BufferedFile()
	{
		header[sizeof(T)] = last_block_alloted;
		flushHeader();

		typename std::unordered_map<block_t, FrameContainer<blksize>*>::iterator got = block_hash.begin();
		while(got!=block_hash.end())
		{
			got->second->flushFrameData(fd);
			got++;
		}
		delete page_cache;

		ftruncate(fd, (last_block_alloted+1)*blksize);

		fsync(fd);
	    
		flock(fd, LOCK_UN | LOCK_NB);
		close(fd);
		delete header;
	}
	virtual T& getHeader() { return &((T)header[0]); }
	virtual void setHeader(const T& _header) { header[0] = _header; }
	virtual void flushHeader() { header->flushFrameData(fd); }
	virtual block_t allotNewBlock() { last_block_alloted++; return last_block_alloted; }
	virtual FrameData<blksize>* readBlock(block_t _blocknumber)
	{
		typename std::unordered_map<block_t, FrameContainer<blksize>*>::iterator got = block_hash.find(_blocknumber);
		if(got==block_hash.end())
		{
			FrameContainer<blksize>* alloted = page_cache->getNewFrame();

			if(alloted->is_valid() && alloted->is_dirty())
				alloted->flushFrameData(fd);

			if(alloted->is_valid())
				block_hash.erase(alloted->getBlockNumber());

			page_cache->setAsLastReplacement(alloted);

			FrameData<blksize>* temp = alloted->setFrameData(fd, _blocknumber);

			block_hash.insert(_blocknumber, alloted);

			return temp;
		}
		else
		{
			page_cache->setAsLastReplacement(got->second);
			return got->second->getFrameData();
		}
	}
	virtual void deleteBlock(block_t _blocknumber)
	{
		if(_blocknumber == 0)
			return;

		typename std::unordered_map<block_t, FrameContainer<blksize>*>::iterator got = block_hash.find(_blocknumber);
		if(got!=block_hash.end())
		{
			//got->second->flushFrameData(fd);
			page_cache->setAsNextReplacement(got->second);
			block_hash.erase(got->fisrt);
		}
		last_block_alloted = _blocknumber - 1;
	}
	virtual void flushBlock(block_t _blocknumber)
	{
		typename std::unordered_map<block_t, FrameContainer<blksize>*>::iterator got = block_hash.find(_blocknumber);
		if(got!=block_hash.end())
			got->second->flushFrameData(fd);
	}
	virtual void flush()
	{
		flushHeader();

		typename std::unordered_map<block_t, FrameContainer<blksize>*>::iterator got = block_hash.begin();
		while(got!=block_hash.end())
		{
			got->second->flushFrameData(fd);
			got++;
		}
	}
};