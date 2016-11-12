/******************************************************************************
ncDataProcessorThread.cpp:
	Copyright (c) Eisoo Software, Inc.(2015), All rights reserved.

Purpose:


Author:
	peng.ting (peng.ting@eisoo.com)

Creating Time:
	2016-11-2
******************************************************************************/

#include <abprec.h>
#include <ncutil.h>
#include "ncDataProcessorThread.h"
#include "ncOracleExecuterCommon.h"
#include "ncOracleBackupException.h"

ncDataProcessorThread::ncDataProcessorThread (ncBackupParam& backupParam, ncIPushStreamEvent* pushStreamEvent, int64& timePoint, String& oraSid, String& initId, bool& hasStop)
							: _backupParam (backupParam)
							, _pushStreamEvent (pushStreamEvent)
							, _timePoint (timePoint)
							, _oraSid (oraSid)
							, _initId (initId)
							, _hasStop (hasStop)
							, _pushReactor (0)
							, _listener (0)
							, _dataStream ()
							, _msgContent (String::EMPTY)
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncDataProcessorThread::ncDataProcessorThread ()"));
	_dataStream = new ncDataStreamClient (::toStlString (_oraSid), ::toStlString (_initId));
}

ncDataProcessorThread::~ncDataProcessorThread ()
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncDataProcessorThread::~ncDataProcessorThread () - begin"));
	if (_dataStream != 0) {
		delete _dataStream;
		_dataStream = 0;
	}

	NC_ORACLE_EXECUTER_TRACE(_T("ncDataProcessorThread::~ncDataProcessorThread () - end"));
}

void
ncDataProcessorThread::run ()
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncDataProcessorThread::run () - begin"));
	init ();
	start ();
	NC_ORACLE_EXECUTER_TRACE(_T("ncDataProcessorThread::run () - end"));
}

void
ncDataProcessorThread::	init ()
{
	nsresult result;
	// 创建备份正向数据流组件
	_pushReactor = do_CreateInstance (NC_PUSH_STREAM_REACTOR_CONTRACTID, &result);
	if (NS_FAILED(result)) {
		String errMsg;
		errMsg.format (LOAD_STRING (_T("IDS_CREATE_PUSH_STREAM_REACTOR_FAILED")).getCStr (), result);
		THROW_ORACLE_BACKUP_ERROR (errMsg, ERROR_CREATE_PUSH_STREAM_REACTOR_FAILED);
	}

	// 创建资源管理器组件
	_resourceMgm = do_CreateInstance (NC_RESOURCE_MGM_CONTRACTID, &result);
	if (NS_FAILED(result)) {
		String errMsg;
		errMsg.format (LOAD_STRING (_T("IDS_CREATE_RESOURCE_MANAGER_FAILED")).getCStr (), result);
		THROW_ORACLE_BACKUP_ERROR (errMsg, ERROR_CREATE_RESOURCE_MANAGER_FAILED);
	}

	// 设置备份参数
	nsCOMPtr<ncIPushStreamConfig> pushConfig = getter_AddRefs (_pushReactor->GetPushConfig ());
	pushConfig->SetFastLoginRequest (_backupParam.fastInfo);
	pushConfig->SetCIDInfo (_backupParam.cidInfo);
	pushConfig->SetPushStreamEvent (_pushStreamEvent);
	pushConfig->SetBackupGeneralParam (_backupParam.generalParam);

	_pushReactor->Start(0);
}

void
ncDataProcessorThread::start ()
{
	sendCIDObject ();
	sendExtensionObject ();
	sendTimePointObject ();

	String msgContent;
	_dataStream->initHandle ();
	while (_hasStop) {
		switch (_dataStream->receiveMessage (msgContent)) {
			case MSG_PIECE_NAME :
				{
					_msgContent = msgContent;
					msgContent = String::EMPTY;
					sendDataObject ();
				}
				break;
			case MSG_BLOCK_SIZE :
				{
					_msgContent = msgContent;
					msgContent = String::EMPTY;
					NC_ORACLE_EXECUTER_TRACE (_T("ncDataProcessorThread::start() _msgContent: %s"), _msgContent.getCStr ());
				}
				break;
			case MSG_BUFFER_SIZE :
				{
					_msgContent = msgContent;
					msgContent = String::EMPTY;
					sendDataBlock ();
				}
				break;
			case MSG_DATA_END :
				{
					_msgContent = msgContent;
					msgContent = String::EMPTY;
					NC_ORACLE_EXECUTER_TRACE(_T("ncDataProcessorThread::start() _msgContent: %s"), _msgContent.getCStr());
					return;
				}
			default:
				{
					// THROW_ORACLE_BACKUP_ERROR ();
				}
				break;
		}
		_dataStream->messageEnd ();
	}
}

void
ncDataProcessorThread::sendCIDObject ()
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncDataProcessorThread::sendCIDObject ()"));
	nsCOMPtr<ncICIDObject> cidObj = dont_AddRef ((ncICIDObject*)_resourceMgm->CreateGNSObject (::ncCanonicalizeGNS (_backupParam.backupCID)));
	if (cidObj.get() == 0)
		THROW_ORACLE_BACKUP_ERROR (LOAD_STRING (_T("IDS_CREATE_ORACLE_CID_OBJECT_ERROR")), ERROR_CREATE_ORACLE_CID_OBJECT_ERROR);

	_pushReactor->SendObject (cidObj);
}

