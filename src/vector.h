#include "buffer.h"
#include <iterator>
#include <stdexcept>
#include <stddef.h>

template <typename T>
class vector
{
public:
	typedef long long int size_type;
private:
	const size_t block_size;
	const size_t element_size;
	const size_t num_elements_per_block;
	BufferedFile* buffered_file;
	size_type sz;

public:	
	class iterator : public std::iterator<std::random_access_iterator_tag, T, size_type, T*, T&> {
	friend class vector;
	private:
		size_type index;
                vector<T>* vec;
	public:
		iterator(size_type i, const vector<T>* v) : index(i), vec(v) {}
		iterator(const iterator& it) : index(it.index), vec(it.vec) {}
		iterator(const vector<T>* v) : index(0), vec(v) {}
		iterator() : index(0), vec(nullptr) {}
		bool operator== (const iterator& rhs) { return (index == rhs.index) || (index>=vec->sz && rhs.index>=vec->sz) || (index<0 || rhs.index<0); }
		bool operator!= (const iterator& rhs) { return !(operator==(rhs)); }
		T& operator* ();
		iterator& operator++ () { index++; return *this; }
		iterator operator++(int) { iterator tmp(*this); operator++(); return tmp; }
		iterator& operator-- () { index--; return *this; }
		iterator operator--(int) { iterator tmp(*this); operator--(); return tmp; }
		iterator operator+ (size_type n) { iterator tmp(*this); tmp.index += n; return tmp; }
		iterator operator- (size_type n) { iterator tmp(*this); tmp.index -= n; return tmp; }
		bool operator> (const iterator& rhs) { return index > rhs.index; }
		bool operator< (const iterator& rhs) { return index < rhs.index; }
		bool operator<= (const iterator& rhs);
		bool operator>= (const iterator& rhs);
		iterator& operator+= (size_type n) { index += n; return *this; }
		iterator& operator-= (size_type n) { index -= n; return *this; }
		T& operator[] (size_type n) { return *(*this+n); }
		typename std::iterator<std::random_access_iterator_tag, T, long long int, T*, T&>::difference_type operator- (const iterator& rhs) { return index - rhs.index; }
	};

	class const_iterator : public std::iterator <std::random_access_iterator_tag, T, size_type, T*, T&> {
	friend class vector;
	private:
		size_type index;
                vector<T>* vec;
	public:
		const_iterator(size_type i, const vector<T>* v) : index(i), vec(v) {}
		const_iterator(const iterator& it) : index(it.index), vec(it.vec) {}
		const_iterator(const vector<T>* v) : index(0), vec(v) {}
		const_iterator() : index(0), vec(nullptr) {}
		bool operator== (const const_iterator& rhs) { return (index == rhs.index) || (index>=vec->sz && rhs.index>=vec->sz) || (index<0 && rhs.index<0); }
		bool operator!= (const const_iterator& rhs) { return !(operator==(rhs)); }
		const T& operator* () const;
		const_iterator& operator++ () { index++; return *this; }
		const_iterator operator++(int) { const_iterator tmp(*this); operator++(); return tmp; }
		const_iterator& operator-- () { index--; return *this; }
		const_iterator operator--(int) { const_iterator tmp(*this); operator--(); return tmp; }
		const_iterator operator+ (size_type n) { const_iterator tmp(*this); tmp.index += n; return tmp; }
		const_iterator operator- (size_type n) { const_iterator tmp(*this); tmp.index -= n; return tmp; }
		bool operator> (const const_iterator& rhs) { return index > rhs.index; }
		bool operator< (const const_iterator& rhs) { return index < rhs.index; }
		bool operator<= (const const_iterator& rhs);
		bool operator>= (const const_iterator& rhs);
		const_iterator& operator+= (size_type n) { index += n; return *this; }
		const_iterator& operator-= (size_type n) { index -= n; return *this; }
		const T& operator[] (size_type n) const { return *(*this+n); }
		typename std::iterator<std::random_access_iterator_tag, T, long long int, T*, T&>::difference_type operator- (const const_iterator& rhs) { return index - rhs.index; }
	};

