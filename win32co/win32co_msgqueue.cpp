
#include "win32co_coroutine.h"

namespace Win32Coroutine {

	namespace MessageQueue {

		/**
		 * 创建一个消息队列
		 */
		HANDLE
			CoCreateMessageQueue(
			) {

			PCOROUTINE_MESSAGE_QUEUE MessageQueue = (PCOROUTINE_MESSAGE_QUEUE)malloc(sizeof(COROUTINE_MESSAGE_QUEUE));
			if (MessageQueue == NULL)
				return NULL;

			CoqInitializeQueue(&MessageQueue->QueueHeader);

			return (HANDLE)MessageQueue;
		}

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
			) {

			PCOROUTINE_MESSAGE_NODE Node = (PCOROUTINE_MESSAGE_NODE)malloc(sizeof(*Node));
			if (Node == NULL)
				return FALSE;

			Node->UserBuffer = malloc(MessageSize);
			if (Node->UserBuffer == NULL) {
				free(Node);
				return FALSE;
			}
			Node->BufferSize = MessageSize;
			memcpy(Node->UserBuffer, Message, Node->BufferSize);

			CoqEnqueue(&MessageQueue->QueueHeader, &Node->QueueNode);

			return TRUE;
		}

		/**
		 * 从消息队列中取出一条消息
		 * @param	MessageQueue	消息队列
		 * @param	MessageSize		消息大小
		 */
		PVOID
			CoDequeueMessage(
				PCOROUTINE_MESSAGE_QUEUE MessageQueue,
				SIZE_T* MessageSize
			) {

			ULONG SwapCount;
			PSLIST_ENTRY Entry;
			SLIST_HEADER TempList;
			InitializeSListHead(&TempList);

			//工作队列中没有消息了
			if (CoqGetQueueDepth(&MessageQueue->WorkerQueueHeader) == 0) {

				//等待队列中还有节点
				if (CoqGetQueueDepth(&MessageQueue->PendingQueueHeader) != 0) {

					//就将等待队列中的节点反序交换到工作队列中，最多10个
				TRANSMIT:
					SwapCount = 10;
					while (SwapCount--) {
						Entry = CoqDequeue(&MessageQueue->PendingQueueHeader);
						if (Entry == NULL)
							break;

						CoqEnqueue(&MessageQueue->WorkerQueueHeader, Entry);
					}
				}
				else {
					CoqSwapQueue(&MessageQueue->QueueHeader, &MessageQueue->PendingQueueHeader);
					if (CoqGetQueueDepth(&MessageQueue->PendingQueueHeader) == 0)
						return NULL;

					goto TRANSMIT;
				}

			}

			Entry = CoqDequeue(&MessageQueue->WorkerQueueHeader);
			if (Entry == NULL);
			//error

			return Entry;
		}

	}

}