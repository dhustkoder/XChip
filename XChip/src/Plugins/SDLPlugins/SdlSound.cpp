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

// needed for M_PI 
#define _USE_MATH_DEFINES 
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <SDL2/SDL.h>
#include <XChip/Plugins/SDLPlugins/SdlSound.h>
#include <XChip/Utils/Log.h>
#include <XChip/Utils/Timer.h>
#include <XChip/Utils/ScopeExit.h>
#include <XChip/Utils/Assert.h>
 
#define _SDLSOUND_INITIALIZED_ASSERT_() ASSERT_MSG(_initialized == true, "SdlSound is not initialized")

namespace xchip {

using namespace xchip::utils;

extern "C" XCHIP_EXPORT void XCHIP_FreePlugin(const iPlugin*);




inline float SdlSound::GetCurFreq() const { return _curFreq * _specs[Have].freq; }
inline float SdlSound::GetPlayFreq() const { return _playFreq * _specs[Have].freq; }
inline void SdlSound::SetCycleTime(const float hz) { _cycleTime = _specs[Have].freq / hz; }
inline void SdlSound::SetCurFreq(const float hz) { _curFreq = hz / _specs[Have].freq; }
inline void SdlSound::SetPlayFreq(const float hz) { _playFreq = hz / _specs[Have].freq; }
inline void SdlSound::SetLenght(const unsigned int len) { _len = _cycleTime * len; }







SdlSound::SdlSound() noexcept
{
	Log("Creating SdlSound object...");
}



SdlSound::~SdlSound()
{
	if (_initialized)
		this->Dispose();

	Log("Destroying SdlSound object...");
}



bool SdlSound::Initialize() noexcept
{
	using namespace xchip::utils::literals;

	if (_initialized)
		this->Dispose();


	if( SDL_InitSubSystem(SDL_INIT_AUDIO) )
		return false;


	const auto cleanup = make_scope_exit([this]() noexcept 
	{
		if (!this->_initialized)
			this->Dispose();
	});


	_specs = new(std::nothrow) SDL_AudioSpec[2];

	
	if (!_specs) {
		LogError("Could not allocate memory for SDL_AudioSpecs");
		return false;
	}

	else if (!InitDevice(_specs[Want], _specs[Have])) {
		return false;
	}

	_len = 0.f;
	_pos = 0u;
	_amplitude = 16000;
	_cycleTime = _specs[Have].freq / 60.f;
	this->SetCurFreq(defaultFreq);

	_initialized = true;
	return true;
}



void SdlSound::Dispose() noexcept
{
	if (_dev != 0)
	{
		SDL_CloseAudioDevice(_dev);
		_dev = 0;
	}

	if (_specs)
	{
		delete[] _specs;
		_specs = nullptr;
	}

	SDL_QuitSubSystem( SDL_INIT_AUDIO );
	_initialized = false;
}


bool SdlSound::IsInitialized() const noexcept
{
	return _initialized;
}



const char* SdlSound::GetPluginName() const noexcept
{
	return PLUGIN_NAME;
}



const char* SdlSound::GetPluginVersion() const noexcept
{
	return PLUGIN_VER;
}

PluginDeleter SdlSound::GetPluginDeleter() const noexcept
{
	return XCHIP_FreePlugin;
}



float SdlSound::GetSoundFreq() const noexcept 
{ 
	return this->GetCurFreq(); 
}


float SdlSound::GetCountdownFreq() const noexcept
{
	_SDLSOUND_INITIALIZED_ASSERT_();
	return _specs[Have].freq / _cycleTime;
}

bool SdlSound::IsPlaying() const  noexcept
{
	_SDLSOUND_INITIALIZED_ASSERT_();
	return SDL_GetAudioDeviceStatus(_dev) == SDL_AUDIO_PLAYING;
}


void SdlSound::SetCountdownFreq(const float hertz) noexcept 
{ 
	_SDLSOUND_INITIALIZED_ASSERT_(); 
	_cycleTime = _specs[Have].freq / hertz; 
}


void SdlSound::SetSoundFreq(const float hz) noexcept 
{ 
	_SDLSOUND_INITIALIZED_ASSERT_();
	this->SetCurFreq(hz); 
}



void SdlSound::Play(const uint8_t soundTimer) noexcept
{
	_SDLSOUND_INITIALIZED_ASSERT_();
	if (!this->IsPlaying()) 
	{
		SetPlayFreq(GetCurFreq() + 2 * soundTimer);
		SetLenght(soundTimer);
	}
	
	else
	{
		SDL_LockAudioDevice(_dev);
		SetPlayFreq(GetCurFreq() + 2 * soundTimer);
		SetLenght(soundTimer);
		SDL_UnlockAudioDevice(_dev);
	}

	SDL_PauseAudioDevice(_dev, 0);
}





void SdlSound::Stop() noexcept
{
 	_SDLSOUND_INITIALIZED_ASSERT_();
	if (this->IsPlaying())
	{
		SDL_LockAudioDevice(_dev);
		_len = 0;
		SDL_UnlockAudioDevice(_dev);
	}
}








bool SdlSound::InitDevice(SDL_AudioSpec& want, SDL_AudioSpec& have)
{
	using namespace xchip::utils::literals;

	memset(&want, 0, sizeof(SDL_AudioSpec));
	want.freq = 44100;
	want.format = AUDIO_S16;
	want.channels = 1;
	want.samples = 1024;
	want.callback = SdlSound::audio_callback<Sint16>;
	want.userdata = this;

	_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);

	if (_dev < 2) 
	{
		LogError("SdlSound: Failed to open audio device: %s", SDL_GetError());
		return false;
	}

	return true;
}










template<class T>
void SdlSound::audio_callback(void* userdata, uint8_t* const stream, const int len) noexcept
{
	auto *const _this = reinterpret_cast<SdlSound*>(userdata);
	auto *const buff = reinterpret_cast<T*>(stream);
	const size_t bufflen = len / sizeof(T);


	constexpr auto _2pi = static_cast<float>(2 * M_PI);
	const auto ampl = _this->_amplitude;
	const auto freq = _this->_playFreq;
	auto pos = _this->_pos;


	if (_this->_len > 0)
	{
		for (size_t i = 0; i < bufflen; ++i, ++pos)
			buff[i] = static_cast<T>(ampl * std::sin(_2pi * freq * pos));

		_this->_pos = pos;
		_this->_len -= bufflen;
	}

	else
	{	// need a better workaround to stop clipping.
		auto downAmpl = ampl;
		for (size_t i = 0; i < bufflen; ++i, ++pos)
		{
			buff[i] = static_cast<T>(downAmpl * std::sin(_2pi * freq * pos));
			if (downAmpl > 100)
				downAmpl -= 60;
		}

		// pause the device from the callback function.
		// is this ok ?
		_this->_pos = 0;
		SDL_PauseAudioDevice(_this->_dev, 1);
		
	}

}














extern "C" XCHIP_EXPORT iPlugin* XCHIP_LoadPlugin()
{
	return new(std::nothrow) SdlSound();
}




extern "C" XCHIP_EXPORT void XCHIP_FreePlugin(const iPlugin* plugin)
{
	const auto* sdlsound = dynamic_cast<const SdlSound*>(plugin);

	if(!sdlsound)
	{
		LogError("XCHIP_FreePlugin: dynamic_cast from iPlugin* to SdlSound* Failed");
		std::exit(EXIT_FAILURE);
	}

	delete sdlsound;
}

















}
