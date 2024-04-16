#include <iostream>
#include <d3d12.h>
#include <d3dcompiler.h>
#include "d3dx12.h"
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <wrl.h>

#include <assert.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace Microsoft::WRL;
using namespace DirectX;


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#define DXCall(call) assert(SUCCEEDED(call))

void load_shader(LPCWSTR fileName, ID3DBlob** bytecode_ptr)
{
    DXCall(D3DReadFileToBlob(fileName, bytecode_ptr));
}

int main()
{
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = NULL;
    wc.lpszClassName = L"Hazelight";
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, L"Hazelight", L"DirectX12", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, NULL, NULL);
    ShowWindow(hwnd, SW_SHOW);

    ComPtr<IDXGIFactory6> factory;
    DXCall(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));

    ComPtr<IDXGIAdapter> adapter;
    DXCall(factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)));

    ComPtr<ID3D12Device> device;
    DXCall(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device)));

    D3D12_COMMAND_QUEUE_DESC queue_desc = { D3D12_COMMAND_LIST_TYPE_DIRECT };
    ComPtr<ID3D12CommandQueue> queue;
    DXCall(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue)));

    DXGI_SWAP_CHAIN_DESC1 swap_desc = {};
    swap_desc.BufferCount = 3;
    swap_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_desc.Width = 800;
    swap_desc.Height = 600;
    swap_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_desc.SampleDesc.Count = 1;
    swap_desc.SampleDesc.Quality = 0;
    swap_desc.Scaling = DXGI_SCALING_NONE;
    swap_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ComPtr<IDXGISwapChain1> temp_swap_chain;
    DXCall(factory->CreateSwapChainForHwnd(queue.Get(), hwnd, &swap_desc, NULL, NULL, temp_swap_chain.GetAddressOf()));

    ComPtr<IDXGISwapChain3> swap_chain;
    DXCall(temp_swap_chain.As(&swap_chain));

    ComPtr<ID3D12CommandAllocator> allocator;
    DXCall(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));

    UINT frame_index = swap_chain->GetCurrentBackBufferIndex();

    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
    rtv_heap_desc.NumDescriptors = 3;
    rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

   UINT rtv_desc_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    ComPtr<ID3D12DescriptorHeap> desc_heap;
    DXCall(device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&desc_heap)));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(desc_heap->GetCPUDescriptorHandleForHeapStart());
    ComPtr<ID3D12Resource> render_targets[3];
    for (int i = 0; i < 3; i++)
    {
        DXCall(swap_chain->GetBuffer(i, IID_PPV_ARGS(&render_targets[i])));
        device->CreateRenderTargetView(render_targets[i].Get(), nullptr, rtv_handle);
        rtv_handle.Offset(1, rtv_desc_size);
    }

    CD3DX12_ROOT_SIGNATURE_DESC root_sginature_desc;
    root_sginature_desc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    DXCall(D3D12SerializeRootSignature(&root_sginature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));

    ComPtr<ID3D12RootSignature> root_signature;
    DXCall(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));

    ComPtr<ID3DBlob> vertex_shader;
    ComPtr<ID3DBlob> pixel_shader;

    load_shader(L"../Data/Shaders/PosColor.VS.cso", &vertex_shader);
    load_shader(L"../Data/Shaders/PosColor.PS.cso", &pixel_shader);

    D3D12_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
    pso_desc.InputLayout = { layout, _countof(layout) };
    pso_desc.pRootSignature = root_signature.Get();
    pso_desc.VS = { reinterpret_cast<UINT8*>(vertex_shader->GetBufferPointer()), vertex_shader->GetBufferSize() };
    pso_desc.PS = { reinterpret_cast<UINT8*>(pixel_shader->GetBufferPointer()), pixel_shader->GetBufferSize() };
    pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pso_desc.DepthStencilState.DepthEnable = FALSE;
    pso_desc.DepthStencilState.StencilEnable = FALSE;
    pso_desc.SampleMask = UINT_MAX;
    pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso_desc.NumRenderTargets = 3;
    pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso_desc.SampleDesc.Count = 1;

    ComPtr<ID3D12PipelineState> pipeline;
    DXCall(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pipeline)));

    ComPtr<ID3D12GraphicsCommandList> cmd_list;
    DXCall(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), pipeline.Get(), IID_PPV_ARGS(&cmd_list)));
    DXCall(cmd_list->Close());

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        DXCall(allocator->Reset());
        DXCall(cmd_list->Reset(allocator.Get(), pipeline.Get()));

        cmd_list->SetGraphicsRootSignature(root_signature.Get());

        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(render_targets[frame_index].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmd_list->ResourceBarrier(1, &barrier);

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(desc_heap->GetCPUDescriptorHandleForHeapStart(), frame_index, rtv_desc_size);
        cmd_list->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);

        const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
        cmd_list->ClearRenderTargetView(rtv_handle, clearColor, 0, nullptr);

        cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        barrier = CD3DX12_RESOURCE_BARRIER::Transition(render_targets[frame_index].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        cmd_list->ResourceBarrier(1, &barrier);

        DXCall(cmd_list->Close());

        ID3D12CommandList* ppCommandLists[] = { cmd_list.Get() };
        queue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

        DXCall(swap_chain->Present(1, 0));

        frame_index = swap_chain->GetCurrentBackBufferIndex();
    }

	return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CLOSE:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}