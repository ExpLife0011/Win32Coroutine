
#include "win32_queue.h"

/**
 * �����������е�����
 */
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