/******************************************************************************
ncDataProcessorThread.h:
Copyright (c) Eisoo Software, Inc.(2015), All rights reserved.

Purpose:


Author:
	peng.ting (peng.ting@eisoo.com)

Creating Time:
	2016-11-2
******************************************************************************/

#ifndef __NC_DATA_PROCESSOR_THREAD_H__
#define __NC_DATA_PROCESSOR_THREAD_H__

#include <public/ncIEEFListener.h>

class ncDataProcessorThread : Thread
{
public:
	ncDataProcessorThread( ncBackupParam& backupParam,
							ncIPushStreamEvent* pushStreamEvent,
							int64& timePoint,
							String& oraSid,
							String& initId,
							bool& hasStop );

	virtual ~ncDataProcessorThread ();
private:
	void run ();

	void init ();
	void start ();
	void sendCIDObject ();
	void sendExtensionObject ();
	void sendTimePointObject ();
	void sendDataObject ();
	void sendDataBlock ();

private:
	ncBackupParam&							_backupParam;
	ncIPushStreamEvent*						_pushStreamEvent;
	nsCOMPtr<ncIPushStreamReactor>			_pushReactor;
	nsCOMPtr<ncIEEFListener>				_listener;

	ncDataStreamClient						_dataStream;
	String									_oraSid;

	String									_msgContent;
};

#endif // __NC_DATA_PROCESSOR_THREAD_H__