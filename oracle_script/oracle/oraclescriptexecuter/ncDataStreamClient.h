/******************************************************************************
ncDataStreamClient.h:
	Copyright (c) Eisoo Software, Inc.(2015), All rights reserved.

Purpose:


Author:
	peng.ting (peng.ting@eisoo.com)

Creating Time:
	2016-11-2
******************************************************************************/

#ifndef __NC_DATA_STREAM__
#define __NC_DATA_STREAM__

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

class ncDataStreamClient
{
public:
	ncDataStreamClient (string& oraSid, string& initId);
	ncDataStreamClient ();
	~ncDataStreamClient ();

	String getInitID ();
	void initHandle ();
	int receiveMessage (String& msgContent);
	void closeHandle ();

	void setOraSid (String& oraSid)
	{
		_oraSid = ::toSTLString (oraSid);
	}

	size_t getMmapPtr ()
	{
		return _mmapPtr[_index];
	}

private:
	string						_oraSid;
	string						_pid;
	string						_initId;

	sem_t*						_semSid;
	mqd_t						_mqdSid;

	vector<sem_t*>				_canWrite;
	vector<sem_t*>				_canRead;
	vector<mqd_t>				_msqVec;
	vector<unsigned char*>		_mmapPtr;

	size_t						_index;
}

#endif // __NC_DATA_STREAM__
