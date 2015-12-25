#include "buffer.h"
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
//#include <cstring>
#include <string>

using namespace std;

template<typename T>
class Vector
{
private:
	int len;
	int blksize;
	int elemsize;
	int elemperblk;
	string *nameondisk;
	BufferedFile *externfile;
	void* header;
	bool isFull();
	
public:
	Vector(string _nameondisk);
	~Vector();
	void pushBack(T _elem);
	T* pop();
	T* getElem(int index);
	void deletePersistentCopy();

};

template<typename T>
Vector<T>::Vector(string _nameondisk, string _devicename)
{
	nameondisk = new string(_nameondisk);
	blksize = getDeviceBlockSize(_devicename.c_str());
	elemsize = sizeof(T);
	elemperblk = blksize/elemsize;

	externfile = new BufferedFile(blksize,this->nameondisk->c_str());
	header = (void*)malloc(blksize);
	try
	{
		externfile->readHeader(header);
		int currpos = 0;
		memcpy(&len,(void*)(((uint8_t*)header)+currpos),sizeof(len));
		currpos += sizeof(len);

		//continue header info extraction.

	}catch(exception e)	//add blank file exception
	{
		len = 0;
		int currpos = 0;
		memcpy((void*)(((uint8_t*)header)+currpos),&len,sizeof(len));
		currpos += sizeof(len);

		//continue header info writing

		externfile->writeHeader(header);
	}
}

template<typename T>
bool Vector<T>::isFull()
{
	return len%elemperblk?false:true;
}

template<typename T>
void Vector<T>::pushBack(T _elem)
{
	if(isFull())
	{
		int elemblknum = externfile->allotBlock();
		void* buff = malloc(blksize);
		memcpy(buff,(void*)&_elem,sizeof(_elem));
		externfile->writeBlock(elemblknum,buff);
	}
	else
	{
		int elemblknum = len/elemperblk;
		int blkoffset = len%elemblknum;
		void* buff = malloc(blksize);
		externfile->readBlock(elemblknum,buff);
		memcpy((void*)(((uint8_t*)buff)+blkoffset),(void*)&_elem,sizeof(_elem));
		externfile->writeBlock(elemblknum,buff);
	}
}

template<typename T>
T* Vector<T>::pop()
{
	if(len==0) return NULL;
	void* buff = malloc(blksize);
	void* newbuff = malloc(blksize);
	externfile->readBlock(1,buff);
	memcpy(newbuff,buff,(blksize - elemsize));
	externfile->writeBlock(1,newbuff);
	T* temp = (T*) malloc(sizeof(T));
	memcpy(temp,buff,sizeof(T));
	return temp;
}

template<typename T>
T* Vector<T>::getElem(int index)
{
	int elemblknum = index/elemperblk;
	int blkoffset = (index%elemperblk)*elemsize;

	void *buff = (void*)malloc(blksize);
	externfile->readBlock(elemblknum,buff);

	T* temp = (T*) malloc(sizeof(T));
	memcpy(temp,(void*)(((uint8_t*)buff)+blkoffset),sizeof(T));
	return temp;
}

template<typename T>
void Vector<T>::deletePersistentCopy()
{

}