	class reverse_iterator : public std::iterator <std::random_access_iterator_tag, T, size_type, T*, T&> {
	friend class vector;
	private:
		size_type index;
                vector<T>* vec;
	public:
		reverse_iterator(size_type i, const vector<T>* v) : index(i), vec(v) {}
		reverse_iterator(const reverse_iterator& it) : index(it.index), vec(it.vec) {}
		reverse_iterator(const vector<T>* v) : index(v->sz-1), vec(v) {}
		reverse_iterator() : index(-1), vec(nullptr) {}
		bool operator== (const reverse_iterator& rhs) { return (index == rhs.index) || (index>=sz && rhs.index>=sz) || (index<0 && rhs.index<0); }
		bool operator!= (const reverse_iterator& rhs) { return !(operator==(rhs)); }
		T& operator* ();
		reverse_iterator& operator++ () { index--; return *this; }
		reverse_iterator operator++(int) { reverse_iterator tmp(*this); operator++(); return tmp; }
		reverse_iterator& operator-- () { index++; return *this; }
		reverse_iterator operator--(int) { reverse_iterator tmp(*this); operator--(); return tmp; }
		reverse_iterator operator+ (size_type n) { reverse_iterator tmp(*this); tmp.index -= n; return tmp; }
		reverse_iterator operator- (size_type n) { reverse_iterator tmp(*this); tmp.index += n; return tmp; }
		bool operator> (const reverse_iterator& rhs) { return index > rhs.index; }		
		bool operator< (const reverse_iterator& rhs) { return index < rhs.index; }		
		bool operator<= (const reverse_iterator& rhs);		
		bool operator>= (const reverse_iterator& rhs);		
		reverse_iterator& operator+= (size_type n) { index -= n; return *this; }
		reverse_iterator& operator-= (size_type n) { index += n; return *this; }
		T& operator[] (size_type n) { return *(*this-n); }
		typename std::iterator<std::random_access_iterator_tag, T, long long int, T*, T&>::difference_type operator- (const reverse_iterator& rhs) { return index - rhs.index; }
	};

	vector(const char* pathname, size_type blocksize) : block_size(blocksize), element_size(sizeof(T)), 
		sz(0), num_elements_per_block(blocksize/(sizeof(T))) {
		buffered_file = new BufferedFile(blocksize, pathname);	
	}
	~vector() { delete buffered_file; }
	void push_back(const T& elem);
	void pop_back();
	void clear();
	void insert(iterator position, const T& val);
	// void insert(iterator position, std::InputIterator first, std::InputIterator last);
	//void erase(iterator first, iterator last);
	T& operator[] (size_type n);
	size_type size() { return sz; }
	iterator begin();
	iterator end();
	
	const_iterator cbegin();
	const_iterator cend();
	reverse_iterator rbegin();
	reverse_iterator rend();
};

template <typename T>
T& vector<T>::iterator::operator* () {
	long block_number = (index / vec->num_elements_per_block) + 1;
	long block_offset = (index % vec->num_elements_per_block) * vec->element_size;

	void* buff = malloc(vec->block_size);
	vec->buffered_file->readBlock(block_number, buff);

	//not sure if this is the right way do it. Right now this is just a hack!
	T* tmp_ref = (T*) malloc(vec->element_size);
	char* tmp_buff = (char*) buff;
	memcpy(tmp_ref, tmp_buff + block_offset, vec->element_size);

	free(buff);

	return *tmp_ref;
}

template <typename T>
bool vector<T>::iterator::operator>= (const vector<T>::iterator& rhs) {
	if(index>=vec->sz && rhs.index>=vec->sz)
		return true;

	return index >= rhs.index;
}

template <typename T>
bool vector<T>::iterator::operator<= (const vector<T>::iterator& rhs) {
	if(index<0 && rhs.index<0)
		return true;

	return index <= rhs.index;
}

template <typename T>
typename vector<T>::iterator vector<T>::begin() {
	iterator iter;
	return iter;
}

template <typename T>
typename vector<T>::iterator vector<T>::end() {
	iterator iter;
	iter.index = sz;
	return iter;
}

template <typename T>
const T& vector<T>::const_iterator::operator* () const {
	long block_number = (index / vec->num_elements_per_block) + 1;
	long block_offset = (index % vec->num_elements_per_block) * vec->element_size;

	void* buff = malloc(vec->block_size);
	vec->buffered_file->readBlock(block_number, buff);

	//not sure if this is the right way do it. Right now this is just a hack!
	T* tmp_ref = (T*) malloc(vec->element_size);
	char* tmp_buff = (char*) buff;
	memcpy(tmp_ref, tmp_buff + block_offset, vec->element_size);

	free(buff);

	return *tmp_ref;
}

template <typename T>
bool vector<T>::const_iterator::operator>= (const vector<T>::const_iterator& rhs) {
	if(index>=vec->sz && rhs.index>=vec->sz)
		return true;

	return index >= rhs.index;
}

template <typename T>
bool vector<T>::const_iterator::operator<= (const vector<T>::const_iterator& rhs) {
	if(index<0 && rhs.index<0)
		return true;

	return index <= rhs.index;
}

template <typename T>
typename vector<T>::const_iterator vector<T>::cbegin() {
	const_iterator iter;
	return iter;
}

template <typename T>
typename vector<T>::const_iterator vector<T>::cend() {
	const_iterator iter;
	iter.index = sz;
	return iter;
}

template <typename T>
T& vector<T>::reverse_iterator::operator* () {
	long block_number = (index / vec->num_elements_per_block) + 1;
	long block_offset = (index % vec->num_elements_per_block) * vec->element_size;

	void* buff = malloc(vec->block_size);
	buffered_file->readBlock(block_number, buff);

	//not sure if this is the right way do it. Right now this is just a hack!
	T* tmp_ref = (T*) malloc(vec->element_size);
	char* tmp_buff = (char*) buff;
	memcpy(tmp_ref, tmp_buff + block_offset, vec->element_size);

	free(buff);

	return *tmp_ref;
}

