
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
			PSLIST_ENTRY Entry;
			SLIST_HEADER TempList;
			InitializeSListHead(&TempList);

			//����������û����Ϣ��
			if (CoqGetQueueDepth(&MessageQueue->WorkerQueueHeader) == 0) {

				//�ȴ������л��нڵ�
				if (CoqGetQueueDepth(&MessageQueue->PendingQueueHeader) != 0) {

					//�ͽ��ȴ������еĽڵ㷴�򽻻������������У����10��
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