
#include "win32co_coroutine.h"

namespace Win32Coroutine {

	namespace MessageQueue {

		/**
		 * ����һ����Ϣ����
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
			) {

			PCOROUTINE_MESSAGE_NODE Node = (PCOROUTINE_MESSAGE_NODE)_aligned_malloc(sizeof(*Node), 8);
			if (Node == NULL)
				return FALSE;

			//����һ����Ϣ�ṹ��
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
		 * ����Ϣ������ȡ��һ����Ϣ
		 * @param	MessageQueue	��Ϣ����
		 * @param	MessageSize		��Ϣ��С
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

			//����������û����Ϣ��
			if (CoqGetQueueDepth(&MessageQueue->WorkerQueue) == 0) {

				PendingCount = CoqGetQueueDepth(&MessageQueue->PendingQueue);
				SwapCount = min(PendingCount, 20);

				//�ȴ������л��нڵ�
				if (SwapCount != 0) {

					//�ͽ��ȴ������еĽڵ㷴�򽻻������������У����10��,��ֹ����
				TRANSMIT:
					while (SwapCount--)
						CoqEnqueue(&MessageQueue->WorkerQueue,
							CoqDequeue(&MessageQueue->PendingQueue)
						);
				}
				else {

					//���ն�����ȴ����н���
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
		 * ɾ��һ����Ϣ����
		 */
		VOID
			CoDeleteQueue(
				PCOROUTINE_MESSAGE_QUEUE MessageQueue
			) {

			free(MessageQueue);
		}

	}

}