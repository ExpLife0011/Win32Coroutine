#pragma once

#include <Windows.h>
#include <interlockedapi.h>

#define CoqInitializeQueue(Queue)		InitializeSListHead(Queue)

#define CoqGetQueueDepth(Queue)			QueryDepthSList(Queue)

#define CoqEnqueue(Queue,Entry)			InterlockedPushEntrySList(Queue,Entry)

#define CoqDequeue(Queue)				InterlockedPopEntrySList(Queue)

/**
 * �����������е�����
 */
VOID
CoqSwapQueue(
	PSLIST_HEADER q1,
	PSLIST_HEADER q2
);