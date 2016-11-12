/******************************************************************************
ncDataStreamServer.cpp:
	Copyright (c) Eisoo Software, Inc.(2015), All rights reserved.

Purpose:


Author:
	peng.ting (peng.ting@eisoo.com)

Creating Time:
	2016-11-2
******************************************************************************/

#include <fstream>
#include "ncDataStreamServer.h"
#include "ncOracleConfig.h"

using std::fstream;

static
void write_trace (const char* fmt, ...)
{
	if (access ("/anyorascript/on_off.txt", 0) == 0) {
		char buf[2048];
		va_list ap;
		va_start(ap, fmt);

		::vsprintf(buf, fmt, ap);

		va_end(ap);
		fstream fs("/anyorascript/server_sbt_log.log", fstream::app | fstream::out | fstream::ate);
		fs << buf << endl;
		fs.close();
	}
}

#ifdef __LINUX__
#define NC_DATA_STREAM_LOG(args...)    \
    ::write_trace ( "[%s %s] [%s %4d] : %s", __DATE__, __TIME__, __FILE__, __LINE__, args)
#else
#define NC_DATA_STREAM_LOG(...)        \
    ::write_trace ( "[%s %s] [%s %4d] : %s", __DATE__, __TIME__, __FILE__, __LINE__, __VA_ARGS__)
#endif


ncDataStreamServer::ncDataStreamServer (string& orasid, string& pid, string& tid)
					: _oracleSid (orasid)
					, _pid (pid)
					, _tid (_tid)
					, _index (0)
{
	NC_DATA_STREAM_LOG (_T("ncDataStreamServer::ncDataStreamServer ()"));

	string descriptor = "/" + _oracleSid;
	NC_DATA_STREAM_LOG (_T("ncDataStreamServer::ncDataStreamServer () descriptor: %s"), descriptor.c_str ());

	mq_attr attr;
	attr.mq_maxmsg = MAXMSGNUM;
	attr.mq_msgsize = MAXMSGSIZE;
	_mqdSid = mq_open (tmp.c_str(), O_RDWR | O_CREAT, 0644, &attr);

	descriptor += "_sem";
	_semSid = sem_open (tmp.c_str(), O_CREAT, 0666, 1);


	//string descriptor = "/" + _oracleSid + "_" + _pid + "_" + _tid;
}

ncDataStreamServer::~ncDataStreamServer ()
{
	NC_DATA_STREAM_LOG (_T("ncDataStreamServer::~ncDataStreamServer ()"));

}

void
ncDataStreamServer::sendInitId ()
{
	NC_DATA_STREAM_LOG (_T("ncDataStreamServer::postInitId ()"));
	// post initId for server side start thread
	string initId = _pid + "_" _tid;
	mq_send (_mqdSid, initId.c_str (), MAXMSGSIZE, 0);

	sem_post (_semSid);
	sem_wait (_semSid);
}