void
ncDataProcessorThread::sendExtensionObject ()
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncDataProcessorThread::sendExtensionObject ()"));
	nsCOMPtr<ncIExtensionObject> extensionObj = dont_AddRef((ncIExtensionObject*)_resourceMgm->CreateGNSObject(::ncCanonicalizeGNS(_backupParam.backupCID, EXT_NAME)));
	if (extensionObj.get() == 0)
		THROW_ORACLE_BACKUP_ERROR(LOAD_STRING(_T("IDS_CREATE_ORACLE_EXTENSION_OBJECT_ERROR")), ERROR_CREATE_ORACLE_EXTENSION_OBJECT_ERROR);

	_pushReactor->SendObject(extensionObj);
}

void
ncDataProcessorThread::sendTimePointObject ()
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncDataProcessorThread::sendTimePointObject () - begin"));
	String gns = ::ncCanonicalizeGNS(_backupParam.backupCID, EXT_NAME, _timePoint);
	NC_ORACLE_EXECUTER_TRACE (_T("ncDataProcessorThread::sendTimePointObject () gns:%s"), gns.getCStr());
	_timePointObj = dont_AddRef((ncITimepointObject *)_resourceMgm->CreateGNSObject(gns));

	if (_timePointObj.get() == 0)
		THROW_ORACLE_BACKUP_ERROR (LOAD_STRING (_T("IDS_CREATE_ORACLE_TIMEPOINT_OBJECT_FAILED")), ERROR_CREATE_ORACLE_TIMEPOINT_OBJECT_ERROR);

	int attribute = 0;
	attribute = OBJ_ATTR_BEGIN_TIMEPOINT | OBJ_ATTR_OBJECT_AUTO_LOCK;
	attribute |= OBJ_ATTR_ABORT_RELATED;
	attribute |= OBJ_ATTR_ISOLATE_TIMEPOINT;
	attribute |= OBJ_ATTR_FULL_TIMEPOINT;
	_timePointObj->SetBasicAttributes(attribute);
	_pushReactor->SendObject(_timePointObj);

	gns = ::ncCanonicalizeGNS (_backupParam.backupCID, EXT_NAME, _timePoint, _oraSid);
	nsCOMPtr<ncIDataObject> dataObj = dont_AddRef ((ncIDataObject *) _resourceMgm->CreateGNSObject (gns));
	if (dataObj.get() == 0)
		THROW_ORACLE_BACKUP_ERROR(LOAD_STRING(_T("IDS_CREATE_ORACLE_DATA_OBJECT_FAILED")), ERROR_CREATE_ORACLE_DATA_OBJECT_FAILED);

	_pushReactor->SendObject(dataObj);
	NC_ORACLE_EXECUTER_TRACE(_T("ncDataProcessorThread::sendTimePointObject () - end"));
}

void
ncDataProcessorThread::sendDataObject ()
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncDataProcessorThread::sendDataObject () - begin"));
	String gns = ::ncCanonicalizeGNS (_backupParam.backupCID, EXT_NAME, _timePoint, _oraSid);
	gns = ncGNSUtil::combine (gns, _msgContent);
	NC_ORACLE_SCHEDULE_TRACE (_T("ncDataProcessorThread::sendDataObject () gns:%s"), gns.getCStr());
	_curBackupDataObj = dont_AddRef ((ncIDataObject *)_resourceMgm->CreateGNSObject(gns));
	if (_curBackupDataObj.get() == 0)
		THROW_ORACLE_BACKUP_ERROR (LOAD_STRING(_T("IDS_CREATE_ORACLE_DATA_OBJECT_FAILED")), ERROR_CREATE_ORACLE_DATA_OBJECT_FAILED);
	NC_ORACLE_EXECUTER_TRACE (_T("ncDataProcessorThread::sendDataObject () - end"));
}

void
ncDataProcessorThread::sendDataBlock ()
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncDataProcessorThread::sendDataBlock ()"));

	void* mmapPtr = _dataStream->getMmapPtr ();
	int64 bufSize = Int64::getValue (_msgContent);

	nsCOMPtr<ncIDataBlock> dataBlockObj = getter_AddRefs(_resourceMgm->CreateDataBlock(_curBackupDataObj, _offset));
	if (dataBlockObj.get() == 0)
		THROW_ORACLE_BACKUP_ERROR (LOAD_STRING (_T("IDS_CREATE_ORACLE_DATA_BLOCK_FAILED")), ERROR_CREATE_ORACLE_DATA_BLOCK_FAILED);
	unsigned char* writeBuf = dataBlockObj->CreateWriteBuffer (bufSize);

	memcpy (writeBuf, mmapPtr, bufSize);

	dataBlockObj->SetWriteLength (bufSize);
	_pushReactor->SendObject (dataBlockObj);
	// 实时显示备份进度
	ncPushStreamStatistics backupstctics;
	_pushReactor->GetStatistics (backupstctics);
	int64 currentSize = backupstctics.totalSize;

	String msg;
	msg.format (LOAD_STRING (_T("IDS_CURRENT_BACKUP_FILENUM_MESSAGE")).getCStr (), Int::toString (_fileNum).getCStr());
	_listener->OnProgress (currentSize, msg);
}
