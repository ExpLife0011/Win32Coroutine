#pragma once

#include <Windows.h>
#include "win32co_queue.h"

namespace Win32Coroutine {

	//��Ϣ���ж���
	//����Queue��
	typedef struct _COROUTINE_MESSAGE_QUEUE {
		SLIST_HEADER QueueHeader;
		SLIST_HEADER PendingQueue;
		SLIST_HEADER WorkerQueue;
		PVOID Fiber;
	}COROUTINE_MESSAGE_QUEUE, *PCOROUTINE_MESSAGE_QUEUE;

	//��Ϣ���нڵ�
	typedef struct _COROUTINE_MESSAGE_NODE {
		SLIST_ENTRY QueueNode;
		PVOID UserBuffer;
		SIZE_T BufferSize;
	}COROUTINE_MESSAGE_NODE, *PCOROUTINE_MESSAGE_NODE;

	//���÷���
	//typedef struct _COROUTINE_MESSAGE_QUEUE {
	//	LIST_ENTRY QueueHead;
	//	SRWLOCK QueueLock;
	//	PVOID Fiber;
	//}COROUTINE_MESSAGE_QUEUE, *PCOROUTINE_MESSAGE_QUEUE;

	////��Ϣ���нڵ�
	//typedef struct _COROUTINE_MESSAGE_NODE {
	//	LIST_ENTRY QueueNode;
	//	PVOID UserBuffer;
	//	SIZE_T BufferSize;
	//}COROUTINE_MESSAGE_NODE, *PCOROUTINE_MESSAGE_NODE;

	namespace MessageQueue {

		/**
		 * ����һ����Ϣ����
		 */
		HANDLE
			CoCreateMessageQueue(
			);

		/**
		 * ����Ϣ�����в���һ����Ϣ
		 * @param	MessageQueue		Ŀ����Ϣ����
		 * @param	Message				��������Ϣ
		 * @param	MessageSize			��Ϣ��С
		 */
		BOOLEAN
			CoEnqueueMessage(
				PCOROUTINE_MESSAGE_QUEUE MessageQueue,
				PVOID Message,
				SIZE_T MessageSize
			);

		/**
		 * ����Ϣ������ȡ��һ����Ϣ
		 * @param	MessageQueue	��Ϣ����
		 * @param	MessageSize		��Ϣ��С
		 */
		PVOID
			CoDequeueMessage(
				PCOROUTINE_MESSAGE_QUEUE MessageQueue,
				SIZE_T* MessageSize
			);

		/**
		 * ɾ��һ����Ϣ����
		 */
		VOID
			CoDeleteQueue(
				PCOROUTINE_MESSAGE_QUEUE MessageQueue
			);

	}

}