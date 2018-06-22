#pragma once

#include <Windows.h>
#include "win32co_queue.h"

namespace Win32Coroutine {

	//消息队列对象
	//由于Queue是
	typedef struct _COROUTINE_MESSAGE_QUEUE {
		SLIST_HEADER QueueHeader;
		SLIST_HEADER PendingQueue;
		SLIST_HEADER WorkerQueue;
		PVOID Fiber;
	}COROUTINE_MESSAGE_QUEUE, *PCOROUTINE_MESSAGE_QUEUE;

	//消息队列节点
	typedef struct _COROUTINE_MESSAGE_NODE {
		SLIST_ENTRY QueueNode;
		PVOID UserBuffer;
		SIZE_T BufferSize;
	}COROUTINE_MESSAGE_NODE, *PCOROUTINE_MESSAGE_NODE;

	//备用方案
	//typedef struct _COROUTINE_MESSAGE_QUEUE {
	//	LIST_ENTRY QueueHead;
	//	SRWLOCK QueueLock;
	//	PVOID Fiber;
	//}COROUTINE_MESSAGE_QUEUE, *PCOROUTINE_MESSAGE_QUEUE;

	////消息队列节点
	//typedef struct _COROUTINE_MESSAGE_NODE {
	//	LIST_ENTRY QueueNode;
	//	PVOID UserBuffer;
	//	SIZE_T BufferSize;
	//}COROUTINE_MESSAGE_NODE, *PCOROUTINE_MESSAGE_NODE;

	namespace MessageQueue {

		/**
		 * 创建一个消息队列
		 */
		HANDLE
			CoCreateMessageQueue(
			);

		/**
		 * 向消息队列中插入一条消息
		 * @param	MessageQueue		目标消息队列
		 * @param	Message				待发送消息
		 * @param	MessageSize			消息大小
		 */
		BOOLEAN
			CoEnqueueMessage(
				PCOROUTINE_MESSAGE_QUEUE MessageQueue,
				PVOID Message,
				SIZE_T MessageSize
			);

		/**
		 * 从消息队列中取出一条消息
		 * @param	MessageQueue	消息队列
		 * @param	MessageSize		消息大小
		 */
		PVOID
			CoDequeueMessage(
				PCOROUTINE_MESSAGE_QUEUE MessageQueue,
				SIZE_T* MessageSize
			);

		/**
		 * 删除一个消息队列
		 */
		VOID
			CoDeleteQueue(
				PCOROUTINE_MESSAGE_QUEUE MessageQueue
			);

	}

}