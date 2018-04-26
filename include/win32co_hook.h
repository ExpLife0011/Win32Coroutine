#pragma once

#include <Windows.h>

namespace Win32Coroutine {

	namespace Hook {

		/**
		 * ���ļ�IO��������hook
		 * ������
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
		 * ��socket��������hook
		 * ������
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