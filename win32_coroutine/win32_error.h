#pragma once

#include <Windows.h>

typedef enum {
	Warning,
	Error,
	FatalError
}ERROR_LEVEL;

/**
 * ��ʾ���������˳�
 */
VOID
CoShowError(
	INT Errno,
	PSTR Syscall,
	ERROR_LEVEL Errlevel
);