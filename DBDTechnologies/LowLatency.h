#pragma once
#include "Utilities.h"






/*
	NVIDIA Reflex on GeForce, AMD Anti-Lag 2 on Radeon (Intel's XeLL is DX12 only).
	Reflex markers follow the official Unreal Engine plugin:
	  SIMULATION_START     game thread, UEngine::UpdateTimeAndHandleMaxTickRate, after Sleep
	  SIMULATION_END       game thread, after UGameEngine::Tick
	  RENDERSUBMIT_*       render thread, BeginFrame / EndFrame render commands of FEngineLoop::Tick
	  PRESENT_*            RHI thread, around RHIEndDrawingViewport
	The frame ID is GFrameCounter.
*/
class LowLatency
{
public:
	enum Mode : int32_t
	{
		Off = 0,
		On = 1,
		Boost = 2,   // NVIDIA only
	};


private:
	enum class Backend
	{
		Unknown,     // No device yet
		None,
		Reflex,
		AntiLag2,
	};

	/* UEngine::UpdateTimeAndHandleMaxTickRate in the UEngine, UGameEngine and game engine vtables */
	static constexpr uintptr_t RVA_UPDATE_TIME = 0x6FAF3A0;
	static constexpr uintptr_t RVA_UPDATE_TIME_SLOTS[3] = { 0x8F96330, 0x9A2C258, 0x9A2FCE0 };

	/* UGameEngine::Tick; the game engine class has a jump thunk to it */
	static constexpr uintptr_t RVA_GAME_TICK = 0x6B2EE60;
	static constexpr uintptr_t RVA_GAME_TICK_THUNK = 0x4288330;
	static constexpr uintptr_t RVA_GAME_TICK_SLOTS[2] = { 0x9A2FCD8, 0x8F96328 };

	/* TGraphTask<FEngineLoop::Tick BeginFrame / EndFrame render command>::ExecuteTask, vtable slot 1 */
	static constexpr uintptr_t RVA_BEGIN_FRAME_TASK = 0x30942B0;
	static constexpr uintptr_t RVA_BEGIN_FRAME_SLOT = 0x8AC0CC0;
	static constexpr uintptr_t RVA_END_FRAME_TASK = 0x30943A0;
	static constexpr uintptr_t RVA_END_FRAME_SLOT = 0x8AC0D10;
	static constexpr uintptr_t OFF_TASK_FRAME_COUNTER = 0x10;   // CurrentFrameCounter captured by the lambda

	/* GFrameCounter, verified through "mov rbx, [GFrameCounter]" in FEngineLoop::Tick */
	static constexpr uintptr_t RVA_FRAME_COUNTER = 0xB4D3F68;
	static constexpr uintptr_t RVA_FRAME_COUNTER_USE = 0x308EDEF;

	using UpdateTime_t = void(__fastcall*)(void* engine);
	using GameTick_t = void(__fastcall*)(void* engine, float deltaSeconds, bool idleMode);
	using ExecuteTask_t = void(__fastcall*)(void* task, void* newTasks, int currentThread);

	static inline UpdateTime_t originalUpdateTime = nullptr;
	static inline GameTick_t originalGameTick = nullptr;
	static inline ExecuteTask_t originalBeginFrame = nullptr;
	static inline ExecuteTask_t originalEndFrame = nullptr;
	static inline volatile uint64_t* frameCounter = nullptr;
	static inline bool markerHooks = false;

	static inline ID3D11Device* volatile device = nullptr;
	static inline Backend backend = Backend::Unknown;
	static inline bool failed = false;

	static inline bool nvapiTried = false;
	static inline bool nvapiReady = false;
	static inline int32_t appliedMode = -1;
	static inline int32_t appliedMarkers = -1;
	static inline volatile bool markersActive = false;
	static inline volatile LONG64 latestRenderFrame = 0;
	static inline LONG64 lastPresentFrame = 0;
	static inline uint64_t presentFrame = 0;

	static inline void* antiLagContext = nullptr;   // AMD::AntiLag2DX11::Context
	static inline bool antiLagTried = false;

	static void __fastcall HookUpdateTime(void* engine);
	static void __fastcall HookGameTick(void* engine, float deltaSeconds, bool idleMode);
	static void __fastcall HookBeginFrame(void* task, void* newTasks, int currentThread);
	static void __fastcall HookEndFrame(void* task, void* newTasks, int currentThread);

	static void DetectBackend();
	static bool EnsureNvapi();
	static void ApplyReflexMode();
	static void UpdateAntiLag();
	static void Marker(int type, uint64_t frame);
	static uint64_t GameFrame();


public:
	static inline int32_t mode = Off;    // r.LowLatency.Mode
	static inline int32_t markers = 1;   // r.LowLatency.Markers

	/* Called at the start of every game thread frame */
	static inline void (*gameThreadCallback)() = nullptr;

	static bool InstallHooks(uintptr_t base);

	/* The backend is chosen by the GPU vendor of this device */
	static void SetDevice(ID3D11Device* device);

	static void ValidateMode();
	static const char* StatusText();

	/* RHI thread, around Present */
	static void PresentStart();
	static void PresentEnd();
};
