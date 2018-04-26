#pragma once

#include <Windows.h>

namespace Win32Coroutine {

	namespace Hook {

		/**
		 * 对文件IO函数进行hook
		 * 包括：
		 *	CreateFileW
		 *	ReadFile
		 *	WriteFile
		 *	DeviceIoControl
		 */
		COEXPORT
		BOOLEAN
			CoSetupFileIoHook(
				PWSTR ModuleName
			);

		/**
		 * 对socket函数进行hook
		 * 包括：
		 *	socket
		 *	accept
		 *	send
		 *	recv
		 */
		COEXPORT
		BOOLEAN
			CoSetupNetIoHook(
				PWSTR ModuleName
			);

	}

}