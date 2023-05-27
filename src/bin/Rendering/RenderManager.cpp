#include "RenderManager.h"

#include <d3d11.h>

#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <dxgi.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
// stole this from MaxSu's detection meter
#include "include/lib/imgui_freetype.h"

#include "bin/Wheeler/Wheeler.h"
#include "bin/Rendering/TextureManager.h"

#include "bin/Animation/TimeInterpolator/TimeInterpolatorManager.h"


namespace stl
{
	using namespace SKSE::stl;

	template <class T>
	void write_thunk_call()
	{
		auto& trampoline = SKSE::GetTrampoline();
		const REL::Relocation<std::uintptr_t> hook{ T::id, T::offset };
		T::func = trampoline.write_call<5>(hook.address(), T::thunk);
	}
}


LRESULT RenderManager::WndProcHook::thunk(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	auto& io = ImGui::GetIO();
	if (uMsg == WM_KILLFOCUS) {
		io.ClearInputCharacters();
		io.ClearInputKeys();
	}

	return func(hWnd, uMsg, wParam, lParam);
}

void RenderManager::D3DInitHook::thunk()
{
	func();

	INFO("D3DInit Hooked!");
	auto render_manager = RE::BSRenderManager::GetSingleton();
	if (!render_manager) {
		ERROR("Cannot find render manager. Initialization failed!");
		return;
	}

	auto render_data = render_manager->GetRuntimeData();

	INFO("Getting swapchain...");
	auto swapchain = render_data.swapChain;
	if (!swapchain) {
		ERROR("Cannot find swapchain. Initialization failed!");
		return;
	}

	INFO("Getting swapchain desc...");
	DXGI_SWAP_CHAIN_DESC sd{};
	if (swapchain->GetDesc(std::addressof(sd)) < 0) {
		ERROR("IDXGISwapChain::GetDesc failed.");
		return;
	}

	device = render_data.forwarder;
	Texture::device_ = device;
	context = render_data.context;

	INFO("Initializing ImGui...");
	ImGui::CreateContext();
	if (!ImGui_ImplWin32_Init(sd.OutputWindow)) {
		ERROR("ImGui initialization failed (Win32)");
		return;
	}
	if (!ImGui_ImplDX11_Init(device, context)) {
		ERROR("ImGui initialization failed (DX11)");
		return;
	}

	INFO("ImGui initialized!");

	initialized.store(true);

	WndProcHook::func = reinterpret_cast<WNDPROC>(
		SetWindowLongPtrA(
			sd.OutputWindow,
			GWLP_WNDPROC,
			reinterpret_cast<LONG_PTR>(WndProcHook::thunk)));
	if (!WndProcHook::func)
		ERROR("SetWindowLongPtrA failed!");
	ImFontAtlas* atlas = ImGui::GetIO().Fonts;
	atlas->FontBuilderIO = ImGuiFreeType::GetBuilderForFreeType();
	atlas->FontBuilderFlags =  ImGuiFreeTypeBuilderFlags_LightHinting;
	std::string path = R"(Data\SKSE\Plugins\wheeler\resources\fonts\SovngardeLight.ttf)";
	auto file_path = std::filesystem::path(path);
	ImGui::GetIO().Fonts->AddFontFromFileTTF(file_path.string().c_str(), 64.0f, NULL, ImGui::GetIO().Fonts->GetGlyphRangesChineseFull());

}

void RenderManager::DXGIPresentHook::thunk(std::uint32_t a_p1)
{
	func(a_p1);

	if (!D3DInitHook::initialized.load())
		return;

	// prologue
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// do stuff
	RenderManager::draw();

	// epilogue
	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

struct ImageSet
{
	std::int32_t my_image_width = 0;
	std::int32_t my_image_height = 0;
	ID3D11ShaderResourceView* my_texture = nullptr;
};


void RenderManager::MessageCallback(SKSE::MessagingInterface::Message* msg)  //CallBack & LoadTextureFromFile should called after resource loaded.
{
	if (msg->type == SKSE::MessagingInterface::kDataLoaded && D3DInitHook::initialized) {
		// Read Texture only after game engine finished load all it renderer resource.
		auto& io = ImGui::GetIO();
		io.MouseDrawCursor = true;
		io.WantSetMousePos = true;
	}
}

bool RenderManager::Install()
{
	auto g_message = SKSE::GetMessagingInterface();
	if (!g_message) {
		ERROR("Messaging Interface Not Found!");
		return false;
	}

	g_message->RegisterListener(MessageCallback);

	SKSE::AllocTrampoline(14 * 2);

	stl::write_thunk_call<D3DInitHook>();
	stl::write_thunk_call<DXGIPresentHook>();

	
	return true;
}



float RenderManager::GetResolutionScaleWidth()
{
	return ImGui::GetIO().DisplaySize.x / 1920.f;
}

float RenderManager::GetResolutionScaleHeight()
{
	return ImGui::GetIO().DisplaySize.y / 1080.f;
}


void RenderManager::draw()
{

	// Add UI elements here
	float deltaTime = ImGui::GetIO().DeltaTime;
	Wheeler::Update(deltaTime);
	TimeFloatInterpolatorManager::Update(deltaTime);

}
