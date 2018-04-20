#pragma once

#include <Windows.h>

typedef enum {
	Warning,
	Error,
	FatalError
}ERROR_LEVEL;

/**
 * ÏÔÊ¾ÖÂÃü´íÎó²¢ÍË³ö
 */
VOID
CoShowError(
	INT Errno,
	PSTR Syscall,
	ERROR_LEVEL Errlevel
);