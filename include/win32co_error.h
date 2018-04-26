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
		 * �����������
		 */
		COEXPORT
			VOID
			CoShowCoroutineError(
				PSTR ErrMsg,
				PSTR FuncName,
				ERROR_LEVEL Errlevel
			);

		/**
		 * ��ʾ���������˳�
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