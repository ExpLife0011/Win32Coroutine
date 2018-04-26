
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "win32co_error.h"

//这个也是抄的

namespace Win32Coroutine {

	namespace Error {

		/**
		 * 报告自身错误
		 */
		COEXPORT
			VOID
			CoShowCoroutineError(
				PSTR ErrMsg,
				PSTR FuncName,
				ERROR_LEVEL Errlevel
			) {

			if (FuncName)
				fprintf(stderr, "Coroutine Error: (%s) %s", FuncName, ErrMsg);
			else
				fprintf(stderr, "Coroutine Error: %s", ErrMsg);

			if (Errlevel == FatalError) {
				DebugBreak();
				abort();
			}
		}

		/**
		 * 显示系统错误并退出
		 */
		COEXPORT
			VOID
			CoShowSystemError(
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
				fprintf(stderr, "Win32 Error: %s - (%d) %s", Syscall, Errno, ErrMsg);
			}
			else {
				fprintf(stderr, "Win32 Error: (%d) %s", Errno, ErrMsg);
			}

			if (Buf) {
				LocalFree(Buf);
			}

			if (Errlevel == FatalError) {
				DebugBreak();
				abort();
			}
		}

		VOID
			CoError_LowMemory(
			) {

			CoShowSystemError(ERROR_NOT_ENOUGH_MEMORY, NULL, Error::ERROR_LEVEL::Error);
		}

	}

}