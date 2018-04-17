
#include "win32_coroutine.h"
#include "win32_queue.h"

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