template <typename T>
bool vector<T>::reverse_iterator::operator>= (const vector<T>::reverse_iterator& rhs) {
	if(index>=vec->sz && rhs.index>=vec->sz)
		return true;

	return index >= rhs.index;
}

template <typename T>
bool vector<T>::reverse_iterator::operator<= (const vector<T>::reverse_iterator& rhs) {
	if(index<0 && rhs.index<0)
		return true;

	return index <= rhs.index;
}

template <typename T>
typename vector<T>::reverse_iterator vector<T>::rbegin() {
	reverse_iterator iter;
	return iter;
}

template <typename T>
typename vector<T>::reverse_iterator vector<T>::rend() {
	reverse_iterator iter;
	iter.index = 0;
	return iter;
}

template <typename T>
T& vector<T>::operator[] (vector<T>::size_type n) {
	if(n >= sz)
		throw std::out_of_range{"vector<T>::operator[]"};

	long block_number = (n / num_elements_per_block) + 1;
	long block_offset = (n % num_elements_per_block) * element_size;

	void* buff = malloc(block_size);
	buffered_file->readBlock(block_number, buff);

	//not sure if this is the right way do it. Right now this is just a hack!
	T* tmp_ref = (T*) malloc(element_size);
	char* tmp_buff = (char*) buff;
	memcpy(tmp_ref, tmp_buff + block_offset, element_size);

	free(buff);

	return *tmp_ref;
}

template <typename T>
void vector<T>::push_back(const T& elem) {
	long block_number = (sz / num_elements_per_block) + 1;
	long block_offset = (sz % num_elements_per_block) * element_size;

	void* buff = malloc(block_size);

	if((block_offset + element_size) > block_size) {
		long new_block = buffered_file->allotBlock();
		buffered_file->readBlock(new_block, buff);                
	} else {
		buffered_file->readBlock(block_number, buff);
	}	

	//not sure if this is the right way do it. Right now this is just a hack!	
	char* tmp_buff = (char*) buff;
	memcpy(tmp_buff + block_offset, &elem, element_size);

	buffered_file->writeBlock(block_number, buff);

	free(buff);

	sz++;
}

template <typename T>
void vector<T>::pop_back() {
	if(sz>0)
		sz--;

	if( sz % num_elements_per_block == 0 )
	{
		buffered_file->deleteBlock( (sz/num_elements_per_block) + 1 );
	}
}

template <typename T>
void vector<T>::clear() {
	for(auto i = (sz/num_elements_per_block) + 1; i > 0; i--)
	{
		buffered_file->deleteBlock(i);
	}
	sz = 0;
}

/* incomplete */
template <typename T>
void vector<T>::insert(vector<T>::iterator position, const T& val) {

	if(position.index > size)
		throw std::out_of_range{"vector<T>::insert()"};

	long block_number = (position.index / num_elements_per_block) + 1;
	long block_offset = (position.index % num_elements_per_block) * element_size;

	long max_block_number = (sz / num_elements_per_block) + 1;
	long max_block_offset = (sz % num_elements_per_block) * element_size;

	if(max_block_number == block_number)
	{
		if(max_block_offset == 0)
		{
			void *buff = malloc(block_size);
			buffered_file->readBlock(block_number,buff);

			long new_block = buffered_file->allotBlock();
			void *new_buff = malloc(block_size);

			char* temp_buff = (char*)buff;
			memcpy(new_buff,temp_buff + (element_size*(num_elements_per_block-1)),element_size);
			buffered_file->writeBlock(new_block,new_buff);

			memcpy(new_buff,temp_buff + block_offset,block_size - element_size - block_offset);
			memcpy(temp_buff + block_offset + element_size, new_buff, block_size - element_size - block_offset);
			memcpy(temp_buff + block_offset, &val, element_size);
			buffered_file->writeBlock(block_number,buff);
		}
		else
		{
			void *buff = malloc(block_size);
			buffered_file->readBlock(block_number,buff);
			void *new_buff = malloc(block_size);
			char* temp_buff = (char*)buff;
			memcpy(new_buff,temp_buff + block_offset, max_block_offset - block_offset);
			memcpy(temp_buff + block_offset + element_size, new_buff, max_block_offset - block_offset);
			memcpy(temp_buff + block_offset, &val, element_size);
			buffered_file->writeBlock(block_number,buff);
		}
	}
	else
	{
		void *buff = malloc(block_size);
		buffered_file->readBlock(block_number,buff);

		char* temp_buff = (char*)buff;
		const T* tempT = (T*)malloc(element_size);
		void* temp_ptr = (void*)tempT;
		memcpy(temp_ptr, temp_buff + block_size - element_size, element_size);

		void *new_buff = malloc(block_size);
		memcpy(new_buff,temp_buff + block_offset, block_size - element_size);
		memcpy(temp_buff + block_offset + element_size, new_buff, block_size - element_size);
		memcpy(temp_buff + block_offset, &val, element_size);
		buffered_file->writeBlock(block_number,buff);

		position.index = block_number*num_elements_per_block;
		insert(position, tempT);
	}
}
