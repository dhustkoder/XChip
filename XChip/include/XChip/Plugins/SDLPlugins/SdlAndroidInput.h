/*

XChip - A chip8 lib and emulator.
Copyright (C) 2016  Rafael Moura

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see http://www.gnu.org/licenses/gpl-3.0.html.

*/

#ifndef XCHIP_PLUGINS_SDLANDROIDINPUT_H_
#define XCHIP_PLUGINS_SDLANDROIDINPUT_H_


#include <XChip/Plugins/iInput.h>

 

namespace xchip {
	
class SdlAndroidInput final : public iInput
{
	static constexpr const char* const PLUGIN_NAME = "SdlAndroidInput";
	static constexpr const char* const PLUGIN_VER = "SdlAndroidInput 1.0. Using SDL2";
public:
	SdlAndroidInput() noexcept;
	~SdlAndroidInput();
	
	bool Initialize() noexcept override;
	void Dispose() noexcept override;
	bool IsInitialized() const noexcept override;
	const char* GetPluginName() const noexcept override;
	const char* GetPluginVersion() const noexcept override;
	PluginDeleter GetPluginDeleter() const noexcept override;
	bool IsKeyPressed(const Key key) const noexcept override;

	bool UpdateKeys() noexcept override;
	Key WaitKeyPress() noexcept override;

	void SetMiddleScreen(const int middleScreen) noexcept { m_middleScreen = middleScreen; }
	void SetWaitKeyCallback(const void* arg, WaitKeyCallback callback) noexcept override;
	void SetResetKeyCallback(const void* arg, ResetKeyCallback callback) noexcept override;
	void SetEscapeKeyCallback(const void* arg, EscapeKeyCallback callback) noexcept override;

private:
	enum { LEFT = 1, RIGHT };
	uint8_t m_direction = 0;
	int m_middleScreen = 32;
	WaitKeyCallback m_waitClbk = nullptr;
	ResetKeyCallback m_resetClbk = nullptr;
	EscapeKeyCallback m_escapeClbk = nullptr;
	const void* m_waitClbkArg;
	const void* m_resetClbkArg;
	const void* m_escapeClbkArg;
	bool m_initialized = false;

};









}














#endif // XCHIP_PLUGINS_SDLANDROIDINPUT_H_
