#include "overlay/dx12_hook.hpp"

#include "overlay/hud.hpp"
#include "util.hpp"

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <MinHook.h>

#include <vector>

namespace erstats {
namespace {

constexpr int kPresentIndex = 8;
constexpr int kResizeBuffersIndex = 13;
constexpr int kExecuteCommandListsIndex = 10;
constexpr UINT kBufferCount = 4;

using Present_t = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using ResizeBuffers_t = HRESULT(STDMETHODCALLTYPE*)(
    IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using ExecuteCommandLists_t = void(STDMETHODCALLTYPE*)(
    ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

Present_t g_original_present = nullptr;
ResizeBuffers_t g_original_resize = nullptr;
ExecuteCommandLists_t g_original_execute = nullptr;

ID3D12Device* g_device = nullptr;
ID3D12CommandQueue* g_queue = nullptr;
ID3D12DescriptorHeap* g_rtv_heap = nullptr;
ID3D12DescriptorHeap* g_srv_heap = nullptr;
ID3D12GraphicsCommandList* g_cmd_list = nullptr;
ID3D12Fence* g_fence = nullptr;
HANDLE g_fence_event = nullptr;
UINT64 g_fence_value = 0;
UINT g_rtv_size = 0;
HWND g_hwnd = nullptr;
bool g_imgui_ready = false;
UINT g_buffer_count = 0;

struct FrameContext {
    ID3D12CommandAllocator* allocator = nullptr;
    ID3D12Resource* render_target = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
};

std::vector<FrameContext> g_frames;

void wait_gpu() {
    if (g_queue == nullptr || g_fence == nullptr || g_fence_event == nullptr) {
        return;
    }
    const UINT64 value = ++g_fence_value;
    if (FAILED(g_queue->Signal(g_fence, value))) {
        return;
    }
    if (g_fence->GetCompletedValue() < value) {
        g_fence->SetEventOnCompletion(value, g_fence_event);
        WaitForSingleObject(g_fence_event, 100);
    }
}

void destroy_render_targets() {
    wait_gpu();
    for (auto& frame : g_frames) {
        if (frame.render_target) {
            frame.render_target->Release();
            frame.render_target = nullptr;
        }
    }
}

void create_render_targets(IDXGISwapChain3* swap) {
    DXGI_SWAP_CHAIN_DESC desc{};
    swap->GetDesc(&desc);
    g_buffer_count = desc.BufferCount;
    if (g_buffer_count == 0 || g_buffer_count > 8) {
        g_buffer_count = kBufferCount;
    }
    g_frames.resize(g_buffer_count);

    D3D12_CPU_DESCRIPTOR_HANDLE handle = g_rtv_heap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < g_buffer_count; ++i) {
        ID3D12Resource* resource = nullptr;
        if (SUCCEEDED(swap->GetBuffer(i, IID_PPV_ARGS(&resource)))) {
            g_device->CreateRenderTargetView(resource, nullptr, handle);
            g_frames[i].render_target = resource;
            g_frames[i].rtv = handle;
            handle.ptr += g_rtv_size;
        }
    }
}

void cleanup_imgui() {
    if (g_imgui_ready) {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_imgui_ready = false;
    }
    destroy_render_targets();
    for (auto& frame : g_frames) {
        if (frame.allocator) {
            frame.allocator->Release();
            frame.allocator = nullptr;
        }
    }
    g_frames.clear();
    if (g_cmd_list) {
        g_cmd_list->Release();
        g_cmd_list = nullptr;
    }
    if (g_rtv_heap) {
        g_rtv_heap->Release();
        g_rtv_heap = nullptr;
    }
    if (g_srv_heap) {
        g_srv_heap->Release();
        g_srv_heap = nullptr;
    }
    if (g_fence) {
        g_fence->Release();
        g_fence = nullptr;
    }
    if (g_fence_event) {
        CloseHandle(g_fence_event);
        g_fence_event = nullptr;
    }
    if (g_device) {
        g_device->Release();
        g_device = nullptr;
    }
}

bool init_imgui(IDXGISwapChain* swap_chain) {
    if (g_queue == nullptr) {
        return false;
    }
    IDXGISwapChain3* swap3 = nullptr;
    if (FAILED(swap_chain->QueryInterface(IID_PPV_ARGS(&swap3)))) {
        return false;
    }

    DXGI_SWAP_CHAIN_DESC desc{};
    swap3->GetDesc(&desc);
    g_hwnd = desc.OutputWindow;

    if (FAILED(swap_chain->GetDevice(IID_PPV_ARGS(&g_device)))) {
        swap3->Release();
        return false;
    }

    g_rtv_size = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC rtv_desc{};
    rtv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_desc.NumDescriptors = kBufferCount;
    rtv_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(g_device->CreateDescriptorHeap(&rtv_desc, IID_PPV_ARGS(&g_rtv_heap)))) {
        swap3->Release();
        return false;
    }

    D3D12_DESCRIPTOR_HEAP_DESC srv_desc{};
    srv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srv_desc.NumDescriptors = 1;
    srv_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(g_device->CreateDescriptorHeap(&srv_desc, IID_PPV_ARGS(&g_srv_heap)))) {
        swap3->Release();
        return false;
    }

