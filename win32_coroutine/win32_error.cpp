
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "win32_error.h"

//这个也是抄的

/**
 * 显示致命错误并退出
 */
VOID
CoShowError(
	INT Errno, 
	PSTR Syscall,
	ERROR_LEVEL Errlevel
) {

	PSTR Buf = NULL;
	PCSTR ErrMsg;

	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS, NULL, Errno,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&Buf, 0, NULL);

	if (Buf) {
		ErrMsg = Buf;
	}
	else {
		ErrMsg = "Unknown error";
	}

	/* FormatMessage messages include a newline character already, */
	/* so don't add another. */
	if (Syscall) {
		fprintf(stderr, "%s: (%d) %s", Syscall, Errno, ErrMsg);
	}
	else {
		fprintf(stderr, "(%d) %s", Errno, ErrMsg);
	}

	if (Buf) {
		LocalFree(Buf);
	}

	if (Errlevel == FatalError) {
		DebugBreak();
		abort();
	}
}