/******************************************************************************
ncDataStreamServer.h:
Copyright (c) Eisoo Software, Inc.(2015), All rights reserved.

Purpose:


Author:
peng.ting (peng.ting@eisoo.com)

Creating Time:
2016-11-2
******************************************************************************/

#ifndef __NC_DATA_STREAM_SERVER_H__
#define __NC_DATA_STREAM_SERVER_H__

#include <string>
#include <vector>

#ifndef __WINDOWS__
#define 
	#include <semaphore.h>
	#include <mqueue.h>
	#include <sys/mman.h>
#endif


using std::string;
using std::vector;

class ncDataStreamServer
{
public:
	ncDataStreamServer ();
	~ncDataStreamServer ();

	void sendInitId ();
	void initHandle ();
	void postMessage (const int msgType, const string& msgContent, void* buffer);
	void messageEnd ();
	void closeHandle ();

private:
	//void shrinkMmapSize ();
	//bool fillInBlock ();
	bool whetherPost ();

private:
	string						_oracleSid;
	string						_pid;
	string						_tid;

	sem_t*						_semSid;
	mqd_t						_mqdSid;

	vector<sem_t*>				_canWrite;
	vector<sem_t*>				_canRead;
	vector<mqd_t>				_msqVec;
	vector<unsigned char*>		_mmapPtr;
	vector<long>				_currentSize;

	long						_defaultBlockSize;
	size_t						_index;
}

#endif // __NC_DATA_STREAM_SERVER_H__