    g_frames.resize(kBufferCount);
    for (auto& frame : g_frames) {
        if (FAILED(g_device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.allocator)))) {
            swap3->Release();
            return false;
        }
    }
    if (FAILED(g_device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_frames[0].allocator, nullptr,
            IID_PPV_ARGS(&g_cmd_list)))) {
        swap3->Release();
        return false;
    }
    g_cmd_list->Close();

    if (FAILED(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)))) {
        swap3->Release();
        return false;
    }
    g_fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);

    create_render_targets(swap3);
    swap3->Release();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    const char* font_paths[] = {
        "C:\\Windows\\Fonts\\constan.ttf",
        "C:\\Windows\\Fonts\\georgia.ttf",
        "C:\\Windows\\Fonts\\times.ttf",
    };
    for (const char* path : font_paths) {
        if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
            continue;
        }
        io.Fonts->AddFontFromFileTTF(path, 16.0f);
        io.Fonts->AddFontFromFileTTF(path, 34.0f);
        break;
    }

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX12_Init(
        g_device,
        static_cast<int>(kBufferCount),
        desc.BufferDesc.Format,
        g_srv_heap,
        g_srv_heap->GetCPUDescriptorHandleForHeapStart(),
        g_srv_heap->GetGPUDescriptorHandleForHeapStart());
    g_imgui_ready = true;
    log_info("D3D12 overlay initialized");
    return true;
}

void render_overlay(IDXGISwapChain* swap_chain) {
    if (!g_imgui_ready && !init_imgui(swap_chain)) {
        return;
    }
    IDXGISwapChain3* swap3 = nullptr;
    if (FAILED(swap_chain->QueryInterface(IID_PPV_ARGS(&swap3)))) {
        return;
    }

    const UINT index = swap3->GetCurrentBackBufferIndex();
    if (index >= g_frames.size() || g_frames[index].render_target == nullptr) {
        swap3->Release();
        return;
    }
    auto& frame = g_frames[index];
    frame.allocator->Reset();
    g_cmd_list->Reset(frame.allocator, nullptr);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = frame.render_target;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    g_cmd_list->ResourceBarrier(1, &barrier);
    g_cmd_list->OMSetRenderTargets(1, &frame.rtv, FALSE, nullptr);
    g_cmd_list->SetDescriptorHeaps(1, &g_srv_heap);

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    draw_death_hud();
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_cmd_list);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    g_cmd_list->ResourceBarrier(1, &barrier);
    g_cmd_list->Close();
    ID3D12CommandList* lists[] = {g_cmd_list};
    g_queue->ExecuteCommandLists(1, lists);
    swap3->Release();
}

HRESULT STDMETHODCALLTYPE hooked_present(IDXGISwapChain* swap, UINT sync, UINT flags) {
    render_overlay(swap);
    return g_original_present(swap, sync, flags);
}