void
ncDataStreamServer::initHandle ()
{
	NC_DATA_STREAM_LOG (_T("ncDataStreamServer::initHandle ()"));
	string descriptor = "/" + _oraSid + "_" + _initId;
	for (size_t i = 0; i < DATA_STREAM_COUNTS; ++i) {
		string tmp = descriptor + "_" + to_string (i);
		int fd = shm_open (tmp.c_str (), O_RDWR | O_CREAT, 0666);
		_mmapPtr.push_back ((unsigned char*) mmap (NULL, INIT_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
		close(fd);
	}

	for (size_t i = 0; i < DATA_STREAM_COUNTS; ++i) {
		string tmp = descriptor + "_write_" + to_string (i);
		_canWrite.push_back (sem_open (tmp.c_str(), O_CREAT, 0666, 1));

		tmp = descriptor + "_read_" + to_string(i);
		_canRead.push_back (sem_open (tmp.c_str(), O_CREAT, 0666, 0));
	}

	mq_attr attr;
	attr.mq_maxmsg = MAXMSGNUM;
	attr.mq_msgsize = MAXMSGSIZE;
	for (size_t i = 0; i < DATA_STREAM_COUNTS; ++i) {
		string tmp = descriptor + "_que_" + to_string (i);
		_msqVec.push_back (mq_open (tmp.c_str(), O_RDWR, 0666, &attr));
	}
}

void
ncDataStreamServer::postMessage (const int msgType, const string& msgContent, void* buffer)
{
	//NC_DATA_STREAM_LOG (_T("ncDataStreamServer::postMessage ()"));

	switch (msgType) {
		case MSG_PIECE_NAME :
			{
				NC_DATA_STREAM_LOG (_T("ncDataStreamServer::postMessage () [ MSG_PIECE_NAME ]: %s"), msgContent.c_str ());
				string msg = "NAME:::" + msgContent;
				mq_send (_msqVec[_index], msg.c_str (), MAXMSGSIZE, 0);
				sem_post (_canRead[_index]);
				_index = (++_index) % (DATA_STREAM_COUNTS - 1);
			}
			break;

		case MSG_BLOCK_SIZE :
			{
				NC_DATA_STREAM_LOG (_T("ncDataStreamServer::postMessage () [ MSG_BLOCK_SIZE ]: %s"), msgContent.c_str());
				_defaultBlockSize = stol (msgContent);
				string msg = "BLOCK:::" + msgContent;
				mq_send (_msqVec[_index], msg.c_str(), MAXMSGSIZE, 0);
				sem_post(_canRead[_index]);
				_index = (++_index) % (DATA_STREAM_COUNTS - 1);
			}
			break;

		case MSG_BUFFER_SIZE :
			{
				//NC_DATA_STREAM_LOG (_T("ncDataStreamServer::postMessage () [ MSG_BUFFER_SIZE ]: %s"), msgContent.c_str());
				long inSize = stol (msgContent);
				_currentSize[_index] += inSize;
				memcpy (_mmapPtr[_index] + _currentSize[_index], buffer, inSize);

				if (whetherPost () == false) {
					return;
				}

				string msg = "BUFF:::" + msgContent;
				mq_send (_msqVec[_index] , msg.c_str(), MAXMSGSIZE, 0);
				sem_post(_canRead[_index]);
				_index = (++_index) % (DATA_STREAM_COUNTS - 1);

				// 准备下一个消息
				sem_wait (_canWrite[_index]);
				_currentSize[_index] = 0;
			}
			break;

		case MSG_DATA_END :
			{
				NC_DATA_STREAM_LOG(_T("ncDataStreamServer::postMessage () [ MSG_DATA_END ]: %s"), msgContent.c_str());
				string msg = "ENDUP:::" + msgContent;
				mq_send (_msqVec[_index], msg.c_str(), MAXMSGSIZE, 0);
				sem_post (_canRead[_index]);
				_index = (++_index) % (DATA_STREAM_COUNTS - 1);
			}
			break;
		default:
			{
				NC_DATA_STREAM_LOG(_T("ncDataStreamServer::postMessage () [ default ] !!!"));
			}
			break;
	}
}

void
ncDataStreamServer::closeHandle ()
{
	NC_DATA_STREAM_LOG(_T("ncDataStreamServer::closeHandle ()"));
	string descriptor = "/" + _oraSid + "_" + _initId;
	for (size_t i = 0; i < DATA_STREAM_COUNTS; ++i) {
		string tmp = descriptor + "_" + to_string(i);
		munmap ((unsigned char*) _mmapPtr[i], INIT_BLOCK_SIZE);
		shm_unlink (tmp.c_str());
	}

	for (size_t i = 0; i < DATA_STREAM_COUNTS; ++i) {
		string tmp = descriptor + "_write_" + to_string(i);
		sem_close (_canWrite[i]);
		sem_unlink (tmp.c_str());

		tmp = descriptor + "_read_" + to_string(i);
		sem_close (_canRead[i]);
		sem_unlink (tmp.c_str());
	}

	for (size_t i = 0; i < DATA_STREAM_COUNTS; ++i) {
		string tmp = descriptor + "_que_" + to_string(i);
		mq_close (_msqVec[i]);
		mq_unlink (tmp.c_str());
	}
}

bool
ncDataStreamServer::whetherPost ()
{
	//
	// 1. 如果当前数据刚好填满，则发送
	// 2. 如果当前数据加上下一次可能的数据大小，会超过默认大小，则发送
	//
	if (_currentSize[_index] == INIT_BLOCK_SIZE)
		return true;
	else if ((_currentSize[_index] + _defaultBlockSize) > INIT_BLOCK_SIZE)
		return true;

	// 继续填充
	return false;
}
