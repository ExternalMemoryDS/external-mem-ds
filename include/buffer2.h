typedef long block_t;
typedef std::size_t blocksize_t;

template <blocksize_t blksize, int poolsize>
class FramePool {
private:
protected:
public:
	FramePool();
	~FramePool();

	class Frame {
	private:
	protected:
		bool is_valid;
		bool is_dirty;
		block_t block_number;
	public:
		/*FrameData is exposed to layer 2*/
		class FrameData {
		private:
		protected:
			void* data;
			Frame* parent_frame;
		public:
			// io primitives.
			static int memcpy(FrameData<blksize>* dest_frame,const void* src, blocksize_t offset, blocksize_t size) {
				dest_frame->parent_frame->is_dirty = true;
				std::memcpy((void*) ((char*)dest_frame->data + offset) , src, size);
				dest_frame->parent_frame->access();
				return 0;
			}

			static int memmove(FrameData<blksize>* dest_frame, const void* src, blocksize_t offset, blocksize_t size) {
				dest_frame->parent_frame->is_dirty = true;
				std::memmove((void*) ((char*)dest_frame->data + offset) , src, size);
				dest_frame->parent_frame->access();
				return 0;
			}

			static int memset(FrameData<blksize>* dest_frame, char ch, blocksize_t offset, blocksize_t size) {
				dest_frame->parent_frame->is_dirty = true;
				std::memset((void*) ((char*)dest_frame->data + offset) , ch, size);
				dest_frame->parent_frame->access();
				return 0;
			}

			static const void* readRawData(const FrameData<blksize>* src_frame, blocksize_t offset) {
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
				FrameDataIOHelper(blocksize_t off, FrameData<blksize>* io_frame) : offset(off), parent_frame(io_frame) {};

				template <typename T>
				FrameDataIOHelper& operator= (const T& rhs)
				{
					FrameData<blksize>::memcpy(parent_frame, &rhs, offset, sizeof(rhs));
				}

				template <typename T>
				T convert_function(true_type) const{
					return ((T)((char*)parent_frame->frame_data->data + offset));
				}

				template <typename T>
				T convert_function(false_type) const{
					return *((T*)((char*)parent_frame->frame_data->data + offset));
				}

				template <typename T>
				operator T() {
					if(is_pointer<T>::value) {
						parent_frame->setDirty();
					}
					return convert_function<T> (typename is_pointer<T>::type());
				}
			};
			FrameDataIOHelper<blksize> operator[](blocksize_t offset) {
				return FrameDataIOHelper<blksize>(offset, this);
			}
		};
		Frame() :
	};

	/* generic container for frames.
	 * in this case, it will behave like a doubly linked list
	 */
	class FrameContainer {
	private:
		FrameContainer *next, *prev;
	protected:
		Frame<blksize>* _frame;
	public:
		Frame<blksize>* getFrame() { return _frame; }
	};

	/*public methods exposed to BufferedFile */
	virtual FrameContainer<blksize>* getNewFrame();
	virtual void setAsNextReplacement(FrameContainer<blksize>* _frame);
	virtual void setAsLastReplacement(FrameContainer<blksize>* _frame);
};


/* Single block header
 * Writing block automatic
 */
template < typename T, blocksize_t blksize = 4096, blocksize_t reserved_memory = 4096*25 >
class BufferedFile {
private:
protected:
public:
	BufferedFile(const char* _filepath);
	~BufferedFile();
	virtual T& getHeader();
	virtual void setHeader(const T& _header);
	virtual void flushHeader();
	virtual block_t allotNewBlock();
	virtual FrameData<blksize>* readBlock(block_t _blocknumber);
	virtual void deleteBlock(block_t _blocknumber);
	virtual void flushBlock(block_t _blocknumber);
	virtual void flush();
};
