#pragma once

#include <Windows.h>

#define COEXPORT __declspec(dllexport)

namespace Win32Coroutine {

	namespace Error {

		typedef enum {
			Warning,
			Error,
			FatalError
		}ERROR_LEVEL;

		/**
		 * 报告自身错误
		 */
		COEXPORT
			VOID
			CoShowCoroutineError(
				PSTR ErrMsg,
				PSTR FuncName,
				ERROR_LEVEL Errlevel
			);

		/**
		 * 显示致命错误并退出
		 */
		COEXPORT
			VOID
			CoShowSystemError(
				INT Errno,
				PSTR Syscall,
				ERROR_LEVEL Errlevel
			);

		VOID
			CoError_LowMemory(
			);

	}

}