HRESULT STDMETHODCALLTYPE hooked_resize(
    IDXGISwapChain* swap, UINT count, UINT w, UINT h, DXGI_FORMAT format, UINT flags) {
    if (g_imgui_ready) {
        ImGui_ImplDX12_InvalidateDeviceObjects();
        destroy_render_targets();
    }
    const HRESULT hr = g_original_resize(swap, count, w, h, format, flags);
    if (g_imgui_ready) {
        IDXGISwapChain3* swap3 = nullptr;
        if (SUCCEEDED(swap->QueryInterface(IID_PPV_ARGS(&swap3)))) {
            create_render_targets(swap3);
            swap3->Release();
        }
        ImGui_ImplDX12_CreateDeviceObjects();
    }
    return hr;
}

void STDMETHODCALLTYPE hooked_execute(
    ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists) {
    if (g_queue == nullptr && queue != nullptr) {
        D3D12_COMMAND_QUEUE_DESC desc = queue->GetDesc();
        if (desc.Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
            g_queue = queue;
        }
    }
    g_original_execute(queue, count, lists);
}

bool steal_vtables(void** present, void** resize, void** execute) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"ERStatsShareDummy";
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowW(
        wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance,
        nullptr);
    if (hwnd == nullptr) {
        return false;
    }

    IDXGIFactory4* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return false;
    }

    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
    ID3D12Device* device = nullptr;
    if (FAILED(D3D12CreateDevice(nullptr, level, IID_PPV_ARGS(&device)))) {
        factory->Release();
        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return false;
    }

    D3D12_COMMAND_QUEUE_DESC qdesc{};
    qdesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ID3D12CommandQueue* queue = nullptr;
    if (FAILED(device->CreateCommandQueue(&qdesc, IID_PPV_ARGS(&queue)))) {
        device->Release();
        factory->Release();
        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 scd{};
    scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.SampleDesc.Count = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 2;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.Width = 2;
    scd.Height = 2;
    IDXGISwapChain1* swap1 = nullptr;
    const HRESULT hr = factory->CreateSwapChainForHwnd(queue, hwnd, &scd, nullptr, nullptr, &swap1);
    if (FAILED(hr) || swap1 == nullptr) {
        queue->Release();
        device->Release();
        factory->Release();
        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return false;
    }

    void** swap_vtbl = *reinterpret_cast<void***>(swap1);
    void** queue_vtbl = *reinterpret_cast<void***>(queue);
    *present = swap_vtbl[kPresentIndex];
    *resize = swap_vtbl[kResizeBuffersIndex];
    *execute = queue_vtbl[kExecuteCommandListsIndex];

    swap1->Release();
    queue->Release();
    device->Release();
    factory->Release();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return true;
}

}  // namespace

bool install_overlay() {
    void* present = nullptr;
    void* resize = nullptr;
    void* execute = nullptr;
    if (!steal_vtables(&present, &resize, &execute)) {
        log_error("failed to discover D3D12 vtables");
        return false;
    }
    if (MH_CreateHook(present, reinterpret_cast<LPVOID>(&hooked_present), reinterpret_cast<LPVOID*>(&g_original_present))
        != MH_OK) {
        log_error("failed to hook Present");
        return false;
    }
    if (MH_CreateHook(resize, reinterpret_cast<LPVOID>(&hooked_resize), reinterpret_cast<LPVOID*>(&g_original_resize))
        != MH_OK) {
        log_error("failed to hook ResizeBuffers");
        return false;
    }
    if (MH_CreateHook(
            execute, reinterpret_cast<LPVOID>(&hooked_execute),
            reinterpret_cast<LPVOID*>(&g_original_execute))
        != MH_OK) {
        log_error("failed to hook ExecuteCommandLists");
        return false;
    }
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        log_error("failed to enable overlay hooks");
        return false;
    }
    log_info("overlay hooks installed");
    return true;
}

void shutdown_overlay() {
    cleanup_imgui();
}

}  // namespace erstats
