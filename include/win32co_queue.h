#pragma once

#include <Windows.h>
#include <interlockedapi.h>

#define CoqInitializeQueue(Queue)		InitializeSListHead(Queue)

#define CoqGetQueueDepth(Queue)			QueryDepthSList(Queue)

#define CoqEnqueue(Queue,Entry)			InterlockedPushEntrySList(Queue,Entry)

#define CoqDequeue(Queue)				InterlockedPopEntrySList(Queue)

/**
 * 交换两个队列的内容
 */
FORCEINLINE
VOID
CoqSwapQueue(
	PSLIST_HEADER q1,
	PSLIST_HEADER q2
) {

	PSLIST_ENTRY q1_List = InterlockedFlushSList(q1);
	PSLIST_ENTRY q2_List = InterlockedFlushSList(q2);

	if (q1_List) {
		InterlockedPushEntrySList(q2, q1_List);
	}

	if (q2_List) {
		InterlockedPushEntrySList(q1, q2_List);
	}
}