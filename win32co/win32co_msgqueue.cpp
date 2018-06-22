
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

			PCOROUTINE_MESSAGE_NODE Node = (PCOROUTINE_MESSAGE_NODE)_aligned_malloc(sizeof(*Node), 8);
			if (Node == NULL)
				return FALSE;

			//分配一个消息结构体
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
			PCOROUTINE_MESSAGE_NODE Node;
			PSLIST_ENTRY Entry;
			USHORT PendingCount;
			PVOID UserBuffer;

			//工作队列中没有消息了
			if (CoqGetQueueDepth(&MessageQueue->WorkerQueue) == 0) {

				PendingCount = CoqGetQueueDepth(&MessageQueue->PendingQueue);
				SwapCount = min(PendingCount, 20);

				//等待队列中还有节点
				if (SwapCount != 0) {

					//就将等待队列中的节点反序交换到工作队列中，最多10个,防止饥饿
				TRANSMIT:
					while (SwapCount--)
						CoqEnqueue(&MessageQueue->WorkerQueue,
							CoqDequeue(&MessageQueue->PendingQueue)
						);
				}
				else {

					//接收队列与等待队列交换
					CoqSwapQueue(&MessageQueue->QueueHeader, &MessageQueue->PendingQueue);
					if (CoqGetQueueDepth(&MessageQueue->PendingQueue) == 0)
						return NULL;

					goto TRANSMIT;
				}

			}

			Node = (PCOROUTINE_MESSAGE_NODE)CoqDequeue(&MessageQueue->WorkerQueue);
			*MessageSize = Node->BufferSize;
			UserBuffer = Node->UserBuffer;

			_aligned_free(Node);

			return UserBuffer;
		}

		/**
		 * 删除一个消息队列
		 */
		VOID
			CoDeleteQueue(
				PCOROUTINE_MESSAGE_QUEUE MessageQueue
			) {

			free(MessageQueue);
		}

	}

}