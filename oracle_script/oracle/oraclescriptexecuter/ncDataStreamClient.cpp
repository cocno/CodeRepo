/******************************************************************************
ncDataStreamClient.cpp:
	Copyright (c) Eisoo Software, Inc.(2015), All rights reserved.

Purpose:


Author:
	peng.ting (peng.ting@eisoo.com)

Creating Time:
	2016-11-2
******************************************************************************/

#include <abprec.h>
#include <ncutil.h>
#include "ncDataStreamClient.h"
#include "ncOracleConfig.h"
#include "ncOracleExecuterCommon.h"

ncDataStreamClient::ncDataStreamClient (string& oraSid, string& initId)
					: _oraSid (oraSid)
					, _initId (initId)
					, _index (0)
{
	NC_ORACLE_EXECUTER_TRACE(_T("ncDataStreamClient::ncDataStreamClient ()"));
}

ncDataStreamClient::ncDataStreamClient ()
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncDataStreamClient::ncDataStreamClient ()"));
}

ncDataStreamClient::~ncDataStreamClient ()
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncDataStreamClient::~ncDataStreamClient ()"));
}

String
ncDataStreamClient::getInitID ()
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncDataStreamClient::getInitID () - begin oraSid : %s"), oraSid);

	_index = (++_index) % (DATA_STREAM_COUNTS - 1);
	string descriptor = "/" + _oraSid.c_str ();

	mq_attr attr;
	attr.mq_maxmsg = MAXMSGNUM;
	attr.mq_msgsize = MAXMSGSIZE;
	_mqdSid = mq_open (descriptor.c_str(), O_RDWR, 0644, &attr);

	// 等待 MML 发送初始化文件描述信息
	descriptor += "_sem";
	_semSid = sem_open (descriptor.c_str(), 0);
	sem_wait (_semSid);

	mq_attr attr;
	mq_getattr (_mqdSid, &attr);
	int msgNum = attr.mq_curmsgs;
	NC_ORACLE_EXECUTER_TRACE (_T("ncDataStreamServer::getInitID () msgNum: %d"), msgNum);

	char* buf = (char*) malloc (FILENAMESIZE);
	mq_getattr (_mqdSid, &attr);
	mq_receive (_mqdSid, buf, attr.mq_msgsize, NULL);

	_initId = buf;
	_pid = _initId.substr (0, _initId.find_first_of ('_') - 1);
	NC_ORACLE_EXECUTER_TRACE (_T("ncDataStreamClient::getInitID () _initId: %s, _pid: %s"), _initId.c_str (), _pid.c_str ());

	sem_post (_semSid);

	return String (_initId.c_str ());
	NC_ORACLE_EXECUTER_TRACE (_T("ncDataStreamClient::getInitID () - end"));
}

void
ncDataStreamClient::initHandle ()
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncDataStreamClient::receiveMessage () - begin"));
	string descriptor = "/" + _oraSid + "_" + _initId;
	for (size_t i = 0; i < DATA_STREAM_COUNTS; ++i) {
		string tmp = descriptor + "_" + to_string (i);
		int fd = shm_open (tmp.c_str(), O_RDWR, 0666);
		_mmapPtr.push_back ((unsigned char*) mmap (NULL, INIT_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
		close (fd);
	}

	for (size_t i = 0; i < DATA_STREAM_COUNTS; ++i) {
		string tmp = descriptor + "_write_" + to_string(i);
		_canWrite.push_back (sem_open (tmp.c_str(), 0));
		tmp = descriptor + "_read_" + to_string(i);
		_canRead.push_back (sem_open (tmp.c_str(), 0));
	}

	mq_attr attr;
	attr.mq_maxmsg = MAXMSGNUM;
	attr.mq_msgsize = MAXMSGSIZE;
	for (size_t i = 0; i < DATA_STREAM_COUNTS; ++i) {
		string tmp = descriptor + "_que_" + to_string(i);
		_msqVec.push_back (mq_open (tmp.c_str(), O_RDWR, 0666, &attr));
	}
	NC_ORACLE_EXECUTER_TRACE(_T("ncDataStreamClient::receiveMessage () - end"));
}

int
ncDataStreamClient::receiveMessage (String& msgContent)
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncDataStreamClient::receiveMessage () - begin"));
	int msgType = 0;

	sem_wait (_canRead[_index]);

	mq_attr attr;
	mq_getattr(_msqVec[_index], &attr);
	int msgNum = attr.mq_curmsgs;
	NC_ORACLE_EXECUTER_TRACE (_T("ncDataStreamServer::receiveMessage () msgNum: %d"), msgNum);

	char* buf = (char*) malloc (MAXMSGSIZE);
	mq_getattr(_msqVec[_index], &attr);
	mq_receive(_msqVec[_index], buf, attr.mq_msgsize, NULL);

	String msg (buf);
	free (buf);
	NC_ORACLE_EXECUTER_TRACE (_T("ncDataStreamServer::receiveMessage () _msqVec[%d]: %s"), _index, msg.getCStr());

	msgContent = msg;
	if (msg.startsWith(_T("NAME:::"))) {
		//NC_ORACLE_EXECUTER_TRACE(_T("ncDataStreamServer::receiveMessage () - NAME: %s"), );
		msgType = MSG_PIECE_NAME;
	}
	else if (msg.startsWith (_T("BLOCK:::"))) {
		//NC_ORACLE_EXECUTER_TRACE(_T("ncDataStreamServer::receiveMessage () - BLOCK SIZE: %s"), );
		msgType = MSG_BLOCK_SIZE;

	}
	else if (msg.startsWith(_T("BUFF:::"))) {
		// NC_ORACLE_EXECUTER_TRACE(_T("ncDataStreamServer::receiveMessage () - BUFF"));
		msgType = MSG_BUFFER_SIZE;
	}
	else if (msg.startsWith(_T("ENDUP:::"))) {
		//NC_ORACLE_EXECUTER_TRACE(_T("ncDataStreamServer::receiveMessage () - BLOCK SIZE: %s"), );
		msgType = MSG_DATA_END;
	}
	else {
		NC_ORACLE_EXECUTER_TRACE (_T("ncDataStreamServer::receiveMessage () !!!"));
	}

	NC_ORACLE_EXECUTER_TRACE(_T("ncDataStreamClient::receiveMessage () - end"));
	return msgType;
}

void
ncDataStreamServer::messageEnd ()
{
	sem_post (_canWrite[_index]);
}

void
ncDataStreamServer::closeHandle ()
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncDataStreamServer::closeHandle ()"));
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
